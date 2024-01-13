#pragma once


#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <array>
#include <cmath>

#include "Socket.hpp"
#include "compression/deflate_decompress.h"
#include "DES.hpp"


// Client-to-Server-message types:
// 0 -> setPixelFormat
// 2 -> setEncodings
// 3 -> framebufferUpdateRequest
// 4 -> keyEvent
// 5 -> pointerEvent
// 6 -> clientCutText


// Server-to-Client-message types:
// 0 -> FramebufferUpdate
// 1 -> SetColorMapEntries
// 2 -> Bell
// 3 -> ServerCutText


class VNC {
private:
	struct PixelFormat {
		uint8_t bits_per_pixel;
		uint8_t depth;
		uint8_t big_endian_flag;
		uint8_t true_color_flag;
		uint16_t red_max;
		uint16_t green_max;
		uint16_t blue_max;
		uint8_t red_shift;
		uint8_t green_shift;
		uint8_t blue_shift;
	};

	struct ServerInit {
		uint16_t fbWidth;
		uint16_t fbHeight;
		PixelFormat pixelFormat;
		uint32_t nameLength;
		std::string name;
	};

	struct RectHeader {
		uint16_t pos_x;
		uint16_t pos_y;
		uint16_t width;
		uint16_t height;
		enum class EncodingType : int32_t {
			RAW = 0,
			COPYRECT = 1,
			RRE = 2,
			HEXTILE = 5,
			TRLE = 15,
			ZRLE = 16,
			CURSOR_PSEUDOENCODING = -239,
			DESKTOPSIZE_PSEUDOENCODING = -223,
		} encoding_type;
	};

private:
	Socket sock;

	std::string name;

	ServerInit serverInit;
	uint16_t fb_width, fb_height;
	uint8_t* pixelData;

public:
	inline VNC(const std::string& host, const uint16_t port):
			sock(host, port) {

		printf("socket connection succeeded\n");

		// Choose RFP Version:
		const std::string& serverRFPVersion = sock.recvString(12);
		std::cout << "Server Version: " << serverRFPVersion << "\n";

		const char* version = "RFB 003.008\n";
		sock.send(version, 12);

		// Choose Security Type:
		const uint8_t numSecurityTypes = sock.recvU8(); // receive number of security types
		std::cout << "Num security types: " << static_cast<int>(numSecurityTypes) << "\n";

		std::unordered_set<uint8_t> securityTypes;
		for(size_t i = 0; i < numSecurityTypes; i++)
			securityTypes.insert(sock.recvU8());
		// sock.recvExactly(securityTypes.data(), numSecurityTypes);
		for(const uint8_t& securityType : securityTypes)
			std::cout << " - SecurityType <" << static_cast<int>(securityType) << "> received\n";
		
		if(securityTypes.contains(1)) { // None (no authentication required)
			std::cout << "Authentication: None\n";

			sock.sendU8(1); // send security type 1 (None)
		} else if(securityTypes.contains(2)) { // VNC Authentication (DES challenge)
			std::cout << "Authentication: VNC\n";

			sock.sendU8(2); // send security type 2 (VNC authentication)

			// Authenticate using VNC challenge:
			std::array<uint8_t, 16> vncChallenge;
			sock.recvExactly(vncChallenge.data(), 16); // receive random 16-byte vnc-auth challenge

			std::array<uint8_t, 16> vncChallengeEncrypted = desEncrypt(vncChallenge, "#Benedik"); // Encrypt vnc-auth challenge
			sock.send(vncChallengeEncrypted.data(), 16); // reply to vnc-auth challenge
			std::cout << "Sent encrypted challenge back to server...\n";
		}

		const uint32_t securityResult = sock.recvU32(); // get SecurityResult handshake
		if(securityResult != 0)
			throw std::runtime_error("Security Handshake failed!"); // TODO: catch and show reason sent by server
		std::cout << "SecurityResult Handshake: PASSED\n";


		// Exchange init messages:
		sock.sendU8(1); // send ClientInit (shared: true -> allow sharing session with other clients)

		const ServerInit serverInit = recvServerInit();
		pixelData = new uint8_t[serverInit.fbWidth * serverInit.fbHeight * 4]; // TODO: correct bytes per pixel

		fb_width = serverInit.fbWidth;
		fb_height = serverInit.fbHeight;

		if(serverInit.pixelFormat.true_color_flag == false)
			throw std::runtime_error("Server does not support true_color. aborting.");

		std::cout << "ServerInit:\n"
			<< " - Width: " << serverInit.fbWidth << "  Height: " << serverInit.fbHeight << "  name: " << serverInit.name << "\n"
			<< " - Pixel Format:\n"
				<< "   - Bits per Pixel: " << (int)serverInit.pixelFormat.bits_per_pixel << "  depth: " << (int)serverInit.pixelFormat.depth << "\n"
				<< "   - big_endian: " << std::boolalpha << (bool)serverInit.pixelFormat.big_endian_flag << "  true_color: " << (bool)serverInit.pixelFormat.true_color_flag << "\n"
				<< "   - red_max: " << (int)serverInit.pixelFormat.red_max << "  green_max: " << (int)serverInit.pixelFormat.green_max << "  blue_max: " << (int)serverInit.pixelFormat.blue_max << "\n"
				<< "   - red_shift: " << (int)serverInit.pixelFormat.red_shift << "  green_shift: " << (int)serverInit.pixelFormat.green_shift << "  blue_shift: " << (int)serverInit.pixelFormat.blue_shift << "\n";

		// setPixelFormat not necessary, will use default server settings...

		// setEncodings:
		std::vector<int32_t> encodings;
		encodings.push_back(0); // Raw
		encodings.push_back(1); // CopyRect
		// encodings.push_back(2); // RRE
		// encodings.push_back(5); // Hextile
		// encodings.push_back(15); // TRLE
		encodings.push_back(16); // ZRLE
		encodings.push_back(-239); // cursor pseudoencoding
		// encodings.push_back(-223); // desktopsize pseudoencoding

		sock.sendU8(2); // Type (= setEncodings)
		sock.sendU8(0); // padding
		sock.sendU16(encodings.size()); // numberOfEncodings: 1
		for(const int32_t& enc : encodings)
			sock.sendS32(enc);
	}


	// ---- Sending ----

	inline void sendUpdateRequest(const size_t posX, const size_t posY, const size_t width, const size_t height, const bool incremental = true) const {
		sock.sendPrimitive<uint8_t>(3); // MessageType (3 = FrameBufferUpdateRequest)
		sock.sendPrimitive<uint8_t>(incremental);
		sock.sendPrimitive<uint16_t>(htons(posX));
		sock.sendPrimitive<uint16_t>(htons(posY));
		sock.sendPrimitive<uint16_t>(htons(width));
		sock.sendPrimitive<uint16_t>(htons(height));
	}

	inline void sendPointerEvent(const uint16_t posX, const uint16_t posY, const uint8_t buttonMask) const {
		sock.sendPrimitive<uint8_t>(5); // MessageType (5 = PointerEvent)
		sock.sendPrimitive<uint8_t>(buttonMask);
		sock.sendPrimitive<uint16_t>(htons(posX));
		sock.sendPrimitive<uint16_t>(htons(posY));
	}

	inline void sendKeyEvent(const bool downFlag, const uint32_t key) const {
		sock.sendPrimitive<uint8_t>(4); // MessageType (4 = KeyEvent)
		sock.sendPrimitive<uint8_t>(downFlag);
		sock.sendPrimitive<uint16_t>(0); // padding
		sock.sendPrimitive<uint32_t>(htonl(key));
	}


	// ---- Receiving ----

	inline ServerInit recvServerInit() const {
		ServerInit serverInit;
		serverInit.fbWidth = ntohs(sock.recvPrimitive<uint16_t>());
		serverInit.fbHeight = ntohs(sock.recvPrimitive<uint16_t>());
		serverInit.pixelFormat = recvPixelFormat();
		serverInit.nameLength = ntohl(sock.recvPrimitive<uint32_t>());
		serverInit.name = sock.recvString(serverInit.nameLength);
		return serverInit;
	}

	inline PixelFormat recvPixelFormat() const {
		PixelFormat pixelFormat;
		pixelFormat.bits_per_pixel  = sock.recvPrimitive<uint8_t>();
		pixelFormat.depth           = sock.recvPrimitive<uint8_t>();
		pixelFormat.big_endian_flag = sock.recvPrimitive<uint8_t>();
		pixelFormat.true_color_flag = sock.recvPrimitive<uint8_t>();
		pixelFormat.red_max   = ntohs(sock.recvPrimitive<uint16_t>());
		pixelFormat.green_max = ntohs(sock.recvPrimitive<uint16_t>());
		pixelFormat.blue_max  = ntohs(sock.recvPrimitive<uint16_t>());
		pixelFormat.red_shift   = sock.recvPrimitive<uint8_t>();
		pixelFormat.green_shift = sock.recvPrimitive<uint8_t>();
		pixelFormat.blue_shift  = sock.recvPrimitive<uint8_t>();

		for(size_t i = 0; i < 3; i++)
			sock.recvPrimitive<uint8_t>(); // receive padding

		return pixelFormat;
	}

	inline void recvUpdates() {
		if(!sock.dataAvailable()) return;

		enum class MessageType : uint8_t {
			FRAMEBUFFER_UPDATE = 0,
			SET_COLORMAP_ENTRIES = 1,
			BELL = 2,
			SERVER_CUT_TEXT = 3
		} messageType;
		sock.recvExactly(&messageType, 1); // receive FrameBufferUpdate

		switch(messageType) {
			case MessageType::FRAMEBUFFER_UPDATE: {
				const uint16_t numRects = recvFrameBufferUpdate();

				// std::cout << "Got FrameBufferUpdate\n";
				// std::cout << " - Num Rectangles: " << numRects << "\n";

				for(size_t i = 0; i < numRects; i++) {
					const RectHeader& rectHeader = recvUpdateRectHeader();

					// std::cout << "Rect:\n"
					// 	<< "  pos_x: " << rectHeader.pos_x << " pos_y: " << rectHeader.pos_y << "\n"
					// 	<< "  width: " << rectHeader.width << " height: " << rectHeader.height << "\n"
					// 	<< "  encoding_type: " << static_cast<int>(rectHeader.encoding_type) << "\n";
					
					recvUpdateRect(rectHeader);
				}
			} break;

			case MessageType::SET_COLORMAP_ENTRIES: {
				throw std::runtime_error("Received SET_COLORMAP_ENTRIES. aborting");
			} break;

			case MessageType::BELL: {
				std::cout << "<BELL>\n";
			} break;

			case MessageType::SERVER_CUT_TEXT: {
				std::cout << "Received Clipboard \"" << recvServerClipboard() << "\"\n";
			} break;

			default:
				throw std::runtime_error("Received update of non-zero type " + std::to_string(static_cast<uint8_t>(messageType)));
		}
	}

	inline uint16_t recvFrameBufferUpdate() {
		sock.recvPrimitive<uint8_t>(); // receive padding

		const uint16_t numRectangles = ntohs(sock.recvPrimitive<uint16_t>());

		return numRectangles;
	}

	inline RectHeader recvUpdateRectHeader() {
		RectHeader rectHeader;
		rectHeader.pos_x  = ntohs(sock.recvPrimitive<uint16_t>());
		rectHeader.pos_y  = ntohs(sock.recvPrimitive<uint16_t>());
		rectHeader.width  = ntohs(sock.recvPrimitive<uint16_t>());
		rectHeader.height = ntohs(sock.recvPrimitive<uint16_t>());
		rectHeader.encoding_type = static_cast<RectHeader::EncodingType>(ntohl(sock.recvPrimitive<int32_t>()));
		return rectHeader;
	}

	inline void recvUpdateRect(const RectHeader& rectHeader) {
		switch(rectHeader.encoding_type) {
			case RectHeader::EncodingType::RAW:
				recvUpdateRectRAW(rectHeader);
				break;
			case RectHeader::EncodingType::COPYRECT:
				recvUpdateRectCOPYRECT(rectHeader);
				break;
			case RectHeader::EncodingType::RRE:
				throw std::runtime_error("Received UpdateRect message of encoding type RRE");
				break;
			case RectHeader::EncodingType::HEXTILE:
				throw std::runtime_error("Received UpdateRect message of encoding type HEXTILE");
				break;
			case RectHeader::EncodingType::TRLE:
				throw std::runtime_error("Received UpdateRect message of encoding type TRLE");
				// std::cout << "Received UpdateRect message of encoding type TRLE\n";
				// recvUpdateRectTRLE(rectHeader);
				break;
			case RectHeader::EncodingType::ZRLE:
				// throw std::runtime_error("Received UpdateRect message of encoding type ZRLE");
				// std::cout << "Received UpdateRect message of encoding type ZRLE\n";
				recvUpdateRectZRLE(rectHeader);
				break;
			case RectHeader::EncodingType::CURSOR_PSEUDOENCODING:
				std::cout << "Received UpdateRect message of encoding type CURSOR_PSEUDOENCODING\n";
				for(size_t i = 0; i < rectHeader.width * rectHeader.height * 4; i++) sock.recvU8(); // read cursor-pixels
				for(size_t i = 0; i < (rectHeader.width + 7) / 8 * rectHeader.height; i++) sock.recvU8(); // read cursor-bitmask
				break;
			case RectHeader::EncodingType::DESKTOPSIZE_PSEUDOENCODING:
				throw std::runtime_error("Received UpdateRect message of encoding type DESKTOPSIZE_PSEUDOENCODING");
				break;
			default:
				throw std::runtime_error("Error: unknown update-rect encoding type " + std::to_string(static_cast<int>(rectHeader.encoding_type)));
				break;
		}
	}

	inline std::string recvServerClipboard() const {
		for(size_t i = 0; i < 3; i++)
			sock.recvPrimitive<uint8_t>(); // receive padding
	
		const uint32_t clipboardLength = ntohl(sock.recvPrimitive<uint32_t>());

		return sock.recvString(clipboardLength);
	}

	// ---- updaterect type implementations ----

	inline void recvUpdateRectRAW(const RectHeader& rectHeader) {
		const size_t BYTES_PER_PIXEL = 4;

		// const size_t rect_size_bytes = rectHeader.width * rectHeader.height * (pixelFormat.bits_per_pixel / 8);
		const size_t rect_size_bytes = rectHeader.width * rectHeader.height * 4; // TODO: correct pixel byte size
		std::vector<uint8_t> updateRect(rect_size_bytes);

		sock.recvExactly(updateRect.data(), rect_size_bytes); // read raw framebuffer overwrite

		for(size_t y = 0; y < rectHeader.height; y++) {
			for(size_t x = 0; x < rectHeader.width; x++) {
				const size_t dstX = rectHeader.pos_x + x;
				const size_t dstY = rectHeader.pos_y + y;
				const size_t ind_dst = (dstY * fb_width + dstX) * 4;

				const size_t ind_update = (y * rectHeader.width + x) * 4;

				for(size_t i = 0; i < BYTES_PER_PIXEL; i++)
					pixelData[ind_dst + i] = updateRect[ind_update + i];
			}
		}
	}

	inline void recvUpdateRectCOPYRECT(const RectHeader& rectHeader) {
		const size_t BYTES_PER_PIXEL = 4;

		const uint16_t srcX0 = sock.recvU16();
		const uint16_t srcY0 = sock.recvU16();

		std::vector<uint8_t> data(rectHeader.width * rectHeader.height * 4);
		for(size_t y = 0; y < rectHeader.height; y++) {
			for(size_t x = 0; x < rectHeader.width; x++) {
				const size_t srcX = srcX0 + x;
				const size_t srcY = srcY0 + y;
				const size_t ind_src = (srcY * fb_width + srcX) * 4;

				const size_t ind_data = (y * rectHeader.width + x) * 4;

				for(size_t i = 0; i < BYTES_PER_PIXEL; i++)
					data[ind_data + i] = pixelData[ind_src + i];
			}
		}

		for(size_t y = 0; y < rectHeader.height; y++) {
			for(size_t x = 0; x < rectHeader.width; x++) {
				const size_t dstX = rectHeader.pos_x + x;
				const size_t dstY = rectHeader.pos_y + y;
				const size_t ind_dst = (dstY * fb_width + dstX) * 4;

				const size_t ind_data = (y * rectHeader.width + x) * 4;

				for(size_t i = 0; i < BYTES_PER_PIXEL; i++)
					pixelData[ind_dst + i] = data[ind_data + i];
			}
		}
	}

	inline void recvUpdateRectTRLE(const RectHeader& rectHeader) { // tiled run-length encoding
	}
	/*
	inline void recvUpdateRectTRLE(const RectHeader& rectHeader) { // tiled run-length encoding
		const size_t numTilesX = rectHeader.width / 16 + !!(rectHeader.width % 16);
		const size_t numTilesY = rectHeader.height / 16 + !!(rectHeader.height % 16);

		struct {
			size_t paletteSize;
			uint32_t palette[16];
		} packedPallette;

		struct {
			size_t paletteSize;
			uint32_t palette[127];
		} rlePallette;

		for(size_t tileY = 0; tileY < numTilesY; tileY++) {
			for(size_t tileX = 0; tileX < numTilesX; tileX++) {
				const uint8_t subEncoding = sock.recvU8();

				const uint8_t width  = std::min<uint8_t>(rectHeader.width  - tileX * 16, 16);
				const uint8_t height = std::min<uint8_t>(rectHeader.height - tileY * 16, 16);

				switch(subEncoding) {
				case 0: { // raw
					for(size_t localY = 0; localY < height; localY++) {
						for(size_t localX = 0; localX < width; localX++) {
							const size_t absX = tileX * 16 + localX;
							const size_t absY = tileY * 16 + localY;

							uint32_t col = 0;
							col |= sock.recvU8() << 16; // red
							col |= sock.recvU8() <<  8; // green
							col |= sock.recvU8() <<  0; // blue

							reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = col;
						}
					}
				} break;
				
				case 1: { // solid color
				}
				
				// packed palette
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16: {
					packedPallette.paletteSize = subEncoding;

					// receive palette:
					for(size_t i = 0; i < packedPallette.paletteSize; i++) {
						packedPallette.palette[i] = 0;
						packedPallette.palette[i] |= sock.recvU8() << 16; // red
						packedPallette.palette[i] |= sock.recvU8() <<  8; // green
						packedPallette.palette[i] |= sock.recvU8() <<  0; // blue
					}
				
				case 127: // reuse palette of the previous tile
					const size_t bitsPerPixel =
						(packedPallette.paletteSize == 2)                                    ? 1 : // paletteSize == 2
						(packedPallette.paletteSize == 3 || packedPallette.paletteSize == 4) ? 2 : // 3 <= paletteSize <= 4
						                                                                       4;  // 5 <= paletteSize <= 16
					const size_t pixelsPerByte = 8 / bitsPerPixel;

					const size_t bytesX = (width + pixelsPerByte - 1) / pixelsPerByte;
						// (paletteSize == 2)                     ? ((width + 7) / 8) : // paletteSize == 2
						// (paletteSize == 3 || paletteSize == 4) ? ((width + 3) / 4) : // 3 <= paletteSize <= 4
						//                                          ((width + 1) / 2);  // 5 <= paletteSize <= 16
					
					// receive packed pixels:
					for(size_t localY = 0; localY < height; localY++) {
						for(size_t byteX = 0; byteX < bytesX; byteX++) {
							const uint8_t byte = sock.recvU8();
							const size_t pixelsInThisByte = std::min<size_t>(width - (byteX * pixelsPerByte), pixelsPerByte);

							for(size_t pixelSubIndex = 0; pixelSubIndex < pixelsInThisByte; pixelSubIndex++) { // iterate over all pixels packed into the current byte
								const uint8_t bitOff = (pixelsInThisByte - 1 - pixelSubIndex) * bitsPerPixel; // bits are packed left -> right : msb -> lsb
								const uint8_t mask = (0x01 << bitsPerPixel) - 1;
								const uint8_t pixel = (byte >> bitOff) & mask; // extract index into palette

								const size_t absX = (tileX) * 16 + (byteX * pixelsPerByte + pixelSubIndex);
								const size_t absY = (tileY) * 16 + localY;

								reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = packedPallette.palette[pixel];
							}
						}
					}
				} break;

				case 128: { // plain RLE (run-length encoding)
					const auto getRunLength = [&]() -> size_t {
							size_t runLength = 0;
							uint8_t byte = 0;
							do {
								byte = sock.recvU8();
								runLength += byte;
							} while(byte == 255);
							return runLength;
						};
					
					const auto getCPixel = [&]() -> uint32_t {
							uint32_t color = 0;
							color |= sock.recvU8() << 16; // red
							color |= sock.recvU8() <<  8; // green
							color |= sock.recvU8() <<  0; // blue
							return color;
						};
					
					size_t runLength = 1;
					uint32_t pixelValue;

					for(size_t localY = 0; localY < height; localY++) {
						for(size_t localX = 0; localX < width; localX++) {
							const size_t absX = tileX * 16 + localX;
							const size_t absY = tileY * 16 + localY;

							if(--runLength == 0) {
								pixelValue = getCPixel();
								runLength = getRunLength();
							}

							reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = pixelValue;
						}
					}
				} break;

				//  Palette RLE
				case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137: case 138: case 139:
				case 140: case 141: case 142: case 143: case 144: case 145: case 146: case 147: case 148: case 149:
				case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159:
				case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167: case 168: case 169:
				case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177: case 178: case 179:
				case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187: case 188: case 189:
				case 190: case 191: case 192: case 193: case 194: case 195: case 196: case 197: case 198: case 199:
				case 200: case 201: case 202: case 203: case 204: case 205: case 206: case 207: case 208: case 209:
				case 210: case 211: case 212: case 213: case 214: case 215: case 216: case 217: case 218: case 219:
				case 220: case 221: case 222: case 223: case 224: case 225: case 226: case 227: case 228: case 229:
				case 230: case 231: case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239:
				case 240: case 241: case 242: case 243: case 244: case 245: case 246: case 247: case 248: case 249:
				case 250: case 251: case 252: case 253: case 254: case 255: {
					rlePallette.paletteSize = subEncoding - 128;

					for(size_t i = 0; i < rlePallette.paletteSize; i++) {
						rlePallette.palette[i] = 0;
						rlePallette.palette[i] |= sock.recvU8() << 16; // red
						rlePallette.palette[i] |= sock.recvU8() <<  8; // green
						rlePallette.palette[i] |= sock.recvU8() <<  0; // blue
					}

				case 129: // Palette RLE using previous palette
					const auto getRunLength = [&]() -> size_t {
							size_t runLength = 0;
							uint8_t byte = 0;
							do {
								byte = sock.recvU8();
								runLength += byte;
							} while(byte == 255);
							return runLength;
						};

					size_t runLength = 1;
					uint32_t paletteIndex;

					for(size_t localY = 0; localY < height; localY++) {
						for(size_t localX = 0; localX < width; localX++) {
							const size_t absX = tileX * 16 + localX;
							const size_t absY = tileY * 16 + localY;

							if(--runLength == 0) {
								const uint8_t rawIndex = sock.recvU8();

								paletteIndex = rawIndex & 0x7F;
								runLength = (rawIndex & 0x80) ? getRunLength() : 1;
							}

							reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = rlePallette.palette[paletteIndex];
						}
					}
				} break;
				}
			}
		}
	}
	*/


	bool firstTime = true; // TODO: refactor
	inline void recvUpdateRectZRLE(const RectHeader& rectHeader) { // tiled run-length encoding
		uint32_t zlibLength = sock.recvU32();

		if(firstTime) { // skip zlib header
			sock.recvU16();
			zlibLength -= 2;
		}
		firstTime = false;

		// std::cout << "Zlib length: " << zlibLength << "\n"; 
		// std::cout << "Representing " << rectHeader.width << " * " << rectHeader.height << " pixels\n";

		// ----------------
		// static std::vector<uint8_t> zlibData;
		// const size_t zlibLengthPrev = zlibData.size();
		// zlibData.resize(zlibLengthPrev + zlibLength);
		// sock.recvExactly(zlibData.data() + zlibLengthPrev, zlibLength);
		// ----------------


		// ----------------
		std::vector<uint8_t> zlibData(zlibLength);
		sock.recvExactly(zlibData.data(), zlibLength);
		// ----------------

		// std::cout << "Zlib data received successfully\n";



		static std::vector<uint8_t> prevData(32768); // store some decoded data for decoding LZ77 backreferences in future DEFLATE blocks
		std::vector<uint8_t> rawData = prevData; // copy old data for decoding LZ77 backreferences

		// std::vector<uint8_t> currentZlibData(zlibData.begin() + zlibLengthPrev, zlibData.end());
		// const Bitstream stream(currentZlibData);
		const Bitstream stream(zlibData);
		BitstreamReader streamReader(stream);
		// try {
		// 	zlib::decompress(streamReader, rawData);

		// std::vector<uint8_t> junkData;
		// while(streamReader.numBytesRead < zlibLengthPrev)
		// 	deflate::decompressBlock(streamReader, junkData);
		// std::cout << "Decoded previous data. prevLength: " << zlibLengthPrev << "  decodedLength: " << streamReader.numBytesRead << "\n";

		while(!streamReader.isEmpty())
			deflate::decompressBlock(streamReader, rawData);

		// std::cout << "zlibLength: " << zlibLength << "  bytesRead: " << streamReader.numBytesRead << "  bitsRead: " << (int)streamReader.numBitsRead << "\n";
		// } catch(const std::exception& e) {
		// 	std::cout << "decompress threw: " << e.what() << "\n";
		// }

		// static size_t usedBytes = 0;
		// const size_t usedBytesBefore = usedBytes;
		// usedBytes += rawData.size() - usedBytes;

		// std::cout << "Decompressed successfully. Length: " << rawData.size() << "\n";

		// size_t dataInd = 0;
		size_t dataInd = 0 + prevData.size();
		// size_t dataInd = 0 + usedBytesBefore;

		const auto recvU8 =
			[&]() -> uint8_t  {
				if(dataInd >= rawData.size())
					throw std::runtime_error("Out of uncompressed zlib data!");
				const uint8_t val = rawData[dataInd]; dataInd++; return val;
			};
		
		constexpr size_t TILE_SIZE = 64;
		const size_t numTilesX = rectHeader.width / TILE_SIZE + !!(rectHeader.width % TILE_SIZE);
		const size_t numTilesY = rectHeader.height / TILE_SIZE + !!(rectHeader.height % TILE_SIZE);

		struct {
			size_t paletteSize;
			uint32_t palette[16];
		} packedPallette;

		struct {
			size_t paletteSize;
			uint32_t palette[127];
		} rlePallette;

		for(size_t tileY = 0; tileY < numTilesY; tileY++) {
			for(size_t tileX = 0; tileX < numTilesX; tileX++) {
				const uint8_t subEncoding = recvU8();

				const size_t width  = std::min<size_t>(rectHeader.width  - tileX * TILE_SIZE, TILE_SIZE);
				const size_t height = std::min<size_t>(rectHeader.height - tileY * TILE_SIZE, TILE_SIZE);

				switch(subEncoding) {
				case 0: { // raw
					for(size_t localY = 0; localY < height; localY++) {
						for(size_t localX = 0; localX < width; localX++) {
							const size_t absX = rectHeader.pos_x + tileX * TILE_SIZE + localX;
							const size_t absY = rectHeader.pos_y + tileY * TILE_SIZE + localY;

							uint32_t col = 0;
							col |= recvU8() <<  0; // blue
							col |= recvU8() <<  8; // green
							col |= recvU8() << 16; // red

							reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = col;
						}
					}
				} break;
				
				case 1: { // solid color
					// std::cout << " - Solid color\n";
					
					uint32_t col = 0;
					col |= recvU8() <<  0; // blue
					col |= recvU8() <<  8; // green
					col |= recvU8() << 16; // red

					for(size_t localY = 0; localY < height; localY++) {
						for(size_t localX = 0; localX < width; localX++) {
							const size_t absX = rectHeader.pos_x + tileX * TILE_SIZE + localX;
							const size_t absY = rectHeader.pos_y + tileY * TILE_SIZE + localY;

							reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = col;
						}
					}
				} break;
				
				// packed palette
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16: {
					// std::cout << " - Packed palette\n";
					packedPallette.paletteSize = subEncoding;

					// receive palette:
					for(size_t i = 0; i < packedPallette.paletteSize; i++) {
						packedPallette.palette[i] = 0;
						packedPallette.palette[i] |= recvU8() <<  0; // blue
						packedPallette.palette[i] |= recvU8() <<  8; // green
						packedPallette.palette[i] |= recvU8() << 16; // red
					}
				
				// case 127: // reuse palette of the previous tile (not valid for ZRLE)
					const size_t bitsPerPixel =
						(packedPallette.paletteSize == 2)                                    ? 1 : // paletteSize == 2
						(packedPallette.paletteSize == 3 || packedPallette.paletteSize == 4) ? 2 : // 3 <= paletteSize <= 4
						                                                                       4;  // 5 <= paletteSize <= 16
					const size_t pixelsPerByte = 8 / bitsPerPixel;

					const size_t bytesX = (width + pixelsPerByte - 1) / pixelsPerByte;
					
					// receive packed pixels:
					for(size_t localY = 0; localY < height; localY++) {
						for(size_t byteX = 0; byteX < bytesX; byteX++) {
							const uint8_t byte = recvU8();
							const size_t pixelsInThisByte = std::min<size_t>(width - (byteX * pixelsPerByte), pixelsPerByte);

							for(size_t pixelSubIndex = 0; pixelSubIndex < pixelsInThisByte; pixelSubIndex++) { // iterate over all pixels packed into the current byte
								const uint8_t bitOff = (pixelsInThisByte - 1 - pixelSubIndex) * bitsPerPixel; // bits are packed left -> right : msb -> lsb
								const uint8_t mask = (0x01 << bitsPerPixel) - 1;
								const uint8_t pixel = (byte >> bitOff) & mask; // extract index into palette

								const size_t absX = rectHeader.pos_x + tileX * TILE_SIZE + (byteX * pixelsPerByte + pixelSubIndex);
								const size_t absY = rectHeader.pos_y + tileY * TILE_SIZE + localY;

								reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = packedPallette.palette[pixel];
							}
						}
					}
				} break;

				case 128: { // plain RLE (run-length encoding)
					// std::cout << " - Plain RLE\n";
					const auto getRunLength = [&]() -> size_t {
							size_t runLength = 0;
							uint8_t byte = 0;
							do {
								byte = recvU8();
								runLength += byte;
							} while(byte == 255);
							return runLength + 1;
						};

					const auto getCPixel = [&]() -> uint32_t {
							uint32_t color = 0;
							color |= recvU8() <<  0; // blue
							color |= recvU8() <<  8; // green
							color |= recvU8() << 16; // red
							return color;
						};

					size_t runLength = 0;
					uint32_t pixelValue;

					for(size_t localY = 0; localY < height; localY++) {
						for(size_t localX = 0; localX < width; localX++) {
							const size_t absX = rectHeader.pos_x + tileX * TILE_SIZE + localX;
							const size_t absY = rectHeader.pos_y + tileY * TILE_SIZE + localY;

							if(runLength == 0) {
								pixelValue = getCPixel();
								runLength = getRunLength();
							}

							reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = pixelValue;

							runLength--;
						}
					}
				} break;

				//  Palette RLE
				case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137: case 138: case 139:
				case 140: case 141: case 142: case 143: case 144: case 145: case 146: case 147: case 148: case 149:
				case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157: case 158: case 159:
				case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167: case 168: case 169:
				case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177: case 178: case 179:
				case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187: case 188: case 189:
				case 190: case 191: case 192: case 193: case 194: case 195: case 196: case 197: case 198: case 199:
				case 200: case 201: case 202: case 203: case 204: case 205: case 206: case 207: case 208: case 209:
				case 210: case 211: case 212: case 213: case 214: case 215: case 216: case 217: case 218: case 219:
				case 220: case 221: case 222: case 223: case 224: case 225: case 226: case 227: case 228: case 229:
				case 230: case 231: case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239:
				case 240: case 241: case 242: case 243: case 244: case 245: case 246: case 247: case 248: case 249:
				case 250: case 251: case 252: case 253: case 254: case 255: {
					// std::cout << " - Palette RLE\n";
					rlePallette.paletteSize = subEncoding - 128;

					for(size_t i = 0; i < rlePallette.paletteSize; i++) {
						rlePallette.palette[i] = 0;
						rlePallette.palette[i] |= recvU8() <<  0; // blue
						rlePallette.palette[i] |= recvU8() <<  8; // green
						rlePallette.palette[i] |= recvU8() << 16; // red
					}

				// case 129: // Palette RLE using previous palette (not valid for ZRLE)
					const auto getRunLength = [&]() -> size_t {
							size_t runLength = 0;
							uint8_t byte = 0;
							do {
								byte = recvU8();
								runLength += byte;
							} while(byte == 255);
							return runLength + 1;
						};

					size_t runLength = 0;
					uint8_t paletteIndex;

					for(size_t localY = 0; localY < height; localY++) {
						for(size_t localX = 0; localX < width; localX++) {
							const size_t absX = rectHeader.pos_x + tileX * TILE_SIZE + localX;
							const size_t absY = rectHeader.pos_y + tileY * TILE_SIZE + localY;

							if(runLength == 0) {
								const uint8_t rawIndex = recvU8();

								paletteIndex = rawIndex & 0x7F;
								runLength = 1;

								if(rawIndex & 0x80)
									runLength = getRunLength();
							}

							reinterpret_cast<uint32_t*>(pixelData)[absY * fb_width + absX] = rlePallette.palette[paletteIndex];

							runLength--;
						}
					}
				} break;
				}
			}
		}

		// store some decoded data for decoding LZ77 backreferences in future DEFLATE blocks
		if(rawData.size() >= prevData.size()) {
			memcpy(prevData.data(), rawData.data() + rawData.size() - prevData.size(), prevData.size());
		} else {
			memcpy(prevData.data() + prevData.size() - rawData.size(), rawData.data(), rawData.size());
		}
	}


	// ---- utilities ----

	inline void close() const {
		sock.close();
	}

	inline uint16_t width() const { return fb_width; }
	inline uint16_t height() const { return fb_height; }
	inline uint8_t* pixel_data() const { return pixelData; }
};
#include <stdexcept>
#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include <array>


#include "VNC.hpp"
#include "BWindow/GDIWindow.h"


void runVNC() {
	constexpr auto queryPerformanceFrequency =
		[]()->size_t {
			LARGE_INTEGER perfFreq;
			QueryPerformanceFrequency(&perfFreq);
			return perfFreq.QuadPart;
		};

	constexpr auto queryPerformanceCounter =
		[]()->size_t {
			LARGE_INTEGER perfCount;
			QueryPerformanceCounter(&perfCount);
			return perfCount.QuadPart;
		};

	// VNC vnc("192.168.178.66", 5900);
	VNC vnc("127.0.0.1", 5900);

	// const auto getChannel = [](const uint32_t value, const uint8_t shift, const uint16_t max_value, const bool big_endian) -> uint16_t {
	// 		return big_endian ? ntohs((value >> shift) & max_value) : (value >> shift) & max_value;
	// 	};

	// const auto getRed = [&pixelFormat, &getChannel](const uint32_t value) -> uint16_t {
	// 		return getChannel(value, pixelFormat.red_shift, pixelFormat.red_max, pixelFormat.big_endian_flag);
	// 	};
	// const auto getGreen = [&pixelFormat, &getChannel](const uint32_t value) -> uint16_t {
	// 		return getChannel(value, pixelFormat.green_shift, pixelFormat.green_max, pixelFormat.big_endian_flag);
	// 	};
	// const auto getBlue = [&pixelFormat, &getChannel](const uint32_t value) -> uint16_t {
	// 		return getChannel(value, pixelFormat.blue_shift, pixelFormat.blue_max, pixelFormat.big_endian_flag);
	// 	};

	GDIWindow window(800, 800);

	constexpr size_t TARGET_FRAMERATE = 100;
	const size_t perfFreq = queryPerformanceFrequency();

	size_t lastUpdateRequestTime = 0;


	// keyboard stuff:
	struct SpecialKey {
		int VKey;
		uint32_t keysym;
	};
	std::vector<SpecialKey> specialKeys {
		{ VK_SPACE,      0x0020 },
		{ VK_MULTIPLY,   0x002A },
		{ VK_OEM_PLUS,   0x002B },
		{ VK_OEM_COMMA,  0x002C },
		{ VK_OEM_MINUS,  0x002D },
		{ VK_OEM_PERIOD, 0x002E },
		{ VK_DIVIDE,     0x002F },

		{ '0', 0x0030 },
		{ '1', 0x0031 },
		{ '2', 0x0032 },
		{ '3', 0x0033 },
		{ '4', 0x0034 },
		{ '5', 0x0035 },
		{ '6', 0x0036 },
		{ '7', 0x0037 },
		{ '8', 0x0038 },
		{ '9', 0x0039 },

		{ VK_BACK,   0xFF08 },
		{ VK_TAB,    0xFF09 },
		{ VK_RETURN, 0xFF0D },
		{ VK_SCROLL, 0xFF14 },
		{ VK_ESCAPE, 0xFF1B },
		{ VK_DELETE, 0xFFFF },
	};

	std::vector<uint32_t> pressedKeys;
	const auto eraseChar = [&](const uint32_t c) {
			for(size_t i = 0; i < pressedKeys.size(); i++) {
				if(pressedKeys[i] == c) {
					pressedKeys.erase(pressedKeys.begin() + i);
					return;
				}
			}
		};

	while(!window.shouldClose()) {
		window.pollMsg();

		window.graphics.clear(0x00000000);


		// const size_t beforeUpdate = queryPerformanceCounter();
		vnc.recvUpdates();
		// const size_t afterUpdate = queryPerformanceCounter();
		// const double msUpdate = (afterUpdate - beforeUpdate) * 1000. / perfFreq;
		// if(msUpdate > 5)
		// 	std::cout << "recvUpdates() took " << msUpdate << "ms\n";


		const double remoteAR = vnc.width() * 1. / vnc.height();
		const double windowAR = window.width * 1. / window.height;
		const bool horizontalClamp = remoteAR > windowAR; // remote framebuffer touches left and right side of window; top and bottom are filled with black pixels

		const double texScale =
			horizontalClamp
				? (window.width * 1. / vnc.width())
				: (window.height * 1. / vnc.height());

		const size_t marginX = window.width - vnc.width() * texScale;
		const size_t marginY = window.height - vnc.height() * texScale;
		const size_t x1 = marginX / 2;
		const size_t y1 = marginY / 2;
		const size_t x2 = window.width - marginX / 2;
		const size_t y2 = window.height - marginY / 2;

		POINT clientRectOrig {};
		ClientToScreen(window.win.wnd, &clientRectOrig);
		RECT windowRect;
		GetWindowRect(window.win.wnd, &windowRect);

		const size_t winMouseX = window.win.mouseX - (windowRect.left - clientRectOrig.x);
		const size_t winMouseY = window.win.mouseY - (windowRect.top - clientRectOrig.y);
		const size_t mouseXRemote = (winMouseX - x1) * vnc.width() / (x2 - x1);
		const size_t mouseYRemote = (winMouseY - y1) * vnc.height() / (y2 - y1);
		
		{
			const size_t t = queryPerformanceCounter();
			const size_t dt = t - lastUpdateRequestTime;
			if(dt > perfFreq / TARGET_FRAMERATE) {
				vnc.sendUpdateRequest(0, 0, vnc.width(), vnc.height());

				const bool left   = GetAsyncKeyState(VK_LBUTTON) & 0x8000;
				const bool middle = GetAsyncKeyState(VK_MBUTTON) & 0x8000;
				const bool right  = GetAsyncKeyState(VK_RBUTTON) & 0x8000;

				if(mouseXRemote < vnc.width() && mouseYRemote < vnc.height())
					vnc.sendPointerEvent(mouseXRemote, mouseYRemote, right << 2 | middle << 1 | left);

				lastUpdateRequestTime = t;
			}
		}

		// send key updates
		constexpr size_t NUM_LETTERS = 26;
		const bool shiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
		for(size_t letter_ind = 0; letter_ind < NUM_LETTERS; letter_ind++) {
			const bool pressed = GetAsyncKeyState('A' + letter_ind) & 0x8000;

			const char kLower = 'a' + letter_ind;
			const char kUpper = 'A' + letter_ind;
			const char k = shiftPressed ? kUpper : kLower;
			const char kInv = (!shiftPressed) ? kUpper : kLower;

			bool wasPressed = false, wasPressedInv = false;
			for(size_t i = 0; i < pressedKeys.size(); i++) {
				const uint32_t& key = pressedKeys[i];
				if(key == k) wasPressed = true;
				if(key == kInv) wasPressedInv = true;
			}

			if(wasPressedInv) {
				vnc.sendKeyEvent(false, kInv);
				std::cout << "SHIFTED " << kInv << "\n";
			}

			if(pressed != wasPressed || wasPressedInv) {
				vnc.sendKeyEvent(pressed, k);
				std::cout << (pressed ? "PRESSED " : "RELEASED ") << k << "\n";
			}

			if(pressed && !wasPressed)
				pressedKeys.push_back(k);

			if(wasPressed && !pressed)
				eraseChar(k);
			if(wasPressedInv)
				eraseChar(kInv);
		}

		for(const SpecialKey& key : specialKeys) {
			const bool pressed = GetAsyncKeyState(key.VKey) & 0x8000;

			bool wasPressed = false;
			for(const uint32_t& sym : pressedKeys) {
				if(key.keysym == sym) {
					wasPressed = true;
					break;
				}
			}

			if(pressed != wasPressed)
				vnc.sendKeyEvent(pressed, key.keysym);

			if(pressed && !wasPressed)
				pressedKeys.push_back(key.keysym);

			if(wasPressed && !pressed)
				eraseChar(key.keysym);
		}

		for(size_t y = 0; y < window.height; y++) {
			for(size_t x = 0; x < window.width; x++) {
				const double xTexNorm = (x - x1) * 1. / (x2 - x1);
				const double yTexNorm = (y - y1) * 1. / (y2 - y1);

				uint32_t& pixel = window.graphics.buffer[y * window.width + x];

				if(xTexNorm < 0. || xTexNorm >= 1.) {
					pixel = 0x000000FF;
					continue;
				}

				if(yTexNorm < 0. || yTexNorm >= 1.) {
					pixel = 0x00000000;
					continue;
				}

				// <bilinear filtering>
				const double xTexSmooth = xTexNorm * vnc.width();
				const double yTexSmooth = yTexNorm * vnc.height();
				const size_t xTex0 = std::min<size_t>(floorl(xTexSmooth), vnc.width() - 2);
				const size_t yTex0 = std::min<size_t>(floorl(yTexSmooth), vnc.height() - 2);
				const size_t xTex1 = xTex0 + 1;
				const size_t yTex1 = yTex0 + 1;

				const double xTexWeight0 = 1. - fabsl(xTexSmooth - xTex0);
				const double xTexWeight1 = 1. - fabsl(xTexSmooth - xTex1);
				const double yTexWeight0 = 1. - fabsl(yTexSmooth - yTex0);
				const double yTexWeight1 = 1. - fabsl(yTexSmooth - yTex1);

				const double texWeight00 = yTexWeight0 * xTexWeight0;
				const double texWeight01 = yTexWeight0 * xTexWeight1;
				const double texWeight10 = yTexWeight1 * xTexWeight0;
				const double texWeight11 = yTexWeight1 * xTexWeight1;

				const uint32_t texIndex00 = yTex0 * vnc.width() + xTex0;
				const uint32_t texIndex01 = yTex0 * vnc.width() + xTex1;
				const uint32_t texIndex10 = yTex1 * vnc.width() + xTex0;
				const uint32_t texIndex11 = yTex1 * vnc.width() + xTex1;

				const uint32_t pixel00 = reinterpret_cast<uint32_t*>(vnc.pixel_data())[texIndex00];
				const uint32_t pixel01 = reinterpret_cast<uint32_t*>(vnc.pixel_data())[texIndex01];
				const uint32_t pixel10 = reinterpret_cast<uint32_t*>(vnc.pixel_data())[texIndex10];
				const uint32_t pixel11 = reinterpret_cast<uint32_t*>(vnc.pixel_data())[texIndex11];

				constexpr auto red   = [](const uint32_t col) -> double { return static_cast<uint8_t>(col >> 16) / 255.; };
				constexpr auto green = [](const uint32_t col) -> double { return static_cast<uint8_t>(col >> 8) / 255.; };
				constexpr auto blue  = [](const uint32_t col) -> double { return static_cast<uint8_t>(col >> 0) / 255.; };

				double redAccum = 0.;
				double greenAccum = 0.;
				double blueAccum = 0.;

				// redAccum += red(pixel00);
				redAccum += texWeight00 * red(pixel00);
				redAccum += texWeight01 * red(pixel01);
				redAccum += texWeight10 * red(pixel10);
				redAccum += texWeight11 * red(pixel11);

				// greenAccum += green(pixel00);
				greenAccum += texWeight00 * green(pixel00);
				greenAccum += texWeight01 * green(pixel01);
				greenAccum += texWeight10 * green(pixel10);
				greenAccum += texWeight11 * green(pixel11);

				// blueAccum += blue(pixel00);
				blueAccum += texWeight00 * blue(pixel00);
				blueAccum += texWeight01 * blue(pixel01);
				blueAccum += texWeight10 * blue(pixel10);
				blueAccum += texWeight11 * blue(pixel11);

				const uint8_t redVal   = redAccum * 255 * .9;
				const uint8_t greenVal = greenAccum * 255 * .9;
				const uint8_t blueVal  = blueAccum * 255 * .9;

				pixel = redVal << 16 | greenVal << 8 | blueVal;
				if(GetAsyncKeyState(VK_CONTROL) & 0x8000)
					pixel = pixel00;
				// </bilinear filtering>
			}
		}

		window.graphics.setPixel(winMouseX, winMouseY, 0x00FF00FF);
		window.graphics.setPixel(winMouseX+1, winMouseY, 0x00FF00FF);
		window.graphics.setPixel(winMouseX, winMouseY+1, 0x00FF00FF);
		window.graphics.setPixel(winMouseX+1, winMouseY+1, 0x00FF00FF);

		window.updateScreen();
	}

	vnc.close();
	WSACleanup();
}


int main(int argc, char **argv) {
	try {
		runVNC();
	} catch(const std::exception& e) {
		std::cout << "Exception thrown: " << e.what() << "\n";
	}

	std::cout << "Terminating program.\n";

	return 0;
}

#pragma once


#include <stdexcept>
#include <cstdint>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>


class Socket {
private:
	static WSADATA wsaData;
	static bool winSockInitialized;

private:
	SOCKET sock;

public:
	inline Socket(const std::string& address, const u_short port) {
		int iResult = 0;

		// Initialize Winsock
		if(!winSockInitialized) {
			iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (iResult != 0)
				throw std::runtime_error("WSAStartup failed: " + std::to_string(iResult));
			
			winSockInitialized = true;
		}

		// create socket:
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET)
			throw std::runtime_error("socket function failed with error = " + std::to_string(WSAGetLastError()));

		// connect to server
		sockaddr_in clientService;
		clientService.sin_family = AF_INET;
		clientService.sin_port = htons(port);
		inet_pton(AF_INET, address.c_str(), &clientService.sin_addr.s_addr);
		printf("Init winsock succeed\n");

		iResult = connect(sock, (SOCKADDR*)&clientService, sizeof(clientService));
		if (iResult == SOCKET_ERROR)
			throw std::runtime_error("connecting socket failed with error: " + std::to_string(WSAGetLastError()));
	}

	inline void close() const {
		// close socket
		if (int iResult = closesocket(sock) == SOCKET_ERROR)
			throw std::runtime_error("closesocket failed with error = " + std::to_string(WSAGetLastError()));
	}


	// -- sending:
	inline void send(const void *const buffer, const int buffer_size) const {
		const int iResult = ::send(sock, reinterpret_cast<const char*>(buffer), buffer_size, 0);

		if(iResult == buffer_size)
			return;
		
		throw std::runtime_error("send(): failed to send entire packet");
	}

	template<typename T>
	inline void sendPrimitive(const std::remove_cv_t<T>& val) const {
		send(&val, sizeof(T));
	}

	inline void sendU8(const uint8_t val) const {
		sendPrimitive<uint8_t>(val);
	}

	inline void sendU16(const uint16_t val) const {
		sendPrimitive<uint16_t>(htons(val));
	}

	inline void sendS32(const int32_t val) const {
		sendPrimitive<int32_t>(htonl(val));
	}


	// -- receiving:
	inline bool dataAvailable() const {
		fd_set set{};
		FD_ZERO(&set);
		FD_SET(sock, &set);
		TIMEVAL timeVal { .tv_sec = 0, .tv_usec = 0 };

		return select(0, &set, nullptr, nullptr, &timeVal);
	}

	inline int recv(void *const buffer, const int buffer_size) const {
		const int iResult = ::recv(sock, reinterpret_cast<char*>(buffer), buffer_size, 0); // blocks and waits for data

		if(iResult > 0)
			return iResult;
		
		if(iResult < 0) {
			closesocket(sock);
			throw std::runtime_error("recv() failed: " + std::to_string(WSAGetLastError()));
		}
		
		throw std::runtime_error("recv(): Connection closing..."); // iResult == 0
	}

	inline void recvExactly(void *const buffer, const size_t buffer_size) const {
		size_t len_received = 0;
		do {
			len_received += recv(reinterpret_cast<char*>(buffer) + len_received, buffer_size - len_received);
		} while(len_received < buffer_size);
	}

	inline std::string recvString(const size_t len) const {
		std::vector<char> text(len);
		recvExactly(text.data(), len);

		std::string str;
		str.append(text.data(), len);
		return str;
	}

	template<typename T>
	inline std::remove_cv_t<T> recvPrimitive() const {
		std::remove_cv_t<T> res;

		recvExactly(&res, sizeof(T));

		return res;
	}

	inline uint8_t recvU8() const {
		return recvPrimitive<uint8_t>();
	}

	inline uint16_t recvU16() const {
		return ntohs(recvPrimitive<uint16_t>());
	}

	inline uint32_t recvU32() const {
		return ntohl(recvPrimitive<uint32_t>());
	}

	inline int32_t recvS32() const {
		return ntohl(recvPrimitive<int32_t>());
	}
};

WSADATA Socket::wsaData = {0};
bool Socket::winSockInitialized = false;
#pragma once

#include "pch.h"

#define BUFFER_SIZE 8192

struct ClientSession
{
    SOCKET hSocket = INVALID_SOCKET;
    std::vector<char> assemblyBuffer;
};

enum class IoType
{
	Recv,
	Send,
	Shutdown
};

struct IoContext
{
	//비동기 수신 처리를 위한 OVERLAPPED 구조체.
	WSAOVERLAPPED overlapped{};
	WSABUF wsaBuf{};				//단지 Descriptor
	char buffer[BUFFER_SIZE] {};	//프로토콜 담김
	IoType mType{};
	ClientSession* owner = nullptr;

	//IoType::Send일 경우 작성.
	size_t totalLen = 0;
	size_t transferredLen = 0;

	IoContext() = delete;
	IoContext(const IoContext& rhs) = delete;
	IoContext(ClientSession* cs, IoType type) : mType(type), owner(cs)
	{
		wsaBuf.buf = buffer;
		wsaBuf.len = sizeof(buffer);
		::ZeroMemory(&overlapped, sizeof(WSAOVERLAPPED));
	}
};
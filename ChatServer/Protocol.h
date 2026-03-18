#pragma once

#include <Windows.h>
#include <cstdint>

enum class CMDCODE : uint16_t
{
	ChatMessage = 1,
	SystemMessage = 100,
	Login = 200,
	Logout = 300
};

#pragma pack(push, 1)
struct PacketHeader
{
	CMDCODE cmd;
	uint32_t payloadSize;
};
#pragma pack(pop)
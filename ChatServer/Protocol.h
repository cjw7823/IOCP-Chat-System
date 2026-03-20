#pragma once

#include <Windows.h>
#include <cstdint>

/*
    로그인 / 회원가입 : 고정 길이
    채팅 / 시스템 : 가변 길이
*/

enum class CMDCODE : uint16_t
{
    ChatMessage = 1,
    SystemMessage = 100,

    LoginRequest = 200,
    LoginResponse = 201,
    LogoutRequest = 300,
    LogoutResponse = 301,

    RegisterRequest = 400,
    RegisterResponse = 401,
};

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t cmd;   //CMDCODE
    uint16_t extHeaderSize;   // 0이면 없음
	uint32_t payloadSize; // 확장헤더 제외한 바디
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ChatExtHeader
{
    uint16_t senderIdLen;
    uint16_t messageLen;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct LoginRequestBody
{
    char id[256];
    char pw[256];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RegisterRequestBody
{
    char id[256];
    char pw[256];
};
#pragma pack(pop)

enum class AuthResult : uint16_t
{
    Success = 0,
    InvalidId = 1,
    WrongPassword = 2,
    DuplicateId = 3,
    ServerError = 4,
};

#pragma pack(push, 1)
struct AuthResponseBody
{
    uint16_t result; //AuthResult
};
#pragma pack(pop)
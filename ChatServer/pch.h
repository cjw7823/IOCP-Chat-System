#pragma once

#include <iostream>
#include <WinSock2.h>
#include <mutex>
#include <list>
#include <vector>
#include <iterator>
#include <stdexcept>
#include <cassert>
#include <string>

#include "Protocol.h"
#include "../SQLlite/sqlite3.h"

inline std::string WideToUTF8(const std::wstring& wstr)
{
	int size = WideCharToMultiByte(
		CP_UTF8,
		0,
		wstr.c_str(),
		-1,
		nullptr,
		0,
		nullptr,
		nullptr
	);

	std::string result(size - 1, 0); // null terminator Á¦¿Ü
	int converted = WideCharToMultiByte(
		CP_UTF8,
		0,
		wstr.c_str(),
		-1,
		result.data(),
		size,
		nullptr,
		nullptr
	);

	return result;
}

#define THROW_RUNTIME_ERROR(msg)							\
{															\
	std::wstring message_w(msg);							\
	std::string message_a = WideToUTF8(message_w);			\
	throw std::runtime_error(								\
		std::string(__FILE__) +	":"	+						\
		std::to_string(__LINE__) + " " +					\
		message_a);							\
}
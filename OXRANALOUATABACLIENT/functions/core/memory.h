#pragma once
// Безопасное чтение/запись произвольных адресов памяти + утилиты CS2 интерфейсов.

#include <Windows.h>
#include <cstdint>

// Шаблоны должны быть доступны во всех TU, поэтому определены прямо здесь.
template <typename T>
inline bool TryRead(std::uintptr_t address, T& out)
{
	__try
	{
		out = *reinterpret_cast<const T*>(address);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

template <typename T>
inline bool TryWrite(std::uintptr_t address, const T& value)
{
	__try
	{
		*reinterpret_cast<T*>(address) = value;
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

bool IsCs2Active();
std::uintptr_t GetClientBase();
void* GetInterface(const char* dllName, const char* interfaceName);

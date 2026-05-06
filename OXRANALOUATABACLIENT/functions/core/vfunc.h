#pragma once
// Утилиты для безопасного вызова виртуальной функции по индексу.
//
// Используется для движковых функций C_EconEntity::update_skin (idx 110),
// C_EconEntity::update_weapon_data (idx 195) — поправить UV шейдеров после
// смены paint kit'а.
//
// SEH-обёртки на стороне вызывающего (см. skinchanger.cpp), потому что
// MSVC не разрешает __try внутри функции с C++-объектами/templates.

#include <Windows.h>
#include <cstdint>
#include <type_traits>

namespace vfn
{
	template <typename Ret = void, typename... Args>
	inline Ret call(void* instance, std::size_t index, Args... args)
	{
		using Fn = Ret(__fastcall*)(void*, Args...);
		void** vt = *reinterpret_cast<void***>(instance);
		Fn fn = reinterpret_cast<Fn>(vt[index]);
		return fn(instance, args...);
	}
}

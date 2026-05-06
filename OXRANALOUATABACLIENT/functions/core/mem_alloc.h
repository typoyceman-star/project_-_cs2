#pragma once
// Тонкий обертчик над tier0!MemAlloc_AllocFunc / MemAlloc_FreeFunc.
// Нужен потому что блоки атрибутов CEconItemAttribute должны быть
// аллоцированы тем же аллокатором, который игра потом будет освобождать
// при удалении item view (иначе — heap corruption и краш).
//
// Возвращаемые указатели можно вернуть только через GameFree();
// нельзя смешивать с обычным new/delete или ::HeapFree.

#include <cstddef>

bool InitGameAlloc();          // Возвращает true при удачном GetProcAddress.
void* GameAlloc(std::size_t size);
void  GameFree(void* ptr);

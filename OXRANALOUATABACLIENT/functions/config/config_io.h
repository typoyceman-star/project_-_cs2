#pragma once
// Сохранение и загрузка config.ini.

void SaveConfig();
void LoadConfig();

// Инициализация рантайм-оффсетов: пытается подгрузить json, иначе использует встроенные.
void InitRuntimeOffsets();

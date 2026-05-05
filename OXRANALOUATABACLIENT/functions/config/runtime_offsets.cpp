#include "runtime_offsets.h"
#include "paths.h"
#include "json_parser.h"
#include "../core/globals.h"

void InitRuntimeOffsets()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

	// Пытаемся загрузить оффсеты из JSON файлов
	std::string dllDir;
	if (GetDllDirA(dllDir))
	{
		// Ищем output/offsets.json и client.dll.json относительно DLL
		std::string offsetsPath = MakePathA(dllDir, "..\\..\\output\\offsets.json");
		std::string schemaPath = MakePathA(dllDir, "..\\..\\output\\client.dll.json");
		
		// Проверяем существование файла
		if (FileExistsA(offsetsPath))
		{
			if (g_offsetsRuntime.LoadFromJson(offsetsPath, schemaPath))
			{
				// Успешно загружены оффсеты из JSON
				char msg[512];
				wsprintfA(msg, "[OXRANA] Offsets loaded from JSON:\n"
					"dwEntityList: 0x%X\n"
					"dwViewMatrix: 0x%X\n"
					"dwViewAngles: 0x%X\n"
					"dwLocalPlayerPawn: 0x%X\n"
					"dwPlantedC4: 0x%X\n"
					"m_iHealth: 0x%X\n"
					"m_iTeamNum: 0x%X\n"
					"m_iShotsFired: 0x%X\n",
					g_offsetsRuntime.dwEntityList,
					g_offsetsRuntime.dwViewMatrix,
					g_offsetsRuntime.dwViewAngles,
					g_offsetsRuntime.dwLocalPlayerPawn,
					g_offsetsRuntime.dwPlantedC4,
					g_offsetsRuntime.m_iHealth,
					g_offsetsRuntime.m_iTeamNum,
					g_offsetsRuntime.m_iShotsFired);
				OutputDebugStringA(msg);
			}
			else
			{
				OutputDebugStringA("[OXRANA] Failed to parse offsets.json, using built-in offsets\n");
			}
		}
		else
		{
			// Пробуем альтернативный путь (если DLL в другой папке)
			offsetsPath = MakePathA(dllDir, "output\\offsets.json");
			schemaPath = MakePathA(dllDir, "output\\client.dll.json");
			if (FileExistsA(offsetsPath))
			{
				if (g_offsetsRuntime.LoadFromJson(offsetsPath, schemaPath))
				{
					OutputDebugStringA("[OXRANA] Offsets loaded from JSON (alternative path)\n");
				}
			}
			else
			{
				OutputDebugStringA("[OXRANA] offsets.json not found, using built-in offsets\n");
			}
		}
	}

}

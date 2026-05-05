#include "nosmoke.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"

void UpdateNoSmoke(bool cs2Active)
{
	if (!cs2Active || !g_noSmokeEnabled)
		return;

	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;

	uintptr_t client = GetClientBase();
	if (!client)
		return;

	uintptr_t entityList = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) || !entityList)
		return;

	__try
	{
		// Гранаты и прочие сущности находятся после игроков (индексы 65-1024)
		for (int i = 65; i < 1024; ++i)
		{
			uintptr_t listEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * ((i & 0x7FFF) >> 9) + 0x10, listEntry) || !listEntry)
				continue;

			uintptr_t entity = 0;
			if (!TryRead<uintptr_t>(listEntry + 0x70 * (i & 0x1FF), entity) || !entity)
				continue;

			// Получаем EntityIdentity (0x10) -> designerName (0x20)
			uintptr_t entityIdentity = 0;
			if (!TryRead<uintptr_t>(entity + 0x10, entityIdentity) || !entityIdentity)
				continue;

			uintptr_t designerNamePtr = 0;
			if (!TryRead<uintptr_t>(entityIdentity + 0x20, designerNamePtr) || !designerNamePtr)
				continue;

			// Читаем имя сущности
			char nameBuf[32] = { 0 };
			for (int j = 0; j < 31; ++j)
			{
				TryRead<char>(designerNamePtr + j, nameBuf[j]);
				if (nameBuf[j] == '\0')
					break;
			}

			// Если это граната дыма - рассеиваем её
			if (strstr(nameBuf, "smoke"))
			{
				(void)TryWrite<bool>(entity + C_SmokeGrenadeProjectile::m_bDidSmokeEffect, true);
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

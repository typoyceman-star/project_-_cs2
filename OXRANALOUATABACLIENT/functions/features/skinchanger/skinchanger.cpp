#include "skinchanger.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"

void InitSkinConfig()
{
	// Все оружия инициализируются дефолтным конфигом (paintKit=0, wear=0.0001, seed=0, без имени).
	// Пользователь выбирает скины в меню, они сохраняются в g_skinConfig.
	static const int defaultWeapons[] = {
		1,  // Desert Eagle
		4,  // Glock-18
		7,  // AK-47
		9,  // AWP
		10, // FAMAS
		13, // Galil-AR
		16, // M4A4
		30, // TEC-9
		40, // SSG-08
		60, // M4A1-S
		61, // USP-S
	};
	for (int def : defaultWeapons) {
		// emplace без перезаписи: если пользователь уже что-то загрузил из конфига, не трогаем.
		g_skinConfig.emplace(def, WeaponSkinCfg{});
	}
}

int GetPaintKitForWeapon(int defIndex)
{
	auto it = g_skinConfig.find(defIndex);
	if (it != g_skinConfig.end())
		return it->second.paintKit;
	return 0; // Нет скина
}

// def_index ножей в CS2: дефолтный CT-knife = 42, T-knife = 59,
// все «коллекционные» ножи лежат в диапазоне 500..526 (Bayonet, Karambit, M9 и т.д.).
static inline bool IsKnifeDefIndex(uint16_t defIndex)
{
	return defIndex == 42 || defIndex == 59 || (defIndex >= 500 && defIndex <= 526);
}

// Безопасно записывает строку в C_EconItemView::m_szCustomName (161 байт).
// Если src пустая — обнуляет первый байт (как делал старый код).
static void WriteCustomName(uintptr_t itemView, const char* src)
{
	using namespace cs2_dumper::schemas::client_dll;
	const uintptr_t base = itemView + C_EconItemView::m_szCustomName;
	if (!src || src[0] == '\0') {
		(void)TryWrite<char>(base, '\0');
		return;
	}
	// Копируем максимум 160 символов + NUL.
	size_t n = 0;
	while (n < 160 && src[n] != '\0') {
		(void)TryWrite<char>(base + n, src[n]);
		++n;
	}
	(void)TryWrite<char>(base + n, '\0');
}


struct UtlVecPtr
{
	uint64_t size;
	uintptr_t ptr;
};

struct UtlVecPtr32
{
	uint32_t size;
	uint32_t pad;
	uintptr_t ptr;
};

bool TryReadEconAttributesVec(uintptr_t itemView, bool dynamicList, uintptr_t& outPtr, uint64_t& outSize)
{
	using namespace cs2_dumper::schemas::client_dll;

	uintptr_t baseList = itemView + (dynamicList ? C_EconItemView::m_NetworkedDynamicAttributes : C_EconItemView::m_AttributeList);
	uintptr_t attributesVec = baseList + CAttributeList::m_Attributes;

	// Auto-detect layout of C_UtlVectorEmbeddedNetworkVar<T> by probing candidate {size,ptr} pairs
	// and validating that ptr points to a CEconItemAttribute array.
	uint8_t blob[0x40] = {};
	if (TryRead<uint8_t>(attributesVec, blob[0]) == false)
		return false;
	for (size_t i = 1; i < sizeof(blob); ++i)
		(void)TryRead<uint8_t>(attributesVec + i, blob[i]);

	auto tryCandidate = [&](size_t sizeOff, bool size32, size_t ptrOff, const char* tag) -> bool {
		uint64_t size = 0;
		uintptr_t ptr = 0;
		if (size32)
		{
			uint32_t s32 = *(uint32_t*)(blob + sizeOff);
			size = (uint64_t)s32;
		}
		else
		{
			size = *(uint64_t*)(blob + sizeOff);
		}
		ptr = *(uintptr_t*)(blob + ptrOff);

		if (size == 0 || size > 256)
			return false;
		if (ptr < 0x10000)
			return false;

		uint16_t def = 0;
		if (!TryRead<uint16_t>(ptr + CEconItemAttribute::m_iAttributeDefinitionIndex, def))
			return false;

		outPtr = ptr;
		outSize = size;

		return true;
	};

	// Common patterns to probe inside the embedded vector blob
	// - {u64 size, ptr} at (0x0,0x8)
	if (tryCandidate(0x0, false, 0x8, "A0")) return true;
	// - {u32 size, u32 pad, ptr} at (0x0,0x8)
	if (tryCandidate(0x0, true, 0x8, "B0")) return true;
	// - {ptr, u64 size} at (0x0,0x8)
	if (tryCandidate(0x8, false, 0x0, "A1")) return true;
	// - {ptr, u32 size} at (0x0,0x8/0x10) variants
	if (tryCandidate(0x8, true, 0x0, "B1")) return true;
	if (tryCandidate(0x10, true, 0x0, "B2")) return true;
	// - CUtlVector-like {ptr, size, capacity} where ptr at 0x0 and size at 0x8 or 0x10
	if (tryCandidate(0x8, true, 0x0, "V0")) return true;
	if (tryCandidate(0x10, true, 0x0, "V1")) return true;

	// If none matched, keep silent; caller will log aggregate ok/found.

	return false;
}

bool TrySetEconAttributeFloatInList(uintptr_t itemView, bool dynamicList, uint16_t attributeDefIndex, float value, bool& outFoundDef)
{
	using namespace cs2_dumper::schemas::client_dll;
	uintptr_t ptr = 0;
	uint64_t size = 0;
	if (!TryReadEconAttributesVec(itemView, dynamicList, ptr, size))
		return false;

	if (size == 0 || !ptr)
		return false;
	if (size > 64)
		return false;

	for (uint64_t i = 0; i < size; ++i)
	{
		uintptr_t attr = ptr + (i * 0x48);
		uint16_t def = 0;
		if (!TryRead<uint16_t>(attr + CEconItemAttribute::m_iAttributeDefinitionIndex, def))
			continue;
		if (def != attributeDefIndex)
			continue;

		outFoundDef = true;
		(void)TryWrite<float>(attr + CEconItemAttribute::m_flValue, value);
		(void)TryWrite<float>(attr + CEconItemAttribute::m_flInitialValue, value);
		return true;
	}

	return false;
}

bool TrySetEconAttributeFloat(uintptr_t itemView, uint16_t attributeDefIndex, float value)
{
	using namespace cs2_dumper::schemas::client_dll;
	bool foundDefA = false;
	bool foundDefB = false;
	bool okA = TrySetEconAttributeFloatInList(itemView, false, attributeDefIndex, value, foundDefA);
	bool okB = TrySetEconAttributeFloatInList(itemView, true, attributeDefIndex, value, foundDefB);

	return okA || okB;
}

void UpdateSkinChangerHooked() {
	uintptr_t client = GetClientBase();
	if (!client) return;

	using namespace cs2_dumper::schemas::client_dll;

	// Стандартные проверки валидности...
	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;

	uintptr_t weaponServices = 0;
	if (!TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices) || !weaponServices) return;

	uintptr_t entityList = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) || !entityList) return;

	uintptr_t hMyWeapons = weaponServices + CPlayer_WeaponServices::m_hMyWeapons;
	int weaponCount = 0;
	TryRead<int>(hMyWeapons, weaponCount);
	uintptr_t pElements = 0;
	TryRead<uintptr_t>(hMyWeapons + 0x8, pElements);
	
	// ЗАЩИТА ОТ КРАША В МЕНЮ: ограничиваем разумным максимумом
	if (weaponCount <= 0 || weaponCount > 64 || !pElements) return;
	
	uint32_t hActiveWeapon = 0;
	TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActiveWeapon);

	// Проверяем, изменил ли юзер что-то в меню
	static int lastRevision = 0;
	bool menuChanged = (g_skinRevision != lastRevision);
	if (menuChanged) lastRevision = g_skinRevision;

	for (int i = 0; i < weaponCount; ++i) {
		uint32_t hWeapon = 0;
		if (!TryRead<uint32_t>(pElements + (i * sizeof(uint32_t)), hWeapon)) continue;
		if (!hWeapon || hWeapon == 0xFFFFFFFF) continue;

		int wIdx = hWeapon & 0x1FF;
		int wEntry = (hWeapon & 0x7FFF) >> 9;
		uintptr_t wListEntry = 0;
		if (!TryRead<uintptr_t>(entityList + 0x8 * wEntry + 0x10, wListEntry) || !wListEntry) continue;
		
		uintptr_t weaponEnt = 0;
		if (!TryRead<uintptr_t>(wListEntry + 0x70 * wIdx, weaponEnt) || !weaponEnt) continue;

		uintptr_t itemView = weaponEnt + C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item;
		
		uint16_t defIndex = 0;
		if (!TryRead<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, defIndex)) continue;

		// --- Решаем, что применять: ножевой конфиг или конфиг скина оружия. ---
		int targetPaintKit = 0;
		float targetWear = 0.0001f;
		int targetSeed = 0;
		const char* targetName = nullptr;
		bool overrideKnifeModel = false;

		const bool isKnife = IsKnifeDefIndex(defIndex);
		if (isKnife && g_knifeEnabled) {
			// Конфиг ножа применяется ко всем ножам в инвентаре.
			targetPaintKit = g_knifePaintKit;
			targetWear = g_knifeWear;
			targetSeed = g_knifeSeed;
			targetName = g_knifeName;
			overrideKnifeModel = (g_knifeDefIndex != 0 && g_knifeDefIndex != defIndex);
		} else if (!isKnife) {
			auto it = g_skinConfig.find(defIndex);
			if (it == g_skinConfig.end()) continue;
			const WeaponSkinCfg& cfg = it->second;
			if (cfg.paintKit <= 0) continue;
			targetPaintKit = cfg.paintKit;
			targetWear = cfg.wear;
			targetSeed = cfg.seed;
			targetName = cfg.customName;
		} else {
			continue;
		}

		if (targetPaintKit <= 0 && !overrideKnifeModel) continue;

		// --- ЖЕСТКАЯ ЛОГИКА ОБНОВЛЕНИЯ ---
		// Если PaintKit изменился ИЛИ мы нажали "обновить" в меню ИЛИ ID еще не сгенерирован
		bool needUpdate = (g_appliedKits[weaponEnt] != targetPaintKit) || menuChanged || (g_generatedIDs[weaponEnt] == 0) || overrideKnifeModel;

		if (needUpdate) {
			// 1. Генерируем НОВЫЙ СЛУЧАЙНЫЙ ID.
			// Это заставляет сервер/клиент думать, что это ВООБЩЕ ДРУГОЙ ПРЕДМЕТ.
			// Кэш шейдеров сбрасывается, потому что ID новый.
			uint32_t newHighID = 1;
			
			g_generatedIDs[weaponEnt] = newHighID;
			g_appliedKits[weaponEnt] = targetPaintKit;

			// 2. Полная зачистка владельца
			(void)TryWrite<uint32_t>(itemView + C_EconItemView::m_iAccountID, 0);
			(void)TryWrite<uint32_t>(weaponEnt + C_EconEntity::m_OriginalOwnerXuidLow, 0);
			(void)TryWrite<uint32_t>(weaponEnt + C_EconEntity::m_OriginalOwnerXuidHigh, 0);

			// 3. Запись "Нового" предмета
			(void)TryWrite<uint32_t>(itemView + C_EconItemView::m_iItemIDHigh, newHighID);
			(void)TryWrite<uint32_t>(itemView + C_EconItemView::m_iItemIDLow, 0);

			// 3b. Если включён knife changer и в слоте — нож, переписываем модель.
			if (overrideKnifeModel) {
				(void)TryWrite<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex,
					(uint16_t)g_knifeDefIndex);
			}

			// 4. Основные параметры (paintKit / seed / wear)
			(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackPaintKit, targetPaintKit);
			(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackSeed, targetSeed);
			(void)TryWrite<float>(weaponEnt + C_EconEntity::m_flFallbackWear, targetWear);

			// 5. StatTrak -1 (Выкл, чтобы не багало UV)
			(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackStatTrak, -1);
			
			// 6. Quality = 4
			(void)TryWrite<int32_t>(itemView + C_EconItemView::m_iEntityQuality, 4);
			
			// 6b. Update attributes (used by engine for skin material/UV)
			// Only patches existing attribute entries; does not allocate/resize.
			(void)TrySetEconAttributeFloat(itemView, 6, (float)targetPaintKit);
			(void)TrySetEconAttributeFloat(itemView, 7, (float)targetSeed);
			(void)TrySetEconAttributeFloat(itemView, 8, targetWear);
			
			// 7. Кастомное имя (или пустое — обнулим первый байт).
			WriteCustomName(itemView, targetName);
			
			// 8. Viewmodel Update
		}
	}
}

#include "skinchanger.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../core/mem_alloc.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"
#include "../../../PatternScan.h"
#include <cstring>

void InitSkinConfig()
{
	// Все оружия инициализируются дефолтным конфигом (paintKit=0, wear=0.0001, seed=0, без имени).
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
		g_skinConfig.emplace(def, WeaponSkinCfg{});
	}
}

int GetPaintKitForWeapon(int defIndex)
{
	auto it = g_skinConfig.find(defIndex);
	if (it != g_skinConfig.end())
		return it->second.paintKit;
	return 0;
}

// def_index ножей: дефолтный CT-knife=42, T-knife=59, коллекционные ножи 500..526.
static inline bool IsKnifeDefIndex(uint16_t defIndex)
{
	return defIndex == 42 || defIndex == 59 || (defIndex >= 500 && defIndex <= 526);
}

// ---------------------------------------------------------------------------
// CEconItemAttribute layout (повторяет реф):
//   pad[0x30]; uint16 def_index@0x30; pad[2]; float value@0x34; float init@0x38;
//   int32 refundable@0x3C; bool set_bonus@0x40; pad[7];  total = 0x48
// ---------------------------------------------------------------------------
struct EconAttribute
{
	char     pad0[0x30];
	uint16_t defIndex;
	char     pad1[2];
	float    value;
	float    initValue;
	int32_t  refundableCurrency;
	bool     setBonus;
	char     pad2[7];
};
static_assert(sizeof(EconAttribute) == 0x48, "EconAttribute size mismatch");

// Внутри m_AttributeList по offset m_Attributes(=0x8) лежит:
//   uint64_t size;   // +0x0
//   uintptr_t ptr;   // +0x8 — указатель на массив EconAttribute
struct AttrVecEmbedded
{
	uint64_t  size;
	uintptr_t ptr;
};

enum : uint16_t
{
	ATTR_PAINT   = 6,
	ATTR_PATTERN = 7,
	ATTR_WEAR    = 8,
};

// Безопасно записывает строку в C_EconItemView::m_szCustomName (161 байт).
static void WriteCustomName(uintptr_t itemView, const char* src)
{
	using namespace cs2_dumper::schemas::client_dll;
	const uintptr_t base = itemView + C_EconItemView::m_szCustomName;
	if (!src || src[0] == '\0') {
		(void)TryWrite<char>(base, '\0');
		return;
	}
	size_t n = 0;
	while (n < 160 && src[n] != '\0') {
		(void)TryWrite<char>(base + n, src[n]);
		++n;
	}
	(void)TryWrite<char>(base + n, '\0');
}

// Освобождает текущий массив атрибутов (если он был выделен GameAlloc'ом).
// Если вектор пуст — ничего не делает.
static void RemoveAttributes(uintptr_t itemView)
{
	using namespace cs2_dumper::schemas::client_dll;
	uintptr_t vecAddr = itemView + C_EconItemView::m_AttributeList + CAttributeList::m_Attributes;
	AttrVecEmbedded vec{};
	if (!TryRead<AttrVecEmbedded>(vecAddr, vec)) return;
	if (vec.size == 0 || vec.ptr == 0) return;

	void* old = reinterpret_cast<void*>(vec.ptr);
	AttrVecEmbedded zero{ 0, 0 };
	(void)TryWrite<AttrVecEmbedded>(vecAddr, zero);
	GameFree(old);
}

// Создаёт массив из 3 атрибутов (paint/pattern/wear) и записывает его в item view.
// Если массив уже не пустой — сначала вызывается RemoveAttributes.
static void CreateAttributes(uintptr_t itemView, int paintKit, float wear, int seed)
{
	using namespace cs2_dumper::schemas::client_dll;
	if (paintKit <= 0) return;

	uintptr_t vecAddr = itemView + C_EconItemView::m_AttributeList + CAttributeList::m_Attributes;
	AttrVecEmbedded vec{};
	if (!TryRead<AttrVecEmbedded>(vecAddr, vec)) return;
	if (vec.size != 0 || vec.ptr != 0) {
		// Если уже есть — освобождаем перед заменой.
		RemoveAttributes(itemView);
	}

	constexpr size_t count = 3;
	auto* attrs = static_cast<EconAttribute*>(GameAlloc(count * sizeof(EconAttribute)));
	if (!attrs) return;
	std::memset(attrs, 0, count * sizeof(EconAttribute));

	attrs[0].defIndex  = ATTR_PAINT;
	attrs[0].value     = static_cast<float>(paintKit);
	attrs[0].initValue = attrs[0].value;

	attrs[1].defIndex  = ATTR_PATTERN;
	attrs[1].value     = static_cast<float>(seed >= 0 ? seed : 0);
	attrs[1].initValue = attrs[1].value;

	attrs[2].defIndex  = ATTR_WEAR;
	attrs[2].value     = wear >= 0.0f ? wear : 0.0001f;
	attrs[2].initValue = attrs[2].value;

	AttrVecEmbedded newVec{ count, reinterpret_cast<uintptr_t>(attrs) };
	(void)TryWrite<AttrVecEmbedded>(vecAddr, newVec);
}

// SEH-обёртки для движковых vfunc'ов. Вынесены в отдельные функции,
// потому что MSVC запрещает __try в функциях с объектами C++ unwinding.
static void SafeUpdateSkinVFunc(uintptr_t weaponEnt)
{
	__try {
		using Fn = void(__fastcall*)(void*, bool);
		void** vt = *reinterpret_cast<void***>(weaponEnt);
		Fn fn = reinterpret_cast<Fn>(vt[110]);
		fn(reinterpret_cast<void*>(weaponEnt), true);
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void SafeUpdateWeaponDataVFunc(uintptr_t weaponEnt)
{
	__try {
		using Fn = void*(__fastcall*)(void*);
		void** vt = *reinterpret_cast<void***>(weaponEnt);
		Fn fn = reinterpret_cast<Fn>(vt[195]);
		(void)fn(reinterpret_cast<void*>(weaponEnt));
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ---------------------------------------------------------------------------
// Pattern scan: set_mesh_group_mask(scene_node, uint64_t mask).
// Сигнатура взята из реф-проекта (skin-_cs2/c_cs_player_pawn.hpp:387).
// Кэшируется при первом успешном поиске.
// ---------------------------------------------------------------------------
using SetMeshGroupMaskFn = void(__fastcall*)(void*, uint64_t);
static SetMeshGroupMaskFn g_setMeshGroupMask = nullptr;

static SetMeshGroupMaskFn ResolveSetMeshGroupMask()
{
	if (g_setMeshGroupMask) return g_setMeshGroupMask;
	auto offset = pattern_scan::FindOffsetInModule(
		"client.dll",
		"48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8D 99 ? ? ? ? 48 8B 71");
	if (!offset.has_value()) return nullptr;
	uintptr_t client = GetClientBase();
	if (!client) return nullptr;
	g_setMeshGroupMask = reinterpret_cast<SetMeshGroupMaskFn>(client + offset.value());
	return g_setMeshGroupMask;
}

static void SetMeshGroupMask(uintptr_t sceneNode, uint64_t mask)
{
	if (!sceneNode) return;
	auto fn = ResolveSetMeshGroupMask();
	if (!fn) return;
	__try { fn(reinterpret_cast<void*>(sceneNode), mask); }
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ---------------------------------------------------------------------------
// Применение скина: повторяет логику c_skin_changer::apply_skin из реф-проекта.
// ---------------------------------------------------------------------------
static void ApplySkin(uintptr_t weaponEnt, uintptr_t itemView,
	int paintKit, float wear, int seed, const char* customName,
	bool overrideKnifeModel, uint16_t newDefIndex)
{
	using namespace cs2_dumper::schemas::client_dll;

	// 1. Заменяем массив атрибутов (paint/pattern/wear).
	RemoveAttributes(itemView);
	if (paintKit > 0)
		CreateAttributes(itemView, paintKit, wear, seed);

	// 2. Меняем модель ножа (если включён knife changer).
	if (overrideKnifeModel) {
		(void)TryWrite<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, newDefIndex);
		(void)TryWrite<int32_t>(itemView + C_EconItemView::m_iEntityQuality, 3); // QUALITY_UNUSUAL
	}

	// 3. Fallback-поля C_EconEntity (читаются движком при отрисовке).
	(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackPaintKit, paintKit);
	(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackSeed, seed);
	(void)TryWrite<float>(weaponEnt  + C_EconEntity::m_flFallbackWear, wear);
	(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackStatTrak, -1);

	// 4. Кастомное имя.
	WriteCustomName(itemView, customName);

	// 5. Обнуляем ItemID (новый ID = заставляем клиент пересчитать кэш материала).
	(void)TryWrite<uint32_t>(itemView + C_EconItemView::m_iItemIDHigh, 1);
	(void)TryWrite<uint32_t>(itemView + C_EconItemView::m_iItemIDLow, 0);
	(void)TryWrite<uint32_t>(itemView + C_EconItemView::m_iAccountID, 0);
	(void)TryWrite<uint32_t>(weaponEnt + C_EconEntity::m_OriginalOwnerXuidLow, 0);
	(void)TryWrite<uint32_t>(weaponEnt + C_EconEntity::m_OriginalOwnerXuidHigh, 0);

	// 6. set_mesh_group_mask на scene_node оружия — фиксит UV-развёртку.
	//    Большинство современных paint kit'ов используют новый меш => mask = 1.
	//    Без этого вызова UV ползёт (это и есть баг, который видит юзер).
	uintptr_t sceneNode = 0;
	if (TryRead<uintptr_t>(weaponEnt + C_BaseEntity::m_pGameSceneNode, sceneNode) && sceneNode) {
		SetMeshGroupMask(sceneNode, /*new model*/ 1);
	}

	// 7. Виртуальные функции движка для пересчёта скина и weapon_data.
	//    В реф-проекте: update_skin = idx 110, update_weapon_data = idx 195.
	SafeUpdateSkinVFunc(weaponEnt);
	SafeUpdateWeaponDataVFunc(weaponEnt);
}

void UpdateSkinChangerHooked()
{
	using namespace cs2_dumper::schemas::client_dll;
	uintptr_t client = GetClientBase();
	if (!client) return;

	// Однократно поднимаем GameAlloc.
	(void)InitGameAlloc();

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

	if (weaponCount <= 0 || weaponCount > 64 || !pElements) return;

	// Триггер «пересобрать скин» при изменении любого UI-параметра.
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

		// --- Решаем, что применять. ---
		int targetPaintKit = 0;
		float targetWear = 0.0001f;
		int targetSeed = 0;
		const char* targetName = nullptr;
		bool overrideKnifeModel = false;
		uint16_t newDefIndex = defIndex;

		const bool isKnife = IsKnifeDefIndex(defIndex);
		if (isKnife && g_knifeEnabled) {
			targetPaintKit = g_knifePaintKit;
			targetWear = g_knifeWear;
			targetSeed = g_knifeSeed;
			targetName = g_knifeName;
			if (g_knifeDefIndex != 0 && (uint16_t)g_knifeDefIndex != defIndex) {
				overrideKnifeModel = true;
				newDefIndex = (uint16_t)g_knifeDefIndex;
			}
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

		// Если ничего не изменилось — пропускаем (иначе движок будет каждый кадр
		// перевыделять память и тормозить).
		bool needUpdate = (g_appliedKits[weaponEnt] != targetPaintKit)
			|| menuChanged
			|| (g_generatedIDs[weaponEnt] == 0)
			|| overrideKnifeModel;
		if (!needUpdate) continue;

		g_appliedKits[weaponEnt] = targetPaintKit;
		g_generatedIDs[weaponEnt] = 1;

		ApplySkin(weaponEnt, itemView,
			targetPaintKit, targetWear, targetSeed, targetName,
			overrideKnifeModel, newDefIndex);
	}
}

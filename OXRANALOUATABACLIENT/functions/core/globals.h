#pragma once
// Все общие глобальные переменные проекта.
// Декларации (extern); определения — в core/globals.cpp.

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cwchar>
#include <algorithm>
#include <limits>
#include <map>
#include <vector>
#include <string>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dcomp.h>

#include "imgui.h"

#include "types.h"
#include "../config/runtime_offsets.h"

// ---- Состояние процесса / основной DLL ----
extern HMODULE g_hModule;
extern volatile bool g_running;
extern HANDLE g_hMainThread;
extern DWORD  g_mainThreadId;

// ---- Оверлейное окно ----
extern HWND g_hOverlayWnd;
extern const wchar_t* g_wndClassName;

// ---- D3D11 + DComp ----
extern ID3D11Device*           g_pd3dDevice;
extern ID3D11DeviceContext*    g_pd3dDeviceContext;
extern IDXGISwapChain1*        g_pSwapChain;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

extern IDCompositionDevice*    g_dcompDevice;
extern IDCompositionTarget*    g_dcompTarget;
extern IDCompositionVisual*    g_dcompVisual;

// ---- Оффсеты CS2 (рантайм) ----
extern RuntimeOffsets g_offsetsRuntime;

// ---- Локальное состояние игрока ----
extern bool g_isLocalSniperScoped;

// ---- ESP / списки целей ----
extern std::vector<EspBox> g_espBoxes;
extern std::vector<std::string> g_spectators;
extern bool g_spectatorListEnabled;
extern std::vector<EspTargetWorld> g_espTargetsWorld;

extern double g_gameTimeBaseTickMs;
extern double g_gameTimeBaseSeconds;

extern ImVec4 g_flashTextColorOn;
extern ImVec4 g_flashTextColorOff;
extern float g_flashTextScale;

extern ImVec4 g_guiColor;

extern std::vector<BoneCacheEntryWorld> g_boneCache;

extern BombData g_bombData;
extern bool g_bombEspEnabled;

// ---- Хуки ----
extern FrameStageNotify_t oFrameStageNotify;
extern void** g_pSource2ClientVTable;

// ВАЖНО: индекс FrameStageNotify в VTable Source2Client может меняться при апдейтах CS2.
constexpr int FRAMESTAGENOTIFY_INDEX = 36;

// ---- Меню / общие тогглы ----
extern bool g_menuOpen;
extern bool g_skinMenuOpen;
extern bool g_skinWarningShown;
extern bool g_antiCaptureEnabled;

// ---- External features ----
extern bool g_snaplinesEnabled;
extern bool g_distanceEnabled;
extern bool g_radarHackEnabled;
extern float g_radarScale;
extern float g_radarSize;
extern bool g_autoStrafeEnabled;

// Bhop
extern bool g_bhopEnabled;
extern int  g_bhopKey;

// ESP (боксы)
extern bool g_whEnabled;
extern int  g_whKey;
extern ImVec4 g_boxColor;
extern float  g_boxPadding;
extern float  g_boxThickness;
extern bool g_dynamicBoxColor;

// Chams
extern bool g_chamsEnabled;
extern ImVec4 g_chamsColorT;
extern ImVec4 g_chamsColorCT;

// No Smoke
extern bool g_noSmokeEnabled;

// ESP extra
extern bool   g_nameEspEnabled;
extern bool   g_gunEspEnabled;
extern ImVec4 g_nameColor;
extern ImVec4 g_gunColor;
extern ImVec4 g_weaponIconColor;
extern float  g_hpBarWidth;
extern float  g_hpBarOffset;
extern float  g_nameOffsetY;
extern float  g_gunOffsetY;
extern float  g_gunOffsetX;

// HP Bar
extern bool  g_hpBarEnabled;
extern ImVec4 g_hpBarColor;

// OOF Arrows
extern bool g_oofArrowsEnabled;
extern float g_oofArrowsRadius;
extern float g_oofArrowsSize;
extern ImVec4 g_oofArrowsColor;

// Crosshair
extern bool  g_customCrosshair;

// Bones
extern bool g_bonesEnabled;
extern bool g_boneDebugIds;
extern bool g_boneHideLines;
extern int  g_bonesKey;
extern ImVec4 g_boneColor;

// Combat (triggerbot + RCS)
extern bool g_triggerEnabled;
extern int  g_triggerKey;
extern bool g_triggerTeamCheck;
extern bool g_triggerScopeOnly;
extern bool g_triggerHeadOnly;
extern bool g_triggerVisCheck;
extern float g_triggerAccuracyThreshold;
extern int  g_triggerToggleKey;

// Aimbot
extern bool g_aimbotEnabled;
extern int  g_aimbotKey;
extern bool g_aimbotTeamCheck;
extern int  g_aimbotBone;
extern float g_aimbotFov;
extern float g_aimbotSmooth;
extern bool g_aimbotDrawFov;
extern ImVec4 g_aimbotFovColor;
extern bool g_aimbotVisCheck;
extern bool g_aimbotAutoFire;

// Player
extern bool g_antiFlashEnabled;

// FOV
extern bool  g_fovEnabled;
extern float g_fovValue;

// No recoil / no spread
extern bool g_noRecoilEnabled;
extern bool g_noSpreadEnabled;

// HitSound & Hitmarker
extern bool g_hitSoundEnabled;
extern float g_hitmarkerAlpha;
extern ULONGLONG g_hitmarkerTime;
extern std::map<std::uintptr_t, int> g_lastEnemyHp;

// Damage indicators
extern bool g_damageIndicatorsEnabled;
extern std::vector<DamageText> g_damageTexts;
extern ImVec4 g_damageColor;
extern float g_damageTextSize;
extern float g_damageTextLifetime;

// Кэш звука + тайминги
extern LPVOID g_hitSoundBuffer;
extern ULONGLONG g_lastLocalShotTime;

// Aim auto fire state
extern bool g_autoFireActive;

// Keybinds list
extern bool g_keybindsListEnabled;

// ---- Skin Changer ----
extern bool g_skinChangerEnabled;
// На каждое оружие — свой набор настроек (paintKit, wear, seed, customName).
extern std::map<int, WeaponSkinCfg> g_skinConfig;
extern std::map<std::uintptr_t, int> g_appliedSkins;
extern std::uint32_t g_lastActiveWeapon;
extern ULONGLONG g_lastSkinUpdateTick;
extern LONG g_skinIdCounter;
extern int g_skinUpdateCounter;
extern int g_skinRevision;
extern int g_lastSkinUpdateCounterSeen;
extern std::map<std::uintptr_t, std::uint32_t> g_generatedIDs;
extern std::map<std::uintptr_t, int> g_appliedKits;

// ---- Knife Changer ----
// При включённом g_knifeEnabled любой нож в инвентаре локального игрока
// будет переписан в g_knifeDefIndex (модель) с заданным paintKit/wear/seed/name.
extern bool g_knifeEnabled;
extern int  g_knifeDefIndex;     // C_EconItemView::m_iItemDefinitionIndex
extern int  g_knifePaintKit;
extern float g_knifeWear;
extern int  g_knifeSeed;
extern char g_knifeName[64];

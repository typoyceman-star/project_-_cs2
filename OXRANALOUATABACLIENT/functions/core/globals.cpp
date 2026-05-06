#include "globals.h"

// ---- Состояние процесса / основной DLL ----
HMODULE g_hModule = nullptr;
volatile bool g_running = true;
HANDLE g_hMainThread = nullptr;
DWORD  g_mainThreadId = 0;

// ---- Оверлейное окно ----
HWND g_hOverlayWnd = nullptr;
const wchar_t* g_wndClassName = L"CS2_DX11_Overlay";

// ---- D3D11 + DComp ----
ID3D11Device*           g_pd3dDevice = nullptr;
ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
IDXGISwapChain1*        g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

IDCompositionDevice*    g_dcompDevice = nullptr;
IDCompositionTarget*    g_dcompTarget = nullptr;
IDCompositionVisual*    g_dcompVisual = nullptr;

// ---- Оффсеты CS2 (рантайм) ----
RuntimeOffsets g_offsetsRuntime{};

// ---- Локальное состояние игрока ----
bool g_isLocalSniperScoped = false;

// ---- ESP / списки целей ----
std::vector<EspBox> g_espBoxes;
std::vector<std::string> g_spectators;
bool g_spectatorListEnabled = true;
std::vector<EspTargetWorld> g_espTargetsWorld;

double g_gameTimeBaseTickMs = 0.0;
double g_gameTimeBaseSeconds = 0.0;

ImVec4 g_flashTextColorOn  = ImVec4(1.0f, 0.9f, 0.1f, 1.0f);
ImVec4 g_flashTextColorOff = ImVec4(0.5f, 0.9f, 0.5f, 1.0f);
float  g_flashTextScale = 1.0f;

ImVec4 g_guiColor = ImVec4(0.35f, 0.45f, 0.85f, 1.00f);

std::vector<BoneCacheEntryWorld> g_boneCache;

BombData g_bombData = { {0,0,0}, 0.0f, false, false };
bool g_bombEspEnabled = true;

// ---- Хуки ----
FrameStageNotify_t oFrameStageNotify = nullptr;
void** g_pSource2ClientVTable = nullptr;

// ---- Меню / общие тогглы ----
bool g_menuOpen = false;
bool g_skinMenuOpen = false;
bool g_skinWarningShown = false;
bool g_antiCaptureEnabled = false;

// ---- External features ----
bool g_snaplinesEnabled = false;
bool g_distanceEnabled = true;
bool g_radarHackEnabled = false;
float g_radarScale = 4.0f;
float g_radarSize  = 160.0f;
bool g_autoStrafeEnabled = false;

// Bhop
bool g_bhopEnabled = true;
int  g_bhopKey = VK_SPACE;

// ESP (боксы)
bool g_whEnabled = true;
int  g_whKey = VK_F7;
ImVec4 g_boxColor = ImVec4(1.0f, 0.90f, 0.43f, 1.0f);
float  g_boxPadding = 1.5f;
float  g_boxThickness = 2.0f;
bool g_dynamicBoxColor = false;

// Chams
bool g_chamsEnabled = false;
ImVec4 g_chamsColorT  = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
ImVec4 g_chamsColorCT = ImVec4(0.0f, 0.65f, 1.0f, 1.0f);

// No Smoke
bool g_noSmokeEnabled = false;

// ESP extra
bool   g_nameEspEnabled = true;
bool   g_gunEspEnabled  = true;
ImVec4 g_nameColor       = ImVec4(0.90f, 0.90f, 0.95f, 1.0f);
ImVec4 g_gunColor        = ImVec4(0.75f, 0.80f, 1.0f, 1.0f);
ImVec4 g_weaponIconColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
float  g_hpBarWidth  = 4.0f;
float  g_hpBarOffset = 3.0f;
float  g_nameOffsetY = 14.0f;
float  g_gunOffsetY  = 2.0f;
float  g_gunOffsetX  = 0.0f;

// HP Bar
bool  g_hpBarEnabled = true;
ImVec4 g_hpBarColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

// OOF Arrows
bool g_oofArrowsEnabled = false;
float g_oofArrowsRadius = 150.0f;
float g_oofArrowsSize   = 15.0f;
ImVec4 g_oofArrowsColor = ImVec4(1.0f, 0.2f, 0.2f, 0.8f);

// Crosshair
bool g_customCrosshair = true;

// Bones
bool g_bonesEnabled = false;
bool g_boneDebugIds = false;
bool g_boneHideLines = false;
int  g_bonesKey = 0;
ImVec4 g_boneColor = ImVec4(0.60f, 0.88f, 1.00f, 1.0f);

// Combat
bool g_triggerEnabled = true;
int  g_triggerKey = 'X';
bool g_triggerTeamCheck = true;
bool g_triggerScopeOnly = false;
bool g_triggerHeadOnly = false;
bool g_triggerVisCheck = true;
float g_triggerAccuracyThreshold = 0.015f;
int  g_triggerToggleKey = 0;

// Aimbot
bool g_aimbotEnabled = false;
int  g_aimbotKey = VK_XBUTTON2;
bool g_aimbotTeamCheck = true;
int  g_aimbotBone = 6;
float g_aimbotFov = 5.0f;
float g_aimbotSmooth = 3.0f;
bool g_aimbotDrawFov = true;
ImVec4 g_aimbotFovColor = ImVec4(1.0f, 1.0f, 0.0f, 0.4f);
bool g_aimbotVisCheck = true;
bool g_aimbotAutoFire = false;

// Player
bool g_antiFlashEnabled = false;

// FOV
bool  g_fovEnabled = false;
float g_fovValue = 90.0f;

// No recoil / no spread
bool g_noRecoilEnabled = false;
bool g_noSpreadEnabled = false;

// HitSound & Hitmarker
bool g_hitSoundEnabled = false;
float g_hitmarkerAlpha = 0.0f;
ULONGLONG g_hitmarkerTime = 0;
std::map<std::uintptr_t, int> g_lastEnemyHp;

// Damage indicators
bool g_damageIndicatorsEnabled = false;
std::vector<DamageText> g_damageTexts;
ImVec4 g_damageColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
float g_damageTextSize = 20.0f;
float g_damageTextLifetime = 4.0f;

LPVOID g_hitSoundBuffer = nullptr;
ULONGLONG g_lastLocalShotTime = 0;

bool g_autoFireActive = false;

bool g_keybindsListEnabled = false;

// ---- Skin Changer ----
// Старые глобалы удалены — теперь используется g_cfg / g_skin_changer / g_glove_changer.

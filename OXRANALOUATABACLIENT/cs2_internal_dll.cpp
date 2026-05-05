#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dcomp.h>
 #include <objbase.h>
#include <cstdint>
#include <cwchar>
#include <vector>
#include <map>
#include <limits>
#include <string>
#include <dwmapi.h>
#include <wincodec.h>
#include <Psapi.h>
#include <cmath>
#include <algorithm>
#include "resource.h"

// Для воспроизведения звуков из ресурса DLL
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// MinHook library for function hooking
#include "MinHook.h"
// MinHook compiled from source (minhook_src/) — no precompiled .lib needed

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct QAngle { float x, y, z; };

// Структура для быстрого чтения строк блоками
struct StringBuf64 { char data[64]; };

// Глобальная переменная для проверки снайперки в рендере
static bool g_isLocalSniperScoped = false;

static QAngle CalcAngle(Vec3 src, Vec3 dst)
{
	QAngle angles;
	// ИСПРАВЛЕНО: Цель минус Я (dst - src), а не наоборот
	Vec3 delta = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
	
	// ИСПРАВЛЕНИЕ: Используем более точный расчет гипотенузы
	float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);

	// ИСПРАВЛЕНИЕ: Pitch (вертикальный угол) - используем отрицательное значение для правильного направления
	angles.x = -std::atan2(delta.z, hyp) * (180.0f / M_PI);
	
	// ИСПРАВЛЕНИЕ: Yaw (горизонтальный угол) - стандартный расчет
	angles.y = std::atan2(delta.y, delta.x) * (180.0f / M_PI);
	
	angles.z = 0.0f;

	return angles;
}

static float GetFov(QAngle viewAngles, QAngle aimAngles)
{
	float deltaX = aimAngles.x - viewAngles.x;
	float deltaY = aimAngles.y - viewAngles.y;

	if (deltaY > 180.0f) deltaY -= 360.0f;
	if (deltaY < -180.0f) deltaY += 360.0f;

	return std::sqrt(deltaX * deltaX + deltaY * deltaY);
}

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "windowscodecs")
#pragma comment(lib, "Psapi")

// Для операторов математики ImGui (требуется до imgui.h)
#define IMGUI_DEFINE_MATH_OPERATORS

// Берём актуальные оффсеты и схемы из output/
#include "output/offsets.hpp"
#include "output/client_dll.hpp"
#include "output/buttons.hpp"

#include "PatternScan.h"

// JSON Parser для динамической загрузки оффсетов
#include <fstream>
#include <sstream>
#include <map>


// Простой JSON парсер для чтения оффсетов
class SimpleJsonParser
{
public:
	static bool LoadOffsets(const std::string& jsonPath, std::map<std::string, uint32_t>& offsets)
	{
		std::ifstream file(jsonPath);
		if (!file.is_open())
			return false;

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string content = buffer.str();
		file.close();

		// Парсим JSON вручную (простой парсер для нашего формата)
		size_t pos = 0;
		while ((pos = content.find("\"dw", pos)) != std::string::npos)
		{
			// Находим имя оффсета
			size_t nameStart = pos + 1;
			size_t nameEnd = content.find("\"", nameStart);
			if (nameEnd == std::string::npos) break;
			
			std::string name = content.substr(nameStart, nameEnd - nameStart);
			
			// Находим значение
			size_t valueStart = content.find(":", nameEnd);
			if (valueStart == std::string::npos) break;
			valueStart++;
			
			// Пропускаем пробелы
			while (valueStart < content.length() && (content[valueStart] == ' ' || content[valueStart] == '\t'))
				valueStart++;
			
			size_t valueEnd = valueStart;
			while (valueEnd < content.length() && (content[valueEnd] >= '0' && content[valueEnd] <= '9'))
				valueEnd++;
			
			if (valueEnd > valueStart)
			{
				std::string valueStr = content.substr(valueStart, valueEnd - valueStart);
				uint32_t value = static_cast<uint32_t>(std::stoul(valueStr));
				offsets[name] = value;
			}
			
			pos = valueEnd;
		}
		
		return !offsets.empty();
	}

	static bool LoadButtons(const std::string& jsonPath, std::map<std::string, uint32_t>& buttons)
	{
		std::ifstream file(jsonPath);
		if (!file.is_open()) return false;
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string content = buffer.str();
		file.close();

		// Простой поиск нужных кнопок
		auto ParseKey = [&](const std::string& key) {
			size_t pos = content.find("\"" + key + "\"");
			if (pos != std::string::npos) {
				size_t colon = content.find(":", pos);
				if (colon != std::string::npos) {
					size_t valStart = colon + 1;
					while (valStart < content.length() && (content[valStart] == ' ' || content[valStart] == '\t')) valStart++;
					size_t valEnd = valStart;
					while (valEnd < content.length() && content[valEnd] >= '0' && content[valEnd] <= '9') valEnd++;
					if (valEnd > valStart) {
						buttons[key] = static_cast<uint32_t>(std::stoul(content.substr(valStart, valEnd - valStart)));
					}
				}
			}
		};

		ParseKey("attack");
		ParseKey("jump");
		ParseKey("left");
		ParseKey("right");
		return !buttons.empty();
	}

	static bool LoadSchemaOffsets(const std::string& jsonPath, const std::string& className, std::map<std::string, uint32_t>& offsets)
	{
		std::ifstream file(jsonPath);
		if (!file.is_open())
			return false;

		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string content = buffer.str();
		file.close();

		// Ищем класс
		std::string classSearch = "\"" + className + "\"";
		size_t classPos = content.find(classSearch);
		if (classPos == std::string::npos)
			return false;

		// Ищем секцию fields
		size_t fieldsPos = content.find("\"fields\"", classPos);
		if (fieldsPos == std::string::npos)
			return false;

		// Находим начало и конец секции fields
		size_t fieldsStart = content.find("{", fieldsPos);
		if (fieldsStart == std::string::npos)
			return false;

		int braceCount = 1;
		size_t fieldsEnd = fieldsStart + 1;
		while (fieldsEnd < content.length() && braceCount > 0)
		{
			if (content[fieldsEnd] == '{') braceCount++;
			else if (content[fieldsEnd] == '}') braceCount--;
			fieldsEnd++;
		}

		std::string fieldsSection = content.substr(fieldsStart, fieldsEnd - fieldsStart);

		// Парсим поля
		size_t pos = 0;
		while ((pos = fieldsSection.find("\"m_", pos)) != std::string::npos)
		{
			size_t nameStart = pos + 1;
			size_t nameEnd = fieldsSection.find("\"", nameStart);
			if (nameEnd == std::string::npos) break;

			std::string name = fieldsSection.substr(nameStart, nameEnd - nameStart);

			size_t valueStart = fieldsSection.find(":", nameEnd);
			if (valueStart == std::string::npos) break;
			valueStart++;

			while (valueStart < fieldsSection.length() && (fieldsSection[valueStart] == ' ' || fieldsSection[valueStart] == '\t'))
				valueStart++;

			size_t valueEnd = valueStart;
			while (valueEnd < fieldsSection.length() && (fieldsSection[valueEnd] >= '0' && fieldsSection[valueEnd] <= '9'))
				valueEnd++;

			if (valueEnd > valueStart)
			{
				std::string valueStr = fieldsSection.substr(valueStart, valueEnd - valueStart);
				uint32_t value = static_cast<uint32_t>(std::stoul(valueStr));
				offsets[name] = value;
			}

			pos = valueEnd;
		}

		return !offsets.empty();
	}
};

struct RuntimeOffsets
{
    // Базовые оффсеты
    std::uint32_t dwEntityList = cs2_dumper::offsets::client_dll::dwEntityList;
    std::uint32_t dwViewMatrix = cs2_dumper::offsets::client_dll::dwViewMatrix;
    std::uint32_t dwViewAngles = cs2_dumper::offsets::client_dll::dwViewAngles;
    std::uint32_t dwLocalPlayerController = cs2_dumper::offsets::client_dll::dwLocalPlayerController;
    std::uint32_t dwLocalPlayerPawn = cs2_dumper::offsets::client_dll::dwLocalPlayerPawn;
    std::uint32_t dwGlobalVars = cs2_dumper::offsets::client_dll::dwGlobalVars;
    std::uint32_t dwCSGOInput = cs2_dumper::offsets::client_dll::dwCSGOInput;
    std::uint32_t dwPlantedC4 = cs2_dumper::offsets::client_dll::dwPlantedC4;
    std::uint32_t dwForceAttack = cs2_dumper::buttons::attack;
    std::uint32_t dwForceJump   = cs2_dumper::buttons::jump;
    std::uint32_t dwForceLeft   = cs2_dumper::buttons::left;
    std::uint32_t dwForceRight  = cs2_dumper::buttons::right;

    // Схемы (классы) - C_BaseEntity
    std::uint32_t m_iHealth = cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth;
    std::uint32_t m_iTeamNum = cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum;
    std::uint32_t m_vecAbsOrigin = cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin;
    std::uint32_t m_pGameSceneNode = cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode;
    std::uint32_t m_vecVelocity = cs2_dumper::schemas::client_dll::C_BaseEntity::m_vecVelocity;
    std::uint32_t m_fFlags = cs2_dumper::schemas::client_dll::C_BaseEntity::m_fFlags;
    std::uint32_t m_pCollision = cs2_dumper::schemas::client_dll::C_BaseEntity::m_pCollision;
    
    // C_BasePlayerPawn
    std::uint32_t m_pObserverServices = cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pObserverServices;
    std::uint32_t m_pWeaponServices = cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices;
    std::uint32_t m_pCameraServices = cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pCameraServices;
    std::uint32_t m_vOldOrigin = cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin;
    
    // C_CSPlayerPawn
    std::uint32_t m_iShotsFired = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired;
    std::uint32_t m_pAimPunchServices = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices;
    std::uint32_t m_bIsScoped = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_bIsScoped;
    std::uint32_t m_entitySpottedState = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_entitySpottedState;
    
    // CCSPlayer_AimPunchServices
    std::uint32_t m_predictableBaseAngle = cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngle;
    std::uint32_t m_predictableBaseAngleVel = cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngleVel;
    
    // C_CSPlayerPawnBase
    std::uint32_t m_flFlashDuration = cs2_dumper::schemas::client_dll::C_CSPlayerPawnBase::m_flFlashDuration;
    std::uint32_t m_flFlashBangTime = cs2_dumper::schemas::client_dll::C_CSPlayerPawnBase::m_flFlashBangTime;
    
    // C_BaseModelEntity
    std::uint32_t m_vecViewOffset = cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset;
    std::uint32_t m_Glow = cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_Glow;
    
    // Observer
    std::uint32_t m_hObserverTarget = cs2_dumper::schemas::client_dll::CPlayer_ObserverServices::m_hObserverTarget;
    
    // Controller
    std::uint32_t m_hObserverPawn = cs2_dumper::schemas::client_dll::CCSPlayerController::m_hObserverPawn;
    std::uint32_t m_hPlayerPawn = cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn;
    std::uint32_t m_iszPlayerName = cs2_dumper::schemas::client_dll::CBasePlayerController::m_iszPlayerName;
    
    // Weapon services
    std::uint32_t m_hActiveWeapon = cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon;
    
    // Econ
    std::uint32_t m_iItemDefinitionIndex = cs2_dumper::schemas::client_dll::C_EconItemView::m_iItemDefinitionIndex;
    
    // Collision
    std::uint32_t m_vecMins = cs2_dumper::schemas::client_dll::CCollisionProperty::m_vecMins;
    std::uint32_t m_vecMaxs = cs2_dumper::schemas::client_dll::CCollisionProperty::m_vecMaxs;
    
    // Camera services
    std::uint32_t m_iFOV = cs2_dumper::schemas::client_dll::CCSPlayerBase_CameraServices::m_iFOV;
    std::uint32_t m_iFOVStart = cs2_dumper::schemas::client_dll::CCSPlayerBase_CameraServices::m_iFOVStart;
    std::uint32_t m_flFOVTime = cs2_dumper::schemas::client_dll::CCSPlayerBase_CameraServices::m_flFOVTime;
    std::uint32_t m_flFOVRate = cs2_dumper::schemas::client_dll::CCSPlayerBase_CameraServices::m_flFOVRate;
    
    // Skeleton
    std::uint32_t m_modelState = cs2_dumper::schemas::client_dll::CSkeletonInstance::m_modelState;
    
    // C_PlantedC4 (bomb)
    std::uint32_t m_bBombTicking = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombTicking;
    std::uint32_t m_bBombDefused = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombDefused;
    std::uint32_t m_flC4Blow = cs2_dumper::schemas::client_dll::C_PlantedC4::m_flC4Blow;
    std::uint32_t m_bBeingDefused = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBeingDefused;
    
    // C_CSPlayerPawn (skin changer)
    std::uint32_t m_hHudModelArms = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_hHudModelArms;

    bool LoadFromJson(const std::string& offsetsJsonPath, const std::string& schemaJsonPath)
    {
        std::map<std::string, uint32_t> offsets;
        if (SimpleJsonParser::LoadOffsets(offsetsJsonPath, offsets)) {
            if (offsets.count("dwEntityList")) dwEntityList = offsets["dwEntityList"];
            if (offsets.count("dwViewMatrix")) dwViewMatrix = offsets["dwViewMatrix"];
            if (offsets.count("dwViewAngles")) dwViewAngles = offsets["dwViewAngles"];
            if (offsets.count("dwLocalPlayerController")) dwLocalPlayerController = offsets["dwLocalPlayerController"];
            if (offsets.count("dwLocalPlayerPawn")) dwLocalPlayerPawn = offsets["dwLocalPlayerPawn"];
            if (offsets.count("dwGlobalVars")) dwGlobalVars = offsets["dwGlobalVars"];
            if (offsets.count("dwPlantedC4")) dwPlantedC4 = offsets["dwPlantedC4"];
        }

        // C_BaseEntity
        std::map<std::string, uint32_t> schema;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_BaseEntity", schema)) {
            if (schema.count("m_iHealth")) m_iHealth = schema["m_iHealth"];
            if (schema.count("m_iTeamNum")) m_iTeamNum = schema["m_iTeamNum"];
            if (schema.count("m_pGameSceneNode")) m_pGameSceneNode = schema["m_pGameSceneNode"];
            if (schema.count("m_vecVelocity")) m_vecVelocity = schema["m_vecVelocity"];
            if (schema.count("m_fFlags")) m_fFlags = schema["m_fFlags"];
            if (schema.count("m_pCollision")) m_pCollision = schema["m_pCollision"];
        }
        // CGameSceneNode
        std::map<std::string, uint32_t> schemaScene;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CGameSceneNode", schemaScene)) {
            if (schemaScene.count("m_vecAbsOrigin")) m_vecAbsOrigin = schemaScene["m_vecAbsOrigin"];
        }
        // C_BasePlayerPawn
        std::map<std::string, uint32_t> schemaPawn;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_BasePlayerPawn", schemaPawn)) {
            if (schemaPawn.count("m_pObserverServices")) m_pObserverServices = schemaPawn["m_pObserverServices"];
            if (schemaPawn.count("m_pWeaponServices")) m_pWeaponServices = schemaPawn["m_pWeaponServices"];
            if (schemaPawn.count("m_pCameraServices")) m_pCameraServices = schemaPawn["m_pCameraServices"];
            if (schemaPawn.count("m_vOldOrigin")) m_vOldOrigin = schemaPawn["m_vOldOrigin"];
        }
        // C_CSPlayerPawn
        std::map<std::string, uint32_t> schemaCSPawn;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_CSPlayerPawn", schemaCSPawn)) {
            if (schemaCSPawn.count("m_iShotsFired")) m_iShotsFired = schemaCSPawn["m_iShotsFired"];
            if (schemaCSPawn.count("m_pAimPunchServices")) m_pAimPunchServices = schemaCSPawn["m_pAimPunchServices"];
            if (schemaCSPawn.count("m_bIsScoped")) m_bIsScoped = schemaCSPawn["m_bIsScoped"];
            if (schemaCSPawn.count("m_entitySpottedState")) m_entitySpottedState = schemaCSPawn["m_entitySpottedState"];
        }
        // CCSPlayer_AimPunchServices
        std::map<std::string, uint32_t> schemaAimPunch;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCSPlayer_AimPunchServices", schemaAimPunch)) {
            if (schemaAimPunch.count("m_predictableBaseAngle")) m_predictableBaseAngle = schemaAimPunch["m_predictableBaseAngle"];
            if (schemaAimPunch.count("m_predictableBaseAngleVel")) m_predictableBaseAngleVel = schemaAimPunch["m_predictableBaseAngleVel"];
        }
        // C_CSPlayerPawnBase
        std::map<std::string, uint32_t> schemaCSBase;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_CSPlayerPawnBase", schemaCSBase)) {
            if (schemaCSBase.count("m_flFlashDuration")) m_flFlashDuration = schemaCSBase["m_flFlashDuration"];
            if (schemaCSBase.count("m_flFlashBangTime")) m_flFlashBangTime = schemaCSBase["m_flFlashBangTime"];
        }
        // C_BaseModelEntity
        std::map<std::string, uint32_t> schemaModel;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_BaseModelEntity", schemaModel)) {
            if (schemaModel.count("m_vecViewOffset")) m_vecViewOffset = schemaModel["m_vecViewOffset"];
            if (schemaModel.count("m_Glow")) m_Glow = schemaModel["m_Glow"];
        }
        // CPlayer_ObserverServices
        std::map<std::string, uint32_t> schemaObs;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CPlayer_ObserverServices", schemaObs)) {
            if (schemaObs.count("m_hObserverTarget")) m_hObserverTarget = schemaObs["m_hObserverTarget"];
        }
        // CBasePlayerController
        std::map<std::string, uint32_t> schemaCtrl;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CBasePlayerController", schemaCtrl)) {
            if (schemaCtrl.count("m_iszPlayerName")) m_iszPlayerName = schemaCtrl["m_iszPlayerName"];
        }
        // CCSPlayerController
        std::map<std::string, uint32_t> schemaCCtrl;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCSPlayerController", schemaCCtrl)) {
            if (schemaCCtrl.count("m_hPlayerPawn")) m_hPlayerPawn = schemaCCtrl["m_hPlayerPawn"];
            if (schemaCCtrl.count("m_hObserverPawn")) m_hObserverPawn = schemaCCtrl["m_hObserverPawn"];
        }
        // CPlayer_WeaponServices
        std::map<std::string, uint32_t> schemaWpn;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CPlayer_WeaponServices", schemaWpn)) {
            if (schemaWpn.count("m_hActiveWeapon")) m_hActiveWeapon = schemaWpn["m_hActiveWeapon"];
        }
        // C_EconItemView
        std::map<std::string, uint32_t> schemaEcon;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_EconItemView", schemaEcon)) {
            if (schemaEcon.count("m_iItemDefinitionIndex")) m_iItemDefinitionIndex = schemaEcon["m_iItemDefinitionIndex"];
        }
        // CCollisionProperty
        std::map<std::string, uint32_t> schemaCol;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCollisionProperty", schemaCol)) {
            if (schemaCol.count("m_vecMins")) m_vecMins = schemaCol["m_vecMins"];
            if (schemaCol.count("m_vecMaxs")) m_vecMaxs = schemaCol["m_vecMaxs"];
        }
        // CCSPlayerBase_CameraServices
        std::map<std::string, uint32_t> schemaCam;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCSPlayerBase_CameraServices", schemaCam)) {
            if (schemaCam.count("m_iFOV")) m_iFOV = schemaCam["m_iFOV"];
            if (schemaCam.count("m_iFOVStart")) m_iFOVStart = schemaCam["m_iFOVStart"];
            if (schemaCam.count("m_flFOVTime")) m_flFOVTime = schemaCam["m_flFOVTime"];
            if (schemaCam.count("m_flFOVRate")) m_flFOVRate = schemaCam["m_flFOVRate"];
        }
        // CSkeletonInstance
        std::map<std::string, uint32_t> schemaSkel;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CSkeletonInstance", schemaSkel)) {
            if (schemaSkel.count("m_modelState")) m_modelState = schemaSkel["m_modelState"];
        }
        // C_PlantedC4
        std::map<std::string, uint32_t> schemaC4;
        if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_PlantedC4", schemaC4)) {
            if (schemaC4.count("m_bBombTicking")) m_bBombTicking = schemaC4["m_bBombTicking"];
            if (schemaC4.count("m_bBombDefused")) m_bBombDefused = schemaC4["m_bBombDefused"];
            if (schemaC4.count("m_flC4Blow")) m_flC4Blow = schemaC4["m_flC4Blow"];
            if (schemaC4.count("m_bBeingDefused")) m_bBeingDefused = schemaC4["m_bBeingDefused"];
        }
        return true;
    }
};

static RuntimeOffsets g_offsetsRuntime{};

static HMODULE g_hModule = nullptr;

static bool FileExistsA(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

static bool GetDllDirA(std::string& outDir)
{
    char dllPath[MAX_PATH];
    DWORD len = GetModuleFileNameA(g_hModule ? g_hModule : GetModuleHandleA(nullptr), dllPath, MAX_PATH);
    if (!len || len >= MAX_PATH)
        return false;
    int lastSlash = -1;
    for (int i = static_cast<int>(len) - 1; i >= 0; --i)
    {
        if (dllPath[i] == '\\' || dllPath[i] == '/')
        {
            lastSlash = i;
            break;
        }
    }
    if (lastSlash < 0)
        return false;
    dllPath[lastSlash] = '\0';
    outDir.assign(dllPath);
    return true;
}

static std::string MakePathA(const std::string& dir, const char* file)
{
    if (dir.empty()) return std::string(file);
    std::string path = dir;
    if (path.back() != '\\' && path.back() != '/')
        path.push_back('\\');
    path.append(file);
    return path;
}

static void InitRuntimeOffsets()
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

// ImGui core + backends (пути настроены через AdditionalIncludeDirectories в .vcxproj)
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// Имплементации ImGui (подключаем .cpp напрямую в этот TU)
#pragma warning(push)
#pragma warning(disable: 6011)
#pragma warning(disable: 6387)
#include "imgui-1.92.5/imgui.cpp"
#include "imgui-1.92.5/imgui_draw.cpp"
#include "imgui-1.92.5/imgui_tables.cpp"
#include "imgui-1.92.5/imgui_widgets.cpp"
#include "imgui-1.92.5/backends/imgui_impl_dx11.cpp"
#include "imgui-1.92.5/backends/imgui_impl_win32.cpp"
#pragma warning(pop)


static volatile bool g_running = true;
static HANDLE g_hMainThread = nullptr;
static DWORD  g_mainThreadId = 0;

template <typename T>
static bool TryRead(uintptr_t address, T& out)
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
static bool TryWrite(uintptr_t address, const T& value)
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

// Хендл нашего overlay-окна
static HWND g_hOverlayWnd = nullptr;

// Имя класса окна (глобально для UnregisterClass)
static const wchar_t* g_wndClassName = L"CS2_DX11_Overlay";

// D3D11 + ImGui globals
static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain1*        g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static IDCompositionDevice*    g_dcompDevice = nullptr;
static IDCompositionTarget*    g_dcompTarget = nullptr;
static IDCompositionVisual*    g_dcompVisual = nullptr;

static bool  CreateDeviceD3D(HWND hWnd);
static void  CleanupDeviceD3D();
static void  CreateRenderTarget();
static void  CleanupRenderTarget();
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declarations
static uintptr_t GetClientBase();
static void RunCombat(bool cs2Active);
static Vec3 GetBonePos(uintptr_t pawn, int boneIndex);
static void ClampAngles(QAngle& angles);
static bool LoadTextureFromFileW(const wchar_t* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
static bool LoadTextureFromResource(int resourceId, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);

static void SetOverlayInputMode(HWND hWnd, bool interactive)
{
	static bool wasInteractive = false;
	if (interactive == wasInteractive) return;
	wasInteractive = interactive;

	LONG exStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
	if (interactive)
	{
		// Меню открыто: окно принимает ввод
		exStyle &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
		SetWindowLongW(hWnd, GWL_EXSTYLE, exStyle);
		SetForegroundWindow(hWnd);
		SetFocus(hWnd);
	}
	else
	{
		// Меню закрыто: окно пропускает клики
		exStyle |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
		SetWindowLongW(hWnd, GWL_EXSTYLE, exStyle);
		
		// Принудительно возвращаем фокус игре
		HWND hGame = FindWindowW(L"SDL_app", L"Counter-Strike 2"); // Класс окна CS2
		if (hGame)
		{
			SetForegroundWindow(hGame);
			SetActiveWindow(hGame);
		}
	}
}

static void UpdateAntiFlash(bool cs2Active);
static void UpdateFovOverride(bool cs2Active);
static void UpdateNoRecoilNoSpread(bool cs2Active);
static void DrawBonesImGui(const float view[16], int width, int height);

// ESP data
struct Box2D { float left, top, right, bottom; };

struct EspBox
{
	Box2D box;
	int hp;
	uintptr_t pawn;
	char name[64];
	char weapon[64];
	int weaponIconIndex;
	bool flashed;
	float flashDur; // Длительность флешки
	float flashEndTime; // Время окончания флешки (game time)
	float distance; // ДОБАВЛЕНО
};

static std::vector<EspBox> g_espBoxes;

// Spectator List
static std::vector<std::string> g_spectators;
static bool g_spectatorListEnabled = true;

struct EspTargetWorld
{
	uintptr_t pawn;
	int hp;
	Vec3 origin;
	Vec3 mins;
	Vec3 maxs;
	// Cached display data
	char name[64];
	char weapon[64];
	int weaponIconIndex;
	bool flashed;
	float flashDur; // Длительность флешки
	float flashEndTime; // Время окончания флешки (game time)
	float distance; // ДОБАВЛЕНО
};

static std::vector<EspTargetWorld> g_espTargetsWorld;

// Локальная оценка "game time" для плавных баров, когда нет доступа к GlobalVars.
// Инициализируется при первом валидном flashEndTime и далее идет монотонно.
static double g_gameTimeBaseTickMs = 0.0;
static double g_gameTimeBaseSeconds = 0.0;

static float GetEstimatedGameTimeSeconds()
{
	if (g_gameTimeBaseTickMs <= 0.0)
		return 0.0f;

	double nowMs = (double)GetTickCount64();
	double dt = (nowMs - g_gameTimeBaseTickMs) / 1000.0;
	return (float)(g_gameTimeBaseSeconds + dt);
}

static ImVec4 g_flashTextColorOn = ImVec4(1.0f, 0.9f, 0.1f, 1.0f);
static ImVec4 g_flashTextColorOff = ImVec4(0.5f, 0.9f, 0.5f, 1.0f);
static float g_flashTextScale = 1.0f;

// GUI Theme Color
static ImVec4 g_guiColor = ImVec4(0.35f, 0.45f, 0.85f, 1.00f); // Midnight Blue по умолчанию

struct CachedBoneWorld
{
	Vec3 world;
	bool valid;
};

struct BoneCacheEntryWorld
{
	uintptr_t pawn;
	CachedBoneWorld b[28];
};

static std::vector<BoneCacheEntryWorld> g_boneCache;

// Bomb ESP data
struct BombData {
	Vec3 pos;
	float blowTime;
	bool isDefusing;
	bool found;
};
static BombData g_bombData = { {0,0,0}, 0.0f, false, false };
static bool g_bombEspEnabled = true; // Включение/выключение Bomb ESP

// ==================== HOOKING UTILS ====================
// 
// АЛЬТЕРНАТИВНЫЙ МЕТОД: Вместо VTable хука используем проверку в цикле
// 
// Преимущества:
// ✅ Не крашит игру
// ✅ Не требует поиска индекса VTable
// ✅ Работает на всех версиях CS2
// ✅ Легко обновлять при патчах
// 
// Недостатки:
// ⚠ Скины применяются с небольшой задержкой (1-2 секунды)
// ⚠ Нужно переключить оружие для обновления
// 
// Метод работы:
// 1. Каждый кадр проверяем активное оружие
// 2. Если оружие изменилось - применяем скин
// 3. Используем форсированное обновление модели
// 
typedef void* (*tCreateInterface)(const char* name, int* returnCode);

static void* GetInterface(const char* dllName, const char* interfaceName) {
	HMODULE hMod = GetModuleHandleA(dllName);
	if (!hMod) return nullptr;
	tCreateInterface createInterface = (tCreateInterface)GetProcAddress(hMod, "CreateInterface");
	if (!createInterface) return nullptr;
	return createInterface(interfaceName, nullptr);
}

// Глобальные переменные для хука VTable
typedef void(__fastcall* FrameStageNotify_t)(void* rcx, int curStage);
static FrameStageNotify_t oFrameStageNotify = nullptr;
static void** g_pSource2ClientVTable = nullptr;

// ВАЖНО: Индекс FrameStageNotify может меняться при обновлениях CS2!
// Текущий индекс: 36 (март 2026) - ОБНОВЛЕНО
// Предыдущий индекс: 35 (февраль 2026)
// Предыдущий индекс: 31 (2025 и ранее)
constexpr int FRAMESTAGENOTIFY_INDEX = 36;
// =======================================================


static bool WorldToScreen(const float m[16], const Vec3& pos, int width, int height, Vec2& out);
static void UpdateESPInternal(int width, int height);
static bool GetEntityBox2DFromWorldAABB(const Vec3& origin, const Vec3& mins, const Vec3& maxs, const float view[16], int width, int height, Box2D& out);
static void ProjectESPBoxes(const float view[16], int width, int height);
static void DrawEspImGui();
static void UpdateBonesCache(int width, int height);
static void DrawMenuImGui();
static bool g_menuOpen = false;
static bool g_skinMenuOpen = false;
static bool g_skinWarningShown = false;

static void ApplyClientStyle();
static bool KeybindWidget(const char* id, int& vk);
static const char* VkToStringA(int vk);

// Stream Proof / Anti-Capture
static bool g_antiCaptureEnabled = false; // Скрывать меню от OBS/Discord

// External features
static bool g_snaplinesEnabled = false;
static bool g_distanceEnabled = true;
static bool g_radarHackEnabled = false;
static float g_radarScale = 4.0f;   // 1 unit = 1/g_radarScale пикселей
static float g_radarSize  = 160.0f;
static bool g_autoStrafeEnabled = false;

// Bhop settings
static bool g_bhopEnabled = true;
static int  g_bhopKey = VK_SPACE;

// ESP (боксы)
static bool g_whEnabled = true;
static int  g_whKey = VK_F7;
static ImVec4 g_boxColor = ImVec4(1.0f, 0.90f, 0.43f, 1.0f);
static float  g_boxPadding = 1.5f;
static float  g_boxThickness = 2.0f;
static bool g_dynamicBoxColor = false; // Dynamic HP-based box color

// Chams
static bool g_chamsEnabled = false;
static ImVec4 g_chamsColorT = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Красный для T
static ImVec4 g_chamsColorCT = ImVec4(0.0f, 0.65f, 1.0f, 1.0f); // Синий для CT

// No Smoke
static bool g_noSmokeEnabled = false;

// ESP extra
static bool   g_nameEspEnabled = true;
static bool   g_gunEspEnabled = true;
static ImVec4 g_nameColor = ImVec4(0.90f, 0.90f, 0.95f, 1.0f);
static ImVec4 g_gunColor = ImVec4(0.75f, 0.80f, 1.0f, 1.0f);
static ImVec4 g_weaponIconColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
static float  g_hpBarWidth = 4.0f;
static float  g_hpBarOffset = 3.0f;
static float  g_nameOffsetY = 14.0f;
static float  g_gunOffsetY = 2.0f;

// HP Bar
static bool  g_hpBarEnabled = true;
static ImVec4 g_hpBarColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

// OOF Arrows
static bool g_oofArrowsEnabled = false;
static float g_oofArrowsRadius = 150.0f; // Радиус от центра
static float g_oofArrowsSize = 15.0f;    // Размер стрелки
static ImVec4 g_oofArrowsColor = ImVec4(1.0f, 0.2f, 0.2f, 0.8f); // Красный

// Custom Crosshair
static bool  g_customCrosshair = true;

// Bones
static bool g_bonesEnabled = false;
static bool g_boneDebugIds = false;
static bool g_boneHideLines = false;
static int  g_bonesKey = 0;
static ImVec4 g_boneColor = ImVec4(0.60f, 0.88f, 1.00f, 1.0f);

// Combat (triggerbot + RCS)
static bool g_triggerEnabled = true;
static int  g_triggerKey = 'X';
static bool g_triggerTeamCheck = true;
static bool g_triggerScopeOnly = false;
static bool g_triggerHeadOnly = false;
static bool g_triggerVisCheck = true; // Проверка видимости (m_bSpotted)
static float g_triggerAccuracyThreshold = 0.015f;

// Optional toggle key: when bound, pressing it will enable/disable triggerbot.
// If left empty, triggerbot works whenever it is enabled in the GUI.
static int  g_triggerToggleKey = 0;

// Aimbot settings
static bool g_aimbotEnabled = false;
static int  g_aimbotKey = VK_XBUTTON2; // Mouse side button
static bool g_aimbotTeamCheck = true;
static int  g_aimbotBone = 6; // 6 = head
static float g_aimbotFov = 5.0f;
static float g_aimbotSmooth = 3.0f;
static bool g_aimbotDrawFov = true; // Рисовать круг FOV
static ImVec4 g_aimbotFovColor = ImVec4(1.0f, 1.0f, 0.0f, 0.4f); // Цвет круга FOV
static bool g_aimbotVisCheck = true; // Проверка на стены
static bool g_aimbotAutoFire = false; // Автовыстрел

// Player
static bool g_antiFlashEnabled = false;

// FOV
static bool  g_fovEnabled = false;
static float g_fovValue = 90.0f;

// No recoil / no spread
static bool g_noRecoilEnabled = false;
static bool g_noSpreadEnabled = false;

// HitSound & Hitmarker
static bool g_hitSoundEnabled = false;
static float g_hitmarkerAlpha = 0.0f;
static ULONGLONG g_hitmarkerTime = 0;
static std::map<uintptr_t, int> g_lastEnemyHp; // pawn -> last known hp

// Floating Damage Indicators with animation
struct DamageText {
	uintptr_t targetPawn;  // Who we hit (for stacking)
	Vec3 worldPos;        // World position of damage
	int damage;
	ULONGLONG spawnTime;
	float alpha;
	float currentYOffset; // For smooth animation
	float targetYOffset;  // Target position in stack
};
static bool g_damageIndicatorsEnabled = false;
static std::vector<DamageText> g_damageTexts;
static ImVec4 g_damageColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Красный
static float g_damageTextSize = 20.0f; // Размер текста урона
static float g_damageTextLifetime = 4.0f; // Время жизни в секундах

// Кэш звука для моментального воспроизведения
static LPVOID g_hitSoundBuffer = nullptr;
static ULONGLONG g_lastLocalShotTime = 0;

// Forward declaration - defined later in file
static bool g_autoFireActive = false;

// Keybinds List
static bool g_keybindsListEnabled = false;

// Gun ESP text offset (X/Y)
static float g_gunOffsetX = 0.0f;

// Сохранение конфига
static void SaveConfig()
{
    std::string dllDir;
    if (!GetDllDirA(dllDir)) return;
    std::string configPath = MakePathA(dllDir, "config.ini");
    char buf[64];
    
    // Binds
    wsprintfA(buf, "%d", g_aimbotKey); WritePrivateProfileStringA("Binds", "AimbotKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_triggerKey); WritePrivateProfileStringA("Binds", "TriggerKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_whKey); WritePrivateProfileStringA("Binds", "EspKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_bonesKey); WritePrivateProfileStringA("Binds", "BonesKey", buf, configPath.c_str());
    wsprintfA(buf, "%d", g_bhopKey); WritePrivateProfileStringA("Binds", "BhopKey", buf, configPath.c_str());
    
    // ESP Floats
    sprintf_s(buf, sizeof(buf), "%.1f", g_boxPadding); WritePrivateProfileStringA("ESP", "BoxPadding", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_boxThickness); WritePrivateProfileStringA("ESP", "BoxThickness", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_hpBarWidth); WritePrivateProfileStringA("ESP", "HPBarWidth", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_hpBarOffset); WritePrivateProfileStringA("ESP", "HPBarOffset", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_nameOffsetY); WritePrivateProfileStringA("ESP", "NameOffsetY", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_gunOffsetY); WritePrivateProfileStringA("ESP", "GunOffsetY", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_gunOffsetX); WritePrivateProfileStringA("ESP", "GunOffsetX", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_damageTextSize); WritePrivateProfileStringA("ESP", "DamageTextSize", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_damageTextLifetime); WritePrivateProfileStringA("ESP", "DamageTextLifetime", buf, configPath.c_str());
    
    // Aimbot Floats & Settings
    sprintf_s(buf, sizeof(buf), "%.1f", g_aimbotFov); WritePrivateProfileStringA("Combat", "AimbotFov", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_aimbotSmooth); WritePrivateProfileStringA("Combat", "AimbotSmooth", buf, configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotTeamCheck", g_aimbotTeamCheck ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotVisCheck", g_aimbotVisCheck ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotAutoFire", g_aimbotAutoFire ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotDrawFov", g_aimbotDrawFov ? "1" : "0", configPath.c_str());

    // Booleans Visuals
    WritePrivateProfileStringA("Visuals", "NameEsp", g_nameEspEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "GunEsp", g_gunEspEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "HpBar", g_hpBarEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "DynamicBoxColor", g_dynamicBoxColor ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "HitSound", g_hitSoundEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "DamageIndicators", g_damageIndicatorsEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "WhEnabled", g_whEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "BonesEnabled", g_bonesEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "ChamsEnabled", g_chamsEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "SpectatorList", g_spectatorListEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "BombEsp", g_bombEspEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "CustomCrosshair", g_customCrosshair ? "1" : "0", configPath.c_str());
    
    // Дополнительные функции Visuals
    WritePrivateProfileStringA("Visuals", "Snaplines", g_snaplinesEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "Distance", g_distanceEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "RadarHack", g_radarHackEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "OofArrows", g_oofArrowsEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Visuals", "KeybindsList", g_keybindsListEnabled ? "1" : "0", configPath.c_str());
    
    // Misc
    WritePrivateProfileStringA("Misc", "SkinWarningShown", g_skinWarningShown ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "AutoStrafe", g_autoStrafeEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "AntiCapture", g_antiCaptureEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "AimbotEnabled", g_aimbotEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Combat", "TriggerEnabled", g_triggerEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "BhopEnabled", g_bhopEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "AntiFlash", g_antiFlashEnabled ? "1" : "0", configPath.c_str());
    WritePrivateProfileStringA("Misc", "NoSmoke", g_noSmokeEnabled ? "1" : "0", configPath.c_str());
    // NoRecoil/NoSpread сохранение отключено - крашит при инжекте в главном меню
    // WritePrivateProfileStringA("Misc", "NoRecoil", g_noRecoilEnabled ? "1" : "0", configPath.c_str());
    // WritePrivateProfileStringA("Misc", "NoSpread", g_noSpreadEnabled ? "1" : "0", configPath.c_str());
    
    // Сохранение значений слайдеров (ползунков)
    sprintf_s(buf, sizeof(buf), "%.0f", g_oofArrowsRadius); WritePrivateProfileStringA("Visuals", "OofRadius", buf, configPath.c_str());
    sprintf_s(buf, sizeof(buf), "%.1f", g_oofArrowsSize); WritePrivateProfileStringA("Visuals", "OofSize", buf, configPath.c_str());
    
    // Сохранение цвета меню (R, G, B, A)
    sprintf_s(buf, sizeof(buf), "%.2f,%.2f,%.2f,%.2f", g_guiColor.x, g_guiColor.y, g_guiColor.z, g_guiColor.w);
    WritePrivateProfileStringA("Menu", "Color", buf, configPath.c_str());
}

// Загрузка конфига
static void LoadConfig()
{
    std::string dllDir;
    if (!GetDllDirA(dllDir)) return;
    std::string configPath = MakePathA(dllDir, "config.ini");
    char buf[64];
    
    // Binds
    g_aimbotKey = GetPrivateProfileIntA("Binds", "AimbotKey", g_aimbotKey, configPath.c_str());
    g_triggerKey = GetPrivateProfileIntA("Binds", "TriggerKey", g_triggerKey, configPath.c_str());
    g_whKey = GetPrivateProfileIntA("Binds", "EspKey", g_whKey, configPath.c_str());
    g_bonesKey = GetPrivateProfileIntA("Binds", "BonesKey", g_bonesKey, configPath.c_str());
    g_bhopKey = GetPrivateProfileIntA("Binds", "BhopKey", g_bhopKey, configPath.c_str());
    
    // ESP Floats
    GetPrivateProfileStringA("ESP", "BoxPadding", "1.5", buf, sizeof(buf), configPath.c_str()); g_boxPadding = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "BoxThickness", "2.0", buf, sizeof(buf), configPath.c_str()); g_boxThickness = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "HPBarWidth", "4.0", buf, sizeof(buf), configPath.c_str()); g_hpBarWidth = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "HPBarOffset", "3.0", buf, sizeof(buf), configPath.c_str()); g_hpBarOffset = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "NameOffsetY", "14.0", buf, sizeof(buf), configPath.c_str()); g_nameOffsetY = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "GunOffsetY", "2.0", buf, sizeof(buf), configPath.c_str()); g_gunOffsetY = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "GunOffsetX", "0.0", buf, sizeof(buf), configPath.c_str()); g_gunOffsetX = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "DamageTextSize", "20.0", buf, sizeof(buf), configPath.c_str()); g_damageTextSize = (float)atof(buf);
    GetPrivateProfileStringA("ESP", "DamageTextLifetime", "4.0", buf, sizeof(buf), configPath.c_str()); g_damageTextLifetime = (float)atof(buf);

    // Aimbot Floats & Settings
    GetPrivateProfileStringA("Combat", "AimbotFov", "5.0", buf, sizeof(buf), configPath.c_str()); g_aimbotFov = (float)atof(buf);
    GetPrivateProfileStringA("Combat", "AimbotSmooth", "3.0", buf, sizeof(buf), configPath.c_str()); g_aimbotSmooth = (float)atof(buf);
    g_aimbotTeamCheck = GetPrivateProfileIntA("Combat", "AimbotTeamCheck", g_aimbotTeamCheck ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotVisCheck = GetPrivateProfileIntA("Combat", "AimbotVisCheck", g_aimbotVisCheck ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotAutoFire = GetPrivateProfileIntA("Combat", "AimbotAutoFire", g_aimbotAutoFire ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotDrawFov = GetPrivateProfileIntA("Combat", "AimbotDrawFov", g_aimbotDrawFov ? 1 : 0, configPath.c_str()) != 0;

    // Booleans Visuals
    g_nameEspEnabled = GetPrivateProfileIntA("Visuals", "NameEsp", g_nameEspEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_gunEspEnabled = GetPrivateProfileIntA("Visuals", "GunEsp", g_gunEspEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_hpBarEnabled = GetPrivateProfileIntA("Visuals", "HpBar", g_hpBarEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_dynamicBoxColor = GetPrivateProfileIntA("Visuals", "DynamicBoxColor", g_dynamicBoxColor ? 1 : 0, configPath.c_str()) != 0;
    g_hitSoundEnabled = GetPrivateProfileIntA("Visuals", "HitSound", g_hitSoundEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_damageIndicatorsEnabled = GetPrivateProfileIntA("Visuals", "DamageIndicators", g_damageIndicatorsEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_whEnabled = GetPrivateProfileIntA("Visuals", "WhEnabled", g_whEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_bonesEnabled = GetPrivateProfileIntA("Visuals", "BonesEnabled", g_bonesEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_chamsEnabled = GetPrivateProfileIntA("Visuals", "ChamsEnabled", g_chamsEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_spectatorListEnabled = GetPrivateProfileIntA("Visuals", "SpectatorList", g_spectatorListEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_bombEspEnabled = GetPrivateProfileIntA("Visuals", "BombEsp", g_bombEspEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_customCrosshair = GetPrivateProfileIntA("Visuals", "CustomCrosshair", g_customCrosshair ? 1 : 0, configPath.c_str()) != 0;
    
    // Дополнительные функции Visuals
    g_snaplinesEnabled = GetPrivateProfileIntA("Visuals", "Snaplines", g_snaplinesEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_distanceEnabled = GetPrivateProfileIntA("Visuals", "Distance", g_distanceEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_radarHackEnabled = GetPrivateProfileIntA("Visuals", "RadarHack", g_radarHackEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_oofArrowsEnabled = GetPrivateProfileIntA("Visuals", "OofArrows", g_oofArrowsEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_keybindsListEnabled = GetPrivateProfileIntA("Visuals", "KeybindsList", g_keybindsListEnabled ? 1 : 0, configPath.c_str()) != 0;
    
    // Misc
    g_skinWarningShown = GetPrivateProfileIntA("Misc", "SkinWarningShown", g_skinWarningShown ? 1 : 0, configPath.c_str()) != 0;
    g_autoStrafeEnabled = GetPrivateProfileIntA("Misc", "AutoStrafe", g_autoStrafeEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_antiCaptureEnabled = GetPrivateProfileIntA("Misc", "AntiCapture", g_antiCaptureEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_aimbotEnabled = GetPrivateProfileIntA("Combat", "AimbotEnabled", g_aimbotEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_triggerEnabled = GetPrivateProfileIntA("Combat", "TriggerEnabled", g_triggerEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_bhopEnabled = GetPrivateProfileIntA("Misc", "BhopEnabled", g_bhopEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_antiFlashEnabled = GetPrivateProfileIntA("Misc", "AntiFlash", g_antiFlashEnabled ? 1 : 0, configPath.c_str()) != 0;
    g_noSmokeEnabled = GetPrivateProfileIntA("Misc", "NoSmoke", g_noSmokeEnabled ? 1 : 0, configPath.c_str()) != 0;
    // NoRecoil/NoSpread загрузка отключена - крашит при инжекте в главном меню
    // g_noRecoilEnabled = GetPrivateProfileIntA("Misc", "NoRecoil", g_noRecoilEnabled ? 1 : 0, configPath.c_str()) != 0;
    // g_noSpreadEnabled = GetPrivateProfileIntA("Misc", "NoSpread", g_noSpreadEnabled ? 1 : 0, configPath.c_str()) != 0;
    
    // Загрузка значений слайдеров
    GetPrivateProfileStringA("Visuals", "OofRadius", "150.0", buf, sizeof(buf), configPath.c_str()); g_oofArrowsRadius = (float)atof(buf);
    GetPrivateProfileStringA("Visuals", "OofSize", "15.0", buf, sizeof(buf), configPath.c_str()); g_oofArrowsSize = (float)atof(buf);
    
    // Загрузка цвета меню
    GetPrivateProfileStringA("Menu", "Color", "", buf, sizeof(buf), configPath.c_str());
    if (buf[0] != '\0') {
        float r, g, b, a;
        if (sscanf_s(buf, "%f,%f,%f,%f", &r, &g, &b, &a) == 4) {
            g_guiColor = ImVec4(r, g, b, a);
            // ApplyClientStyle(); // ИСПРАВЛЕНО: Убрано. Контекст ImGui тут еще не создан!
        }
    }
}

static void UpdateFovOverride(bool cs2Active)
{
	if (!cs2Active || !g_fovEnabled)
		return;

	uintptr_t client = GetClientBase();
	if (!client)
		return;

	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;

	__try
	{
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		if (!localPawn)
			return;

		// ИСПРАВЛЕНИЕ: Не меняем FOV в режиме наблюдателя (когда HP <= 0)
		int localHp = 0;
		if (!TryRead<int>(localPawn + C_BaseEntity::m_iHealth, localHp))
			return;
		if (localHp <= 0)
			return; // Мы мертвы или наблюдаем - не трогаем FOV

		// Не меняем FOV, если в прицеле (чтобы не ломать зум AWP/Scout)
		bool isScoped = false;
		(void)TryRead<bool>(localPawn + C_CSPlayerPawn::m_bIsScoped, isScoped);
		if (isScoped)
			return;

		uintptr_t camServices = 0;
		if (!TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pCameraServices, camServices))
			return;
		if (!camServices)
			return;

		uint32_t neededFov = static_cast<uint32_t>(g_fovValue);
		
		// ИСПРАВЛЕНИЕ МЕРЦАНИЯ: Выключаем скорость смены FOV и пишем каждый кадр
		(void)TryWrite<float>(camServices + CCSPlayerBase_CameraServices::m_flFOVTime, 0.0f);
		(void)TryWrite<float>(camServices + CCSPlayerBase_CameraServices::m_flFOVRate, 0.0f);
		
		// Пишем сразу в оба значения каждую итерацию
		(void)TryWrite<uint32_t>(camServices + CCSPlayerBase_CameraServices::m_iFOV, neededFov);
		(void)TryWrite<uint32_t>(camServices + CCSPlayerBase_CameraServices::m_iFOVStart, neededFov);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

static void UpdateNoRecoilNoSpread(bool cs2Active)
{
	if (!cs2Active)
		return;
	if (!g_noRecoilEnabled && !g_noSpreadEnabled)
		return;

	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;

	uintptr_t client = GetClientBase();
	if (!client)
		return;

	__try
	{
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		if (!localPawn)
			return;

		int hp = 0;
		if (!TryRead<int>(localPawn + C_BaseEntity::m_iHealth, hp))
			return;
		if (hp <= 0)
			return;

		if (g_noRecoilEnabled)
		{
			int shotsFired = 0;
			(void)TryRead<int>(localPawn + C_CSPlayerPawn::m_iShotsFired, shotsFired);
			
			uintptr_t aimPunchSvc = 0;
			(void)TryRead<uintptr_t>(localPawn + C_CSPlayerPawn::m_pAimPunchServices, aimPunchSvc);
			if (aimPunchSvc) {
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0x0, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0x4, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0x8, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngleVel + 0x0, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngleVel + 0x4, 0.0f);
				(void)TryWrite<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngleVel + 0x8, 0.0f);
			}
			
			uintptr_t camServices = 0;
			(void)TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pCameraServices, camServices);
			if (camServices)
			{
				(void)TryWrite<float>(camServices + CPlayer_CameraServices::m_vecCsViewPunchAngle + 0x0, 0.0f);
				(void)TryWrite<float>(camServices + CPlayer_CameraServices::m_vecCsViewPunchAngle + 0x4, 0.0f);
				(void)TryWrite<float>(camServices + CPlayer_CameraServices::m_vecCsViewPunchAngle + 0x8, 0.0f);
			}
		}

		if (g_noSpreadEnabled)
		{
			uintptr_t weaponServices = 0;
			if (!TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices))
				return;
			if (!weaponServices)
				return;
			uint32_t hActive = 0;
			if (!TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive))
				return;
			if (!hActive)
				return;
			uintptr_t entityList = 0;
			if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
				return;
			if (!entityList)
				return;

			int weIndex = hActive & 0x1FF;
			int weEntryIndex = (hActive & 0x7FFF) >> 9;
			uintptr_t weListEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * weEntryIndex + 0x10, weListEntry))
				return;
			if (!weListEntry)
				return;
			uintptr_t weaponEnt = 0;
			if (!TryRead<uintptr_t>(weListEntry + 0x70 * weIndex, weaponEnt))
				return;
			if (!weaponEnt)
				return;

			// Сбрасываем все известные модификаторы точности
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_fAccuracyPenalty, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_flRecoilIndex, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_flTurningInaccuracy, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_flTurningInaccuracyDelta, 0.0f);
			(void)TryWrite<float>(weaponEnt + C_CSWeaponBase::m_fAccuracySmoothedForZoom, 0.0f);

			// ИСПРАВЛЕНИЕ БАГА: Блок VData ЗАКОММЕНТИРОВАН!
			// Запись в VData ломает кэш движка и вызывает пропадание рук/оружия/звуков при смене раунда.
			// Для NoSpread достаточно обнуления m_fAccuracyPenalty и m_flRecoilIndex выше.
			/*
			uintptr_t vData = 0;
			if (TryRead<uintptr_t>(weaponEnt + 0x380, vData) && vData)
			{
				(void)TryWrite<float>(vData + 0x228, 0.0f); // m_flMaxInaccuracy
				(void)TryWrite<float>(vData + 0x22C, 0.0f); // m_flInaccuracyJumpInitial
			}
			*/

			// Velocity compensation for better spread control while moving
			float* vecVel = reinterpret_cast<float*>(localPawn + C_BaseEntity::m_vecVelocity);
			if (vecVel) {
				// We don't zero velocity as that would break movement, 
				// but the game uses it to calculate spread. 
				// The offsets above should cover it, but some weapons have specific logic.
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

// Skin Changer - включен/выключен
static bool g_skinChangerEnabled = true; // Включен по умолчанию

// Конфигурация скинов для оружия (defIndex -> paintKit)
static std::map<int, int> g_skinConfig;

// Переменные для Skin Changer
static std::map<uintptr_t, int> g_appliedSkins;           // Уже применённые скины
static uint32_t g_lastActiveWeapon = 0;                   // Последнее активное оружие
static ULONGLONG g_lastSkinUpdateTick = 0;                // Время последнего обновления
static LONG g_skinIdCounter = 0;                          // Счётчик уникальных ID

// Триггер для реал-тайм обновления скинов
int g_skinUpdateCounter = 0;
static int g_skinRevision = 1;
static int g_lastSkinUpdateCounterSeen = 0;

// Инициализация скинов (defIndex -> paintKit) - ВСЕ DEFAULT по умолчанию!
static void InitSkinConfig()
{
	// Все оружия инициализируются как Default (0)
	// Пользователь выбирает скины в меню, они сохраняются в config
	g_skinConfig[1] = 0;   // Desert Eagle - Default
	g_skinConfig[4] = 0;   // Glock-18 - Default
	g_skinConfig[7] = 0;   // AK-47 - Default
	g_skinConfig[9] = 0;   // AWP - Default
	g_skinConfig[10] = 0;  // FAMAS - Default
	g_skinConfig[13] = 0;  // Galil-AR - Default
	g_skinConfig[16] = 0;  // M4A4 - Default
	g_skinConfig[30] = 0;  // TEC-9 - Default
	g_skinConfig[40] = 0;  // SSG-08 - Default
	g_skinConfig[60] = 0;  // M4A1-S - Default
	g_skinConfig[61] = 0;  // USP-S - Default
}

// Функция для получения paint kit для оружия
static int GetPaintKitForWeapon(int defIndex)
{
	auto it = g_skinConfig.find(defIndex);
	if (it != g_skinConfig.end())
		return it->second;
	return 0; // Нет скина
}

// UpdateSkinChangerImproved - УДАЛЕНА (устаревшая, заменена на UpdateSkinChangerHooked)

// ==================== NUCLEAR SKIN CHANGER (ID CYCLING) ====================

// Структура для запоминания, какой ID мы выдали оружию
static std::map<uintptr_t, uint32_t> g_generatedIDs;
static std::map<uintptr_t, int> g_appliedKits;

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

static bool TryReadEconAttributesVec(uintptr_t itemView, bool dynamicList, uintptr_t& outPtr, uint64_t& outSize)
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

static bool TrySetEconAttributeFloatInList(uintptr_t itemView, bool dynamicList, uint16_t attributeDefIndex, float value, bool& outFoundDef)
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

static bool TrySetEconAttributeFloat(uintptr_t itemView, uint16_t attributeDefIndex, float value)
{
	using namespace cs2_dumper::schemas::client_dll;
	bool foundDefA = false;
	bool foundDefB = false;
	bool okA = TrySetEconAttributeFloatInList(itemView, false, attributeDefIndex, value, foundDefA);
	bool okB = TrySetEconAttributeFloatInList(itemView, true, attributeDefIndex, value, foundDefB);

	return okA || okB;
}

static void UpdateSkinChangerHooked() {
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

		int targetPaintKit = GetPaintKitForWeapon(defIndex);
		if (targetPaintKit <= 0) continue;

		// --- ЖЕСТКАЯ ЛОГИКА ОБНОВЛЕНИЯ ---
		// Если PaintKit изменился ИЛИ мы нажали "обновить" в меню ИЛИ ID еще не сгенерирован
		bool needUpdate = (g_appliedKits[weaponEnt] != targetPaintKit) || menuChanged || (g_generatedIDs[weaponEnt] == 0);

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
			
			// 4. Основные параметры
			(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackPaintKit, targetPaintKit);
			(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackSeed, 1);
			(void)TryWrite<float>(weaponEnt + C_EconEntity::m_flFallbackWear, 0.001f);
			
			// 5. StatTrak -1 (Выкл, чтобы не багало UV)
			(void)TryWrite<int32_t>(weaponEnt + C_EconEntity::m_nFallbackStatTrak, -1);
			
			// 6. Quality = 4
			(void)TryWrite<int32_t>(itemView + C_EconItemView::m_iEntityQuality, 4);
			
			// 6b. Update attributes (used by engine for skin material/UV)
			// Only patches existing attribute entries; does not allocate/resize.
			(void)TrySetEconAttributeFloat(itemView, 6, (float)targetPaintKit);
			(void)TrySetEconAttributeFloat(itemView, 7, 1.0f);
			(void)TrySetEconAttributeFloat(itemView, 8, 0.001f);
			
			// 7. Стираем кастомное имя (иногда там мусор, который ломает парсер)
			char emptyName[32] = {0};
			(void)TryWrite<char>(itemView + C_EconItemView::m_szCustomName, 0);
			
			// 8. Viewmodel Update
		}
	}
}

// ==================== НОВЫЕ ФУНКЦИИ (INTERNAL HOOKS) ====================

static void __fastcall hkFrameStageNotify(void* rcx, int curStage) {
	// ВОЗВРАЩАЕМ FRAME_RENDER_START (6)
	// Стадия 4 не работает на текущем патче CS2 для записи в Weapons (перезаписывается сервером).
	// Стадия 6 (5 или 6) - это момент отрисовки, тут наше изменение будет финальным.
	
	// Проверка ревизии для обновления при смене в меню
	if (g_skinUpdateCounter != g_lastSkinUpdateCounterSeen) {
		g_lastSkinUpdateCounterSeen = g_skinUpdateCounter;
		++g_skinRevision;
		if (g_skinRevision <= 0) g_skinRevision = 1;
	}
	
	if ((curStage == 6) && g_skinChangerEnabled) 
	{
		UpdateSkinChangerHooked();
	}
	oFrameStageNotify(rcx, curStage);
}

static void InitHooks() {
	// --- ПРЕДЗАГРУЗКА ЗВУКА В КЭШ ДЛЯ МОМЕНТАЛЬНОГО ВОСПРОИЗВЕДЕНИЯ ---
	HRSRC hRes = FindResourceW(g_hModule, MAKEINTRESOURCEW(IDR_WAV_HITSOUND), RT_RCDATA);
	if (hRes) {
		HGLOBAL hMem = LoadResource(g_hModule, hRes);
		if (hMem) {
			g_hitSoundBuffer = LockResource(hMem);
		}
	}

	MH_Initialize(); // Инициализируем MinHook

	// Ставим VTable хук на скинченджер
	void* pSource2Client = GetInterface("client.dll", "Source2Client002");
	if (pSource2Client) {
		g_pSource2ClientVTable = *(void***)pSource2Client;
		DWORD oldProtect;
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
		oFrameStageNotify = (FrameStageNotify_t)g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX];
		g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX] = (void*)hkFrameStageNotify;
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), oldProtect, &oldProtect);
	}
}

static void RemoveHooks() {
	if (g_pSource2ClientVTable && oFrameStageNotify)
	{
		// ИСПРАВЛЕНО: Индекс изменен с 31 на 35
		DWORD oldProtect;
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
		
		g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX] = (void*)oFrameStageNotify;
		
		VirtualProtect(&g_pSource2ClientVTable[FRAMESTAGENOTIFY_INDEX], sizeof(void*), oldProtect, &oldProtect);
	}
}

// UpdateSkinChanger - УДАЛЕНА (устаревшая, заменена на UpdateSkinChangerHooked)

static bool IsCs2Active()
{
	HWND hwnd = GetForegroundWindow();
	if (!hwnd) return false;

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	return pid == GetCurrentProcessId();
}

static uintptr_t GetClientBase()
{
	HMODULE hClient = GetModuleHandleW(L"client.dll");
	if (!hClient) return 0;
	return reinterpret_cast<uintptr_t>(hClient);
}

static bool WorldToScreen(const float m[16], const Vec3& pos, int width, int height, Vec2& out)
{
	float w = m[12] * pos.x + m[13] * pos.y + m[14] * pos.z + m[15];
	if (w < 0.001f) return false;

	float x = m[0] * pos.x + m[1] * pos.y + m[2] * pos.z + m[3];
	float y = m[4] * pos.x + m[5] * pos.y + m[6] * pos.z + m[7];

	float invW = 1.0f / w;
	x *= invW;
	y *= invW;

	float cx = width / 2.0f;
	float cy = height / 2.0f;

	out.x = cx + (cx * x);
	out.y = cy - (cy * y);
	return true;
}

// Получение имени оружия по defIndex
static const char* GetWeaponName(int defIndex)
{
	switch(defIndex) {
		case 1: return "DEAGLE";
		case 2: return "ELITE";
		case 3: return "FIVESEVEN";
		case 4: return "GLOCK";
		case 7: return "AK-47";
		case 8: return "AUG";
		case 9: return "AWP";
		case 10: return "FAMAS";
		case 11: return "G3SG1";
		case 13: return "GALIL";
		case 14: return "M249";
		case 16: return "M4A4";
		case 17: return "MAC10";
		case 19: return "P90";
		case 23: return "MP5";
		case 24: return "UMP45";
		case 25: return "XM1014";
		case 26: return "BIZON";
		case 27: return "MAG7";
		case 28: return "NEGEV";
		case 29: return "SAWEDOFF";
		case 30: return "TEC9";
		case 31: return "ZEUS";
		case 32: return "P2000";
		case 33: return "MP7";
		case 34: return "MP9";
		case 35: return "NOVA";
		case 36: return "P250";
		case 38: return "SCAR20";
		case 39: return "SG556";
		case 40: return "SCOUT";
		case 41: return "KNIFE";
		case 42: return "KNIFE";
		case 43: return "FLASH";
		case 44: return "HE";
		case 45: return "SMOKE";
		case 46: return "MOLOTOV";
		case 47: return "DECOY";
		case 48: return "INCENDIARY";
		case 49: return "C4";
		case 59: return "KNIFE";
		case 60: return "M4A1-S";
		case 61: return "USP-S";
		case 63: return "CZ75";
		case 64: return "REVOLVER";
		case 500: return "BAYONET";
		case 503: return "CLASSIC";
		case 505: return "FLIP";
		case 506: return "GUT";
		case 507: return "KARAMBIT";
		case 508: return "M9";
		case 509: return "HUNTSMAN";
		case 512: return "FALCHION";
		case 514: return "BOWIE";
		case 515: return "BUTTERFLY";
		case 516: return "DAGGERS";
		default: return "WEAPON";
	}
}

// ИСПРАВЛЕНИЕ C2712: Структура для временного хранения зрителей
struct SpectatorTemp {
	char name[64];
	bool valid;
};

// ИСПРАВЛЕНИЕ C2712: Helper-функция без C++ объектов для использования __try/__except
static void UpdateESPInternal_TryBlock(
	uintptr_t client,
	EspTargetWorld* tempTargets,
	int* targetCount,
	SpectatorTemp* tempSpectators,
	int* spectatorCount
)
{
	__try
	{
		using namespace cs2_dumper;
		using namespace cs2_dumper::schemas::client_dll;

		// ИСПРАВЛЕНИЕ: Строгая проверка валидности локального игрока
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		
		// 1. Проверка на nullptr (мы в меню)
		if (!localPawn) {
			return;
		}
		
		// 2. ПРОВЕРКА ОТ КРАША: Проверяем GameSceneNode у себя. Если его нет - мир не загружен.
		uintptr_t localScene = 0;
		if (!TryRead<uintptr_t>(localPawn + C_BaseEntity::m_pGameSceneNode, localScene))
			return;
		if (!localScene) {
			return;
		}
		
		// 3. ИСПРАВЛЕНИЕ: НЕ проверяем HP локального игрока!
		// При слежке за другим игроком (spectator mode) наш HP = 0, но ESP должен работать.
		// Проверка здоровья нужна только для защиты от мусорных значений при смене карты.
		int localHp = 0;
		if (!TryRead<int>(localPawn + C_BaseEntity::m_iHealth, localHp))
			return;
		// Разрешаем HP от -100 до 10000 (включая 0 для режима наблюдателя)
		if (localHp < -100 || localHp > 10000) {
			return;
		}
		
		// ОПТИМИЗАЦИЯ: Проверяем снайперку здесь, а не в рендере
		g_isLocalSniperScoped = false;
		uintptr_t weaponServices = 0;
		if (TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices) && weaponServices)
		{
			uint32_t hActive = 0;
			if (TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive) && hActive)
			{
				uintptr_t entityList = 0;
				if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) && entityList)
				{
					int weIndex = hActive & 0x1FF;
					int weEntryIndex = (hActive & 0x7FFF) >> 9;
					uintptr_t weList = 0;
					if (TryRead<uintptr_t>(entityList + 0x8 * weEntryIndex + 0x10, weList) && weList)
					{
						uintptr_t weaponEnt = 0;
						if (TryRead<uintptr_t>(weList + 0x70 * weIndex, weaponEnt) && weaponEnt)
						{
							uintptr_t itemView = weaponEnt + C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item;
							uint16_t defIndex = 0;
							if (TryRead<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, defIndex))
							{
								// 9 = AWP, 40 = SSG08 (Scout)
								g_isLocalSniperScoped = (defIndex == 9 || defIndex == 40);
							}
						}
					}
				}
			}
		}

		uintptr_t entityList = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
			return;
		if (!entityList) return;
		
		// ИСПРАВЛЕНИЕ: При слежке (HP <= 0) не проверяем команду, показываем всех
		bool isSpectating = (localHp <= 0);
		
		int localTeam = 0;
		bool hasLocalTeam = false;
		if (localPawn && !isSpectating)
		{
			if (!TryRead<int>(localPawn + C_BaseEntity::m_iTeamNum, localTeam))
				localTeam = 0;
			hasLocalTeam = (localTeam >= 2 && localTeam <= 3);
		}

		// Импульс отключения радара (одиночный сброс флага при выключении)
		static bool s_wasRadarHackEnabled = false;
		bool radarDisablePulse = false;
		if (s_wasRadarHackEnabled && !g_radarHackEnabled) {
			radarDisablePulse = true;
		}
		s_wasRadarHackEnabled = g_radarHackEnabled;

		// Находим наш локальный Pawn Handle для Spectator List
		uint32_t localPawnHandle = 0;
		uintptr_t localCtrl = 0;
		if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerController, localCtrl) && localCtrl) {
			TryRead<uint32_t>(localCtrl + g_offsetsRuntime.m_hPlayerPawn, localPawnHandle);
		}

		for (int id = 1; id <= 64; ++id)
		{
			uintptr_t listEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry) || !listEntry) continue;

			uintptr_t controller = 0;
			if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller) || !controller) continue;

			// --- ИДЕАЛЬНЫЙ SPECTATOR CHECK (работает даже когда мертвы) ---
			uint32_t pawnHandle = 0, obsPawnHandle = 0;
			TryRead<uint32_t>(controller + g_offsetsRuntime.m_hPlayerPawn, pawnHandle);
			TryRead<uint32_t>(controller + g_offsetsRuntime.m_hObserverPawn, obsPawnHandle);

			bool isAlive = false;
			uintptr_t activePawnToCheck = 0;

			// 1. Проверяем основную пешку игрока (жив ли он?)
			if (pawnHandle) {
				int pIdx = pawnHandle & 0x1FF;
				int pEntry = (pawnHandle & 0x7FFF) >> 9;
				uintptr_t pList = 0;
				if (TryRead<uintptr_t>(entityList + 0x8 * pEntry + 0x10, pList) && pList) {
					uintptr_t actualPawn = 0;
					if (TryRead<uintptr_t>(pList + 0x70 * pIdx, actualPawn) && actualPawn) {
						int actualHp = 0;
						TryRead<int>(actualPawn + g_offsetsRuntime.m_iHealth, actualHp);
						if (actualHp > 0) {
							isAlive = true; // Игрок возродился, пропускаем проверку зрителя
						} else {
							activePawnToCheck = actualPawn; // Игрок мертв, запоминаем его пешку
						}
					}
				}
			}

			// 2. Если игрок мертв, но у него есть активная пешка обсервера, отдаем ей приоритет
			if (!isAlive && obsPawnHandle) {
				int pIdx = obsPawnHandle & 0x1FF;
				int pEntry = (obsPawnHandle & 0x7FFF) >> 9;
				uintptr_t pList = 0;
				if (TryRead<uintptr_t>(entityList + 0x8 * pEntry + 0x10, pList) && pList) {
					uintptr_t obsPawn = 0;
					if (TryRead<uintptr_t>(pList + 0x70 * pIdx, obsPawn) && obsPawn) {
						activePawnToCheck = obsPawn;
					}
				}
			}

			// 3. Только если игрок мертв (isAlive == false), проверяем, за кем он следит
			if (!isAlive && activePawnToCheck && localPawnHandle && *spectatorCount < 64 && controller != localCtrl) {
				uintptr_t obsServices = 0;
				if (TryRead<uintptr_t>(activePawnToCheck + g_offsetsRuntime.m_pObserverServices, obsServices) && obsServices) {
					uint32_t obsTarget = 0;
					if (TryRead<uint32_t>(obsServices + g_offsetsRuntime.m_hObserverTarget, obsTarget) && obsTarget) {
						// Если цель обсервера - МЫ, добавляем в список
						if (obsTarget == localPawnHandle && localHp > 0) {
							StringBuf64 specName = {0};
							if (TryRead<StringBuf64>(controller + g_offsetsRuntime.m_iszPlayerName, specName)) {
								specName.data[63] = '\0';
								// Исключаем пустые имена
								if (specName.data[0] != '\0') {
									memcpy(tempSpectators[*spectatorCount].name, specName.data, 64);
									tempSpectators[*spectatorCount].valid = true;
									(*spectatorCount)++;
								}
							}
						}
					}
				}
			}
			// --- КОНЕЦ SPECTATOR CHECK ---

			// --- PAWN LOGIC ДЛЯ ESP И HITSOUND ---
			if (!pawnHandle) continue;
			int pawnIndex = pawnHandle & 0x1FF;
			int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;
			uintptr_t pawnListEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry) || !pawnListEntry) continue;
			uintptr_t pawn = 0;
			if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn) || !pawn) continue;
			if (localPawn && pawn == localPawn) continue;

			// СНАЧАЛА проверяем команду (чтобы не реагировать на урон по своим)
			int team = 0;
			if (!TryRead<int>(pawn + g_offsetsRuntime.m_iTeamNum, team)) continue;
			if (team < 2 || team > 3) continue;
			if (!isSpectating && hasLocalTeam && team == localTeam) continue;

			int hp = 0;
			if (!TryRead<int>(pawn + g_offsetsRuntime.m_iHealth, hp)) continue;
			if (hp <= 0 || hp > 200) continue; // Мертвецов не рисуем в ESP

			// --- GLOW (свечение сквозь стены) ---
				if (g_chamsEnabled)
				{
					ImVec4 c = (team == 2) ? g_chamsColorT : g_chamsColorCT;
					uintptr_t pGlow = pawn + C_BaseModelEntity::m_Glow;

					(void)TryWrite<bool>(pGlow + 0x51, true);   // m_bGlowing
					(void)TryWrite<int> (pGlow + 0x30, 3);      // Type 3 = только контур (outline)
					
					struct ColorRGBA { uint8_t r, g, b, a; };
					ColorRGBA glowColor = {
						(uint8_t)(c.x * 255.0f), (uint8_t)(c.y * 255.0f), (uint8_t)(c.z * 255.0f), (uint8_t)(c.w * 255.0f)
					};
					(void)TryWrite<ColorRGBA>(pGlow + 0x40, glowColor);

					// ИСПРАВЛЕНИЕ: true = рендерить сквозь стены!
					(void)TryWrite<bool>(pGlow + 0x52, true);   // m_bRenderWhenOccluded
					(void)TryWrite<bool>(pGlow + 0x53, true);   // m_bRenderWhenUnoccluded
				}
				else
				{
					// Сброс при выключении
					uintptr_t pGlow = pawn + C_BaseModelEntity::m_Glow;
					(void)TryWrite<bool>(pGlow + 0x51, false);
				}

				// Читаем origin, mins, maxs для ESP бокса
				Vec3 origin{}, mins{}, maxs{};
				uintptr_t gameScene = 0;
				if (TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gameScene) && gameScene) {
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, origin.x);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, origin.y);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, origin.z);
					
					// Collision bounds для 2D бокса
					uintptr_t collision = 0;
					if (TryRead<uintptr_t>(pawn + C_BaseEntity::m_pCollision, collision) && collision) {
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMins + 0x0, mins.x);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMins + 0x4, mins.y);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMins + 0x8, mins.z);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMaxs + 0x0, maxs.x);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMaxs + 0x4, maxs.y);
						(void)TryRead<float>(collision + CCollisionProperty::m_vecMaxs + 0x8, maxs.z);
					}
				}

				EspTargetWorld tgt{};
				tgt.pawn = pawn;
				tgt.hp = hp;
				tgt.origin = origin;
				tgt.mins = mins;
				tgt.maxs = maxs;
				tgt.weaponIconIndex = -1;
				tgt.flashed = false;
				{
					// ОПТИМИЗАЦИЯ: Читаем имя блоком за 1 вызов TryRead
					StringBuf64 nameRaw = {0};
					if (TryRead<StringBuf64>(controller + CBasePlayerController::m_iszPlayerName, nameRaw)) {
						memcpy(tgt.name, nameRaw.data, 64);
						tgt.name[63] = '\0'; // Гарантируем закрытие строки
					} else {
						tgt.name[0] = '\0';
					}
				}
				{
					uintptr_t weaponServices = 0;
					if (!TryRead<uintptr_t>(pawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices))
						weaponServices = 0;
					if (weaponServices)
					{
						uint32_t hActive = 0;
						if (!TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive))
							hActive = 0;
						if (hActive)
						{
							int weIndex = hActive & 0x1FF;
							int weEntryIndex = (hActive & 0x7FFF) >> 9;
							uintptr_t weListEntry = 0;
							if (!TryRead<uintptr_t>(entityList + 0x8 * weEntryIndex + 0x10, weListEntry))
								weListEntry = 0;
							if (weListEntry)
							{
								uintptr_t weaponEnt = 0;
								if (!TryRead<uintptr_t>(weListEntry + 0x70 * weIndex, weaponEnt))
									weaponEnt = 0;
								if (weaponEnt)
								{
									uintptr_t econEntity = weaponEnt;
									uintptr_t itemView = econEntity + C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item;
									uint16_t defIndex = 0;
									if (!TryRead<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, defIndex))
										defIndex = 0;
									if (defIndex != 0)
									{
										// ИСПРАВЛЕНИЕ: Используем текстовое имя вместо иконок
										const char* weaponName = GetWeaponName((int)defIndex);
										strncpy_s(tgt.weapon, sizeof(tgt.weapon), weaponName, _TRUNCATE);
									}
								}
							}
						}
					}
				}

				float flashDur = 0.0f;
				if (!TryRead<float>(pawn + C_CSPlayerPawnBase::m_flFlashDuration, flashDur))
					flashDur = 0.0f;
				float flashEndTime = 0.0f;
				(void)TryRead<float>(pawn + C_CSPlayerPawnBase::m_flFlashBangTime, flashEndTime);

				// Синхронизация локального времени с "game time" по первому валидному значению.
				if (flashEndTime > 0.0f && g_gameTimeBaseTickMs <= 0.0) {
					g_gameTimeBaseTickMs = (double)GetTickCount64();
					// flashBangTime в игре обычно является "absolute game time" конца ослепления.
					// Поэтому базу ставим чуть раньше на flashDur.
					g_gameTimeBaseSeconds = (double)flashEndTime - (double)flashDur;
				}

				tgt.flashEndTime = flashEndTime;
				tgt.flashed = (flashDur > 0.0f);
				tgt.flashDur = flashDur; // Добавляем длительность флешки
				
				// RADAR HACK (Native) - Раздельная независимая проверка (использует 0x8)
				if (g_radarHackEnabled) {
					(void)TryWrite<bool>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0x8, true);
				} else if (radarDisablePulse) {
					(void)TryWrite<bool>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0x8, false);
				}
				
				// ИСПРАВЛЕНИЕ C2712: Записываем в POD-буфер вместо vector::push_back
				if (*targetCount < 64) {
					tempTargets[*targetCount] = tgt;
					(*targetCount)++;
				}
			}
		
		// --- Поиск установленной C4 (работает даже если игрок мертв) ---
		g_bombData.found = false;
		if (g_offsetsRuntime.dwPlantedC4)
		{
			uintptr_t c4Ptr = 0;
			if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwPlantedC4, c4Ptr) && c4Ptr)
			{
				uintptr_t c4Node = 0;
				if (TryRead<uintptr_t>(c4Ptr, c4Node) && c4Node)
				{
					bool isTicking = false;
					bool isDefused = false;
					(void)TryRead<bool>(c4Node + g_offsetsRuntime.m_bBombTicking, isTicking);
					(void)TryRead<bool>(c4Node + g_offsetsRuntime.m_bBombDefused, isDefused);
					
					if (isTicking && !isDefused) {
						uintptr_t gameScene = 0;
						if (TryRead<uintptr_t>(c4Node + g_offsetsRuntime.m_pGameSceneNode, gameScene) && gameScene)
						{
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, g_bombData.pos.x);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, g_bombData.pos.y);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x8, g_bombData.pos.z);
							
							if (g_bombData.pos.x != 0.0f || g_bombData.pos.y != 0.0f) {
								TryRead<float>(c4Node + g_offsetsRuntime.m_flC4Blow, g_bombData.blowTime);
								TryRead<bool>(c4Node + g_offsetsRuntime.m_bBeingDefused, g_bombData.isDefusing);
								g_bombData.found = true;
							}
						}
					}
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// Вектор уже очищен в начале UpdateESPInternal
	}
}

// Основная функция UpdateESPInternal
static void UpdateESPInternal(int width, int height)
{
	// ИСПРАВЛЕНИЕ C2712: Операции с std::vector ПЕРЕД вызовом helper-функции
	g_espTargetsWorld.clear();
	g_espBoxes.clear();
	g_spectators.clear(); // Принудительно очищаем в начале каждого тика
	
	// Сброс данных бомбы каждый кадр
	g_bombData.found = false;

	uintptr_t client = GetClientBase();
	if (!client) return;

	// ИСПРАВЛЕНИЕ C2712: Временные POD-буферы
	SpectatorTemp tempSpectators[64] = {};
	int spectatorCount = 0;
	
	EspTargetWorld tempTargets[64] = {};
	int targetCount = 0;

	// Вызываем helper-функцию с __try/__except
	UpdateESPInternal_TryBlock(client, tempTargets, &targetCount, tempSpectators, &spectatorCount);

	// ИСПРАВЛЕНИЕ C2712: Переносим ESP цели из POD-буфера в вектор (вне __try блока)
	g_espTargetsWorld.reserve(targetCount);
	for (int i = 0; i < targetCount; ++i) {
		g_espTargetsWorld.push_back(tempTargets[i]);
	}

	// Переносим зрителей из временного буфера в вектор (вне __try блока)
	g_spectators.clear();
	for (int i = 0; i < spectatorCount; ++i) {
		if (tempSpectators[i].valid) {
			g_spectators.push_back(tempSpectators[i].name);
		}
	}
}

static bool GetEntityBox2DFromWorldAABB(const Vec3& origin, const Vec3& mins, const Vec3& maxs, const float view[16], int width, int height, Box2D& out)
{
	const Vec3 corners[8] = {
		{ origin.x + mins.x, origin.y + mins.y, origin.z + mins.z },
		{ origin.x + mins.x, origin.y + maxs.y, origin.z + mins.z },
		{ origin.x + maxs.x, origin.y + maxs.y, origin.z + mins.z },
		{ origin.x + maxs.x, origin.y + mins.y, origin.z + mins.z },
		{ origin.x + mins.x, origin.y + mins.y, origin.z + maxs.z },
		{ origin.x + mins.x, origin.y + maxs.y, origin.z + maxs.z },
		{ origin.x + maxs.x, origin.y + maxs.y, origin.z + maxs.z },
		{ origin.x + maxs.x, origin.y + mins.y, origin.z + maxs.z },
	};

	float minX = (std::numeric_limits<float>::max)();
	float minY = (std::numeric_limits<float>::max)();
	float maxX = (std::numeric_limits<float>::lowest)();
	float maxY = (std::numeric_limits<float>::lowest)();
	bool anyProjected = false;

	for (const Vec3& c : corners)
	{
		Vec2 p2{};
		if (!WorldToScreen(view, c, width, height, p2))
			continue;
		anyProjected = true;
		if (p2.x < minX) minX = p2.x;
		if (p2.y < minY) minY = p2.y;
		if (p2.x > maxX) maxX = p2.x;
		if (p2.y > maxY) maxY = p2.y;
	}

	if (!anyProjected) return false;
	if (maxY <= minY || maxX <= minX) return false;

	const float pad = g_boxPadding;
	out = { minX - pad, minY - pad, maxX + pad, maxY + pad };
	return true;
}

static void ProjectESPBoxes(const float view[16], int width, int height)
{
	g_espBoxes.clear();
	g_espBoxes.reserve(g_espTargetsWorld.size());

	for (const EspTargetWorld& t : g_espTargetsWorld)
	{
		Box2D box{};
		if (!GetEntityBox2DFromWorldAABB(t.origin, t.mins, t.maxs, view, width, height, box))
			continue;
		EspBox e{};
		e.box = box;
		e.hp = t.hp;
		e.pawn = t.pawn;
		// copy cached name/weapon for draw phase
		for (int i = 0; i < 64; ++i)
		{
			e.name[i] = t.name[i];
			if (!t.name[i]) break;
		}
		for (int i = 0; i < 64; ++i)
		{
			e.weapon[i] = t.weapon[i];
			if (!t.weapon[i]) break;
		}
		e.flashed = t.flashed;
		e.flashDur = t.flashDur; // Добавляем длительность флешки
		e.flashEndTime = t.flashEndTime;
		e.weaponIconIndex = t.weaponIconIndex;
		g_espBoxes.push_back(e);
	}
}

static void RunBhop(bool cs2Active)
{
	if (!cs2Active || !g_bhopEnabled) return;

	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;

	static uintptr_t s_forceJump = 0;
	static uintptr_t s_forceLeft = 0;
	static uintptr_t s_forceRight = 0;
	static bool jumpActive = false;
	static ULONGLONG lastActionTime = 0;
	static bool wasOnGround = true;

	__try
	{
		uintptr_t client = GetClientBase();
		if (!client) { s_forceJump = s_forceLeft = s_forceRight = 0; return; }
		if (!s_forceJump)  s_forceJump  = client + g_offsetsRuntime.dwForceJump;
		if (!s_forceLeft)  s_forceLeft  = client + g_offsetsRuntime.dwForceLeft;
		if (!s_forceRight) s_forceRight = client + g_offsetsRuntime.dwForceRight;

		const bool keyDown = (g_bhopKey != 0) && ((GetAsyncKeyState(g_bhopKey) & 0x8000) != 0);
		if (!keyDown)
		{
			if (jumpActive && s_forceJump) { (void)TryWrite<int>(s_forceJump, 256); jumpActive = false; }
			return;
		}

		ULONGLONG now = GetTickCount64();

		// --- Bhop основной ---
		if (now - lastActionTime >= 10ULL)
		{
			(void)TryWrite<int>(s_forceJump, jumpActive ? 256 : 65537);
			jumpActive = !jumpActive;
			lastActionTime = now;
		}

		// --- Auto-Strafe (если включён) ---
		if (g_autoStrafeEnabled && s_forceLeft && s_forceRight)
		{
			uintptr_t localPawn = 0;
			if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;

			// Читаем флаги (onGround?)
			uint32_t flags = 0;
			(void)TryRead<uint32_t>(localPawn + C_BaseEntity::m_fFlags, flags);
			bool onGround = (flags & 1) != 0;

			Vec3 vel{};
			(void)TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x0, vel.x);
			(void)TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x4, vel.y);

			QAngle* viewAnglesPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
			QAngle va{};
			if (!TryRead<QAngle>((uintptr_t)viewAnglesPtr, va)) return;

			if (!onGround)
			{
				// Считаем угол скорости относительно взгляда
				float velAngle = std::atan2(vel.y, vel.x) * (180.0f / M_PI);
				float yaw = va.y;
				float diff = velAngle - yaw;
				while (diff > 180.0f) diff -= 360.0f;
				while (diff < -180.0f) diff += 360.0f;

				// Стрейфим перпендикулярно скорости
				if (diff > 0.0f) {
					(void)TryWrite<int>(s_forceLeft,  65537); // A
					(void)TryWrite<int>(s_forceRight, 256);
				} else {
					(void)TryWrite<int>(s_forceRight, 65537); // D
					(void)TryWrite<int>(s_forceLeft,  256);
				}
			}
			else
			{
				// На земле — отпускаем стрейф
				(void)TryWrite<int>(s_forceLeft,  256);
				(void)TryWrite<int>(s_forceRight, 256);
			}
			wasOnGround = onGround;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		s_forceJump = s_forceLeft = s_forceRight = 0; jumpActive = false;
	}
}

// Нормализация углов (обязательно, чтобы не крашнуло и не крутило)
static void ClampAngles(QAngle& angles)
{
	if (angles.x > 89.0f) angles.x = 89.0f;
	if (angles.x < -89.0f) angles.x = -89.0f;
	while (angles.y > 180.0f) angles.y -= 360.0f;
	while (angles.y < -180.0f) angles.y += 360.0f;
	angles.z = 0.0f;
}

// Получение позиции кости (Internal Memory Access)
static Vec3 GetBonePos(uintptr_t pawn, int boneIndex)
{
	Vec3 pos = { 0, 0, 0 };
	if (!pawn) return pos;

	// Получаем GameSceneNode
	uintptr_t gameScene = 0;
	if (!TryRead<uintptr_t>(pawn + g_offsetsRuntime.m_pGameSceneNode, gameScene) || !gameScene)
		return pos;

	// Получаем BoneArray (ModelState + 0x80)
	uintptr_t boneMatrix = 0;
	if (!TryRead<uintptr_t>(gameScene + g_offsetsRuntime.m_modelState + 0x80, boneMatrix) || !boneMatrix)
		return pos;

	// Читаем координаты напрямую из памяти
	float x = 0.0f, y = 0.0f, z = 0.0f;
	if (TryRead<float>(boneMatrix + boneIndex * 0x20 + 0x0, x) &&
		TryRead<float>(boneMatrix + boneIndex * 0x20 + 0x4, y) &&
		TryRead<float>(boneMatrix + boneIndex * 0x20 + 0x8, z))
	{
		pos.x = x;
		pos.y = y;
		pos.z = z;
	}
	return pos;
}

// Глобальные переменные для контроля автовыстрела
// g_autoFireActive уже объявлен выше
static ULONGLONG g_lastShotTime = 0;

// Aimbot функция с RCS (Recoil Control System)
static void RunAimbot(bool cs2Active)
{
	if (!cs2Active || !g_aimbotEnabled) {
		// Сброс автовыстрела при отпускании кнопки
		if (g_autoFireActive) {
			uintptr_t client = GetClientBase();
			if (client) (void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
		return;
	}

	if (g_aimbotKey != 0 && !(GetAsyncKeyState(g_aimbotKey) & 0x8000)) {
		if (g_autoFireActive) {
			uintptr_t client = GetClientBase();
			if (client) (void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
		return;
	}

	uintptr_t client = GetClientBase();
	if (!client) return;

	using namespace cs2_dumper::schemas::client_dll;
	using namespace cs2_dumper;

	__try
	{
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		if (!localPawn) return;

		int localTeam = 0;
		(void)TryRead<int>(localPawn + C_BaseEntity::m_iTeamNum, localTeam);

		// Получаем углы и позицию
		QAngle* viewAnglesPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
		QAngle currentAngles{};
		if (!TryRead<QAngle>((uintptr_t)viewAnglesPtr, currentAngles))
			return;

		// ИСПРАВЛЕНИЕ: Используем актуальную позицию из GameSceneNode вместо m_vOldOrigin
		uintptr_t localGameScene = 0;
		if (!TryRead<uintptr_t>(localPawn + C_BaseEntity::m_pGameSceneNode, localGameScene))
			return;
		if (!localGameScene) return;

		Vec3 localOrigin{};
		(void)TryRead<float>(localGameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, localOrigin.x);
		(void)TryRead<float>(localGameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, localOrigin.y);
		(void)TryRead<float>(localGameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, localOrigin.z);
		
		Vec3 viewOffset{};
		(void)TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x0, viewOffset.x);
		(void)TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x4, viewOffset.y);
		(void)TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x8, viewOffset.z);
		Vec3 localEyePos = { localOrigin.x + viewOffset.x, localOrigin.y + viewOffset.y, localOrigin.z + viewOffset.z };
		
		// Проверяем, в прицеле ли мы (для снайперок)
		bool isScoped = false;
		(void)TryRead<bool>(localPawn + C_CSPlayerPawn::m_bIsScoped, isScoped);

		// Читаем AimPunch (отдачу) через m_pAimPunchServices
		QAngle aimPunch = {0,0,0};
		uintptr_t aimPunchSvc = 0;
		(void)TryRead<uintptr_t>(localPawn + C_CSPlayerPawn::m_pAimPunchServices, aimPunchSvc);
		if (aimPunchSvc) {
			(void)TryRead<QAngle>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle, aimPunch);
		}

		// ДОБАВЛЕНО: Читаем количество произведенных выстрелов
		int shotsFired = 0;
		(void)TryRead<int>(localPawn + C_CSPlayerPawn::m_iShotsFired, shotsFired);

		// ИСПРАВЛЕНИЕ: Читаем ViewMatrix и размеры экрана для Screen-Space расчетов
		const float* viewMatrix = reinterpret_cast<const float*>(client + g_offsetsRuntime.dwViewMatrix);
		if (!viewMatrix) return;
		
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		int width = (int)ds.x;
		int height = (int)ds.y;
		
		// Вычисляем радиус FOV в пикселях (та же формула, что в DrawEspImGui)
		float fovRadius = std::tan(g_aimbotFov * M_PI / 180.0f / 2.0f) * (height * 0.5f) / std::tan(90.0f * M_PI / 180.0f / 2.0f);

		// Логика поиска цели (SCREEN-SPACE для легита)
		float bestFovPixels = fovRadius; // Для Legit
		Vec3 bestTargetPos = {0,0,0};
		bool foundTarget = false;
		uintptr_t bestPawn = 0;

		uintptr_t entityList = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
			return;
		if (!entityList) return;

		// Находим ID (индекс) локального игрока для проверки видимости
		int localPlayerId = -1;
		for (int i = 1; i <= 64; ++i) { // ВАЖНО: Итерация от 1 до 64
			uintptr_t lEntry = 0; 
			if (!TryRead<uintptr_t>(entityList + 0x8 * ((i & 0x7FFF) >> 9) + 0x10, lEntry)) continue;
			if (!lEntry) continue;
			uintptr_t ctrl = 0; 
			if (!TryRead<uintptr_t>(lEntry + 0x70 * (i & 0x1FF), ctrl)) continue;
			if (!ctrl) continue;
			uint32_t pHandle = 0; 
			if (!TryRead<uint32_t>(ctrl + CCSPlayerController::m_hPlayerPawn, pHandle)) continue;
			if (!pHandle) continue;
			uintptr_t pList = 0; 
			if (!TryRead<uintptr_t>(entityList + 0x8 * ((pHandle & 0x7FFF) >> 9) + 0x10, pList)) continue;
			if (!pList) continue;
			uintptr_t pwn = 0; 
			if (!TryRead<uintptr_t>(pList + 0x70 * (pHandle & 0x1FF), pwn)) continue;
			if (pwn == localPawn) { localPlayerId = i; break; }
		}

		for (int id = 0; id < 64; ++id)
		{
			uintptr_t listEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry))
				continue;
			if (!listEntry) continue;

			uintptr_t controller = 0;
			if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller))
				continue;
			if (!controller) continue;

			uint32_t pawnHandle = 0;
			if (!TryRead<uint32_t>(controller + CCSPlayerController::m_hPlayerPawn, pawnHandle))
				continue;
			if (!pawnHandle) continue;

			int pawnIndex = pawnHandle & 0x1FF;
			int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;

			uintptr_t pawnListEntry = 0;
			if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry))
				continue;
			if (!pawnListEntry) continue;

			uintptr_t pawn = 0;
			if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn))
				continue;
			if (!pawn) continue;
			if (localPawn && pawn == localPawn) continue;

			int tHp = 0;
			if (!TryRead<int>(pawn + C_BaseEntity::m_iHealth, tHp))
				continue;
			if (tHp <= 0 || tHp > 200) continue;

			int tTeam = 0;
			if (!TryRead<int>(pawn + C_BaseEntity::m_iTeamNum, tTeam))
				continue;
			if (tTeam < 2 || tTeam > 3) continue;
			if (g_aimbotTeamCheck && tTeam == localTeam) continue;

			// 1. ПРОВЕРКА ВИДИМОСТИ
			bool isVisible = false;
			if (localPlayerId != -1)
			{
				uint64_t spottedMask = 0;
				if (TryRead<uint64_t>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0xC, spottedMask))
				{
					isVisible = (spottedMask & (1ULL << (localPlayerId - 1))) != 0;
				}
			}

			// Если включена проверка стен, скипаем тех, кого не видно
			if (g_aimbotEnabled && g_aimbotVisCheck)
			{
				if (!isVisible) continue;
			}

			// ИСПРАВЛЕНИЕ: Получаем АКТУАЛЬНУЮ позицию кости каждый кадр
			Vec3 targetBone = GetBonePos(pawn, g_aimbotBone);

			// Fallback если кость не найдена - используем более точный расчет
			if (targetBone.x == 0 && targetBone.y == 0 && targetBone.z == 0) {
				uintptr_t gameScene = 0;
				if (!TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gameScene))
					continue;
				if (!gameScene) continue;

				Vec3 absOrigin{};
				(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, absOrigin.x);
				(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, absOrigin.y);
				(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, absOrigin.z);

				Vec3 targetViewOffset{};
				(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x0, targetViewOffset.x);
				(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x4, targetViewOffset.y);
				(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x8, targetViewOffset.z);

				// ИСПРАВЛЕНИЕ: Используем точные координаты без смещения по X/Y
				targetBone.x = absOrigin.x;
				targetBone.y = absOrigin.y;

				// ИСПРАВЛЕНИЕ: Высота в зависимости от выбранной кости (без viewOffset по X/Y)
				if (g_aimbotBone == 6) {
					// Голова - используем высоту глаз (это и есть центр головы)
					targetBone.z = absOrigin.z + targetViewOffset.z;
				}
				else if (g_aimbotBone == 5) {
					// Шея - чуть ниже головы
					targetBone.z = absOrigin.z + targetViewOffset.z * 0.85f;
				}
				else if (g_aimbotBone == 4) {
					// Грудь - середина тела
					targetBone.z = absOrigin.z + targetViewOffset.z * 0.5f;
				}
				else if (g_aimbotBone == 2) {
					// Живот - ниже середины
					targetBone.z = absOrigin.z + targetViewOffset.z * 0.3f;
				}
				else {
					// По умолчанию - голова
					targetBone.z = absOrigin.z + targetViewOffset.z;
				}
			}

			// 2. ЛОГИКА ВЫБОРА ЦЕЛИ
			// LEGITBOT: Обычный поиск по радиусу FOV возле прицела
			Vec2 screenPos;
			if (!WorldToScreen(viewMatrix, targetBone, width, height, screenPos))
				continue;

			float dx = screenPos.x - (width / 2.0f);
			float dy = screenPos.y - (height / 2.0f);
			float fovPixels = std::sqrt(dx * dx + dy * dy);

			if (fovPixels < bestFovPixels && fovPixels <= fovRadius)
			{
				bestFovPixels = fovPixels;
				bestTargetPos = targetBone;
				foundTarget = true;
				bestPawn = pawn;
			}
		}

		if (foundTarget) {
			// ИСПРАВЛЕНИЕ: Перечитываем позицию кости ПРЯМО ПЕРЕД наведением
			Vec3 freshTargetPos = GetBonePos(bestPawn, g_aimbotBone);
			
			// Если кость найдена, используем её напрямую без корректировок
			if (freshTargetPos.x != 0 || freshTargetPos.y != 0 || freshTargetPos.z != 0) {
				bestTargetPos = freshTargetPos;
			}
			// Если кость не найдена, используем fallback с точным расчетом
			else {
				uintptr_t gameScene = 0;
				if (TryRead<uintptr_t>(bestPawn + C_BaseEntity::m_pGameSceneNode, gameScene) && gameScene) {
					Vec3 absOrigin{};
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x0, absOrigin.x);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x4, absOrigin.y);
					(void)TryRead<float>(gameScene + CGameSceneNode::m_vecAbsOrigin + 0x8, absOrigin.z);
					
					Vec3 targetViewOffset{};
					(void)TryRead<float>(bestPawn + C_BaseModelEntity::m_vecViewOffset + 0x0, targetViewOffset.x);
					(void)TryRead<float>(bestPawn + C_BaseModelEntity::m_vecViewOffset + 0x4, targetViewOffset.y);
					(void)TryRead<float>(bestPawn + C_BaseModelEntity::m_vecViewOffset + 0x8, targetViewOffset.z);
					
					// Используем центр модели без смещения по X/Y
					bestTargetPos.x = absOrigin.x;
					bestTargetPos.y = absOrigin.y;
					bestTargetPos.z = absOrigin.z + targetViewOffset.z; // Высота глаз = центр головы
				}
			}
			
			// === VELOCITY PREDICTION (упреждение под движение) ===
			{
				Vec3 enemyVel{};
				TryRead<float>(bestPawn + C_BaseEntity::m_vecVelocity + 0x0, enemyVel.x);
				TryRead<float>(bestPawn + C_BaseEntity::m_vecVelocity + 0x4, enemyVel.y);
				TryRead<float>(bestPawn + C_BaseEntity::m_vecVelocity + 0x8, enemyVel.z);

				float enemySpeed = std::sqrt(enemyVel.x * enemyVel.x + enemyVel.y * enemyVel.y);

				// Применяем упреждение только если враг реально движется (> 10 юнитов/с)
				if (enemySpeed > 10.0f)
				{
					// Дистанция до врага в юнитах
					float dx = bestTargetPos.x - localEyePos.x;
					float dy = bestTargetPos.y - localEyePos.y;
					float dz = bestTargetPos.z - localEyePos.z;
					float distToTarget = std::sqrt(dx*dx + dy*dy + dz*dz);

					// Скорость пули CS2 примерно 50000 юн/с (зависит от оружия, но среднее)
					// Время полёта пули до цели
					constexpr float bulletSpeed = 50000.0f;
					float bulletTravelTime = distToTarget / bulletSpeed;

					// Масштабируем упреждение: чем дальше цель, тем больше упреждение
					// Clamp чтобы на очень большой дистанции не было безумного смещения
					float predScale = bulletTravelTime;
					if (predScale > 0.15f) predScale = 0.15f; // максимум 150мс упреждения

					bestTargetPos.x += enemyVel.x * predScale;
					bestTargetPos.y += enemyVel.y * predScale;
					// Z не трогаем — вертикальное упреждение обычно не нужно и даёт промахи
				}
			}
			// === END VELOCITY PREDICTION ===
			
			QAngle targetAngles = CalcAngle(localEyePos, bestTargetPos);

			// ВАЖНО: Компенсация отдачи (RCS)
			// ИСПРАВЛЕНИЕ: Вычитаем отдачу ТОЛЬКО во время стрельбы (shotsFired > 0).
			// Иначе прицел "съезжает" с головы от тряски камеры при ходьбе и прыжках.
			if (!isScoped && shotsFired > 0) {
				targetAngles.x -= aimPunch.x * 2.0f;
				targetAngles.y -= aimPunch.y * 2.0f;
			}

			ClampAngles(targetAngles); // ОБЯЗАТЕЛЬНО нормализуем ДО смуса!

			// Smooth
			if (g_aimbotSmooth > 1.0f) {
				float deltaX = targetAngles.x - currentAngles.x;
				float deltaY = targetAngles.y - currentAngles.y;
				if (deltaY > 180.0f) deltaY -= 360.0f;
				if (deltaY < -180.0f) deltaY += 360.0f;

				targetAngles.x = currentAngles.x + deltaX / g_aimbotSmooth;
				targetAngles.y = currentAngles.y + deltaY / g_aimbotSmooth;
			}

			ClampAngles(targetAngles);
			(void)TryWrite<QAngle>((uintptr_t)viewAnglesPtr, targetAngles);

			// --- ИСПРАВЛЕННЫЙ AUTOFIRE (Точный расчет без учета Smooth) ---
			if (g_aimbotAutoFire) {
				// 1. Считаем ИДЕАЛЬНЫЙ угол до кости прямо сейчас (без Smooth)
				QAngle idealAngles = CalcAngle(localEyePos, bestTargetPos);
				
				// 2. Учитываем отдачу для автовыстрела
				if (!isScoped && shotsFired > 0) {
					idealAngles.x -= aimPunch.x * 2.0f;
					idealAngles.y -= aimPunch.y * 2.0f;
				}
				ClampAngles(idealAngles);
				
				// 3. Считаем реальную погрешность между ТЕКУЩИМ прицелом и ИДЕАЛЬНЫМ углом
				float errorX = idealAngles.x - currentAngles.x;
				float errorY = idealAngles.y - currentAngles.y;
				while (errorY > 180.0f) errorY -= 360.0f;
				while (errorY < -180.0f) errorY += 360.0f;
				
				// Переводим в градусы отклонения
				float errorDegrees = std::sqrt(errorX * errorX + errorY * errorY);
				
				// Если прицел в радиусе 1.2 градуса от идеальной точки кости — стреляем!
				bool onTarget = (errorDegrees < 1.2f);

				// Определяем semi-auto оружие (пистолеты, снайперки)
				bool isSemiAuto = false;
				{
					uintptr_t ws2 = 0;
					if (TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, ws2) && ws2) {
						uint32_t ha2 = 0;
						if (TryRead<uint32_t>(ws2 + CPlayer_WeaponServices::m_hActiveWeapon, ha2) && ha2) {
							int wi2 = ha2 & 0x1FF, we2 = (ha2 & 0x7FFF) >> 9;
							uintptr_t wle2 = 0;
							if (TryRead<uintptr_t>(entityList + 0x8 * we2 + 0x10, wle2) && wle2) {
								uintptr_t went2 = 0;
								if (TryRead<uintptr_t>(wle2 + 0x70 * wi2, went2) && went2) {
									uint16_t di2 = 0;
									TryRead<uint16_t>(went2 + C_EconEntity::m_AttributeManager +
										C_AttributeContainer::m_Item +
										C_EconItemView::m_iItemDefinitionIndex, di2);
									// Пистолеты и снайперки — semi-auto
									isSemiAuto = (di2==1||di2==2||di2==3||di2==4||di2==9||di2==30||di2==32||di2==36||di2==40||di2==61||di2==63||di2==64);
								}
							}
						}
					}
				}

				static bool  s_firePressed   = false;
				static ULONGLONG s_pressTime = 0;
				static ULONGLONG s_cooldownStart = 0;
				ULONGLONG nowFire = GetTickCount64();

				const ULONGLONG HOLD_MS     = 25;   // время удержания кнопки
				// ИСПРАВЛЕНИЕ: Добавили 40мс кулдауна для автоматов, чтобы разброс успевал остывать, и немного увеличили для полуавтоматов
				const ULONGLONG COOLDOWN_MS = isSemiAuto ? 220 : 40;

				bool inCooldown = (nowFire - s_cooldownStart < COOLDOWN_MS);

				if (onTarget && !inCooldown) {
					if (!s_firePressed) {
						(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 65537);
						s_firePressed = true;
						s_pressTime = nowFire;
						g_autoFireActive = true;
					} else if (nowFire - s_pressTime >= HOLD_MS) {
						(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
						s_firePressed = false;
						s_cooldownStart = nowFire; // начинаем паузу
						g_autoFireActive = false;
					}
				} else if (!onTarget) {
					if (s_firePressed) {
						(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
						s_firePressed = false;
						g_autoFireActive = false;
					}
				}
			}
		} else {
			// Цель вообще не найдена в цикле
			if (g_autoFireActive) {
				(void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
				g_autoFireActive = false;
			}
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		if (g_autoFireActive) {
			uintptr_t client = GetClientBase();
			if (client) (void)TryWrite<int>(client + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
	}
}

// Функция для правильной записи углов через CInput (для Internal чита)
static void RunCombat(bool cs2Active)
{
	// --- СОСТОЯНИЕ СТРЕЛЬБЫ ---
	static int s_triggerState = 0; // 0=Idle, 1=Delay, 2=Shooting, 3=Cooldown, 4=WaitingForAccuracy
	static ULONGLONG s_stateTime = 0;
	static int s_currentWeaponDefIndex = 0;
	static int s_shotsFired = 0;

	if (!cs2Active) {
		s_triggerState = 0;
		s_shotsFired = 0;
		return;
	}

	uintptr_t client = GetClientBase();
	if (!client) return;

	using namespace cs2_dumper::schemas::client_dll;
	using namespace cs2_dumper;

	uintptr_t attackPtr = client + g_offsetsRuntime.dwForceAttack;
	if (!attackPtr) return;

	__try
	{

	// ИСПРАВЛЕНИЕ: Строгая проверка валидности локального игрока
	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
		return;
	
	// 1. Проверка на nullptr (мы в меню)
	if (!localPawn) {
		g_espTargetsWorld.clear();
		g_espBoxes.clear();
		return;
	}
	
	// 2. Проверка здоровья (при смене карты часто бывает мусорное значение)
	int localHp = 0;
	if (!TryRead<int>(localPawn + C_BaseEntity::m_iHealth, localHp))
		return;
	if (localHp <= 0 || localHp > 10000) {
		return;
	}
	
	uintptr_t entityList = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList))
		return;
	if (!entityList) return; // Также проверяем EntityList
	
	int localTeam = 0;
	if (!TryRead<int>(localPawn + C_BaseEntity::m_iTeamNum, localTeam))
		return;

	// Читаем Punch (отдачу) для коррекции триггера через m_pAimPunchServices
	Vec3 aimPunch = {0,0,0};
	if (localPawn) {
		uintptr_t aimPunchSvc = 0;
		(void)TryRead<uintptr_t>(localPawn + C_CSPlayerPawn::m_pAimPunchServices, aimPunchSvc);
		if (aimPunchSvc) {
			(void)TryRead<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 0, aimPunch.x);
			(void)TryRead<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 4, aimPunch.y);
			(void)TryRead<float>(aimPunchSvc + CCSPlayer_AimPunchServices::m_predictableBaseAngle + 8, aimPunch.z);
		}
	}

	// 2. Углы и позиция
	QAngle* viewAnglesPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
	if (!viewAnglesPtr) return;
	QAngle currentAngles{};
	if (!TryRead<QAngle>((uintptr_t)viewAnglesPtr, currentAngles))
		return;
	
	// Эффективный угол пули = Куда смотрим + (Отдача * 2)
	QAngle bulletAngles = currentAngles;
	if (g_noRecoilEnabled) {
		// Если RCS включен, пули летят в центр
	} else {
		bulletAngles.x += aimPunch.x * 2.0f;
		bulletAngles.y += aimPunch.y * 2.0f;
	}

	Vec3 localOrigin{};
	if (!TryRead<float>(localPawn + C_BasePlayerPawn::m_vOldOrigin + 0x0, localOrigin.x)) return;
	if (!TryRead<float>(localPawn + C_BasePlayerPawn::m_vOldOrigin + 0x4, localOrigin.y)) return;
	if (!TryRead<float>(localPawn + C_BasePlayerPawn::m_vOldOrigin + 0x8, localOrigin.z)) return;
	Vec3 viewOffset{};
	if (!TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x0, viewOffset.x)) return;
	if (!TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x4, viewOffset.y)) return;
	if (!TryRead<float>(localPawn + C_BaseModelEntity::m_vecViewOffset + 0x8, viewOffset.z)) return;
	Vec3 localEyePos = { localOrigin.x + viewOffset.x, localOrigin.y + viewOffset.y, localOrigin.z + viewOffset.z };

	// Проверка скорости (для точности)
	Vec3 velocity{};
	if (!TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x0, velocity.x)) return;
	if (!TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x4, velocity.y)) return;
	if (!TryRead<float>(localPawn + C_BaseEntity::m_vecVelocity + 0x8, velocity.z)) return;
	float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

	// 3. Оружие
	bool isSniper = false;
	bool isPistol = false;
	uintptr_t weaponServices = 0;
	(void)TryRead<uintptr_t>(localPawn + C_BasePlayerPawn::m_pWeaponServices, weaponServices);
	if (weaponServices) {
		uint32_t hActive = 0;
		(void)TryRead<uint32_t>(weaponServices + CPlayer_WeaponServices::m_hActiveWeapon, hActive);
		if (hActive) {
			uintptr_t entityList = 0;
			(void)TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList);
			int weIdx = hActive & 0x1FF;
			int weEntryIdx = (hActive & 0x7FFF) >> 9;
			uintptr_t weList = 0;
			if (entityList)
				(void)TryRead<uintptr_t>(entityList + 0x8 * weEntryIdx + 0x10, weList);
			if (weList) {
				uintptr_t weaponEnt = 0;
				(void)TryRead<uintptr_t>(weList + 0x70 * weIdx, weaponEnt);
				if (weaponEnt) {
					uintptr_t itemView = weaponEnt + C_EconEntity::m_AttributeManager + C_AttributeContainer::m_Item;
					uint16_t defIndex = 0;
					if (TryRead<uint16_t>(itemView + C_EconItemView::m_iItemDefinitionIndex, defIndex))
						s_currentWeaponDefIndex = defIndex;
					
					if (s_currentWeaponDefIndex == 9 || s_currentWeaponDefIndex == 40) isSniper = true;
					if (s_currentWeaponDefIndex == 1 || s_currentWeaponDefIndex == 4 || s_currentWeaponDefIndex == 61 || s_currentWeaponDefIndex == 32 || s_currentWeaponDefIndex == 36) isPistol = true;
				}
			}
		}
	}

	ULONGLONG now = GetTickCount64();
	bool targetInSight = false;
	float distance = 0.0f;

	// Проверка нажатия кнопки
	bool triggerKeyHeld = (g_triggerKey != 0) && ((GetAsyncKeyState(g_triggerKey) & 0x8000) != 0);
	if (g_triggerEnabled && g_triggerKey == 0) triggerKeyHeld = true;
	
	if (g_triggerEnabled && triggerKeyHeld)
	{
		// Если мы бежим с винтовкой - не стрелять (разброс дикий)
		float maxSpeed = (isPistol || s_currentWeaponDefIndex == 17) ? 80.0f : 5.0f;
		if (speed <= maxSpeed || g_noSpreadEnabled)
		{
			uintptr_t entityList = 0;
			if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) || !entityList)
				return;
			
			// Находим ID (индекс) локального игрока для проверки видимости
			int localPlayerId = -1;
			for (int i = 1; i <= 64; ++i) { // ВАЖНО: Итерация от 1 до 64
				uintptr_t lEntry = 0; 
				if (!TryRead<uintptr_t>(entityList + 0x8 * ((i & 0x7FFF) >> 9) + 0x10, lEntry)) continue;
				if (!lEntry) continue;
				uintptr_t ctrl = 0; 
				if (!TryRead<uintptr_t>(lEntry + 0x70 * (i & 0x1FF), ctrl)) continue;
				if (!ctrl) continue;
				uint32_t pHandle = 0; 
				if (!TryRead<uint32_t>(ctrl + CCSPlayerController::m_hPlayerPawn, pHandle)) continue;
				if (!pHandle) continue;
				uintptr_t pList = 0; 
				if (!TryRead<uintptr_t>(entityList + 0x8 * ((pHandle & 0x7FFF) >> 9) + 0x10, pList)) continue;
				if (!pList) continue;
				uintptr_t pwn = 0; 
				if (!TryRead<uintptr_t>(pList + 0x70 * (pHandle & 0x1FF), pwn)) continue;
				if (pwn == localPawn) { localPlayerId = i; break; }
			}
			
			__try
			{
				for (int id = 0; id < 64; ++id)
				{
					uintptr_t listEntry = 0;
					if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry) || !listEntry) continue;
					uintptr_t controller = 0;
					if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller) || !controller) continue;

					uint32_t pawnHandle = 0;
					if (!TryRead<uint32_t>(controller + CCSPlayerController::m_hPlayerPawn, pawnHandle) || !pawnHandle) continue;
					int pawnIndex = pawnHandle & 0x1FF;
					int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;
					uintptr_t pawnListEntry = 0;
					if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry) || !pawnListEntry) continue;
					uintptr_t pawn = 0;
					if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn) || !pawn) continue;
					if (localPawn && pawn == localPawn) continue;

					int tHp = 0;
					if (!TryRead<int>(pawn + C_BaseEntity::m_iHealth, tHp) || tHp <= 0 || tHp > 200) continue;
					int tTeam = 0;
					if (!TryRead<int>(pawn + C_BaseEntity::m_iTeamNum, tTeam) || tTeam < 2 || tTeam > 3) continue;
					if (g_triggerTeamCheck && tTeam == localTeam) continue;

					// Проверка видимости (Отдельная от радархака, использует 0xC)
					if (g_triggerVisCheck && localPlayerId != -1) {
						uint64_t spottedMask = 0;
						if (!TryRead<uint64_t>(pawn + C_CSPlayerPawn::m_entitySpottedState + 0xC, spottedMask))
							continue;
						
						// ИСПРАВЛЕНИЕ: Сдвиг бита на (ID - 1)
						if (!(spottedMask & (1ULL << (localPlayerId - 1)))) continue;
					}

					int bonesToCheck[4]; int boneCount = 0;
					if (g_triggerHeadOnly) { bonesToCheck[0] = 6; boneCount = 1; }
					else { bonesToCheck[0]=6; bonesToCheck[1]=5; bonesToCheck[2]=4; bonesToCheck[3]=2; boneCount=4; }

					bool aimOnTarget = false;
					for (int i = 0; i < boneCount && !aimOnTarget; i++) {
						Vec3 bonePos = GetBonePos(pawn, bonesToCheck[i]);
						if (bonePos.x == 0 && bonePos.y == 0 && bonePos.z == 0) {
							uintptr_t gs = 0;
							if (!TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gs) || !gs) continue;
							Vec3 ao{}, tvo{};
							(void)TryRead<float>(gs + CGameSceneNode::m_vecAbsOrigin + 0x0, ao.x);
							(void)TryRead<float>(gs + CGameSceneNode::m_vecAbsOrigin + 0x4, ao.y);
							(void)TryRead<float>(gs + CGameSceneNode::m_vecAbsOrigin + 0x8, ao.z);
							(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x0, tvo.x);
							(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x4, tvo.y);
							(void)TryRead<float>(pawn + C_BaseModelEntity::m_vecViewOffset + 0x8, tvo.z);
							bonePos = { ao.x, ao.y, ao.z + (bonesToCheck[i]==6?tvo.z : bonesToCheck[i]==5?tvo.z*0.85f : bonesToCheck[i]==4?tvo.z*0.5f : tvo.z*0.3f) };
						}
						QAngle angToBone = CalcAngle(localEyePos, bonePos);
						float fov = GetFov(bulletAngles, angToBone);
						if (fov < 4.0f) aimOnTarget = true; // Увеличили с 2.5 до 4.0
					}

					if (aimOnTarget) {
						bool scopeOk = true;
						if (g_triggerScopeOnly && isSniper) {
							bool sc = false;
							TryRead<bool>(localPawn + C_CSPlayerPawn::m_bIsScoped, sc);
							scopeOk = sc;
						}
						if (scopeOk) { targetInSight = true; break; }
					}
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				targetInSight = false;
			}
		}
	}

	// Настройка задержек с контролем отдачи
	ULONGLONG delayBeforeShot = 0;
	if (isSniper) delayBeforeShot = 30;
	else if (distance > 1000.0f) delayBeforeShot = 40;
	else delayBeforeShot = 0;

	// Небольшая дополнительная задержка для не-снайперских оружий,
	// чтобы дать разбросу пуль чуть упасть перед выстрелом при зажатом триггере
	if (!isSniper)
	{
		delayBeforeShot += 5; // ~1-2 мс в игровых тиках, визуально не ощущается, но уменьшает промахи от разброса
	}

	ULONGLONG holdTime = 0;
	ULONGLONG cooldownTime = 0;
	ULONGLONG accuracyWaitTime = 0;

	// Конфиг оружия с контролем отдачи
	if (s_currentWeaponDefIndex == 7) { // AK-47
		holdTime = 90; 
		cooldownTime = 130;
		accuracyWaitTime = 150; // Ждем пока разброс уйдет
	}
	else if (isPistol) { 
		holdTime = 20; 
		cooldownTime = 40;
		accuracyWaitTime = 80; // Для пистолетов меньше
	}
	else if (isSniper) { 
		holdTime = 50; 
		cooldownTime = 1000;
		accuracyWaitTime = 0; // Снайперки не нуждаются в контроле отдачи
	}
	else { // Другие винтовки
		holdTime = 40; 
		cooldownTime = 100;
		accuracyWaitTime = 120;
	}

	// STATE MACHINE (ИСПРАВЛЕНО для One Tap)
	if (s_triggerState == 0) // Ждем цель
	{
		if (targetInSight) {
			(void)TryWrite<int>((uintptr_t)attackPtr, 65537); // Нажали (+attack)
			s_stateTime = now;
			s_triggerState = 1;
		}
	}
	else if (s_triggerState == 1) // Ждем отпускания
	{
		// Держим нажатым всего 15-30 мс (1-2 тика)
		if (now - s_stateTime >= 30) {
			(void)TryWrite<int>((uintptr_t)attackPtr, 256); // Отпустили (-attack)
			s_stateTime = now;
			s_triggerState = 2; // Переходим в кулдаун
		}
	}
	else if (s_triggerState == 2) // Пауза между выстрелами
	{
		(void)TryWrite<int>((uintptr_t)attackPtr, 256); // Гарантируем, что кнопка отпущена
		// Ждем 150-200мс перед следующим выстрелом
		if (now - s_stateTime >= 200) {
			s_triggerState = 0; // Готовы стрелять снова
		}
	}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		if (g_autoFireActive) {
			uintptr_t client2 = GetClientBase();
			if (client2) (void)TryWrite<int>(client2 + g_offsetsRuntime.dwForceAttack, 256);
			g_autoFireActive = false;
		}
	}
}

// Premium outlined text for ESP (black outline for readability on bright maps)
static void DrawOutlinedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text)
{
	ImU32 outlineCol = IM_COL32(0, 0, 0, 255);
	dl->AddText(font, size, ImVec2(pos.x - 1, pos.y - 1), outlineCol, text);
	dl->AddText(font, size, ImVec2(pos.x + 1, pos.y - 1), outlineCol, text);
	dl->AddText(font, size, ImVec2(pos.x - 1, pos.y + 1), outlineCol, text);
	dl->AddText(font, size, ImVec2(pos.x + 1, pos.y + 1), outlineCol, text);
	dl->AddText(font, size, pos, color, text);
}

// Instant HitSound detection - runs EVERY FRAME without ESP update limits
static void UpdateHitInfo(uintptr_t client)
{
	if (!g_hitSoundEnabled && !g_damageIndicatorsEnabled) return;
	if (!client) return;

	uintptr_t entityList = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwEntityList, entityList) || !entityList) return;

	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;
	
	int localTeam = 0;
	if (!TryRead<int>(localPawn + g_offsetsRuntime.m_iTeamNum, localTeam)) return;

	// Iterate all players (1-64)
	for (int id = 1; id <= 64; ++id)
	{
		uintptr_t listEntry = 0;
		if (!TryRead<uintptr_t>(entityList + 0x8 * ((id & 0x7FFF) >> 9) + 0x10, listEntry) || !listEntry) continue;

		uintptr_t controller = 0;
		if (!TryRead<uintptr_t>(listEntry + 0x70 * (id & 0x1FF), controller) || !controller) continue;

		uint32_t pawnHandle = 0;
		if (!TryRead<uint32_t>(controller + g_offsetsRuntime.m_hPlayerPawn, pawnHandle) || !pawnHandle) continue;

		int pawnIndex = pawnHandle & 0x1FF;
		int pawnEntryIndex = (pawnHandle & 0x7FFF) >> 9;
		uintptr_t pawnListEntry = 0;
		if (!TryRead<uintptr_t>(entityList + 0x8 * pawnEntryIndex + 0x10, pawnListEntry) || !pawnListEntry) continue;

		uintptr_t pawn = 0;
		if (!TryRead<uintptr_t>(pawnListEntry + 0x70 * pawnIndex, pawn) || !pawn) continue;
		if (pawn == localPawn) continue;

		int team = 0;
		if (!TryRead<int>(pawn + g_offsetsRuntime.m_iTeamNum, team)) continue;
		if (team == localTeam) continue; // Skip teammates

		int hp = 0;
		if (!TryRead<int>(pawn + g_offsetsRuntime.m_iHealth, hp)) continue;

		auto it = g_lastEnemyHp.find(pawn);
		if (it != g_lastEnemyHp.end())
		{
			int prevHp = it->second;
			if (hp < prevHp && prevHp > 0 && hp >= 0)
			{
				// Check if we shot recently
				bool isShooting = (GetTickCount64() - g_lastLocalShotTime < 600) || (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || g_autoFireActive;
				
				if (isShooting)
				{
					int damage = prevHp - hp;
					g_hitmarkerAlpha = 1.0f;
					g_hitmarkerTime = GetTickCount64();

					if (g_hitSoundEnabled && g_hitSoundBuffer)
					{
						PlaySoundA((LPCSTR)g_hitSoundBuffer, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
					}

					if (g_damageIndicatorsEnabled && damage > 0)
					{
						DamageText dt;
						dt.targetPawn = pawn;
						dt.damage = damage;
						dt.spawnTime = GetTickCount64();
						dt.alpha = 1.0f;
						dt.currentYOffset = 0.0f;
						dt.targetYOffset = 0.0f;
						
						uintptr_t gameScene = 0;
						if (TryRead<uintptr_t>(pawn + g_offsetsRuntime.m_pGameSceneNode, gameScene) && gameScene)
						{
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, dt.worldPos.x);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, dt.worldPos.y);
							TryRead<float>(gameScene + g_offsetsRuntime.m_vecAbsOrigin + 0x8, dt.worldPos.z);
							dt.worldPos.z += 40.0f;
						}
						g_damageTexts.push_back(dt);
					}
				}
			}
		}
		g_lastEnemyHp[pawn] = hp;
	}
}

// Helper function for bomb ESP rendering (SEH requires no objects with destructors)
static void DrawBombEsp(ImDrawList* drawList)
{
	if (!g_bombEspEnabled || !g_bombData.found)
		return;

	uintptr_t client = GetClientBase();
	if (!client)
		return;

	__try
	{
		const float* viewMatrix = reinterpret_cast<const float*>(client + g_offsetsRuntime.dwViewMatrix);
		if (viewMatrix)
		{
			ImVec2 ds = ImGui::GetIO().DisplaySize;
			int width = (int)ds.x;
			int height = (int)ds.y;

			Vec2 sPos;
			if (WorldToScreen(viewMatrix, g_bombData.pos, width, height, sPos))
			{
				// Рисуем красный квадрат вокруг бомбы
				drawList->AddRect(ImVec2(sPos.x - 15, sPos.y - 15), ImVec2(sPos.x + 15, sPos.y + 15), IM_COL32(255, 0, 0, 255), 0.0f, 0, 2.0f);

				// Таймер с fallback
				uintptr_t globalVars = 0;
				float timeLeft = -1.0f;
				bool hasTime = false;

				if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwGlobalVars, globalVars) && globalVars)
				{
					float curTime = 0.0f;
					// ИСПРАВЛЕНИЕ: Оффсет времени изменен с 0x2C на 0x30 (на некоторых версиях игры это 0x34)
					if (TryRead<float>(globalVars + 0x30, curTime) && curTime > 0.0f) {
						timeLeft = g_bombData.blowTime - curTime;
						hasTime = true;
					}
				}

				// ИСПРАВЛЕНИЕ: Отрисовываем только если время адекватное (отсекаем старые раунды)
				char buf[64];
				if (hasTime && timeLeft > 0.0f && timeLeft <= 45.0f) {
					sprintf_s(buf, sizeof(buf), "C4: %.1f", timeLeft);
					if (g_bombData.isDefusing) lstrcatA(buf, " [DEF]");
					drawList->AddText(ImVec2(sPos.x - 20, sPos.y + 16), IM_COL32(255, 255, 0, 255), buf);
				} else if (hasTime && timeLeft <= 0.0f && timeLeft > -5.0f) {
					// Бомба только что взорвалась
					drawList->AddText(ImVec2(sPos.x - 20, sPos.y + 16), IM_COL32(255, 0, 0, 255), "BOOM");
				} else if (!hasTime) {
					// Fallback: если время не читается
					lstrcpyA(buf, "C4");
					if (g_bombData.isDefusing) lstrcatA(buf, " [DEF]");
					drawList->AddText(ImVec2(sPos.x - 20, sPos.y + 16), IM_COL32(255, 200, 0, 255), buf);
				}

				// DEBUG: Логируем данные бомбы
				#ifdef _DEBUG
				char bombDbg[256];
				sprintf_s(bombDbg, "[BOMB] found=%d pos=%.1f,%.1f,%.1f blowTime=%.2f timeLeft=%.2f\n",
					g_bombData.found, g_bombData.pos.x, g_bombData.pos.y, g_bombData.pos.z,
					g_bombData.blowTime, timeLeft);
				OutputDebugStringA(bombDbg);
				#endif
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}

// Helper for reading view matrix safely (SEH requires no objects with destructors)
static bool TryReadViewMatrix(uintptr_t client, float* viewMatrix)
{
	if (!client) return false;
	__try {
		for (int i = 0; i < 16; ++i) {
			viewMatrix[i] = *(float*)(client + g_offsetsRuntime.dwViewMatrix + i * sizeof(float));
		}
		return true;
	} __except(1) {}
	return false;
}

static void DrawEspImGui()
{
	// Если ничего не включено, не рисуем (но hitmarker должен рисоваться!)
	if (!g_whEnabled && !g_bonesEnabled && !g_nameEspEnabled && !g_gunEspEnabled && !g_hpBarEnabled && !(g_aimbotEnabled && g_aimbotDrawFov) && !g_customCrosshair && g_bombEspEnabled == false && g_hitmarkerAlpha <= 0.0f) return;

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	const ImU32 staticBoxCol = ImGui::ColorConvertFloat4ToU32(g_boxColor);

	// --- ОТРИСОВКА ESP (БОКСЫ, ИМЕНА, ОРУЖИЕ) ---
	for (const EspBox& e : g_espBoxes)
	{
		const Box2D& box = e.box;
		
		// Dynamic box color based on HP
		ImU32 boxCol = staticBoxCol;
		if (g_dynamicBoxColor) {
			float hpPct = e.hp / 100.0f;
			if (hpPct < 0.0f) hpPct = 0.0f;
			if (hpPct > 1.0f) hpPct = 1.0f;
			// Green (100%) -> Yellow (50%) -> Red (0%)
			float r = (hpPct > 0.5f) ? (1.0f - (hpPct - 0.5f) * 2.0f) : 1.0f;
			float g = (hpPct > 0.5f) ? 1.0f : (hpPct * 2.0f);
			boxCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.1f, 1.0f));
		}
		
		// Боксы (Corner Box style с черной аутлайн-тенью)
		if (g_whEnabled)
		{
			float bW = box.right - box.left;
			float bH = box.bottom - box.top;
			float lX = bW * 0.25f; // Длина уголка (25% от ширины)
			float lY = bW * 0.25f; // Длина уголка по вертикали
			float thick = g_boxThickness;

			ImU32 outlineCol = IM_COL32(0, 0, 0, 200); // Черная обводка для читаемости

			auto drawCorner = [&](float x1, float y1, float x2, float y2, float x3, float y3) {
				// Тень (Outline)
				drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), outlineCol, thick + 2.0f);
				drawList->AddLine(ImVec2(x2, y2), ImVec2(x3, y3), outlineCol, thick + 2.0f);
				// Основная линия
				drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), boxCol, thick);
				drawList->AddLine(ImVec2(x2, y2), ImVec2(x3, y3), boxCol, thick);
			};

			// Top Left
			drawCorner(box.left, box.top + lY, box.left, box.top, box.left + lX, box.top);
			// Top Right
			drawCorner(box.right - lX, box.top, box.right, box.top, box.right, box.top + lY);
			// Bottom Left
			drawCorner(box.left, box.bottom - lY, box.left, box.bottom, box.left + lX, box.bottom);
			// Bottom Right
			drawCorner(box.right - lX, box.bottom, box.right, box.bottom, box.right, box.bottom - lY);
		}

		// HP Bar с плавной анимацией
		if (g_hpBarEnabled)
		{
			// Статическая мапа для хранения "текущего" анимированного HP каждого игрока
			static std::map<uintptr_t, float> lerpedHpMap;
			
			float targetHp = (float)e.hp;
			if (lerpedHpMap.find(e.pawn) == lerpedHpMap.end()) lerpedHpMap[e.pawn] = targetHp;
			
			// Плавное приближение (Lerp) к реальному HP, скорость зависит от FPS (DeltaTime)
			lerpedHpMap[e.pawn] += (targetHp - lerpedHpMap[e.pawn]) * 10.0f * ImGui::GetIO().DeltaTime;
			float displayHp = lerpedHpMap[e.pawn];

			const float hp01 = displayHp / 100.0f;
			float r = (hp01 > 0.5f) ? (1.0f - (hp01 - 0.5f) * 2.0f) : 1.0f;
			float g = (hp01 > 0.5f) ? 1.0f : (hp01 * 2.0f);
			
			// Добавим легкое свечение (Glow) для полоски здоровья
			ImU32 hpCol = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.2f, 1.0f));
			ImU32 hpGlow = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, 0.2f, 0.4f)); 

			const float barW = g_hpBarWidth;
			const float barPad = g_hpBarOffset;
			const float barX0 = box.left - barPad - barW;
			const float barY0 = box.top;
			const float barX1 = box.left - barPad;
			const float barY1 = box.bottom;
			const float barH = (barY1 - barY0);
			const float fillH = barH * hp01;

			// Тень/Фон бара
			drawList->AddRectFilled(ImVec2(barX0, barY0), ImVec2(barX1, barY1), IM_COL32(0, 0, 0, 180), 2.0f);
			// Сама полоска с закруглениями
			drawList->AddRectFilled(ImVec2(barX0 + 1.0f, barY1 - fillH + 1.0f), ImVec2(barX1 - 1.0f, barY1 - 1.0f), hpCol, 1.5f);
			// Glow эффект вокруг полоски
			drawList->AddRect(ImVec2(barX0 + 1.0f, barY1 - fillH + 1.0f), ImVec2(barX1 - 1.0f, barY1 - 1.0f), hpGlow, 1.5f, 0, 2.0f);
			// Обводка бара
			drawList->AddRect(ImVec2(barX0, barY0), ImVec2(barX1, barY1), IM_COL32(0, 0, 0, 255), 2.0f, 0, 1.0f);
		}

		// Name ESP
		if (g_nameEspEnabled && e.pawn)
		{
			if (e.name[0] != '\0')
			{
				ImU32 nameColU32 = ImGui::ColorConvertFloat4ToU32(g_nameColor);
				ImVec2 textSize = ImGui::CalcTextSize(e.name);
				float xCenter = (box.left + box.right) * 0.5f;
				ImVec2 pos(xCenter - textSize.x * 0.5f, box.top - g_nameOffsetY - textSize.y);
				drawList->AddText(pos, nameColU32, e.name);
			}
		}
		
		// Gun ESP (ИСПРАВЛЕНО: Убраны скобки [], цвет теперь берется из правильной переменной)
		if (g_gunEspEnabled && e.pawn && e.weapon[0] != '\0')
		{
			char wepText[64];
			// Убрали квадратные скобки
			wsprintfA(wepText, "%s", e.weapon);
			
			// Используем g_weaponIconColor, который меняется в меню
			ImU32 gunColU32 = ImGui::ColorConvertFloat4ToU32(g_weaponIconColor);
			
			ImVec2 textSize = ImGui::CalcTextSize(wepText);
			float xCenter = (box.left + box.right) * 0.5f;
			ImVec2 pos(xCenter - textSize.x * 0.5f, box.bottom + g_gunOffsetY);
			drawList->AddText(pos, gunColU32, wepText);
		}

		// Flash Bar (вместо текста)
		if (e.flashed && (e.flashDur > 0.0f || e.flashEndTime > 0.0f))
		{
			// Максимальная длительность флешки около 5 секунд
			float flashMax = 5.0f;
			float flashPct = 0.0f;

			float nowGameTime = GetEstimatedGameTimeSeconds();
			if (e.flashEndTime > 0.0f && nowGameTime > 0.0f) {
				float remaining = e.flashEndTime - nowGameTime;
				if (remaining < 0.0f) remaining = 0.0f;
				flashPct = remaining / flashMax;
			} else {
				// Fallback: если не можем оценить game time, используем duration как раньше
				flashPct = e.flashDur / flashMax;
			}
			if (flashPct > 1.0f) flashPct = 1.0f;
			if (flashPct < 0.0f) flashPct = 0.0f;

			// Рисуем бар справа от бокса
			float barW = 3.0f;
			float barH = (box.bottom - box.top) * flashPct;

			float x = box.right + 4.0f;
			float y = box.bottom - barH;

			// Фон бара (темный)
			drawList->AddRectFilled(ImVec2(x, box.top), ImVec2(x + barW, box.bottom), IM_COL32(0, 0, 0, 150));
			// Белый бар (показывает длительность)
			drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + barW, box.bottom), IM_COL32(255, 255, 255, 255));
		}
	}

	// --- ОТРИСОВКА ПРИЦЕЛА СНАЙПЕРКИ (AWP/SCOUT) ---
	// ИСПРАВЛЕНИЕ: Добавлена проверка g_customCrosshair
	if (g_customCrosshair && g_isLocalSniperScoped)
	{
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImVec2 c(ds.x * 0.5f, ds.y * 0.5f);
		const float len = 10.0f;
		const float gap = 6.0f;
		const float thick = 1.5f;
		const ImU32 col = IM_COL32(255, 255, 255, 220);

		drawList->AddLine(ImVec2(c.x - gap - len, c.y), ImVec2(c.x - gap, c.y), col, thick);
		drawList->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + len, c.y), col, thick);
		drawList->AddLine(ImVec2(c.x, c.y - gap - len), ImVec2(c.x, c.y - gap), col, thick);
		drawList->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + gap + len), col, thick);
	}

	// --- HITMARKER (Анимированный с разлетом) ---
	if (g_hitmarkerAlpha > 0.0f) {
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImVec2 c(ds.x * 0.5f, ds.y * 0.5f);
		
		ULONGLONG now = GetTickCount64();
		float timeSinceHit = (float)(now - g_hitmarkerTime);
		
		// Анимация разлета (от 2px до 10px) - создает чувство удара
		float hitGap = 2.0f + (timeSinceHit * 0.03f); 
		if (hitGap > 10.0f) hitGap = 10.0f;
		
		const float hitLen = 8.0f; // Чуть компактнее
		const float hitThick = 2.0f;
		
		// Плавное затухание
		float fadeTime = 400.0f; 
		if (timeSinceHit > 100.0f) { 
			g_hitmarkerAlpha = 1.0f - ((timeSinceHit - 100.0f) / fadeTime);
			if (g_hitmarkerAlpha < 0.0f) g_hitmarkerAlpha = 0.0f;
		}
		
		ImU32 hitCol = IM_COL32(255, 255, 255, (int)(g_hitmarkerAlpha * 255));
		ImU32 shadowCol = IM_COL32(0, 0, 0, (int)(g_hitmarkerAlpha * 150));
		
		// Рисуем тень для лучшей видимости на светлых текстурах
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen + 1, c.y - hitGap - hitLen + 1), ImVec2(c.x - hitGap + 1, c.y - hitGap + 1), shadowCol, hitThick + 1.0f);
		drawList->AddLine(ImVec2(c.x + hitGap + 1, c.y + hitGap + 1), ImVec2(c.x + hitGap + hitLen + 1, c.y + hitGap + hitLen + 1), shadowCol, hitThick + 1.0f);
		drawList->AddLine(ImVec2(c.x + hitGap + 1, c.y - hitGap - hitLen + 1), ImVec2(c.x + hitGap + hitLen + 1, c.y - hitGap + 1), shadowCol, hitThick + 1.0f);
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen + 1, c.y + hitGap + 1), ImVec2(c.x - hitGap + 1, c.y + hitGap + hitLen + 1), shadowCol, hitThick + 1.0f);
		
		// Рисуем сам маркер
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen, c.y - hitGap - hitLen), ImVec2(c.x - hitGap, c.y - hitGap), hitCol, hitThick);
		drawList->AddLine(ImVec2(c.x + hitGap, c.y + hitGap), ImVec2(c.x + hitGap + hitLen, c.y + hitGap + hitLen), hitCol, hitThick);
		drawList->AddLine(ImVec2(c.x + hitGap, c.y - hitGap - hitLen), ImVec2(c.x + hitGap + hitLen, c.y - hitGap), hitCol, hitThick);
		drawList->AddLine(ImVec2(c.x - hitGap - hitLen, c.y + hitGap), ImVec2(c.x - hitGap, c.y + hitGap + hitLen), hitCol, hitThick);
	}

	// --- ОТРИСОВКА FOV (ТЕПЕРЬ ВСЕГДА В КОНЦЕ) ---
	if (g_aimbotEnabled && g_aimbotDrawFov)
	{
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		ImVec2 center(ds.x * 0.5f, ds.y * 0.5f);
		
		// Конвертируем FOV в пиксели (примерная формула для 16:9)
		// Радиус = tan(FOV/2) * (ScreenHeight/2) / tan(CameraFOV/2)
		// Упрощенно:
		float fovRadius = std::tan(g_aimbotFov * M_PI / 180.0f / 2.0f) * (ds.y * 0.5f) / std::tan(90.0f * M_PI / 180.0f / 2.0f);
		
		ImU32 fovColor = ImGui::ColorConvertFloat4ToU32(g_aimbotFovColor);
		drawList->AddCircle(center, fovRadius, fovColor, 64, 1.0f);
	}

	// --- ОТРИСОВКА БОМБЫ (C4) ---
	DrawBombEsp(drawList);

	// --- OUT OF FOV ARROWS ---
	if (g_oofArrowsEnabled && !g_espTargetsWorld.empty())
	{
		uintptr_t client = GetClientBase();
		if (client) {
			QAngle* vpPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
			QAngle va{};
			if (TryRead<QAngle>((uintptr_t)vpPtr, va)) {
				
				uintptr_t localPawn = 0;
				if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) && localPawn) {
					uintptr_t localScene = 0;
					if (TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene) && localScene) {
						Vec3 myPos{};
						(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, myPos.x);
						(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, myPos.y);

						ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
						
						// Анимация пульсации прозрачности (дыхание)
						float oofPulse = (sinf((float)ImGui::GetTime() * 6.0f) * 0.4f) + 0.6f; // От 0.6 до 1.0
						ImVec4 animColor = g_oofArrowsColor;
						animColor.w *= oofPulse; // Применяем пульсацию к альфа-каналу
						ImU32 arrowCol = ImGui::ColorConvertFloat4ToU32(animColor);
						
						// Переводим текущий угол взгляда (yaw) в радианы
						float viewYaw = va.y * (M_PI / 180.0f);

						for (const EspTargetWorld& t : g_espTargetsWorld) {
							// Проверяем, на экране ли враг
							Vec2 screenPos;
							bool onScreen = WorldToScreen(reinterpret_cast<const float*>(client + g_offsetsRuntime.dwViewMatrix), t.origin, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y), screenPos);
							
							// Если враг вне экрана
							if (!onScreen) {
								float dx = t.origin.x - myPos.x;
								float dy = t.origin.y - myPos.y;
								
								// Угол на врага в мире
								float angleToEnemy = std::atan2(dy, dx);
								
								// Разница между нашим взглядом и углом на врага
								// В CS2 X/Y оси повернуты, поэтому вычитаем
								float relativeAngle = viewYaw - angleToEnemy - (M_PI / 2.0f);
								
								// Позиция стрелки на экране
								float arrowX = center.x + std::cos(relativeAngle) * g_oofArrowsRadius;
								float arrowY = center.y + std::sin(relativeAngle) * g_oofArrowsRadius;
								
								// Рисуем треугольник (стрелку), указывающую в эту сторону
								float size = g_oofArrowsSize;
								ImVec2 p1(arrowX + std::cos(relativeAngle) * size, arrowY + std::sin(relativeAngle) * size);
								ImVec2 p2(arrowX + std::cos(relativeAngle + 2.5f) * size * 0.8f, arrowY + std::sin(relativeAngle + 2.5f) * size * 0.8f);
								ImVec2 p3(arrowX + std::cos(relativeAngle - 2.5f) * size * 0.8f, arrowY + std::sin(relativeAngle - 2.5f) * size * 0.8f);
								
								drawList->AddTriangleFilled(p1, p2, p3, arrowCol);
								drawList->AddTriangle(p1, p2, p3, IM_COL32(0,0,0,200), 1.5f); // Обводка
							}
						}
					}
				}
			}
		}
	}

	// --- FLOATING DAMAGE INDICATORS (STACKED WITH ANIMATION) ---
	if (g_damageIndicatorsEnabled && !g_damageTexts.empty())
	{
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		ULONGLONG now = GetTickCount64();
		
		uintptr_t client = GetClientBase();
		
		// Безопасное чтение матрицы
		float viewMatrix[16] = {0};
		bool hasViewMatrix = TryReadViewMatrix(client, viewMatrix);
		
		ImVec2 ds = ImGui::GetIO().DisplaySize;
		float lifetimeMs = g_damageTextLifetime * 1000.0f;
		
		// Count stack per target for vertical offset
		std::map<uintptr_t, int> stackCount;
		
		// Iterate from end (newest first, at bottom)
		for (int i = (int)g_damageTexts.size() - 1; i >= 0; --i)
		{
			DamageText& dt = g_damageTexts[i];
			float ageMs = (float)(now - dt.spawnTime);
			
			// Remove old entries
			if (ageMs > lifetimeMs) {
				g_damageTexts.erase(g_damageTexts.begin() + i);
				continue;
			}
			
			// Stack logic: each new hit on same target goes 25px higher
			int index = stackCount[dt.targetPawn]++;
			dt.targetYOffset = index * 25.0f;
			
			// Smooth animation interpolation
			dt.currentYOffset += (dt.targetYOffset - dt.currentYOffset) * 0.15f;
			
			Vec2 screenPos;
			if (hasViewMatrix && WorldToScreen(viewMatrix, dt.worldPos, (int)ds.x, (int)ds.y, screenPos))
			{
				// Fade in last second of life
				if (ageMs > lifetimeMs - 1000.0f) {
					dt.alpha = (lifetimeMs - ageMs) / 1000.0f;
				} else {
					dt.alpha = 1.0f;
				}
				
				char buf[16];
				sprintf_s(buf, sizeof(buf), "-%d", dt.damage);
				
				ImU32 col = IM_COL32((int)(g_damageColor.x * 255), (int)(g_damageColor.y * 255), (int)(g_damageColor.z * 255), (int)(dt.alpha * 255));
				ImU32 outlineCol = IM_COL32(0, 0, 0, (int)(dt.alpha * 200));
				
				// Draw to the right of model, rising up
				ImVec2 textPos(screenPos.x + 30.0f, screenPos.y - 20.0f - dt.currentYOffset);
				
				// Premium outline (4 corners)
				ImFont* font = ImGui::GetFont();
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x + 1, textPos.y + 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x - 1, textPos.y - 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x + 1, textPos.y - 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, ImVec2(textPos.x - 1, textPos.y + 1), outlineCol, buf);
				dl->AddText(font, g_damageTextSize, textPos, col, buf);
			}
		}
	}
}

static void DrawWatermarkImGui()
{
	static int cs2Fps = 0;
	static ULONGLONG lastFpsTime = GetTickCount64();
	static int lastFrameCount = 0;
	
	uintptr_t client = GetClientBase();
	int currentPing = 0;
	char playerName[64] = "Unknown";
	
	if (client) {
		uintptr_t globalVars = 0;
		if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwGlobalVars, globalVars) && globalVars) {
			int frameCount = 0;
			if (TryRead<int>(globalVars + 0x4, frameCount)) { // 0x4 - это m_real_frametime/framecount в CS2
				ULONGLONG now = GetTickCount64();
				if (now - lastFpsTime >= 1000) {
					cs2Fps = frameCount - lastFrameCount;
					lastFrameCount = frameCount;
					lastFpsTime = now;
				}
			}
		}
		
		uintptr_t localCtrl = 0;
		if (TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerController, localCtrl) && localCtrl) {
			TryRead<int>(localCtrl + 0x740, currentPing); // 0x740 - это m_iPing
			
			StringBuf64 nameRaw = {0};
			if (TryRead<StringBuf64>(localCtrl + g_offsetsRuntime.m_iszPlayerName, nameRaw)) {
				memcpy(playerName, nameRaw.data, 64);
				playerName[63] = '\0';
			}
		}
	}
	
	if (cs2Fps <= 0 || cs2Fps > 2000) cs2Fps = (int)ImGui::GetIO().Framerate; // Fallback

	ImGuiWindowFlags wmFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;
	if (!g_menuOpen) wmFlags |= ImGuiWindowFlags_NoInputs; // Пропускать клики, если меню закрыто!
	
	ImGui::SetNextWindowBgAlpha(0.7f);
	ImGui::Begin("Watermark", nullptr, wmFlags);
	ImGui::TextColored(g_guiColor, "OXRANA LOUTABA");
	ImGui::SameLine();
	ImGui::Text("| User: %s | FPS: %d | Ping: %d ms", playerName, cs2Fps, currentPing);
	ImGui::End();
}

static void UpdateBonesCache(int width, int height)
{
	uintptr_t client = GetClientBase();
	if (!client) return;
	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;
	static const int neededBones[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 };

	// Временный массив для защиты от краша (используется ВНЕ блока __try)
	BoneCacheEntryWorld tempBones[64] = {};
	int boneCount = 0;

	__try
	{
		for (const EspTargetWorld& e : g_espTargetsWorld)
		{
			if (boneCount >= 64) break;
			uintptr_t pawn = e.pawn;
			if (!pawn) continue;

			uintptr_t gameScene = 0;
			if (!TryRead<uintptr_t>(pawn + C_BaseEntity::m_pGameSceneNode, gameScene) || !gameScene) continue;

			uintptr_t boneMatrix = 0;
			if (!TryRead<uintptr_t>(gameScene + CSkeletonInstance::m_modelState + 0x80, boneMatrix) || !boneMatrix) continue;

			BoneCacheEntryWorld entry{};
			entry.pawn = pawn;
			for (auto& cb : entry.b) cb.valid = false;

			for (int boneId : neededBones)
			{
				Vec3 p3{};
				if (TryRead<float>(boneMatrix + boneId * 0x20 + 0x0, p3.x) &&
					TryRead<float>(boneMatrix + boneId * 0x20 + 0x4, p3.y) &&
					TryRead<float>(boneMatrix + boneId * 0x20 + 0x8, p3.z))
				{
					entry.b[boneId].world = p3;
					entry.b[boneId].valid = true;
				}
			}
			tempBones[boneCount++] = entry;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	// ИСПРАВЛЕНИЕ: std::vector используется ВНЕ блока __try!
	g_boneCache.clear();
	for (int i = 0; i < boneCount; ++i) {
		g_boneCache.push_back(tempBones[i]);
	}
}

// ===================== RADAR HACK =====================
static void DrawRadarImGui()
{
	if (!g_radarHackEnabled || g_espTargetsWorld.empty()) return;

	uintptr_t client = GetClientBase();
	if (!client) return;

	QAngle* vpPtr = reinterpret_cast<QAngle*>(client + g_offsetsRuntime.dwViewAngles);
	QAngle va{};
	if (!TryRead<QAngle>((uintptr_t)vpPtr, va)) return;

	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;
	uintptr_t localScene = 0;
	if (!TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene) || !localScene) return;
	Vec3 myPos{};
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x0, myPos.x);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x4, myPos.y);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 0x8, myPos.z);

	ImVec2 ds = ImGui::GetIO().DisplaySize;

	// Радар: позиция и размер
	static float radarX = 20.0f, radarY = 20.0f;
	float radarSize = g_radarSize;
	float radarScale = g_radarScale;
	ImVec2 radarCenter(radarX + radarSize * 0.5f, radarY + radarSize * 0.5f);

	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	// Фон
	dl->AddCircleFilled(radarCenter, radarSize * 0.5f, IM_COL32(0, 0, 0, 160), 64);
	dl->AddCircle(radarCenter, radarSize * 0.5f, IM_COL32(80, 80, 80, 255), 64, 1.5f);
	// Кресты
	dl->AddLine(ImVec2(radarCenter.x - radarSize*0.5f, radarCenter.y), ImVec2(radarCenter.x + radarSize*0.5f, radarCenter.y), IM_COL32(60,60,60,150), 0.5f);
	dl->AddLine(ImVec2(radarCenter.x, radarCenter.y - radarSize*0.5f), ImVec2(radarCenter.x, radarCenter.y + radarSize*0.5f), IM_COL32(60,60,60,150), 0.5f);
	// Игрок (белая точка в центре)
	dl->AddCircleFilled(radarCenter, 4.0f, IM_COL32(255, 255, 255, 255));

	float yawRad = va.y * (M_PI / 180.0f);
	float cosYaw = std::cos(-yawRad);
	float sinYaw = std::sin(-yawRad);

	for (const EspTargetWorld& t : g_espTargetsWorld)
	{
		float dx = t.origin.x - myPos.x;
		float dy = t.origin.y - myPos.y;

		// Поворачиваем относительно взгляда
		float rx = dx * cosYaw - dy * sinYaw;
		float ry = dx * sinYaw + dy * cosYaw;

		// Масштабируем
		float px = radarCenter.x + rx / radarScale;
		float py = radarCenter.y - ry / radarScale; // Y инвертирован

		// Ограничиваем по кругу
		float relX = px - radarCenter.x;
		float relY = py - radarCenter.y;
		float dist = std::sqrt(relX * relX + relY * relY);
		float maxR = radarSize * 0.5f - 5.0f;
		if (dist > maxR) { px = radarCenter.x + relX * maxR / dist; py = radarCenter.y + relY * maxR / dist; }

		// Цвет: красный для CT, желтый для T
		ImU32 dotCol = IM_COL32(255, 80, 80, 255);
		dl->AddCircleFilled(ImVec2(px, py), 4.0f, dotCol);
		dl->AddCircle(ImVec2(px, py), 4.5f, IM_COL32(0,0,0,180), 8, 1.0f);
	}

	// Бомба на радаре
	if (g_bombEspEnabled && g_bombData.found)
	{
		float bdx = g_bombData.pos.x - myPos.x;
		float bdy = g_bombData.pos.y - myPos.y;
		float brx = bdx * cosYaw - bdy * sinYaw;
		float bry = bdx * sinYaw + bdy * cosYaw;
		float bpx = radarCenter.x + brx / radarScale;
		float bpy = radarCenter.y - bry / radarScale;
		float relBX = bpx - radarCenter.x, relBY = bpy - radarCenter.y;
		float bd = std::sqrt(relBX*relBX + relBY*relBY);
		float maxR2 = radarSize * 0.5f - 5.0f;
		if (bd > maxR2) { bpx = radarCenter.x + relBX*maxR2/bd; bpy = radarCenter.y + relBY*maxR2/bd; }
		dl->AddRectFilled(ImVec2(bpx-4, bpy-4), ImVec2(bpx+4, bpy+4), IM_COL32(255, 220, 0, 255));
	}
}

// ===================== SNAPLINES + DISTANCE =====================
static void DrawSnaplinesImGui()
{
	if (!g_snaplinesEnabled && !g_distanceEnabled) return;
	if (g_espBoxes.empty()) return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImVec2 ds = ImGui::GetIO().DisplaySize;
	ImVec2 bottom(ds.x * 0.5f, ds.y);

	uintptr_t client = GetClientBase();
	if (!client) return;
	uintptr_t localPawn = 0;
	if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn) || !localPawn) return;
	uintptr_t localScene = 0;
	if (!TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene) || !localScene) return;
	Vec3 myPos{};
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin, myPos.x);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 4, myPos.y);
	(void)TryRead<float>(localScene + g_offsetsRuntime.m_vecAbsOrigin + 8, myPos.z);

	int idx = 0;
	for (const auto& tgt : g_espTargetsWorld)
	{
		if (idx >= (int)g_espBoxes.size()) break;
		const EspBox& eb = g_espBoxes[idx++];

		if (g_snaplinesEnabled)
		{
			ImVec2 boxBottom((eb.box.left + eb.box.right) * 0.5f, eb.box.bottom);
			dl->AddLine(bottom, boxBottom, IM_COL32(255, 200, 80, 130), 1.0f);
		}

		if (g_distanceEnabled)
		{
			float dx = tgt.origin.x - myPos.x;
			float dy = tgt.origin.y - myPos.y;
			float dz = tgt.origin.z - myPos.z;
			float dist = std::sqrt(dx*dx + dy*dy + dz*dz) * 0.01905f; // units→metres
			char distBuf[16];
			sprintf_s(distBuf, sizeof(distBuf), "%.0fm", dist);
			ImVec2 distPos((eb.box.left + eb.box.right) * 0.5f - 12.0f, eb.box.top - 28.0f);
			dl->AddText(distPos, IM_COL32(200, 200, 200, 200), distBuf);
		}
	}
}

static void DrawBonesImGui(const float view[16], int width, int height)
{
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	const ImU32 col = ImGui::ColorConvertFloat4ToU32(g_boneColor);

	// Draw-only: must not read memory here.
	// Reduced chains: spine/head/arms/legs
	static const int spineChain[] = { 6, 5, 4, 2, 0 };
	static const int leftArmChain[] = { 4, 7, 8, 9, 10 };
	static const int rightArmChain[] = { 4, 12, 13, 14, 15 };
	static const int leftLegChain[] = { 0, 22, 23, 24 };
	static const int rightLegChain[] = { 0, 25, 26, 27 };

	auto DrawChain = [&](const BoneCacheEntryWorld& entry, const int* chain, int count)
	{
		ImVec2 pts[8]{};
		int n = 0;
		for (int i = 0; i < count; ++i)
		{
			int id = chain[i];
			if (id < 0 || id >= 28) continue;
			if (!entry.b[id].valid) continue;
			Vec2 p2{};
			if (!WorldToScreen(view, entry.b[id].world, width, height, p2))
				continue;
			pts[n++] = ImVec2(p2.x, p2.y);
		}
		if (n >= 2)
			drawList->AddPolyline(pts, n, col, 0, 1.5f);
	};

	for (const BoneCacheEntryWorld& entry : g_boneCache)
	{
		if (!g_boneHideLines)
		{
			DrawChain(entry, spineChain, (int)(sizeof(spineChain) / sizeof(spineChain[0])));
			DrawChain(entry, leftArmChain, (int)(sizeof(leftArmChain) / sizeof(leftArmChain[0])));
			DrawChain(entry, rightArmChain, (int)(sizeof(rightArmChain) / sizeof(rightArmChain[0])));
			DrawChain(entry, leftLegChain, (int)(sizeof(leftLegChain) / sizeof(leftLegChain[0])));
			DrawChain(entry, rightLegChain, (int)(sizeof(rightLegChain) / sizeof(rightLegChain[0])));
		}

		if (g_boneDebugIds)
		{
			static const int knownBones[] = { 0, 4, 5, 6, 7, 8, 10, 12, 13, 15, 22, 24, 25, 27 };
			static const int extraBones[] = { 1, 2, 3, 9, 11, 14, 23, 26 };
			for (int id : knownBones)
			{
				if (!entry.b[id].valid) continue;
				Vec2 p2{};
				if (!WorldToScreen(view, entry.b[id].world, width, height, p2)) continue;
				char buf[8];
				sprintf_s(buf, "%d", id);
				drawList->AddCircleFilled(ImVec2(p2.x, p2.y), 3.0f, IM_COL32(255, 255, 0, 255));
				drawList->AddText(ImVec2(p2.x + 5, p2.y - 6), IM_COL32(255, 255, 0, 255), buf);
			}
			for (int id : extraBones)
			{
				if (!entry.b[id].valid) continue;
				Vec2 p2{};
				if (!WorldToScreen(view, entry.b[id].world, width, height, p2)) continue;
				char buf[8];
				sprintf_s(buf, "%d", id);
				drawList->AddCircleFilled(ImVec2(p2.x, p2.y), 4.0f, IM_COL32(0, 255, 0, 255));
				drawList->AddText(ImVec2(p2.x + 5, p2.y - 6), IM_COL32(0, 255, 0, 255), buf);
			}
		}
	}
}

static bool PremiumToggle(const char* label, bool* v)
{
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	
	float height = 20.0f;
	float width = 38.0f;
	float radius = height * 0.5f;

	ImGui::InvisibleButton(label, ImVec2(width, height));
	bool clicked = ImGui::IsItemClicked();
	if (clicked) *v = !*v;
	bool hovered = ImGui::IsItemHovered();

	// Анимация 
	ImGuiID id = ImGui::GetID(label);
	float* anim_ptr = ImGui::GetStateStorage()->GetFloatRef(id, *v ? 1.0f : 0.0f);
	float& anim = *anim_ptr;
	
	// Скорость анимации (12.0f = плавно, но быстро)
	anim += (*v ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime * 12.0f;
	if (anim < 0.0f) anim = 0.0f;
	if (anim > 1.0f) anim = 1.0f;

	ImVec4 col_off = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
	ImVec4 col_on = g_guiColor;
	
	// Интерполяция цвета от серого к цвету темы
	ImVec4 current_col = ImVec4(
		col_off.x + (col_on.x - col_off.x) * anim,
		col_off.y + (col_on.y - col_off.y) * anim,
		col_off.z + (col_on.z - col_off.z) * anim,
		1.0f
	);
	
	if (hovered) {
		current_col.x = (current_col.x + 0.1f > 1.0f) ? 1.0f : current_col.x + 0.1f;
		current_col.y = (current_col.y + 0.1f > 1.0f) ? 1.0f : current_col.y + 0.1f;
		current_col.z = (current_col.z + 0.1f > 1.0f) ? 1.0f : current_col.z + 0.1f;
	}

	// Фон свитча
	draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), ImGui::ColorConvertFloat4ToU32(current_col), radius);
	
	// Плавный сдвиг кружка
	float circle_x = p.x + radius + (width - radius * 2.0f) * anim;
	draw_list->AddCircleFilled(ImVec2(circle_x, p.y + radius), radius - 2.0f, IM_COL32(255, 255, 255, 255));
	
	// Мягкое свечение кружка
	if (anim > 0.0f) {
		draw_list->AddCircleFilled(ImVec2(circle_x, p.y + radius), radius + 2.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(current_col.x, current_col.y, current_col.z, 0.4f * anim)));
	}

	// Плавное изменение цвета текста
	ImVec4 text_col = ImVec4(
		0.5f + (current_col.x - 0.5f) * anim,
		0.5f + (current_col.y - 0.5f) * anim,
		0.5f + (current_col.z - 0.5f) * anim,
		1.0f
	);
	
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (height - ImGui::GetTextLineHeight()) * 0.5f);
	
	ImGui::PushStyleColor(ImGuiCol_Text, text_col);
	ImGui::TextUnformatted(label);
	ImGui::PopStyleColor();
	
	return clicked;
}

static void DrawSpectatorListImGui()
{
	// Функция включена? Если нет — вообще не рендерим
	if (!g_spectatorListEnabled) return;

	ImGui::SetNextWindowBgAlpha(0.85f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.6f));
	
	// Логика взаимодействия: если меню закрыто, окно прозрачно для кликов и зафиксировано
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
	if (!g_menuOpen) {
		windowFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
	}

	// Задаем минимальный размер, чтобы окно не "схлопывалось" до микро-размера, когда никого нет
	ImGui::SetNextWindowSizeConstraints(ImVec2(150, 0), ImVec2(300, 500));
	
	ImGui::Begin("Spectators", nullptr, windowFlags);
	
	ImGui::PushFont(ImGui::GetFont());
	ImGui::TextColored(g_guiColor, "SPECTATORS");
	ImGui::PopFont();
	ImGui::Separator();
	
	if (g_spectators.empty()) 
	{
		// Состояние "Пусто"
		ImGui::TextDisabled("No Spectators");
	} 
	else 
	{
		// Динамическое наполнение
		for (const auto& spec : g_spectators) {
			ImGui::Text("👀 %s", spec.c_str());
		}
	}
	
	ImGui::End();
	
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

// Premium Panel Functions - красивые карточки с заголовком
static void BeginPremiumChild(const char* str_id, const char* title, ImVec2 size) 
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.09f, 0.85f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 14));
	
	ImGui::BeginChild(str_id, size, true, ImGuiWindowFlags_NoScrollbar);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 w_pos = ImGui::GetWindowPos();
	float w_width = ImGui::GetWindowWidth();
	
	// Отрисовка красивого заголовка панели
	ImU32 headBgL = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x * 0.2f, g_guiColor.y * 0.2f, g_guiColor.z * 0.3f, 0.7f));
	ImU32 headBgR = ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.09f, 0.0f));
	
	// Градиент в шапке
	draw_list->AddRectFilledMultiColor(w_pos, ImVec2(w_pos.x + w_width, w_pos.y + 35.0f), headBgL, headBgR, headBgR, headBgL);
	
	// Акцентная линия снизу заголовка
	draw_list->AddLine(ImVec2(w_pos.x, w_pos.y + 35.0f), ImVec2(w_pos.x + w_width, w_pos.y + 35.0f), ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.5f)), 1.0f);

	// Текст заголовка
	ImGui::SetCursorPos(ImVec2(14, 10));
	ImGui::TextColored(g_guiColor, title);
	
	// Сдвигаем курсор для контента (чтобы не рисовалось на шапке)
	ImGui::SetCursorPos(ImVec2(14, 45)); 
}

static void EndPremiumChild() 
{
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);
}

// Векторная шестеренка (Gear) без файлов - всегда работает
static bool PremiumSettingsButton(const char* id)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(id, ImVec2(24, 24));
	bool hovered = ImGui::IsItemHovered();
	
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImU32 col = ImGui::GetColorU32(hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
	
	ImVec2 c(pos.x + 12, pos.y + 12);
	
	// Отрисовка идеальной шестеренки (Gear) математикой
	dl->AddCircle(c, 4.5f, col, 12, 2.0f); // Основное кольцо
	for (int i = 0; i < 6; i++) {
		float angle = i * (3.1415926f / 3.0f);
		dl->AddLine(
			ImVec2(c.x + cosf(angle) * 4.5f, c.y + sinf(angle) * 4.5f),
			ImVec2(c.x + cosf(angle) * 7.0f, c.y + sinf(angle) * 7.0f), 
			col, 2.5f
		); // Зубья
	}
	
	// Центральное отверстие (цвет фона)
	dl->AddCircleFilled(c, 2.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.06f, 0.07f, 1.0f))); 
	
	return clicked;
}

static void DrawMenuImGui()
{
	if (!g_menuOpen) return;

	// Авто-сохранение: сохраняем через 800мс после последнего изменения
	static bool  s_cfgDirty = false;
	static ULONGLONG s_cfgDirtyTs = 0;
	auto MarkDirty = [&]() { s_cfgDirty = true; s_cfgDirtyTs = GetTickCount64(); };
	if (s_cfgDirty && (GetTickCount64() - s_cfgDirtyTs > 800)) { SaveConfig(); s_cfgDirty = false; }

	ImGui::SetNextWindowBgAlpha(0.96f);

	ImGui::SetNextWindowSize(ImVec2(650.0f, 420.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("##midnight_menu", &g_menuOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetWindowPos();
	ImVec2 s_win = ImGui::GetWindowSize();
	
	// --- ПЛАВАЮЩИЕ ЧАСТИЦЫ НА ФОНЕ МЕНЮ ---
	{
		struct Particle { ImVec2 pos; ImVec2 vel; };
		static std::vector<Particle> particles;
		static bool initParticles = false;
		if (!initParticles) {
			for (int i = 0; i < 40; i++) {
				particles.push_back({
					ImVec2(p.x + rand() % (int)s_win.x, p.y + rand() % (int)s_win.y),
					ImVec2((rand() % 100 - 50) / 100.0f, (rand() % 100 - 50) / 100.0f)
				});
			}
			initParticles = true;
		}

		ImU32 particleColor = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.3f));
		ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.15f));

		for (auto& part : particles) {
			// Движение
			part.pos.x += part.vel.x * ImGui::GetIO().DeltaTime * 60.0f;
			part.pos.y += part.vel.y * ImGui::GetIO().DeltaTime * 60.0f;

			// Отражение от краев окна
			if (part.pos.x < p.x || part.pos.x > p.x + s_win.x) part.vel.x *= -1.0f;
			if (part.pos.y < p.y || part.pos.y > p.y + s_win.y) part.vel.y *= -1.0f;

			// Отрисовка точек
			drawList->AddCircleFilled(part.pos, 1.5f, particleColor);

			// Соединение линиями
			for (auto& other : particles) {
				float dist = std::hypot(part.pos.x - other.pos.x, part.pos.y - other.pos.y);
				if (dist < 60.0f && dist > 0.0f) {
					drawList->AddLine(part.pos, other.pos, lineColor, 1.0f);
				}
			}
		}
	}
	// --------------------------------------
	
	// Улучшенный Ultra-Premium Glow (мягкое объемное свечение вокруг окна)
	for (int i = 0; i < 12; ++i) {
		float alpha = 0.05f * std::pow(0.7f, (float)i); // Экспоненциальное затухание для крутой тени
		drawList->AddRect(
			ImVec2(p.x - i, p.y - i), ImVec2(p.x + s_win.x + i, p.y + s_win.y + i), 
			ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, alpha)), 
			8.0f + i, 0, 2.0f
		);
	}

	// Полная шапка с градиентом
	ImU32 hdrL = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x*0.4f, g_guiColor.y*0.3f, g_guiColor.z*0.8f, 1.0f));
	ImU32 hdrR = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 1.0f));
	drawList->AddRectFilledMultiColor(p, ImVec2(p.x + s_win.x, p.y + 36.0f), hdrL, hdrR, hdrR, hdrL);

	// Иконка + название
	ImGui::SetCursorPos(ImVec2(14, 10));
	ImGui::TextColored(ImVec4(1,1,1,1), "OXRANA LOUTABA CLIENT");

	// Версия справа
	ImGui::SetCursorPos(ImVec2(s_win.x - 50, 10));
	ImGui::TextColored(ImVec4(1,1,1,0.5f), "v2.0");

	ImGui::SetCursorPos(ImVec2(0, 36));
	ImGui::Separator();
	ImGui::SetCursorPosY(46);

	enum Tab : int { TAB_COMBAT = 0, TAB_ESP = 1, TAB_PLAYER = 2, TAB_MOVEMENT = 3, TAB_SKINS = 4, TAB_CONFIGS = 5, TAB_OTHER = 6 };
	static int activeTab = TAB_ESP;
	static int prevTab = -1;
	static float tabFadeAlpha = 1.0f;
	static float tabFadeStart = 0.0f;
	
	// Tab fade-in animation (0.2 seconds)
	if (prevTab != activeTab) {
		tabFadeStart = (float)ImGui::GetTime();
		prevTab = activeTab;
	}
	float fadeElapsed = (float)ImGui::GetTime() - tabFadeStart;
	tabFadeAlpha = ImMin(1.0f, fadeElapsed / 0.2f);

	const float btnH = 30.0f;
	ImGuiStyle& s = ImGui::GetStyle();
	ImVec4 base = ImGui::GetStyleColorVec4(ImGuiCol_Button);
	ImVec4 baseH = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
	ImVec4 baseA = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);

	auto TabButton = [&](const char* label, int tabId)
	{
		const bool isActive = (activeTab == tabId);
		float t = (float)ImGui::GetTime();
		
		ImVec4 col = isActive ? baseA : base;
		
		ImGui::BeginGroup();
		
		// Анимация при наведении
		if (isActive) {
			float pulse = 0.8f + 0.2f * sinf(t * 4.0f);
			col.w = pulse;
		}
		
		ImGui::PushStyleColor(ImGuiCol_Button, col);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isActive ? 1.0f : 0.0f);
		
		// Анимированная рамка для активной вкладки
		float borderGlow = 0.6f + 0.4f * sinf(t * 5.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(borderGlow, 0.3f, 0.9f, 1.0f));
		
		ImGui::PushID(tabId);
		if (ImGui::Button(label, ImVec2(-1.0f, btnH))) activeTab = tabId;
		
		// Эффект свечения при наведении
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) 
		{
			ImVec4 hoverCol = baseH;
			float hoverAnim = 0.5f + 0.5f * sinf(t * 8.0f);
			hoverCol.w = 0.8f + 0.2f * hoverAnim;
			
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 rmin = ImGui::GetItemRectMin();
			ImVec2 rmax = ImGui::GetItemRectMax();
			
			// Двойная рамка для эффекта свечения
			dl->AddRect(rmin, rmax, ImGui::ColorConvertFloat4ToU32(hoverCol), 6.0f, 0, 1.5f);
			ImVec2 rmin2 = ImVec2(rmin.x - 2, rmin.y - 2);
			ImVec2 rmax2 = ImVec2(rmax.x + 2, rmax.y + 2);
			dl->AddRect(rmin2, rmax2, ImGui::ColorConvertFloat4ToU32(ImVec4(hoverCol.x, hoverCol.y, hoverCol.z, hoverCol.w * 0.3f)), 6.0f, 0, 2.0f);
		}
		ImGui::PopID();

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
		
		ImGui::EndGroup();
	};

	// Три колонки: [Tabs 140px] | [Content flex] | [Preview 210px]
	// Ширина превью будет 0, если мы не во вкладке ESP
	float actualPreviewW = (activeTab == TAB_ESP) ? 210.0f : 0.0f;
	float contentW = ImGui::GetContentRegionAvail().x - 150.0f - actualPreviewW - 16.0f;

	ImGui::BeginChild("##left_tabs", ImVec2(140.0f, 0), true, ImGuiWindowFlags_None);
	TabButton("Combat", TAB_COMBAT);
	TabButton("Render", TAB_ESP);
	TabButton("Player", TAB_PLAYER);
	TabButton("Movement", TAB_MOVEMENT);
	TabButton("Skins", TAB_SKINS);
	TabButton("Configs", TAB_CONFIGS);
	TabButton("Other", TAB_OTHER);
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::BeginChild("##content", ImVec2(contentW, 0), false, ImGuiWindowFlags_None);
	
	// Apply fade-in animation to tab content
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tabFadeAlpha);

	if (activeTab == TAB_COMBAT)
	{
		// Панель 1: Aimbot
		BeginPremiumChild("##aimbot_panel", "Aimbot Settings", ImVec2(contentW, 180));
		PremiumToggle("Enable Aimbot", &g_aimbotEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		KeybindWidget("##aimbot_bind", g_aimbotKey);
		ImGui::SameLine();
		ImGui::SameLine();
		if (PremiumSettingsButton("##aimbot_gear_btn")) ImGui::OpenPopup("##aimbot_ctx");

		if (ImGui::BeginPopup("##aimbot_ctx"))
		{
			ImGui::TextUnformatted("Настройки Аимбота");
			ImGui::Separator();
			PremiumToggle("Проверка команды", &g_aimbotTeamCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Проверка видимости", &g_aimbotVisCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Автовыстрел", &g_aimbotAutoFire);
			if (ImGui::IsItemClicked()) MarkDirty();
			
			// АВТОСОХРАНЕНИЕ: сохраняем только после отпускания слайдера
			ImGui::SliderFloat("FOV", &g_aimbotFov, 1.0f, 20.0f, "%.1f");
			if (ImGui::IsItemDeactivatedAfterEdit()) { MarkDirty(); }
			
			// АВТОСОХРАНЕНИЕ: сохраняем только после отпускания слайдера
			ImGui::SliderFloat("Smooth", &g_aimbotSmooth, 1.0f, 10.0f, "%.1f");
			if (ImGui::IsItemDeactivatedAfterEdit()) { MarkDirty(); }
			
			const char* boneNames[] = { "Голова", "Шея", "Грудь", "Живот" };
			const int boneIds[] = { 6, 5, 4, 2 };
			int currentBoneIdx = 0;
			for (int i = 0; i < 4; i++) {
				if (g_aimbotBone == boneIds[i]) {
					currentBoneIdx = i;
					break;
				}
			}
			if (ImGui::Combo("Цель", &currentBoneIdx, boneNames, 4)) {
				g_aimbotBone = boneIds[currentBoneIdx];
				MarkDirty();
			}
			PremiumToggle("Показать FOV", &g_aimbotDrawFov);
			ImGui::ColorEdit4("Цвет FOV", (float*)&g_aimbotFovColor, ImGuiColorEditFlags_NoInputs);
			ImGui::EndPopup();
		}
		EndPremiumChild();

		ImGui::Spacing();

		// Панель 2: Triggerbot
		BeginPremiumChild("##trigger_panel", "Triggerbot", ImVec2(contentW, 140));
		PremiumToggle("Enable Triggerbot", &g_triggerEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		KeybindWidget("##trigger_bind", g_triggerKey);
		ImGui::SameLine();
		if (PremiumSettingsButton("##trigger_gear_btn")) ImGui::OpenPopup("##trigger_ctx");

		if (ImGui::BeginPopup("##trigger_ctx"))
		{
			ImGui::TextUnformatted("Настройки Триггербота");
			ImGui::Separator();
			PremiumToggle("Только в голову", &g_triggerHeadOnly);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Проверка команды", &g_triggerTeamCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Только с прицелом", &g_triggerScopeOnly);
			if (ImGui::IsItemClicked()) MarkDirty();
			PremiumToggle("Проверка видимости", &g_triggerVisCheck);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::EndPopup();
		}
		EndPremiumChild();

		ImGui::Spacing();

		// Панель 3: Accuracy
		BeginPremiumChild("##accuracy_panel", "Accuracy", ImVec2(contentW, 100));
		PremiumToggle("No Recoil", &g_noRecoilEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		PremiumToggle("No Spread", &g_noSpreadEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		EndPremiumChild();
		
		ImGui::Spacing();
	}
	else if (activeTab == TAB_ESP)
	{
		// Начинаем таблицу на 2 колонки
		if (ImGui::BeginTable("##esp_columns", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableNextRow();
			
			// --- КОЛОНКА 1 ---
			ImGui::TableSetColumnIndex(0);
			
			PremiumToggle("Boxes", &g_whEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##box_gear_btn")) ImGui::OpenPopup("##box_ctx");

			if (ImGui::BeginPopup("##box_ctx")) {
				ImGui::TextUnformatted("Настройки боксов");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_boxColor, ImGuiColorEditFlags_NoInputs);
				PremiumToggle("Dynamic HP Color", &g_dynamicBoxColor);
				if (ImGui::IsItemClicked()) MarkDirty();
				ImGui::SliderFloat("Отступы", &g_boxPadding, 0.0f, 10.0f, "%.1f");
				ImGui::SliderFloat("Толщина", &g_boxThickness, 0.1f, 5.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("HP Bar", &g_hpBarEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##hp_gear_btn")) ImGui::OpenPopup("##hp_ctx");

			if (ImGui::BeginPopup("##hp_ctx")) {
				ImGui::TextUnformatted("Настройки HP Bar");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_hpBarColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Ширина", &g_hpBarWidth, 1.0f, 10.0f, "%.1f");
				ImGui::SliderFloat("Отступ", &g_hpBarOffset, 0.0f, 20.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("Skeletons", &g_bonesEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##bone_gear_btn")) ImGui::OpenPopup("##bone_ctx");

			if (ImGui::BeginPopup("##bone_ctx")) {
				ImGui::TextUnformatted("Настройки скелета");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_boneColor, ImGuiColorEditFlags_NoInputs);
				ImGui::Checkbox("Debug: bone IDs", &g_boneDebugIds);
				ImGui::Checkbox("Hide lines", &g_boneHideLines);
				ImGui::EndPopup();
			}

			ImGui::Separator();

			// --- КОЛОНКА 2 ---
			ImGui::TableSetColumnIndex(1);
			
			PremiumToggle("Names", &g_nameEspEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##name_gear_btn")) ImGui::OpenPopup("##name_ctx");

			if (ImGui::BeginPopup("##name_ctx")) {
				ImGui::TextUnformatted("Настройки имён");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет", (float*)&g_nameColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Смещение Y", &g_nameOffsetY, 0.0f, 20.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("Weapons", &g_gunEspEnabled);
			ImGui::SameLine();
			if (PremiumSettingsButton("##weapon_gear_btn")) ImGui::OpenPopup("##weapon_ctx");

			if (ImGui::BeginPopup("##weapon_ctx")) {
				ImGui::TextUnformatted("Настройки оружия (текст)");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет текста", (float*)&g_weaponIconColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Смещение X", &g_gunOffsetX, -30.0f, 30.0f, "%.1f");
				ImGui::SliderFloat("Смещение Y", &g_gunOffsetY, 0.0f, 30.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			
			PremiumToggle("Chams (Glow)", &g_chamsEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##chams_gear_btn")) ImGui::OpenPopup("##chams_ctx");

			if (ImGui::BeginPopup("##chams_ctx")) {
				ImGui::TextUnformatted("Настройки Chams");
				ImGui::Separator();
				ImGui::ColorEdit4("Цвет T", (float*)&g_chamsColorT, ImGuiColorEditFlags_NoInputs);
				ImGui::ColorEdit4("Цвет CT", (float*)&g_chamsColorCT, ImGuiColorEditFlags_NoInputs);
				ImGui::EndPopup();
			}

			// Продолжаем ПЕРВУЮ КОЛОНКУ (добавляем туда элементы)
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			
			PremiumToggle("Snaplines", &g_snaplinesEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			
			ImGui::Separator();
			PremiumToggle("Distance", &g_distanceEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();

			ImGui::Separator();
			PremiumToggle("Custom Crosshair", &g_customCrosshair);

			ImGui::Separator();
			PremiumToggle("Damage Indicators", &g_damageIndicatorsEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##dmg_gear_btn")) ImGui::OpenPopup("##dmg_ctx");
			if (ImGui::BeginPopup("##dmg_ctx")) {
				ImGui::ColorEdit4("Цвет урона", (float*)&g_damageColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Размер текста", &g_damageTextSize, 10.0f, 40.0f, "%.1f");
				ImGui::SliderFloat("Время (сек)", &g_damageTextLifetime, 1.0f, 10.0f, "%.1f");
				ImGui::EndPopup();
			}

			// --- ВТОРАЯ КОЛОНКА ---
			ImGui::TableSetColumnIndex(1);
			
			PremiumToggle("Radar Hack (Native)", &g_radarHackEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			
			ImGui::Separator();
			PremiumToggle("Out of FOV Arrows", &g_oofArrowsEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();
			ImGui::SameLine();
			if (PremiumSettingsButton("##oof_gear_btn")) ImGui::OpenPopup("##oof_ctx");
			if (ImGui::BeginPopup("##oof_ctx")) {
				ImGui::ColorEdit4("Цвет стрелок", (float*)&g_oofArrowsColor, ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Радиус", &g_oofArrowsRadius, 50.0f, 500.0f, "%.0f");
				ImGui::SliderFloat("Размер", &g_oofArrowsSize, 10.0f, 30.0f, "%.1f");
				ImGui::EndPopup();
			}

			ImGui::Separator();
			PremiumToggle("Bomb ESP (Timer)", &g_bombEspEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();

			ImGui::Separator();
			PremiumToggle("Spectator List", &g_spectatorListEnabled);
			if (ImGui::IsItemClicked()) MarkDirty();

			ImGui::EndTable(); // ТЕПЕРЬ ТАБЛИЦА ЗАКРЫВАЕТСЯ ЗДЕСЬ
		}

		// Эти можно оставить под таблицей
		ImGui::Spacing();
		PremiumToggle("Keybinds List", &g_keybindsListEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		
		ImGui::Separator();
		ImGui::Spacing();
	}
	else if (activeTab == TAB_PLAYER)
	{
		PremiumToggle("Anti-Flash", &g_antiFlashEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		
		PremiumToggle("No Smoke", &g_noSmokeEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		ImGui::TextDisabled("(Removes smoke grenades)");
		
		PremiumToggle("Hit Sound & Hitmarker", &g_hitSoundEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		ImGui::TextDisabled("(Plays sound on hit)");
		
		ImGui::Separator();
		
		if (PremiumToggle("FOV Override", &g_fovEnabled))
		{
			MarkDirty();
		}
		ImGui::SliderFloat("FOV Value", &g_fovValue, 60.0f, 140.0f, "%.1f");
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			MarkDirty();
		}
		
		ImGui::Separator();
		
		// Stream Proof / Anti-Capture
		if (PremiumToggle("Anti-Capture (OBS/Discord)", &g_antiCaptureEnabled))
		{
			MarkDirty();
			// Применяем сразу
			if (g_hOverlayWnd) {
				SetWindowDisplayAffinity(g_hOverlayWnd, g_antiCaptureEnabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
			}
		}
		ImGui::TextDisabled("Скрывает меню и ESP от захвата экрана");
	}
	else if (activeTab == TAB_MOVEMENT)
	{
		PremiumToggle("Bhop", &g_bhopEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::SameLine();
		KeybindWidget("##bhop_bind", g_bhopKey);
		PremiumToggle("Auto-Strafe (ADAD sync)", &g_autoStrafeEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		ImGui::TextDisabled("Auto-Strafe работает совместно с Bhop");
	}
	else if (activeTab == TAB_SKINS)
	{
		if (!g_skinWarningShown) {
			ImGui::OpenPopup("##skin_warning");
		}
		
		ImGui::SetNextWindowSize(ImVec2(650.0f, 420.0f));
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("##skin_warning", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
			ImDrawList* pdl = ImGui::GetWindowDrawList();
			ImVec2 wp = ImGui::GetWindowPos();
			ImVec2 ws = ImGui::GetWindowSize();
			
			// Та же градиентная линия сверху как у основного меню
			ImU32 col1 = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 1.0f));
			ImU32 col2 = ImGui::ColorConvertFloat4ToU32(ImVec4(g_guiColor.x * 0.5f, g_guiColor.y * 0.3f, 1.0f, 1.0f));
			pdl->AddRectFilledMultiColor(wp, ImVec2(wp.x + ws.x, wp.y + 4.0f), col1, col2, col2, col1);
			
			ImGui::SetCursorPos(ImVec2(18, 20));
			ImGui::TextColored(g_guiColor, "OXRANALOUTABA CLIENT — Skin Changer");
			ImGui::SetCursorPosY(45);
			ImGui::Separator();
			
			ImGui::SetCursorPos(ImVec2(18, 70));
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Предупреждение");
			ImGui::Spacing();
			ImGui::SetCursorPosX(18);
			ImGui::TextWrapped("Скины применяются через FallbackPaintKit — это клиентский метод.");
			ImGui::Spacing();
			ImGui::SetCursorPosX(18);
			ImGui::TextWrapped("UV-развёртка на некоторых скинах может отображаться некорректно. Это ожидаемое поведение данного метода, а не баг.");
			ImGui::Spacing();
			ImGui::SetCursorPosX(18);
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Это сообщение показывается только один раз.");
			
			ImGui::SetCursorPos(ImVec2(ws.x * 0.5f - 60.0f, ws.y - 55.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, g_guiColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(g_guiColor.x + 0.1f, g_guiColor.y + 0.1f, g_guiColor.z + 0.1f, 1.0f));
			if (ImGui::Button("Хорошо", ImVec2(120.0f, 36.0f))) {
				g_skinWarningShown = true;
				SaveConfig();
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor(2);
			
			ImGui::EndPopup();
		}
		
		PremiumToggle("Enable Skin Changer", &g_skinChangerEnabled);
		if (ImGui::IsItemClicked()) MarkDirty();
		
		ImGui::Separator();
		ImGui::Text("Скины (Обновляются моментально!):");
		ImGui::Spacing();

		// === TEC-9 ===
		const char* tec9Skins[] = { "Default", "Decimator", "Fuel Injector", "Remote Control", "Isaac", "Toxic", "Avalanche", "Re-Entry", "Brother" };
		const int tec9PaintKits[] = { 0, 644, 614, 791, 303, 374, 520, 539, 1099 };
		static int tec9SkinIdx = 0;
		if (ImGui::Combo("Tec-9", &tec9SkinIdx, tec9Skins, IM_ARRAYSIZE(tec9Skins))) { g_skinConfig[30] = tec9PaintKits[tec9SkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === USP-S ===
		const char* uspSkins[] = { "Default", "Printstream", "The Traitor", "Neo-Noir", "Kill Confirmed", "Jawbreaker", "Monster Mashup", "Caiman", "Serum", "Orion", "Whiteout", "Target Acquired", "Ticket to Hell", "Cortex" };
		const int uspPaintKits[] = { 0, 1142, 1040, 653, 504, 1173, 991, 339, 221, 313, 1065, 1027, 1146, 705 };
		static int uspSkinIdx = 0;
		if (ImGui::Combo("USP-S", &uspSkinIdx, uspSkins, IM_ARRAYSIZE(uspSkins))) { g_skinConfig[61] = uspPaintKits[uspSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === GLOCK-18 ===
		const char* glockSkins[] = { "Default", "Twilight Galaxy", "Vogue", "Water Elemental", "Snack Attack", "Gamma Doppler Emerald", "Wasteland Rebel", "Bullet Queen", "Neo-Noir", "Fade", "Moonrise", "Nuclear Garden" };
		const int glockPaintKits[] = { 0, 437, 963, 353, 1100, 1119, 586, 957, 988, 38, 707, 536 };
		static int glockSkinIdx = 0;
		if (ImGui::Combo("Glock-18", &glockSkinIdx, glockSkins, IM_ARRAYSIZE(glockSkins))) { g_skinConfig[4] = glockPaintKits[glockSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === AK-47 ===
		const char* akSkins[] = { "Default", "Nightwish", "Leet Museo", "Legion of Anubis", "Asiimov", "Neon Rider", "The Empress", "Bloodsport", "Neon Revolution", "Fuel Injector", "Aquamarine Revenge", "Wasteland Rebel", "Jaguar", "Vulcan", "Fire Serpent", "Gold Arabesque", "X-Ray", "Wild Lotus", "Ice Coaled", "Phantom Disruptor", "Point Disarray", "Frontside Misty", "Cartel", "Redline", "Case Hardened", "Red Laminate", "Panthera onca", "Hydroponic", "Jet Set" };
		const int akPaintKits[] = { 0, 1141, 1087, 959, 551, 433, 675, 597, 600, 524, 474, 380, 316, 302, 180, 1026, 1004, 724, 1143, 941, 506, 490, 528, 282, 44, 14, 1018, 456, 340 };
		static int akSkinIdx = 0;
		if (ImGui::Combo("AK-47", &akSkinIdx, akSkins, IM_ARRAYSIZE(akSkins))) { g_skinConfig[7] = akPaintKits[akSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === AWP ===
		const char* awpSkins[] = { "Default", "Printstream", "Chromatic Aberration", "Containment Breach", "Wildfire", "Neo-Noir", "Oni Taiji", "Hyper Beast", "Man-o'-war", "Asiimov", "Lightning Strike", "Desert Hydra", "Fade", "The Prince", "Gungnir", "Medusa", "Dragon Lore", "Ice Coaled", "Mortis", "Fever Dream", "Elite Build", "Corticera", "Redline", "Electric Hive", "Graphite", "BOOM", "Silk Tiger" };
		const int awpPaintKits[] = { 0, 1144, 1120, 887, 917, 803, 662, 475, 395, 279, 51, 1058, 1022, 736, 756, 446, 344, 1143, 691, 640, 525, 181, 259, 227, 212, 174, 1029 };
		static int awpSkinIdx = 0;
		if (ImGui::Combo("AWP", &awpSkinIdx, awpSkins, IM_ARRAYSIZE(awpSkins))) { g_skinConfig[9] = awpPaintKits[awpSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === FAMAS ===
		const char* famasSkins[] = { "Default", "Commemoration", "Roll Cage", "Rapid Eye Movement", "Eye of Athena", "Mecha Industries", "Djinn", "Afterimage", "Waters of Nephthys", "Meltdown", "Valence" };
		const int famasPaintKits[] = { 0, 919, 604, 1127, 723, 587, 429, 154, 1128, 1053, 529 };
		static int famasSkinIdx = 0;
		if (ImGui::Combo("FAMAS", &famasSkinIdx, famasSkins, IM_ARRAYSIZE(famasSkins))) { g_skinConfig[10] = famasPaintKits[famasSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === GALIL-AR ===
		const char* galilSkins[] = { "Default", "Chatterbox", "Chromatic Aberration", "Sugar Rush", "Eco", "Cerberus", "Rocket Pop" };
		const int galilPaintKits[] = { 0, 398, 1144, 661, 428, 379, 478 };
		static int galilSkinIdx = 0;
		if (ImGui::Combo("Galil-AR", &galilSkinIdx, galilSkins, IM_ARRAYSIZE(galilSkins))) { g_skinConfig[13] = galilPaintKits[galilSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === M4A1-S ===
		const char* m4a1sSkins[] = { "Default", "Printstream", "Player Two", "Mecha Industries", "Chantico's Fire", "Golden Coil", "Hyper Beast", "Cyrex", "Fade", "Imminent Danger", "Welcome to the Jungle", "Black Lotus", "Nightmare", "Leaded Glass", "Decimator", "Atomic Alloy", "Guardian", "Blue Phosphor", "Control Panel", "Hot Rod", "Master Piece", "Knight" };
		const int m4a1sPaintKits[] = { 0, 984, 946, 587, 548, 497, 430, 312, 1041, 1073, 1001, 1102, 714, 681, 644, 301, 257, 1017, 792, 445, 321, 326 };
		static int m4a1sSkinIdx = 0;
		if (ImGui::Combo("M4A1-S", &m4a1sSkinIdx, m4a1sSkins, IM_ARRAYSIZE(m4a1sSkins))) { g_skinConfig[60] = m4a1sPaintKits[m4a1sSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === M4A4 ===
		const char* m4a4Skins[] = { "Default", "Howl", "In Living Color", "The Emperor", "Neo-Noir", "Buzz Kill", "The Battlestar", "Royal Paladin", "Bullet Rain", "Desert-Strike", "Asiimov", "X-Ray", "The Coalition", "Cyber Security", "Tooth Fairy", "Hellfire", "Desolate Space", "Dragon King", "Poseidon" };
		const int m4a4PaintKits[] = { 0, 309, 1041, 844, 695, 632, 533, 512, 155, 336, 255, 215, 1063, 985, 971, 664, 588, 400, 449 };
		static int m4a4SkinIdx = 0;
		if (ImGui::Combo("M4A4", &m4a4SkinIdx, m4a4Skins, IM_ARRAYSIZE(m4a4Skins))) { g_skinConfig[16] = m4a4PaintKits[m4a4SkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === SSG-08 ===
		const char* ssgSkins[] = { "Default", "Dragonfire", "Blood in the Water", "Turbo Peek", "Bloodshot", "Big Iron", "Death Strike" };
		const int ssgPaintKits[] = { 0, 624, 222, 1101, 899, 503, 1052 };
		static int ssgSkinIdx = 0;
		if (ImGui::Combo("SSG-08", &ssgSkinIdx, ssgSkins, IM_ARRAYSIZE(ssgSkins))) { g_skinConfig[40] = ssgPaintKits[ssgSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		// === DESERT EAGLE ===
		const char* deagleSkins[] = { "Default", "Ocean Drive", "Printstream", "Code Red", "Golden Koi", "Mecha Industries", "Kumicho Dragon", "Conspiracy", "Cobalt Disruption", "Hypnotic", "Fennec Fox" };
		const int deaglePaintKits[] = { 0, 1090, 984, 711, 185, 587, 527, 351, 231, 61, 1051 };
		static int deagleSkinIdx = 0;
		if (ImGui::Combo("Desert Eagle", &deagleSkinIdx, deagleSkins, IM_ARRAYSIZE(deagleSkins))) { g_skinConfig[1] = deaglePaintKits[deagleSkinIdx]; g_skinUpdateCounter++; MarkDirty(); }

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Агенты, Ножи и Перчатки отключены для 100% стабильности (Anti-Crash).");
	}
	else if (activeTab == TAB_CONFIGS)
	{
		ImGui::TextUnformatted("Config Manager");
		ImGui::Separator();
		
		// Get DLL directory for configs
		static std::string s_dllDir;
		static std::vector<std::string> s_configFiles;
		static int s_selectedConfig = 0;
		static ULONGLONG s_lastRefresh = 0;
		
		// Refresh config list periodically
		ULONGLONG now = GetTickCount64();
		if (s_dllDir.empty() || now - s_lastRefresh > 2000) {
			if (GetDllDirA(s_dllDir)) {
				s_configFiles.clear();
				
				std::string searchPath = s_dllDir + "\\*.ini";
				WIN32_FIND_DATAA findData;
				HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
				if (hFind != INVALID_HANDLE_VALUE) {
					do {
						if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
							s_configFiles.push_back(findData.cFileName);
						}
					} while (FindNextFileA(hFind, &findData));
					FindClose(hFind);
				}
			}
			s_lastRefresh = now;
		}
		
		// Config list
		ImGui::Text("Available Configs:");
		if (ImGui::BeginListBox("##configs_list", ImVec2(-1, 200))) {
			for (int i = 0; i < (int)s_configFiles.size(); i++) {
				bool isSelected = (s_selectedConfig == i);
				if (ImGui::Selectable(s_configFiles[i].c_str(), isSelected)) {
					s_selectedConfig = i;
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndListBox();
		}
		
		ImGui::Spacing();
		
		// Action buttons
		if (ImGui::Button("Load Selected", ImVec2(120, 0))) {
			if (s_selectedConfig >= 0 && s_selectedConfig < (int)s_configFiles.size()) {
				// Copy selected config to config.ini
				std::string srcPath = s_dllDir + "\\" + s_configFiles[s_selectedConfig];
				std::string dstPath = s_dllDir + "\\config.ini";
				CopyFileA(srcPath.c_str(), dstPath.c_str(), FALSE);
				LoadConfig();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Save As...", ImVec2(120, 0))) {
			static char newConfigName[64] = "new_config.ini";
			ImGui::OpenPopup("Save Config As");
		}
		
		// Save As popup
		if (ImGui::BeginPopupModal("Save Config As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			static char newConfigName[64] = "new_config.ini";
			ImGui::InputText("Filename", newConfigName, sizeof(newConfigName));
			ImGui::Spacing();
			if (ImGui::Button("Save", ImVec2(100, 0))) {
				std::string srcPath = s_dllDir + "\\config.ini";
				std::string dstPath = s_dllDir + "\\" + std::string(newConfigName);
				SaveConfig();
				CopyFileA(srcPath.c_str(), dstPath.c_str(), FALSE);
				s_lastRefresh = 0; // Force refresh
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Configs are .ini files in the DLL folder.");
		ImGui::TextDisabled("Select a config and click Load to switch.");
	}
	else if (activeTab == TAB_OTHER)
	{
		ImGui::ColorEdit4("Menu Color", (float*)&g_guiColor, ImGuiColorEditFlags_NoInputs);
		if (ImGui::IsItemDeactivatedAfterEdit())
			{
				ApplyClientStyle();
				SaveConfig();
			}
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Config is saved automatically.");
		
		ImGui::Spacing();
		if (ImGui::Button("Unload Cheat")) {
			// Здесь можно добавить логику выгрузки
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::SetWindowFontScale(1.1f);
	ImGui::TextUnformatted("END - Unload DLL");
	ImGui::SetWindowFontScale(1.0f);

	// End fade-in animation
	ImGui::PopStyleVar();

	ImGui::EndChild();
	
	// PREVIEW PANEL
	if (activeTab == TAB_ESP) {
		ImGui::SameLine();
		ImGui::BeginChild("##right_preview", ImVec2(actualPreviewW, 0), true, ImGuiWindowFlags_None);
		ImGui::TextDisabled("Preview");
		ImGui::Separator();
		
		ImGui::TextUnformatted("ESP Settings");
		ImGui::Spacing();
		ImVec2 prevSize(180.0f, 170.0f); // 1. УВЕЛИЧИЛИ ВЫСОТУ ДО 170
		ImGui::BeginChild("##esp_preview_inner", prevSize, true, ImGuiWindowFlags_NoScrollbar);
		{
			ImDrawList* pdl = ImGui::GetWindowDrawList();
			ImVec2 wp = ImGui::GetWindowPos();
			ImVec2 ws = ImGui::GetWindowSize();
			
			pdl->AddRectFilled(wp, ImVec2(wp.x+ws.x, wp.y+ws.y), IM_COL32(15,15,25,255)); // Фон
			
			float cx = wp.x + ws.x * 0.5f;
			float cy = wp.y + ws.y * 0.5f + 5.0f; // Чуть подняли центр
			float bw = 40.0f, bh = 85.0f;
			float bl = cx - bw*0.5f, bt = cy - bh*0.5f;

			// 2. РИСУЕМ GLOW (если включены Chams) ПЕРЕД боксами
			if (g_chamsEnabled) {
				ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(g_chamsColorT.x, g_chamsColorT.y, g_chamsColorT.z, 0.3f));
				// Эффект свечения (несколько полупрозрачных прямоугольников)
				for (int i = 1; i <= 4; i++) {
					pdl->AddRect(ImVec2(bl - i, bt - i), ImVec2(bl + bw + i, bt + bh + i), glowCol, 0.0f, 0, 1.0f);
				}
				// Заливка
				pdl->AddRectFilled(ImVec2(bl, bt), ImVec2(bl + bw, bt + bh), ImGui::ColorConvertFloat4ToU32(ImVec4(g_chamsColorT.x, g_chamsColorT.y, g_chamsColorT.z, 0.15f)));
			}

			// Bounding Box
			if (g_whEnabled) pdl->AddRect(ImVec2(bl,bt), ImVec2(bl+bw,bt+bh), ImGui::ColorConvertFloat4ToU32(g_boxColor), 0.0f, 0, g_boxThickness);
			
			// HP Bar
			if (g_hpBarEnabled) {
				pdl->AddRectFilled(ImVec2(bl-8,bt), ImVec2(bl-4,bt+bh), IM_COL32(0,0,0,180));
				pdl->AddRectFilled(ImVec2(bl-7,bt+bh*0.25f+1), ImVec2(bl-5,bt+bh-1), IM_COL32(80,220,80,255));
			}
			
			// Skeletons
			if (g_bonesEnabled) {
				ImU32 bc = ImGui::ColorConvertFloat4ToU32(g_boneColor);
				pdl->AddLine(ImVec2(cx,bt+5),  ImVec2(cx,bt+45), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+20), ImVec2(bl,bt+40), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+20), ImVec2(bl+bw,bt+40), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+45), ImVec2(bl+5,bt+bh), bc, 1.5f);
				pdl->AddLine(ImVec2(cx,bt+45), ImVec2(bl+bw-5,bt+bh), bc, 1.5f);
				pdl->AddCircleFilled(ImVec2(cx,bt+5), 5.0f, bc);
			}
			
			// Name (3. ПОДНЯЛИ ЧУТЬ ВЫШЕ)
			if (g_nameEspEnabled)
				pdl->AddText(ImVec2(bl - 5.0f, bt - 18.0f), ImGui::ColorConvertFloat4ToU32(g_nameColor), "Player");
				
			// Weapon (4. ОПУСТИЛИ ЧУТЬ НИЖЕ)
			if (g_gunEspEnabled)
				pdl->AddText(ImVec2(bl, bt + bh + 4.0f), ImGui::ColorConvertFloat4ToU32(g_weaponIconColor), "AK-47");
		}
		ImGui::EndChild();
		
		ImGui::EndChild();
	}
	
	ImGui::End();
}



static void UpdateAntiFlash(bool cs2Active)
{
	if (!cs2Active || !g_antiFlashEnabled)
		return;

	using namespace cs2_dumper;
	using namespace cs2_dumper::schemas::client_dll;

	uintptr_t client = GetClientBase();
	if (!client)
		return;

	__try
	{
		uintptr_t localPawn = 0;
		if (!TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn))
			return;
		if (!localPawn)
			return;
		(void)TryWrite<float>(localPawn + C_CSPlayerPawnBase::m_flFlashDuration, 0.0f);
		(void)TryWrite<float>(localPawn + C_CSPlayerPawnBase::m_flFlashMaxAlpha, 0.0f);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

static void UpdateNoSmoke(bool cs2Active)
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

static bool KeybindWidget(const char* id, int& vk)
{
	ImGui::PushID(id);
	static ImGuiID s_capturing = 0;
	ImGuiID my = ImGui::GetID("keybind");

	bool changed = false;
	const bool capturing = (s_capturing == my);
	
	// Визуализация
	char buf[32]{};
	if (capturing)
		lstrcpyA(buf, "Press any key...");
	else if (vk == 0)
		lstrcpyA(buf, "[ None ]");
	else
		wsprintfA(buf, "[ %s ]", VkToStringA(vk));

	ImVec4 col = capturing ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	
	if (ImGui::Button(buf, ImVec2(110.0f, 0.0f)))
	{
		s_capturing = my; // Начинаем слушать ввод по клику
	}
	ImGui::PopStyleColor();

	// Обработка ввода если виджет активен
	if (capturing)
	{
		// Проверяем ESC для сброса
		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
		{
			vk = 0;
			s_capturing = 0;
			changed = true;
			SaveConfig(); // Автосохранение
			
			// Ждем пока отпустят кнопку чтобы не мигало
			while(GetAsyncKeyState(VK_ESCAPE) & 0x8000) Sleep(1);
		}
		// Проверяем Shift для сброса (как ты просил)
		else if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
		{
			// Если нажали Shift во время бинда - считаем это сбросом? 
			// Или можно биндить Shift. Сделаем как просил: Shift очищает.
			vk = 0;
			s_capturing = 0;
			changed = true;
			SaveConfig();
		}
		else
		{
			// Перебор всех клавиш
			for (int k = 1; k < 255; ++k)
			{
				if (k == VK_ESCAPE) continue;
				
				// Пропускаем клик ЛКМ, которым мы активировали кнопку (ждем пока отпустят)
				if (k == VK_LBUTTON && ImGui::IsMouseDown(0)) continue;

				// Проверка нажатия (async state)
				if (GetAsyncKeyState(k) & 0x8000)
				{
					vk = k;
					s_capturing = 0; // Перестаем слушать
					changed = true;
					SaveConfig(); // Автосохранение
					break;
				}
			}
		}
	}

	ImGui::PopID();
	return changed;
}

static const char* VkToStringA(int vk)
{
	static char buf[64];
	buf[0] = '\0';
	if (vk == 0) return "[none]";

	UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
	LONG lParam = (LONG)(scan << 16);
	if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME || vk == VK_INSERT || vk == VK_DELETE)
		lParam |= 1 << 24;

	int n = GetKeyNameTextA(lParam, buf, (int)sizeof(buf));
	if (n > 0) return buf;
	wsprintfA(buf, "VK_%d", vk);
	return buf;
}

static void DrawKeybindsListImGui() {
    if (!g_keybindsListEnabled) return;
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;
    if (!g_menuOpen) {
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    }
    
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    
    ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
    ImGui::Begin("KeybindsList", nullptr, flags);
    
    ImGui::TextColored(g_guiColor, "Keybinds");
    ImGui::Separator();
    
    // Aimbot
    if (g_aimbotEnabled) {
        bool active = (GetAsyncKeyState(g_aimbotKey) & 0x8000) != 0;
        ImGui::TextColored(active ? g_guiColor : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Aimbot [%s]", VkToStringA(g_aimbotKey));
        if (active) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        }
    }
    
    // Triggerbot
    if (g_triggerEnabled) {
        bool active = (GetAsyncKeyState(g_triggerKey) & 0x8000) != 0;
        ImGui::TextColored(active ? g_guiColor : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Triggerbot [%s]", VkToStringA(g_triggerKey));
        if (active) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        }
    }
    
    // Bhop
    if (g_bhopEnabled) {
        bool active = (GetAsyncKeyState(g_bhopKey) & 0x8000) != 0;
        ImGui::TextColored(active ? g_guiColor : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Bhop [%s]", VkToStringA(g_bhopKey));
        if (active) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

static void ApplyClientStyle()
{
	ImGuiStyle& s = ImGui::GetStyle();
	
	// Современная геометрия
	s.WindowPadding = ImVec2(16, 16);
	s.FramePadding = ImVec2(10, 6);
	s.ItemSpacing = ImVec2(10, 10);
	s.ItemInnerSpacing = ImVec2(8, 6);
	
	// Скругления (Стиль macOS/Modern UI)
	s.WindowRounding = 10.0f; 
	s.ChildRounding = 8.0f;
	s.FrameRounding = 8.0f;
	s.PopupRounding = 8.0f;
	s.ScrollbarRounding = 12.0f;
	s.GrabMinSize = 10.0f; // Тонкий ползунок слайдера
	s.GrabRounding = 8.0f; // Круглый ползунок
	
	// Бордеры (Минимализм)
	s.WindowBorderSize = 0.0f;
	s.ChildBorderSize = 1.0f;
	s.FrameBorderSize = 0.0f;
	s.PopupBorderSize = 1.0f;

	ImVec4* c = s.Colors;
	
	// Deep Dark Theme с фиолетово-синим акцентом
	c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.97f);
	c[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.09f, 0.60f);
	c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
	
	c[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
	c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	
	c[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

	// Элементы управления
	c[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
	c[ImGuiCol_ButtonActive] = g_guiColor;

	// Инпуты (темнее фона для глубины)
	c[ImGuiCol_FrameBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.00f);
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

	// Акценты
	c[ImGuiCol_CheckMark] = g_guiColor;
	c[ImGuiCol_SliderGrab] = g_guiColor;
	c[ImGuiCol_SliderGrabActive] = ImVec4(g_guiColor.x + 0.1f, g_guiColor.y + 0.1f, g_guiColor.z + 0.1f, 1.0f);
	
	// Хедеры
	c[ImGuiCol_Header] = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.30f);
	c[ImGuiCol_HeaderHovered] = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.60f);
	c[ImGuiCol_HeaderActive] = g_guiColor;
	
	// Сепараторы
	c[ImGuiCol_Separator] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
	c[ImGuiCol_SeparatorHovered] = g_guiColor;
	c[ImGuiCol_SeparatorActive] = g_guiColor;
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.75f);
	
	// Табы
	s.TabRounding = 6.0f;
	s.TabBorderSize = 0.0f;
	c[ImGuiCol_Tab]                = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
	c[ImGuiCol_TabHovered]         = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.5f);
	c[ImGuiCol_TabActive]          = ImVec4(g_guiColor.x, g_guiColor.y, g_guiColor.z, 0.9f);
	c[ImGuiCol_TitleBg]            = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);
	c[ImGuiCol_TitleBgActive]      = ImVec4(g_guiColor.x*0.5f, g_guiColor.y*0.4f, g_guiColor.z*0.9f, 1.0f);
	s.ItemSpacing                  = ImVec2(8, 7);
	s.IndentSpacing                = 16.0f;
}


static bool CreateDeviceD3D(HWND hWnd)
{
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		featureLevelArray,
		2,
		D3D11_SDK_VERSION,
		&g_pd3dDevice,
		&featureLevel,
		&g_pd3dDeviceContext);

	if (hr == DXGI_ERROR_UNSUPPORTED)
	{
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0,
			featureLevelArray,
			2,
			D3D11_SDK_VERSION,
			&g_pd3dDevice,
			&featureLevel,
			&g_pd3dDeviceContext);
	}

	if (FAILED(hr)) return false;

	IDXGIDevice* dxgiDevice = nullptr;
	IDXGIAdapter* adapter = nullptr;
	IDXGIFactory2* factory2 = nullptr;

	hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
	if (FAILED(hr) || !dxgiDevice) return false;

	hr = dxgiDevice->GetAdapter(&adapter);
	if (FAILED(hr) || !adapter) { dxgiDevice->Release(); return false; }

	hr = adapter->GetParent(IID_PPV_ARGS(&factory2));
	if (FAILED(hr) || !factory2) { adapter->Release(); dxgiDevice->Release(); return false; }

	DXGI_SWAP_CHAIN_DESC1 scd{};
	RECT rc{};
	GetClientRect(hWnd, &rc);
	UINT w = static_cast<UINT>((rc.right > rc.left) ? (rc.right - rc.left) : 1);
	UINT h = static_cast<UINT>((rc.bottom > rc.top) ? (rc.bottom - rc.top) : 1);

	scd.Width = w;
	scd.Height = h;
	scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.Stereo = FALSE;
	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	scd.Scaling = DXGI_SCALING_STRETCH;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
	scd.Flags = 0;

	hr = factory2->CreateSwapChainForComposition(g_pd3dDevice, &scd, nullptr, &g_pSwapChain);
	if (FAILED(hr) || !g_pSwapChain)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&g_dcompDevice));
	if (FAILED(hr) || !g_dcompDevice)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompDevice->CreateTargetForHwnd(hWnd, TRUE, &g_dcompTarget);
	if (FAILED(hr) || !g_dcompTarget)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompDevice->CreateVisual(&g_dcompVisual);
	if (FAILED(hr) || !g_dcompVisual)
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompVisual->SetContent(g_pSwapChain);
	if (FAILED(hr))
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompTarget->SetRoot(g_dcompVisual);
	if (FAILED(hr))
	{
		factory2->Release();
		adapter->Release();
		dxgiDevice->Release();
		return false;
	}

	hr = g_dcompDevice->Commit();

	factory2->Release();
	adapter->Release();
	dxgiDevice->Release();

	if (FAILED(hr)) return false;

	CreateRenderTarget();
	return true;
}

static void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer = nullptr;
	if (g_pSwapChain && SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))) && pBackBuffer)
	{
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}
}

static void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_dcompVisual) { g_dcompVisual->Release(); g_dcompVisual = nullptr; }
	if (g_dcompTarget) { g_dcompTarget->Release(); g_dcompTarget = nullptr; }
	if (g_dcompDevice) { g_dcompDevice->Release(); g_dcompDevice = nullptr; }
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}


static bool LoadTextureFromResource(int resourceId, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
	if (!out_srv || !g_pd3dDevice || !g_pd3dDeviceContext)
		return false;

	*out_srv = nullptr;
	if (out_width) *out_width = 0;
	if (out_height) *out_height = 0;

	// Загружаем ресурс
	HRSRC hResource = FindResourceW(g_hModule, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
	if (!hResource)
		return false;

	HGLOBAL hMemory = LoadResource(g_hModule, hResource);
	if (!hMemory)
		return false;

	DWORD dwSize = SizeofResource(g_hModule, hResource);
	LPVOID lpAddress = LockResource(hMemory);
	if (!lpAddress || dwSize == 0)
		return false;

	// Создаем IStream из памяти
	IStream* stream = nullptr;
	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwSize);
	if (!hGlobal)
		return false;

	void* pGlobal = GlobalLock(hGlobal);
	if (!pGlobal)
	{
		GlobalFree(hGlobal);
		return false;
	}

	memcpy(pGlobal, lpAddress, dwSize);
	GlobalUnlock(hGlobal);

	HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &stream);
	if (FAILED(hr) || !stream)
	{
		GlobalFree(hGlobal);
		return false;
	}

	// Создаем WIC factory
	IWICImagingFactory* factory = nullptr;
	hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (FAILED(hr) || !factory)
	{
		stream->Release();
		return false;
	}

	// Декодируем из stream
	IWICBitmapDecoder* decoder = nullptr;
	hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
	stream->Release();
	if (FAILED(hr) || !decoder)
	{
		factory->Release();
		return false;
	}

	IWICBitmapFrameDecode* frame = nullptr;
	hr = decoder->GetFrame(0, &frame);
	if (FAILED(hr) || !frame)
	{
		decoder->Release();
		factory->Release();
		return false;
	}

	IWICFormatConverter* converter = nullptr;
	hr = factory->CreateFormatConverter(&converter);
	if (FAILED(hr) || !converter)
	{
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	UINT w = 0, h = 0;
	hr = converter->GetSize(&w, &h);
	if (FAILED(hr) || w == 0 || h == 0)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	std::vector<BYTE> pixels;
	const UINT stride = w * 4;
	const UINT imageSize = stride * h;
	pixels.resize(imageSize);
	hr = converter->CopyPixels(nullptr, stride, imageSize, pixels.data());
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = pixels.data();
	initData.SysMemPitch = stride;

	ID3D11Texture2D* tex = nullptr;
	hr = g_pd3dDevice->CreateTexture2D(&texDesc, &initData, &tex);
	if (FAILED(hr) || !tex)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	ID3D11ShaderResourceView* srv = nullptr;
	hr = g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, &srv);
	tex->Release();
	converter->Release();
	frame->Release();
	decoder->Release();
	factory->Release();

	if (FAILED(hr) || !srv)
		return false;

	*out_srv = srv;
	if (out_width) *out_width = (int)w;
	if (out_height) *out_height = (int)h;
	return true;
}

static bool LoadTextureFromFileW(const wchar_t* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
	if (!filename || !out_srv || !g_pd3dDevice || !g_pd3dDeviceContext)
		return false;
	*out_srv = nullptr;
	if (out_width) *out_width = 0;
	if (out_height) *out_height = 0;

	IWICImagingFactory* factory = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (FAILED(hr) || !factory)
		return false;

	IWICBitmapDecoder* decoder = nullptr;
	hr = factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	if (FAILED(hr) || !decoder)
	{
		factory->Release();
		return false;
	}

	IWICBitmapFrameDecode* frame = nullptr;
	hr = decoder->GetFrame(0, &frame);
	if (FAILED(hr) || !frame)
	{
		decoder->Release();
		factory->Release();
		return false;
	}

	IWICFormatConverter* converter = nullptr;
	hr = factory->CreateFormatConverter(&converter);
	if (FAILED(hr) || !converter)
	{
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	UINT w = 0, h = 0;
	hr = converter->GetSize(&w, &h);
	if (FAILED(hr) || w == 0 || h == 0)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	std::vector<BYTE> pixels;
	const UINT stride = w * 4;
	const UINT imageSize = stride * h;
	pixels.resize(imageSize);
	hr = converter->CopyPixels(nullptr, stride, imageSize, pixels.data());
	if (FAILED(hr))
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = pixels.data();
	initData.SysMemPitch = stride;

	ID3D11Texture2D* tex = nullptr;
	hr = g_pd3dDevice->CreateTexture2D(&texDesc, &initData, &tex);
	if (FAILED(hr) || !tex)
	{
		converter->Release();
		frame->Release();
		decoder->Release();
		factory->Release();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	ID3D11ShaderResourceView* srv = nullptr;
	hr = g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, &srv);
	tex->Release();
	converter->Release();
	frame->Release();
	decoder->Release();
	factory->Release();
	if (FAILED(hr) || !srv)
		return false;

	*out_srv = srv;
	if (out_width) *out_width = (int)w;
	if (out_height) *out_height = (int)h;
	return true;
}



LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Если меню открыто, сначала даем ImGui обработать сообщение
	if (g_menuOpen)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return 1;
	}

	switch (msg)
	{
	case WM_CREATE:
	{
		MARGINS margin = { -1, -1, -1, -1 };
		DwmExtendFrameIntoClientArea(hWnd, &margin);
		return 0;
	}
	case WM_NCHITTEST:
	{
		// Если меню открыто — это обычное окно (клики работают).
		// Если закрыто — окно прозрачно для кликов (пропускает в игру).
		if (g_menuOpen) {
			// Важно: возвращаем HTCLIENT, чтобы ImGui получал события мыши
			return HTCLIENT;
		}
		return HTTRANSPARENT;
	}
	case WM_MOUSEACTIVATE:
		// При открытом меню разрешаем активацию окна
		return g_menuOpen ? MA_ACTIVATE : MA_NOACTIVATE;

	// УДАЛЕНО: WM_LBUTTONDOWN / UP с SetCapture — это ломало клики!

	case WM_ERASEBKGND:
		return 1;
	case WM_SIZE:
		if (g_pd3dDevice && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX11_InvalidateDeviceObjects();
			CleanupRenderTarget();
			if (g_pSwapChain)
			{
				g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
				CreateRenderTarget();
				ImGui_ImplDX11_CreateDeviceObjects();
				if (g_dcompDevice) g_dcompDevice->Commit();
			}
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}


DWORD WINAPI MainThread(LPVOID)
{
	HRESULT coInitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool coInitOk = (coInitHr == S_OK || coInitHr == S_FALSE);

	timeBeginPeriod(1); // ФИКС: Убираем лок на 64 FPS, повышая точность таймера Windows до 1мс

	InitRuntimeOffsets();
	InitSkinConfig();
	LoadConfig();
	
	InitHooks(); // Инициализация VTable хука для реал-тайм скинов

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = OverlayWndProc;
	wc.hInstance = g_hModule;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = g_wndClassName;

	ATOM classAtom = RegisterClassExW(&wc);
	if (!classAtom) { FreeLibraryAndExitThread(g_hModule, 0); return 0; }

	int screenW = GetSystemMetrics(SM_CXSCREEN);
	int screenH = GetSystemMetrics(SM_CYSCREEN);

	HWND hwnd = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
		g_wndClassName, L"", WS_POPUP, 0, 0, screenW, screenH, nullptr, nullptr, wc.hInstance, nullptr);

	if (!hwnd) { UnregisterClassW(g_wndClassName, wc.hInstance); FreeLibraryAndExitThread(g_hModule, 0); return 0; }
	g_hOverlayWnd = hwnd;
	SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
	
	// Stream Proof: Скрываем оверлей от захвата OBS/Discord (по умолчанию выключено)
	if (g_antiCaptureEnabled) {
		SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
	}
	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(hwnd, &margins);

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	if (!CreateDeviceD3D(hwnd)) {
		CleanupDeviceD3D(); DestroyWindow(hwnd); UnregisterClassW(g_wndClassName, wc.hInstance); FreeLibraryAndExitThread(g_hModule, 0); return 0;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Шрифты
	{
		// Попытка 1: Ресурсы
		bool fontLoaded = false;
		HRSRC hResource = FindResourceW(g_hModule, MAKEINTRESOURCEW(IDR_FONT_MAIN), RT_RCDATA);
		if (hResource) {
			HGLOBAL hMemory = LoadResource(g_hModule, hResource);
			if (hMemory) {
				DWORD dwSize = SizeofResource(g_hModule, hResource);
				LPVOID lpAddress = LockResource(hMemory);
				if (lpAddress && dwSize > 0) {
					void* fontData = IM_ALLOC(dwSize);
					memcpy(fontData, lpAddress, dwSize);
					ImFont* font = io.Fonts->AddFontFromMemoryTTF(fontData, dwSize, 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
					if (font) { io.FontDefault = font; fontLoaded = true; }
				}
			}
		}
		// Попытка 2: Системные
		if (!fontLoaded) {
			if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic())) fontLoaded = true;
			else if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic())) fontLoaded = true;
		}
	}

	ApplyClientStyle();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	MSG msg{};
	static ULONGLONG s_lastFrameTick = 0;

	// ОСНОВНОЙ ЦИКЛ
	while (g_running)
	{
		// 1. Обработка сообщений Windows (клики, перемещение окна)
		while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			if (msg.message == WM_QUIT) g_running = false;
		}
		if (!g_running) break;

		// 2. Проверка состояния игры и кнопок
		bool cs2Active = IsCs2Active();
		if (!cs2Active) g_menuOpen = false;

		// Горячие клавиши (работают всегда, если окно CS2 активно)
		if (cs2Active) {
			if (GetAsyncKeyState(VK_F6) & 1) g_bhopEnabled = !g_bhopEnabled;
			if (g_whKey != 0 && (GetAsyncKeyState(g_whKey) & 1)) g_whEnabled = !g_whEnabled;
			if (g_bonesKey != 0 && (GetAsyncKeyState(g_bonesKey) & 1)) g_bonesEnabled = !g_bonesEnabled;
			if (g_triggerToggleKey != 0 && (GetAsyncKeyState(g_triggerToggleKey) & 1)) g_triggerEnabled = !g_triggerEnabled;
			
			// Меню
			if (GetAsyncKeyState(VK_RSHIFT) & 1) g_menuOpen = !g_menuOpen;
			if (g_menuOpen && (GetAsyncKeyState(VK_ESCAPE) & 1)) g_menuOpen = false;
			
			// Выгрузка
			if (GetAsyncKeyState(VK_END) & 1) { g_running = false; break; }
		}

		// 3. Чтение данных из памяти (Game Data)
		uintptr_t client = GetClientBase();
		uintptr_t localPawn = 0;
		uintptr_t localCtrl = 0; // ДОБАВЛЕНО: проверка на главное меню
		if (client) {
			(void)TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerPawn, localPawn);
			(void)TryRead<uintptr_t>(client + g_offsetsRuntime.dwLocalPlayerController, localCtrl); // ДОБАВЛЕНО
		}
		uintptr_t localScene = 0;
		int localHp = 0;
		if (localPawn && localCtrl) { // ДОБАВЛЕНО && localCtrl
			(void)TryRead<uintptr_t>(localPawn + g_offsetsRuntime.m_pGameSceneNode, localScene);
			(void)TryRead<int>(localPawn + g_offsetsRuntime.m_iHealth, localHp);
		}
		
		// СТРОГАЯ ПРОВЕРКА: Если localCtrl == 0, мы 100% в главном меню
		bool validLocalPlayer = (client && localPawn && localCtrl && localScene && localHp >= -100 && localHp < 10000);
		static bool s_prevValid = false;
		if (s_prevValid && !validLocalPlayer)
		{
			g_espBoxes.clear();
			g_espTargetsWorld.clear();
			g_boneCache.clear();
			g_spectators.clear(); // Очистка списка зрителей при смерти/смене раунда
		}
		s_prevValid = validLocalPlayer;
		
		// Логика функций (Aim, Bhop, Trigger)
		if (validLocalPlayer && cs2Active) {
			
			// --- ТРЕКИНГ ВЫСТРЕЛОВ ДЛЯ ТОЧНОГО HITSOUND ---
			static int s_lastShots = 0;
			int currentShots = 0;
			if (TryRead<int>(localPawn + g_offsetsRuntime.m_iShotsFired, currentShots)) {
				if (currentShots > s_lastShots) {
					g_lastLocalShotTime = GetTickCount64(); // Запоминаем время выстрела
				}
				s_lastShots = currentShots;
			}
			// ----------------------------------------------
			
			// МГНОВЕННАЯ ДЕТЕКЦИЯ ПОПАДАНИЙ (каждый кадр, без лимитов ESP)
			UpdateHitInfo(client);
			
			RunAimbot(true);
			RunCombat(true);
			RunBhop(true);
			
			// Skin Changer работает через VTable хук (hkFrameStageNotify)
			
			UpdateFovOverride(true);
			UpdateNoRecoilNoSpread(true);
			UpdateAntiFlash(true);
			UpdateNoSmoke(true); // ДОБАВЛЕНО: No Smoke
		} else {
			// Очистка кэша, если мы вышли в меню или мир выгружается
			if (!g_espBoxes.empty()) g_espBoxes.clear();
			if (!g_espTargetsWorld.empty()) g_espTargetsWorld.clear();
			if (!g_boneCache.empty()) g_boneCache.clear();
			if (!g_spectators.empty()) g_spectators.clear(); // Очистка списка зрителей
			g_bombData.found = false;
			g_bombData.pos = { 0,0,0 };
			g_autoFireActive = false;
		}

		// 4. Решение: нужно ли рисовать оверлей?
		bool needRender = g_menuOpen || (validLocalPlayer && cs2Active && (g_whEnabled || g_bonesEnabled || g_aimbotDrawFov || g_bombEspEnabled || g_spectatorListEnabled)); // ИСПРАВЛЕНО

		// Управление видимостью и кликабельностью окна
		// Если меню открыто -> кликабельно. Если нет -> прозрачно для кликов.
		SetOverlayInputMode(hwnd, g_menuOpen);

		
		// Показываем окно, если нужно рисовать, иначе прячем (оптимизация)
		static bool s_prevShow = false;
		if (needRender != s_prevShow) {
			ShowWindow(hwnd, needRender ? SW_SHOWNA : SW_HIDE);
			s_prevShow = needRender;
		}

		if (!needRender) {
			Sleep(16); // Экономим CPU, если ничего не рисуем
			continue;
		}

		// 5. Ограничитель FPS (чтобы не жрать 100% CPU оверлеем)
		ULONGLONG frameNow = GetTickCount64();
		if (s_lastFrameTick != 0 && (frameNow - s_lastFrameTick) < 8) { // ~120 FPS limit
			Sleep(1);
			continue;
		}
		s_lastFrameTick = frameNow;

		// 6. Подготовка данных для отрисовки (ESP)
		// Обновляем данные только если мы в матче
		RECT rc{}; GetClientRect(hwnd, &rc);
		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;
		float view[16]{}; // Матрица вида

		if (validLocalPlayer && cs2Active) {
			// Читаем ViewMatrix
			__try {
				for (int i = 0; i < 16; ++i)
					(void)TryRead<float>(client + g_offsetsRuntime.dwViewMatrix + i * sizeof(float), view[i]);
			} __except (1) { }

			// Обновляем списки ESP (раз в 33мс для оптимизации)
			static ULONGLONG s_lastEspTick = 0;
			if (frameNow - s_lastEspTick >= 33) {
				// ИСПРАВЛЕНО: Добавлено условие g_damageIndicatorsEnabled
				if (g_whEnabled || g_bonesEnabled || g_bombEspEnabled || g_spectatorListEnabled || g_hitSoundEnabled || g_damageIndicatorsEnabled) UpdateESPInternal(width, height);
				if (g_bonesEnabled) UpdateBonesCache(width, height);
				s_lastEspTick = frameNow;
			}
			
			// Проецируем боксы, если включена ЛЮБАЯ 2D функция
			if (g_whEnabled || g_nameEspEnabled || g_gunEspEnabled || g_hpBarEnabled) {
				ProjectESPBoxes(view, width, height);
			}
		}

		// 7. Рендер ImGui
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		
		// Передаем фокус ввода в ImGui, если меню открыто
		if (g_menuOpen) {
			ImGuiIO& curIO = ImGui::GetIO();
			curIO.MouseDrawCursor = true; // Рисуем курсор ImGui
		} else {
			ImGuiIO& curIO = ImGui::GetIO();
			curIO.MouseDrawCursor = false;
			// ИСПРАВЛЕНИЕ: Принудительно возвращаем фокус в CS2 при закрытии меню
			static bool s_wasMenuOpen = false;
			if (s_wasMenuOpen && !g_menuOpen) {
				// Меню только что закрылось - возвращаем фокус
				HWND cs2Wnd = FindWindowW(nullptr, L"Counter-Strike 2");
				if (cs2Wnd) {
					SetForegroundWindow(cs2Wnd);
					SetActiveWindow(cs2Wnd);
					SetFocus(cs2Wnd);
				}
			}
			s_wasMenuOpen = g_menuOpen;
		}

		ImGui::NewFrame();

		// Рисуем элементы
		if (validLocalPlayer && cs2Active) {
			DrawEspImGui();
			DrawSnaplinesImGui();
			if (g_bonesEnabled) DrawBonesImGui(view, width, height);
			DrawSpectatorListImGui();
		}
		
		DrawKeybindsListImGui();
		DrawWatermarkImGui();
		DrawMenuImGui();

		ImGui::Render();

		// 8. Вывод на экран (DirectX Present)
		if (!g_mainRenderTargetView) CreateRenderTarget();
		if (g_mainRenderTargetView) {
			const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Прозрачный фон
			g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
			g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}
		g_pSwapChain->Present(0, 0);
	}

	// Выход
	ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
	CleanupDeviceD3D();
	if (g_hOverlayWnd) DestroyWindow(g_hOverlayWnd);
	UnregisterClassW(g_wndClassName, wc.hInstance);
	if (coInitOk) CoUninitialize();
	
	RemoveHooks(); // <--- ОЧЕНЬ ВАЖНО: Снимаем хук перед выгрузкой, иначе игра крашнется
	
	timeEndPeriod(1); // Возвращаем системный таймер в норму
	FreeLibraryAndExitThread(g_hModule, 0);
	return 0;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		g_hModule = hModule;
		DisableThreadLibraryCalls(hModule);
		g_hMainThread = CreateThread(nullptr, 0, MainThread, nullptr, 0, &g_mainThreadId);
		break;

	case DLL_PROCESS_DETACH:
		// Если DLL выгружается не через FreeLibraryAndExitThread
		if (lpReserved == nullptr)
		{
			g_running = false;
			if (g_hOverlayWnd)
			{
				PostMessageW(g_hOverlayWnd, WM_QUIT, 0, 0);
			}
			// В DllMain нельзя блокироваться (loader lock), и тем более ждать свой же поток.
			// Просто закрываем handle (не влияет на выполнение потока).
			if (g_hMainThread)
			{
				CloseHandle(g_hMainThread);
				g_hMainThread = nullptr;
			}
		}
		break;
	}
	return TRUE;
}

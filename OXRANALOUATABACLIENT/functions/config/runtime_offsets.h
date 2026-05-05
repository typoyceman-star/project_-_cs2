#pragma once
// RuntimeOffsets — кэш всех CS2 оффсетов из output/*.hpp с возможностью
// рантайм-обновления из JSON.

#include <cstdint>
#include <map>
#include <string>

#include "../../output/offsets.hpp"
#include "../../output/client_dll.hpp"
#include "../../output/buttons.hpp"

#include "json_parser.h"

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

	// C_BaseEntity
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

	// C_PlantedC4
	std::uint32_t m_bBombTicking = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombTicking;
	std::uint32_t m_bBombDefused = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombDefused;
	std::uint32_t m_flC4Blow = cs2_dumper::schemas::client_dll::C_PlantedC4::m_flC4Blow;
	std::uint32_t m_bBeingDefused = cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBeingDefused;

	// C_CSPlayerPawn (skin changer)
	std::uint32_t m_hHudModelArms = cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_hHudModelArms;

	bool LoadFromJson(const std::string& offsetsJsonPath, const std::string& schemaJsonPath)
	{
		std::map<std::string, std::uint32_t> offsets;
		if (SimpleJsonParser::LoadOffsets(offsetsJsonPath, offsets)) {
			if (offsets.count("dwEntityList")) dwEntityList = offsets["dwEntityList"];
			if (offsets.count("dwViewMatrix")) dwViewMatrix = offsets["dwViewMatrix"];
			if (offsets.count("dwViewAngles")) dwViewAngles = offsets["dwViewAngles"];
			if (offsets.count("dwLocalPlayerController")) dwLocalPlayerController = offsets["dwLocalPlayerController"];
			if (offsets.count("dwLocalPlayerPawn")) dwLocalPlayerPawn = offsets["dwLocalPlayerPawn"];
			if (offsets.count("dwGlobalVars")) dwGlobalVars = offsets["dwGlobalVars"];
			if (offsets.count("dwPlantedC4")) dwPlantedC4 = offsets["dwPlantedC4"];
		}

		std::map<std::string, std::uint32_t> schema;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_BaseEntity", schema)) {
			if (schema.count("m_iHealth")) m_iHealth = schema["m_iHealth"];
			if (schema.count("m_iTeamNum")) m_iTeamNum = schema["m_iTeamNum"];
			if (schema.count("m_pGameSceneNode")) m_pGameSceneNode = schema["m_pGameSceneNode"];
			if (schema.count("m_vecVelocity")) m_vecVelocity = schema["m_vecVelocity"];
			if (schema.count("m_fFlags")) m_fFlags = schema["m_fFlags"];
			if (schema.count("m_pCollision")) m_pCollision = schema["m_pCollision"];
		}
		std::map<std::string, std::uint32_t> schemaScene;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CGameSceneNode", schemaScene)) {
			if (schemaScene.count("m_vecAbsOrigin")) m_vecAbsOrigin = schemaScene["m_vecAbsOrigin"];
		}
		std::map<std::string, std::uint32_t> schemaPawn;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_BasePlayerPawn", schemaPawn)) {
			if (schemaPawn.count("m_pObserverServices")) m_pObserverServices = schemaPawn["m_pObserverServices"];
			if (schemaPawn.count("m_pWeaponServices")) m_pWeaponServices = schemaPawn["m_pWeaponServices"];
			if (schemaPawn.count("m_pCameraServices")) m_pCameraServices = schemaPawn["m_pCameraServices"];
			if (schemaPawn.count("m_vOldOrigin")) m_vOldOrigin = schemaPawn["m_vOldOrigin"];
		}
		std::map<std::string, std::uint32_t> schemaCSPawn;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_CSPlayerPawn", schemaCSPawn)) {
			if (schemaCSPawn.count("m_iShotsFired")) m_iShotsFired = schemaCSPawn["m_iShotsFired"];
			if (schemaCSPawn.count("m_pAimPunchServices")) m_pAimPunchServices = schemaCSPawn["m_pAimPunchServices"];
			if (schemaCSPawn.count("m_bIsScoped")) m_bIsScoped = schemaCSPawn["m_bIsScoped"];
			if (schemaCSPawn.count("m_entitySpottedState")) m_entitySpottedState = schemaCSPawn["m_entitySpottedState"];
		}
		std::map<std::string, std::uint32_t> schemaAimPunch;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCSPlayer_AimPunchServices", schemaAimPunch)) {
			if (schemaAimPunch.count("m_predictableBaseAngle")) m_predictableBaseAngle = schemaAimPunch["m_predictableBaseAngle"];
			if (schemaAimPunch.count("m_predictableBaseAngleVel")) m_predictableBaseAngleVel = schemaAimPunch["m_predictableBaseAngleVel"];
		}
		std::map<std::string, std::uint32_t> schemaCSBase;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_CSPlayerPawnBase", schemaCSBase)) {
			if (schemaCSBase.count("m_flFlashDuration")) m_flFlashDuration = schemaCSBase["m_flFlashDuration"];
			if (schemaCSBase.count("m_flFlashBangTime")) m_flFlashBangTime = schemaCSBase["m_flFlashBangTime"];
		}
		std::map<std::string, std::uint32_t> schemaModel;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_BaseModelEntity", schemaModel)) {
			if (schemaModel.count("m_vecViewOffset")) m_vecViewOffset = schemaModel["m_vecViewOffset"];
			if (schemaModel.count("m_Glow")) m_Glow = schemaModel["m_Glow"];
		}
		std::map<std::string, std::uint32_t> schemaObs;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CPlayer_ObserverServices", schemaObs)) {
			if (schemaObs.count("m_hObserverTarget")) m_hObserverTarget = schemaObs["m_hObserverTarget"];
		}
		std::map<std::string, std::uint32_t> schemaCtrl;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CBasePlayerController", schemaCtrl)) {
			if (schemaCtrl.count("m_iszPlayerName")) m_iszPlayerName = schemaCtrl["m_iszPlayerName"];
		}
		std::map<std::string, std::uint32_t> schemaCCtrl;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCSPlayerController", schemaCCtrl)) {
			if (schemaCCtrl.count("m_hPlayerPawn")) m_hPlayerPawn = schemaCCtrl["m_hPlayerPawn"];
			if (schemaCCtrl.count("m_hObserverPawn")) m_hObserverPawn = schemaCCtrl["m_hObserverPawn"];
		}
		std::map<std::string, std::uint32_t> schemaWpn;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CPlayer_WeaponServices", schemaWpn)) {
			if (schemaWpn.count("m_hActiveWeapon")) m_hActiveWeapon = schemaWpn["m_hActiveWeapon"];
		}
		std::map<std::string, std::uint32_t> schemaEcon;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_EconItemView", schemaEcon)) {
			if (schemaEcon.count("m_iItemDefinitionIndex")) m_iItemDefinitionIndex = schemaEcon["m_iItemDefinitionIndex"];
		}
		std::map<std::string, std::uint32_t> schemaCol;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCollisionProperty", schemaCol)) {
			if (schemaCol.count("m_vecMins")) m_vecMins = schemaCol["m_vecMins"];
			if (schemaCol.count("m_vecMaxs")) m_vecMaxs = schemaCol["m_vecMaxs"];
		}
		std::map<std::string, std::uint32_t> schemaCam;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CCSPlayerBase_CameraServices", schemaCam)) {
			if (schemaCam.count("m_iFOV")) m_iFOV = schemaCam["m_iFOV"];
			if (schemaCam.count("m_iFOVStart")) m_iFOVStart = schemaCam["m_iFOVStart"];
			if (schemaCam.count("m_flFOVTime")) m_flFOVTime = schemaCam["m_flFOVTime"];
			if (schemaCam.count("m_flFOVRate")) m_flFOVRate = schemaCam["m_flFOVRate"];
		}
		std::map<std::string, std::uint32_t> schemaSkel;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "CSkeletonInstance", schemaSkel)) {
			if (schemaSkel.count("m_modelState")) m_modelState = schemaSkel["m_modelState"];
		}
		std::map<std::string, std::uint32_t> schemaC4;
		if (SimpleJsonParser::LoadSchemaOffsets(schemaJsonPath, "C_PlantedC4", schemaC4)) {
			if (schemaC4.count("m_bBombTicking")) m_bBombTicking = schemaC4["m_bBombTicking"];
			if (schemaC4.count("m_bBombDefused")) m_bBombDefused = schemaC4["m_bBombDefused"];
			if (schemaC4.count("m_flC4Blow")) m_flC4Blow = schemaC4["m_flC4Blow"];
			if (schemaC4.count("m_bBeingDefused")) m_bBeingDefused = schemaC4["m_bBeingDefused"];
		}
		return true;
	}
};

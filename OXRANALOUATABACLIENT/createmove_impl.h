#pragma once
#include <cstdint>

// ==================== CREATEMOVE STRUCTURES ====================
// Based on CS2 2025-2026 research from UnknownCheats and YouGame forums

// Типы Vec3/QAngle (определены также в cs2_internal_dll.cpp)
#ifndef VEC3_DEFINED
#define VEC3_DEFINED
struct Vec3 { float x, y, z; };
#endif
#ifndef QANGLE_DEFINED
#define QANGLE_DEFINED
struct QAngle { float x, y, z; };
#endif

// CUserCmd structures for CS2 (Source 2)
struct CInButtonState
{
	uint64_t m_nValue[3];
	uint64_t m_nValueChanged[3];
	uint64_t m_nValueScroll[3];
};

struct CBaseUserCmdPB
{
	void* m_pCommandNumber;
	void* m_pTickCount;
	void* m_pButtonState;
	void* m_pViewAngles;
	void* m_pForwardMove;
	void* m_pSideMove;
	void* m_pUpMove;
	void* m_pImpulse;
	void* m_pWeaponSelect;
	void* m_pRandomSeed;
	void* m_pMousedx;
	void* m_pMousedy;
	void* m_pConsumedServerAngleChanges;
	void* m_pCmdFlags;
	void* m_pPawnEntityHandle;
};

struct CCSGOUserCmdPB
{
	CBaseUserCmdPB m_base;
	void* m_pLefthandDesired;
	void* m_pAttack3StartHistoryIndex;
	void* m_pAttack1StartHistoryIndex;
	void* m_pAttack2StartHistoryIndex;
};

// Main CUserCmd structure
struct CUserCmd
{
	void* vtable;
	CCSGOUserCmdPB m_csgoUserCmd;
	CInButtonState m_nButtons;
	uint32_t m_nCommandNumber;
	uint32_t m_nTickCount;
	QAngle m_angViewAngles;  // THIS IS WHAT WE MODIFY FOR SILENT AIM
	Vec3 m_vecForwardMove;
	Vec3 m_vecSideMove;
	Vec3 m_vecUpMove;
	uint32_t m_nImpulse;
	uint32_t m_nWeaponSelect;
	uint32_t m_nRandomSeed;
	int16_t m_nMousedx;
	int16_t m_nMousedy;
	bool m_bHasBeenPredicted;
	uint32_t m_nPawnEntityHandle;
};

// CreateMove function pointer type
typedef bool(__fastcall* CreateMove_t)(void* pInput, int nSlot, bool bActive);

// Global variables for CreateMove hook
inline CreateMove_t oCreateMove = nullptr;
inline void** g_pCSGOInputVTable = nullptr;
inline bool g_bCreateMoveHooked = false;

// Forward declarations
static void ProcessRagebotInCreateMove(CUserCmd* cmd, uintptr_t localPawn, uintptr_t client);
static void ProcessAntiAimInCreateMove(CUserCmd* cmd, uintptr_t localPawn, uintptr_t client);


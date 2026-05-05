#pragma once
// Общие типы данных проекта.
// Раньше были inline-объявлены в cs2_internal_dll.cpp.

#include <cstdint>
#include <vector>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct QAngle { float x, y, z; };

// Структура для быстрого чтения строк блоками.
struct StringBuf64 { char data[64]; };

struct Box2D { float left, top, right, bottom; };

struct EspBox
{
	Box2D box;
	int hp;
	std::uintptr_t pawn;
	char name[64];
	char weapon[64];
	int weaponIconIndex;
	bool flashed;
	float flashDur;
	float flashEndTime;
	float distance;
};

struct EspTargetWorld
{
	std::uintptr_t pawn;
	int hp;
	Vec3 origin;
	Vec3 mins;
	Vec3 maxs;
	char name[64];
	char weapon[64];
	int weaponIconIndex;
	bool flashed;
	float flashDur;
	float flashEndTime;
	float distance;
};

struct CachedBoneWorld
{
	Vec3 world;
	bool valid;
};

struct BoneCacheEntryWorld
{
	std::uintptr_t pawn;
	CachedBoneWorld b[28];
};

struct BombData
{
	Vec3 pos;
	float blowTime;
	bool isDefusing;
	bool found;
};

struct DamageText
{
	std::uintptr_t targetPawn;
	Vec3 worldPos;
	int damage;
	unsigned long long spawnTime;
	float alpha;
	float currentYOffset;
	float targetYOffset;
};

// VTable-хук CreateInterface
typedef void(__fastcall* FrameStageNotify_t)(void* rcx, int curStage);

#include "math.h"
#include "globals.h"
#include "memory.h"
#include "../../output/offsets.hpp"
#include "../../output/client_dll.hpp"

QAngle CalcAngle(Vec3 src, Vec3 dst)
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

float GetFov(QAngle viewAngles, QAngle aimAngles)
{
	float deltaX = aimAngles.x - viewAngles.x;
	float deltaY = aimAngles.y - viewAngles.y;

	if (deltaY > 180.0f) deltaY -= 360.0f;
	if (deltaY < -180.0f) deltaY += 360.0f;

	return std::sqrt(deltaX * deltaX + deltaY * deltaY);
}

bool WorldToScreen(const float m[16], const Vec3& pos, int width, int height, Vec2& out)
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


bool GetEntityBox2DFromWorldAABB(const Vec3& origin, const Vec3& mins, const Vec3& maxs, const float view[16], int width, int height, Box2D& out)
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

void ClampAngles(QAngle& angles)
{
	if (angles.x > 89.0f) angles.x = 89.0f;
	if (angles.x < -89.0f) angles.x = -89.0f;
	while (angles.y > 180.0f) angles.y -= 360.0f;
	while (angles.y < -180.0f) angles.y += 360.0f;
	angles.z = 0.0f;
}


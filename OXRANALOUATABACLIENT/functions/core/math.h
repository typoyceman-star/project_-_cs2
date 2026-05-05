#pragma once
// Геометрия: углы, проекция, AABB.

#include "types.h"

QAngle CalcAngle(Vec3 src, Vec3 dst);
float GetFov(QAngle viewAngles, QAngle aimAngles);
void ClampAngles(QAngle& angles);
bool WorldToScreen(const float m[16], const Vec3& pos, int width, int height, Vec2& out);
bool GetEntityBox2DFromWorldAABB(const Vec3& origin, const Vec3& mins, const Vec3& maxs,
                                 const float view[16], int width, int height, Box2D& out);

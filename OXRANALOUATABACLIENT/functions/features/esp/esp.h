#pragma once
// ESP / box / radar / hitsound / damage indicators / bomb.

#include <cstdint>

float GetEstimatedGameTimeSeconds();

const char* GetWeaponName(int defIndex);

void UpdateESPInternal(int width, int height);
void UpdateHitInfo(std::uintptr_t client);

void ProjectESPBoxes(const float view[16], int width, int height);
void DrawEspImGui();

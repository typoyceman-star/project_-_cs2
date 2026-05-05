#pragma once
// VTable-хуки на Source2Client (FrameStageNotify) для скинченджера.

void InitHooks();
void RemoveHooks();

// Хук: вызывается каждый кадр на стадии 6 (FRAME_RENDER_START).
void __fastcall hkFrameStageNotify(void* rcx, int curStage);

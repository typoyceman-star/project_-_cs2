#include "bhop.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"

void RunBhop(bool cs2Active)
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


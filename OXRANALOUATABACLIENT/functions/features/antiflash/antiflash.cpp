#include "antiflash.h"
#include "../../core/globals.h"
#include "../../core/memory.h"
#include "../../../output/offsets.hpp"
#include "../../../output/client_dll.hpp"

void UpdateAntiFlash(bool cs2Active)
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

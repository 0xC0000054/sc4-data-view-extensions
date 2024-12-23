////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-data-view-extensions, a DLL Plugin for
// SimCity 4 that extends the game's data views.
//
// Copyright (c) 2024 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
////////////////////////////////////////////////////////////////////////

#include "ParkEffectFilter.h"
#include "cISCPropertyHolder.h"
#include "cISC4Occupant.h"

bool ParkEffectFilter::IsOccupantIncluded(cISC4Occupant* pOccupant)
{
	constexpr uint32_t OccupantType_Building = 0x278128A0;
	constexpr uint32_t ParkEffectPropertyId = 0x27812850;
	bool result = false;

	if (pOccupant)
	{
		if (pOccupant->GetType() == OccupantType_Building)
		{
			result = pOccupant->AsPropertyHolder()->HasProperty(ParkEffectPropertyId);
		}
	}

	return result;
}

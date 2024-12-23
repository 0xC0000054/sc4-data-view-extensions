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

#include "LandmarkEffectFilter.h"
#include "cISCPropertyHolder.h"
#include "cISC4Occupant.h"

LandmarkEffectFilter::LandmarkEffectFilter()
{
}

bool LandmarkEffectFilter::IsOccupantIncluded(cISC4Occupant* pOccupant)
{
	constexpr uint32_t OccupantGroup_Building = 0x278128A0;
	constexpr uint32_t LandmarkEffectPropertyId = 0x2781284F;

	bool result = false;

	if (pOccupant)
	{
		if (pOccupant->GetType() == OccupantGroup_Building)
		{
			result = pOccupant->AsPropertyHolder()->HasProperty(LandmarkEffectPropertyId);
		}
	}

	return result;
}


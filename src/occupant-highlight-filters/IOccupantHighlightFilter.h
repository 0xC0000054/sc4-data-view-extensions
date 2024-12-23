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

#pragma once

class cISC4Occupant;

class IOccupantHighlightFilter
{
	virtual bool IsOccupantIncluded(cISC4Occupant* pOccupant) const = 0;
};
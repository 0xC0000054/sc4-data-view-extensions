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
#include "cIGZMessageTarget2.h"
#include "cISC4Occupant.h"
#include "cISC4OccupantFilter.h"
#include "cRZAutoRefCount.h"
#include <vector>

enum DataViewHighlight : uint32_t
{
	// Maxis implemented 9 highlight modes.
	// The TransitSwitch value was not documented
	// in Ingred.ini, perhaps it was only intended as
	// an internal network debugging aid.

	DataViewHighlightNone = 0,
	DataViewHighlightEducation = 1,
	DataViewHighlightHealth = 2,
	DataViewHighlightFire = 3,
	DataViewHighlightPolice = 4,
	DataViewHighlightCrime = 5,
	DataViewHighlightGarbage = 6,
	DataViewHighlightPower = 7,
	DataViewHighlightWater = 8,
	DataViewHighlightTransitSwitch = 9,
	// All the values below are new highlight modes
	// implemented by this DLL.

	DataViewHighlightParkEffect = 10,
	DataViewHighlightLandmarkEffect = 11,
};

class cIGZMessage2Standard;

class DataViewHighlightManager : private cIGZMessageTarget2
{
public:
	DataViewHighlightManager();

	void Init(uint32_t highlightType);
	void Shutdown();

	const std::vector<cISC4Occupant*>& GetAffectedOccupants();

private:

	// cIGZUnknown

	bool QueryInterface(uint32_t riid, void** ppvObj);
	uint32_t AddRef();
	uint32_t Release();

	// cIGZMessageTarget2

	bool DoMessage(cIGZMessage2* pMsg);

	// Private members

	static bool IterateOccupantsCallback(cISC4Occupant* pOccupant, void* pContext);

	void OccupantInserted(cISC4Occupant* pOccupant);
	void OccupantRemoved(cISC4Occupant* pOccupant);

	uint32_t refCount;
	cRZAutoRefCount<cISC4OccupantFilter> occupantFilter;
	std::vector<cISC4Occupant*> affectedOccupants;
};


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

#include "DataViewHighlightManager.h"
#include "cIGZMessage2Standard.h"
#include "cIGZMessageServer2.h"
#include "cISC4Occupant.h"
#include "GlobalPointers.h"
#include "GZCLSIDDefs.h"
#include "GZServPtrs.h"
#include "LandmarkEffectFilter.h"
#include "ParkEffectFilter.h"
#include <algorithm>
#include <array>
#include <vector>

static const uint32_t kSC4MessageInsertOccupant = 0x99EF1142;
static const uint32_t kSC4MessageRemoveOccupant = 0x99EF1143;

static constexpr std::array<uint32_t, 2> RequiredNotifications =
{
	kSC4MessageInsertOccupant,
	kSC4MessageRemoveOccupant,
};

DataViewHighlightManager::DataViewHighlightManager()
	: refCount(0)
{
}

void DataViewHighlightManager::Init(uint32_t highlightType)
{
	switch (highlightType)
	{
	case DataViewHighlightParkEffect:
		occupantFilter = new ParkEffectFilter();
		break;
	case DataViewHighlightLandmarkEffect:
		occupantFilter = new LandmarkEffectFilter();
		break;
	}

	if (occupantFilter)
	{
		if (spOccupantManager)
		{
			spOccupantManager->IterateOccupants(
				IterateOccupantsCallback,
				this,
				nullptr,
				nullptr,
			    static_cast<cISC4OccupantFilter*>(occupantFilter));

			cIGZMessageServer2Ptr pMS2;

			if (pMS2)
			{
				for (uint32_t messageID : RequiredNotifications)
				{
					pMS2->AddNotification(this, messageID);
				}
			}
		}
	}
}

void DataViewHighlightManager::Shutdown()
{
	for (cISC4Occupant* pOccupant : affectedOccupants)
	{
		pOccupant->Release();
	}

	affectedOccupants.clear();
	occupantFilter.Reset();

	cIGZMessageServer2Ptr pMS2;

	if (pMS2)
	{
		for (uint32_t messageID : RequiredNotifications)
		{
			pMS2->RemoveNotification(this, messageID);
		}
	}
}

const std::vector<cISC4Occupant*>& DataViewHighlightManager::GetAffectedOccupants()
{
	return affectedOccupants;
}

bool DataViewHighlightManager::QueryInterface(uint32_t riid, void** ppvObj)
{
	if (riid == GZCLSID::kcIGZMessageTarget2)
	{
		*ppvObj = static_cast<cIGZMessageTarget2*>(this);
		AddRef();

		return true;
	}
	else if (riid ==GZIID_cIGZUnknown)
	{
		*ppvObj = static_cast<cIGZUnknown*>(this);
		AddRef();

		return true;
	}

	return false;
}

uint32_t DataViewHighlightManager::AddRef()
{
	return ++refCount;
}

uint32_t DataViewHighlightManager::Release()
{
	if (refCount > 0)
	{
		--refCount;
	}

	return refCount;
}

bool DataViewHighlightManager::DoMessage(cIGZMessage2* pMsg)
{
	cIGZMessage2Standard* pStandardMsg = static_cast<cIGZMessage2Standard*>(pMsg);
	const uint32_t type = pStandardMsg->GetType();

	if (type == kSC4MessageInsertOccupant)
	{
		cISC4Occupant* pOccupant = static_cast<cISC4Occupant*>(pStandardMsg->GetVoid1());

		if (occupantFilter && occupantFilter->IsOccupantIncluded(pOccupant))
		{
			OccupantInserted(pOccupant);
		}
	}
	else if (type == kSC4MessageRemoveOccupant)
	{
		cISC4Occupant* pOccupant = static_cast<cISC4Occupant*>(pStandardMsg->GetVoid1());

		if (occupantFilter && occupantFilter->IsOccupantIncluded(pOccupant))
		{
			OccupantRemoved(pOccupant);
		}
	}

	return true;
}

bool DataViewHighlightManager::IterateOccupantsCallback(cISC4Occupant* pOccupant, void* pContext)
{
	static_cast<DataViewHighlightManager*>(pContext)->OccupantInserted(pOccupant);
	return true;
}

void DataViewHighlightManager::OccupantInserted(cISC4Occupant* pOccupant)
{
	// Only add the item if it isn't already in the list.

	auto item = std::find(affectedOccupants.begin(), affectedOccupants.end(), pOccupant);

	if (item == affectedOccupants.end())
	{
		pOccupant->AddRef();
		affectedOccupants.push_back(pOccupant);
	}
}

void DataViewHighlightManager::OccupantRemoved(cISC4Occupant* pOccupant)
{
	auto item = std::find(affectedOccupants.begin(), affectedOccupants.end(), pOccupant);

	if (item != affectedOccupants.end())
	{
		(*item)->Release();
		affectedOccupants.erase(item);
	}
}

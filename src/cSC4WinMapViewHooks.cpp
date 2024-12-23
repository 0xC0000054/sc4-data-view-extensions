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

#include "cSC4WinMapViewHooks.h"
#include "cGZMessage.h"
#include "cIGZWin.h"
#include "cISC4AuraSimulator.h"
#include "cRZAutoRefCount.h"
#include "DebugUtil.h"
#include "GlobalPointers.h"
#include "DataViewHighlightManager.h"
#include "Patcher.h"
#include <array>

namespace
{
	typedef void(__thiscall* pfnOnViewModeButtonSelected)(void* pThis, uint32_t buttonID, uint32_t viewMode);
	static const pfnOnViewModeButtonSelected OnViewModeButtonSelected = reinterpret_cast<pfnOnViewModeButtonSelected>(0x7A0F30);

	typedef void(__thiscall* pfnUnhookUpdateMessages)(void* pThis);
	static const pfnUnhookUpdateMessages UnhookUpdateMessages = reinterpret_cast<pfnUnhookUpdateMessages>(0x79FB20);

	typedef bool(__thiscall* pfnUpdateHighlights)(void* pThis);
	static const pfnUpdateHighlights UpdateHighlights = reinterpret_cast<pfnUpdateHighlights>(0x7A1A00);

	typedef void(__thiscall* pfnAddNewHighlight)(void* pThis, cISC4Occupant* pOccupant, float coverageRadius);
	static const pfnAddNewHighlight AddNewHighlight = reinterpret_cast<pfnAddNewHighlight>(0x7A1220);

	static const uint32_t DataViewType_Moisture = 29;
	static const uint32_t DataViewType_ParkAura = 12;
	static const uint32_t DataViewType_LandmarkAura = 13;
	static const uint32_t DataViewType_TrafficVolume = 76;

	static const uint32_t MoistureButtonID1 = 0x5012;
	static const uint32_t MoistureButtonID2 = 0x5112;
	static const uint32_t ParkAuraButtonID1 = 0x5013;
	static const uint32_t ParkAuraButtonID2 = 0x5113;
	static const uint32_t LandmarkAuraButtonID1 = 0x5014;
	static const uint32_t LandmarkAuraButtonID2 = 0x5114;

	static constexpr std::array<uint32_t, 6> NewNotificationTargets =
	{
		MoistureButtonID1, MoistureButtonID2, ParkAuraButtonID1,
		ParkAuraButtonID2, LandmarkAuraButtonID1, LandmarkAuraButtonID2,
	};

	DataViewHighlightManager occupantHighlightManager;

	static const uintptr_t DoMessage_HandledRadioButton_Continue = 0x7A592B;
	static const uintptr_t DoMessage_UnhandledRadioButton_Continue = 0x7A571B;

	void NAKED_FUN DoMessageHook()
	{
		__asm
		{
			push eax
			push ecx
			push edx
			cmp eax, MoistureButtonID1
			jz moistureRadioButtonClick
			cmp eax, MoistureButtonID2
			jz moistureRadioButtonClick
			cmp eax, ParkAuraButtonID1
			jz parkRadioButtonClick
			cmp eax, ParkAuraButtonID2
			jz parkRadioButtonClick
			cmp eax, LandmarkAuraButtonID1
			jz landmarkRadioButtonClick
			cmp eax, LandmarkAuraButtonID2
			jz landmarkRadioButtonClick
			pop edx
			pop ecx
			pop eax
			cmp eax, 0x5101
			jmp DoMessage_UnhandledRadioButton_Continue

			moistureRadioButtonClick:
			push DataViewType_Moisture
			push MoistureButtonID1
			lea ecx, [esi + -8]
			call OnViewModeButtonSelected // (thiscall)
			pop edx
			pop ecx
			pop eax
			jmp DoMessage_HandledRadioButton_Continue

			parkRadioButtonClick:
			push DataViewType_ParkAura
			push ParkAuraButtonID1
			lea ecx, [esi + -8]
			call OnViewModeButtonSelected // (thiscall)
			pop edx
			pop ecx
			pop eax
			jmp DoMessage_HandledRadioButton_Continue

			landmarkRadioButtonClick:
			push DataViewType_LandmarkAura
			push LandmarkAuraButtonID1
			lea ecx, [esi + -8]
			call OnViewModeButtonSelected // (thiscall)
			pop edx
			pop ecx
			pop eax
			jmp DoMessage_HandledRadioButton_Continue
		}
	}

	static const uintptr_t Update_DataTypeSwitch_CaseDefault_Continue = 0x7A4375;
	static const uintptr_t Update_DataTypeSwitch_Continue = 0x7A30B3;
	static const uintptr_t Update_DataViewTypeLandmark_Continue = 0x7A331D;
	static const uintptr_t Update_NullPointer_Continue = 0x7A487C;

	cISC4SimGrid<int16_t>* GetLandmarkMap()
	{
		cISC4SimGrid<int16_t>* landmarkMap = nullptr;

		if (spAura)
		{
			landmarkMap = spAura->GetLandmarkMap();
		}

		return landmarkMap;
	}

	void NAKED_FUN UpdateHook()
	{
		__asm
		{
			cmp eax, DataViewType_LandmarkAura
			jz updateLandmarkDataView
			cmp eax, DataViewType_TrafficVolume
			ja dataTypeDefaultSwitchCase
			jmp Update_DataTypeSwitch_Continue

			dataTypeDefaultSwitchCase:
			jmp Update_DataTypeSwitch_CaseDefault_Continue

			updateLandmarkDataView:
			call GetLandmarkMap // (cdecl)
			test eax, eax
			jz nullPointer
			jmp Update_DataViewTypeLandmark_Continue

			nullPointer:
			jmp Update_NullPointer_Continue
		}
	}

	void RegisterCustomRadioButtonNotifications(cIGZWin* parent, cIGZUnknown* mapView)
	{
		cRZAutoRefCount<cIGZWin> pTargetWin;

		if (mapView->QueryInterface(GZIID_cIGZWin, pTargetWin.AsPPVoid()))
		{
			for (const uint32_t& buttonID : NewNotificationTargets)
			{
				cIGZWin* child = parent->GetChildWindowFromIDRecursive(buttonID);

				if (child)
				{
					child->SetNotificationTarget(pTargetWin);
				}
			}
		}
	}

	static const uintptr_t ShowWindow_Continue = 0x7A62E0;

	void NAKED_FUN HookedShowWindow()
	{
		__asm
		{
			push esi // this pointer
			push edi // parent window
			call RegisterCustomRadioButtonNotifications // (cdecl)
			add esp, 8
			xor ebx, ebx
			jmp ShowWindow_Continue
		}
	}

	void __fastcall InitHighlightManager(uint32_t highlightType, void* edxUnused)
	{
		occupantHighlightManager.Init(highlightType);
	}

	void ShutdownHighlightManager()
	{
		occupantHighlightManager.Shutdown();
	}

	void __fastcall RefreshHighlightedOccupants(void* pThis, void* edxUnused)
	{
		const std::vector<cISC4Occupant*>& affectedOccupants = occupantHighlightManager.GetAffectedOccupants();

		for (cISC4Occupant* pOccupant : affectedOccupants)
		{
			AddNewHighlight(pThis, pOccupant, 0.0f);
		}
	}

	static const uintptr_t HighlightOccupant_Switch_Continue = 0x7A1A6E;
	static const uintptr_t HighlightOccupant_SwitchCaseDefault_Continue = 0x7A1FD2;

	void NAKED_FUN UpdateHighlightsHook()
	{
		_asm
		{
			mov eax, [edi + 0x980]
			cmp eax, DataViewHighlightParkEffect
			jz refreshCustomHighlightedOccupants
			cmp eax, DataViewHighlightLandmarkEffect
			jz refreshCustomHighlightedOccupants
			jmp HighlightOccupant_Switch_Continue

			refreshCustomHighlightedOccupants:
			mov ecx, edi
			call RefreshHighlightedOccupants // (fastcall)
			jmp HighlightOccupant_SwitchCaseDefault_Continue
		}
	}


	static const uintptr_t SetDataView_DataViewInit_Hook_Continue = 0x7A53D5;

	void NAKED_FUN SetDataView_DataViewInit_Hook()
	{
		__asm
		{
			mov ecx, dword ptr[edi + 0x980] // highlight type
			call InitHighlightManager // (fastcall)
			mov ecx, edi
			call UpdateHighlights
			jmp SetDataView_DataViewInit_Hook_Continue
		}
	}

	static const uintptr_t SetDataView_DataViewShutdown_Hook_Continue = 0x7A4A1F;

	void NAKED_FUN SetDataView_DataViewShutdown_Hook()
	{
		__asm
		{
			call ShutdownHighlightManager // (cdecl)
			mov ecx, edi
			call UnhookUpdateMessages
			jmp SetDataView_DataViewShutdown_Hook_Continue
		}
	}

	void InstallDoMessageHook()
	{
		Patcher::InstallJump(0x7A5716, reinterpret_cast<uintptr_t>(&DoMessageHook));
	}

	void InstallMaxRadioButtonIDPatch()
	{
		// Change the original value 0x11 to 0x14
		Patcher::OverwriteMemoryUint32(0x7A0F3D, 0x14);
	}

	void InstallSetDataViewHooks()
	{
		Patcher::InstallJump(0x7A53D0, reinterpret_cast<uintptr_t>(&SetDataView_DataViewInit_Hook));
		Patcher::InstallJump(0x7A4A1A, reinterpret_cast<uintptr_t>(&SetDataView_DataViewShutdown_Hook));
	}

	void InstallShowWindowHook()
	{
		Patcher::InstallJump(0x7A62D8, reinterpret_cast<uintptr_t>(&HookedShowWindow));
	}

	void InstallUpdateHook()
	{
		Patcher::InstallJump(0x7A30AA, reinterpret_cast<uintptr_t>(&UpdateHook));
	}

	void InstallUpdateHighlightsHook()
	{
		Patcher::InstallJump(0x7A1A68, reinterpret_cast<uintptr_t>(&UpdateHighlightsHook));
	}
}

void cSC4WinMapViewHooks::Install()
{
	InstallDoMessageHook();
	InstallSetDataViewHooks();
	InstallShowWindowHook();
	InstallMaxRadioButtonIDPatch();
	InstallUpdateHook();
	InstallUpdateHighlightsHook();
}

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
#include "cRZBaseString.h"
#include <array>
#include <cstdio>

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
	static const uint32_t DataViewType_TransientAura = 14;
	static const uint32_t DataViewType_TrafficVolume = 76;

	// The radio buttons the DLL adds to the Data Views panel and the view mode each selects (the game shows the data
	// view exemplar 0x4A0B6800 + view mode). ID2 is the alternate button below the grid, as for the Maxis buttons.
	struct ExtraDataViewButton
	{
		uint32_t id1;
		uint32_t id2;
		uint32_t viewMode;
	};

	static constexpr std::array<ExtraDataViewButton, 4> ExtraDataViewButtons =
	{{
		{ 0x5012, 0x5112, DataViewType_Moisture },
		{ 0x5013, 0x5113, DataViewType_ParkAura },
		{ 0x5014, 0x5114, DataViewType_LandmarkAura },
		{ 0x5015, 0x5115, DataViewType_TransientAura },
	}};

	// Radio buttons 0x5001 up to this one are checked and unchecked as one group (the game uses 0x11).
	static constexpr uint32_t RadioButtonCount = 0x15;

	// Pages of the Data Views panel (ExtendedDataViewUI.dat). Row 0 of the button grid is the same on every page; rows
	// 1-4 are the page containers PageContainerBase + page (1, 2, ...), one shown at a time. Previous, next and the
	// page number sit in the last cell of row 0.
	static const uint32_t ButtonGridID = 0x8A2871D6;
	static const uint32_t PageContainerBase = 0x5C4DF100;
	static const uint32_t PreviousPageButtonID = 0x5C4DF1F0;
	static const uint32_t NextPageButtonID = 0x5C4DF1F1;
	static const uint32_t PageLabelID = 0x5C4DF1F2;
	static const uint32_t MaxPages = 16;

	static constexpr std::array<uint32_t, 2> PageButtons = { PreviousPageButtonID, NextPageButtonID };

	uint32_t currentPage = 1;

	DataViewHighlightManager occupantHighlightManager;

	uint32_t CountPages(cIGZWin* grid)
	{
		uint32_t pages = 0;

		while (pages < MaxPages && grid->GetChildWindowFromID(PageContainerBase + pages + 1))
		{
			pages++;
		}

		return pages;
	}

	void ShowPage(cIGZWin* panel, uint32_t page)
	{
		if (!panel)
		{
			return;
		}

		cIGZWin* grid = panel->GetChildWindowFromIDRecursive(ButtonGridID);

		if (!grid)
		{
			return;
		}

		const uint32_t pages = CountPages(grid);

		if (pages == 0)
		{
			return; // A data view UI without pages (the Maxis UI or an older ExtendedDataViewUI.dat).
		}

		if (page < 1 || page > pages)
		{
			page = 1;
		}

		currentPage = page;

		for (uint32_t i = 1; i <= pages; i++)
		{
			cIGZWin* container = grid->GetChildWindowFromID(PageContainerBase + i);

			if (i == page)
			{
				container->ShowWindow();
			}
			else
			{
				container->HideWindow();
			}
		}

		const uint32_t navigationIDs[] = { PreviousPageButtonID, NextPageButtonID, PageLabelID };

		for (const uint32_t id : navigationIDs)
		{
			cIGZWin* window = grid->GetChildWindowFromID(id);

			if (window)
			{
				if (pages > 1)
				{
					window->ShowWindow();
				}
				else
				{
					window->HideWindow();
				}
			}
		}

		cIGZWin* label = grid->GetChildWindowFromID(PageLabelID);

		if (label)
		{
			char text[32]{};
			std::snprintf(text, sizeof(text), "%u / %u", page, pages);
			label->SetCaption(cRZBaseString(text));
		}
	}

	// Called from DoMessageHook with the clicked button ID and the ESI register of the map view's DoMessage (the
	// map view is at ESI - 8, its Data Views panel at [ESI + 0x48]). Returns true when the click was handled.
	bool HandleCustomButtonClick(uint32_t buttonID, uint8_t* esi)
	{
		if (buttonID == PreviousPageButtonID || buttonID == NextPageButtonID)
		{
			cIGZWin* panel = *reinterpret_cast<cIGZWin**>(esi + 0x48);
			cIGZWin* grid = panel ? panel->GetChildWindowFromIDRecursive(ButtonGridID) : nullptr;
			const uint32_t pages = grid ? CountPages(grid) : 0;

			if (pages > 1)
			{
				uint32_t page = currentPage;

				if (buttonID == NextPageButtonID)
				{
					page = page >= pages ? 1 : page + 1;
				}
				else
				{
					page = page <= 1 ? pages : page - 1;
				}

				ShowPage(panel, page);
			}

			return true;
		}

		for (const ExtraDataViewButton& button : ExtraDataViewButtons)
		{
			if (buttonID == button.id1 || buttonID == button.id2)
			{
				OnViewModeButtonSelected(esi - 8, button.id1, button.viewMode);
				return true;
			}
		}

		return false;
	}

	static const uintptr_t DoMessage_HandledRadioButton_Continue = 0x7A592B;
	static const uintptr_t DoMessage_UnhandledRadioButton_Continue = 0x7A571B;

	void NAKED_FUN DoMessageHook()
	{
		__asm
		{
			push eax
			push ecx
			push edx
			push esi
			push eax
			call HandleCustomButtonClick // (cdecl)
			add esp, 8
			test al, al
			pop edx
			pop ecx
			pop eax
			jnz handled
			cmp eax, 0x5101
			jmp DoMessage_UnhandledRadioButton_Continue

			handled:
			jmp DoMessage_HandledRadioButton_Continue
		}
	}

	static const uintptr_t Update_DataTypeSwitch_CaseDefault_Continue = 0x7A4375;
	static const uintptr_t Update_DataTypeSwitch_Continue = 0x7A30B3;
	static const uintptr_t Update_Sint8Grid_Continue = 0x7A3240;
	static const uintptr_t Update_Sint16Grid_Continue = 0x7A331D;
	static const uintptr_t Update_NullPointer_Continue = 0x7A487C;

	cISC4SimGrid<int8_t>* GetTransientAuraGrid()
	{
		cISC4SimGrid<int8_t>* transientAuraGrid = nullptr;

		if (spAura)
		{
			transientAuraGrid = spAura->GetTransientAuraGrid();
		}

		return transientAuraGrid;
	}

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
			cmp eax, DataViewType_TransientAura
			jz updateTransientAuraDataView
			cmp eax, DataViewType_TrafficVolume
			ja dataTypeDefaultSwitchCase
			jmp Update_DataTypeSwitch_Continue

			dataTypeDefaultSwitchCase:
			jmp Update_DataTypeSwitch_CaseDefault_Continue

			updateLandmarkDataView:
			call GetLandmarkMap // (cdecl)
			test eax, eax
			jz nullPointer
			jmp Update_Sint16Grid_Continue

			updateTransientAuraDataView:
			call GetTransientAuraGrid // (cdecl)
			test eax, eax
			jz nullPointer
			jmp Update_Sint8Grid_Continue

			nullPointer:
			jmp Update_NullPointer_Continue
		}
	}

	void SetNotificationTarget(cIGZWin* parent, cIGZWin* target, uint32_t buttonID)
	{
		cIGZWin* child = parent->GetChildWindowFromIDRecursive(buttonID);

		if (child)
		{
			child->SetNotificationTarget(target);
		}
	}

	void RegisterCustomRadioButtonNotifications(cIGZWin* parent, cIGZUnknown* mapView)
	{
		cRZAutoRefCount<cIGZWin> pTargetWin;

		if (mapView->QueryInterface(GZIID_cIGZWin, pTargetWin.AsPPVoid()))
		{
			for (const ExtraDataViewButton& button : ExtraDataViewButtons)
			{
				SetNotificationTarget(parent, pTargetWin, button.id1);
				SetNotificationTarget(parent, pTargetWin, button.id2);
			}

			for (const uint32_t& buttonID : PageButtons)
			{
				SetNotificationTarget(parent, pTargetWin, buttonID);
			}
		}

		ShowPage(parent, currentPage);
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
		// Change the original value 0x11 in the three calls that check one radio button and uncheck the others
		// (OnViewModeButtonSelected and the Desirability and Traffic buttons, which select their last sub view).
		Patcher::OverwriteMemoryUint32(0x7A0F3D, RadioButtonCount);
		Patcher::OverwriteMemoryUint32(0x7A57DF, RadioButtonCount);
		Patcher::OverwriteMemoryUint32(0x7A5879, RadioButtonCount);
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

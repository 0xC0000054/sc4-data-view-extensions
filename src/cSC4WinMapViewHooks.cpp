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
#include "cGZPersistResourceKey.h"
#include "cIGZPersistResourceManager.h"
#include "cIGZVariant.h"
#include "cISCProperty.h"
#include "cISCPropertyHolder.h"
#include "cISCResExemplar.h"
#include "GZServPtrs.h"
#include "Logger.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

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

	// Buttons of the 1.0 ExtendedDataViewUI.dat and the view mode each selects (the game shows the data view exemplar
	// 0x4A0B6800 + view mode). ID2 is the alternate button below the grid, as for the Maxis buttons.
	struct LegacyDataViewButton
	{
		uint32_t id1;
		uint32_t id2;
		uint32_t viewMode;
	};

	static constexpr std::array<LegacyDataViewButton, 4> LegacyDataViewButtons =
	{{
		{ 0x5012, 0x5112, DataViewType_Moisture },
		{ 0x5013, 0x5113, DataViewType_ParkAura },
		{ 0x5014, 0x5114, DataViewType_LandmarkAura },
		{ 0x5015, 0x5115, DataViewType_TransientAura },
	}};

	// Data View Button Cell: the data views with this property in their exemplar (type 0x6534284A, group 0x690F693F,
	// instance 0x4A0B6800 + view mode) get a button in the Data Views panel, in that cell. The Maxis data views use
	// the Maxis buttons, every other one the button CustomButtonBase + view mode, which must be in the UI
	// (ExtendedDataViewUI.dat): buttons created at run time do not receive clicks.
	static const uint32_t ExemplarType = 0x6534284A;
	static const uint32_t DataViewGroup = 0x690F693F;
	static const uint32_t DataViewInstanceBase = 0x4A0B6800;
	static const uint32_t PropDataViewButtonCell = 0x5C4DF150;
	static const uint32_t CustomButtonBase = 0x5200;
	static const uint32_t MaxViewMode = 0xFF;

	struct MaxisDataViewButton
	{
		uint32_t viewMode;
		uint32_t buttonID;
	};

	// The view mode of each Maxis button, from the button switch of cSC4WinMapView::DoMessage (0x7A5716). Desirability
	// and Traffic open their sub views; their cell comes from the R$ Desirability and Traffic data views.
	static constexpr std::array<MaxisDataViewButton, 16> MaxisDataViewButtons =
	{{
		{ 0x05, 0x5002 }, { 0x03, 0x5003 }, { 0x1B, 0x5004 }, { 0x28, 0x5005 },
		{ 0x0A, 0x5006 }, { 0x07, 0x5007 }, { 0x09, 0x5008 }, { 0x20, 0x5009 },
		{ 0x02, 0x500A }, { 0x01, 0x500B }, { 0x1E, 0x500C }, { 0x1C, 0x500D },
		{ 0x08, 0x500E }, { 0x18, 0x500F }, { 0x19, 0x5010 }, { 0x1A, 0x5011 },
	}};

	// Radio buttons 0x5001 up to 0x5000 + this count are checked and unchecked as one group (the game uses 0x11).
	static constexpr uint32_t RadioButtonCount = CustomButtonBase + MaxViewMode - 0x5000;

	// The button grid (4 columns of the Maxis cells, 5 rows of 18 pixels). Row 0 holds All Off and the page controls
	// on every page; cell row r is grid row r % 4 + 1 of page r / 4 + 1.
	static const uint32_t ButtonGridID = 0x8A2871D6;
	static const uint32_t PreviousPageButtonID = 0x5C4DF1F0;
	static const uint32_t NextPageButtonID = 0x5C4DF1F1;
	static const uint32_t PageLabelID = 0x5C4DF1F2;
	static const int32_t GridColumns = 4;
	static const int32_t PageRows = 4;
	static const int32_t MaxPages = 16;
	static const int32_t RowHeight = 18;
	static constexpr int32_t ColumnLeft[GridColumns] = { 4, 128, 212, 356 };
	static constexpr int32_t ColumnRight[GridColumns] = { 124, 212, 352, 502 };

	static constexpr std::array<uint32_t, 2> PageButtons = { PreviousPageButtonID, NextPageButtonID };

	struct PlacedButton
	{
		uint32_t buttonID;
		int32_t page;
	};

	std::vector<PlacedButton> placedButtons;
	int32_t pageCount = 0;
	int32_t currentPage = 1;
	bool layoutLogged = false;

	DataViewHighlightManager occupantHighlightManager;

	uint32_t MaxisButtonForViewMode(uint32_t viewMode)
	{
		for (const MaxisDataViewButton& entry : MaxisDataViewButtons)
		{
			if (entry.viewMode == viewMode)
			{
				return entry.buttonID;
			}
		}

		return 0;
	}

	bool ReadButtonCell(cIGZPersistResourceManager* resources, uint32_t viewMode, int32_t& column, int32_t& row)
	{
		const cGZPersistResourceKey key(ExemplarType, DataViewGroup, DataViewInstanceBase + viewMode);

		if (!resources->TestForKey(key))
		{
			return false;
		}

		cRZAutoRefCount<cISCResExemplar> exemplar;

		if (!resources->GetResource(key, GZIID_cISCResExemplar, exemplar.AsPPVoid(), 0, nullptr) || !exemplar)
		{
			return false;
		}

		const cISCPropertyHolder* holder = exemplar->AsISCPropertyHolder();
		const cISCProperty* property = holder ? holder->GetProperty(PropDataViewButtonCell) : nullptr;
		const cIGZVariant* value = property ? property->GetPropertyValue() : nullptr;

		if (!value || value->GetCount() < 2)
		{
			return false;
		}

		switch (value->GetType())
		{
		case cIGZVariant::Uint8Array:
			column = value->RefUint8()[0];
			row = value->RefUint8()[1];
			return true;
		case cIGZVariant::Uint32Array:
			column = static_cast<int32_t>(value->RefUint32()[0]);
			row = static_cast<int32_t>(value->RefUint32()[1]);
			return true;
		case cIGZVariant::Sint32Array:
			column = value->RefSint32()[0];
			row = value->RefSint32()[1];
			return true;
		default:
			return false;
		}
	}

	// Places every button with a Data View Button Cell, hides the custom buttons without one and counts the pages.
	// Returns false when no data view has the property: the panel is then left as the UI builds it.
	bool LayOutButtons(cIGZWin* grid)
	{
		cIGZPersistResourceManagerPtr resources;

		placedButtons.clear();
		pageCount = 0;

		if (!resources)
		{
			return false;
		}

		bool taken[MaxPages * PageRows][GridColumns]{};
		uint32_t listed = 0;
		uint32_t placed = 0;
		uint32_t missing = 0;
		uint32_t moved = 0;
		uint32_t kept = 0;

		struct Pending
		{
			uint32_t viewMode;
			cIGZWin* button;
			int32_t column;
			int32_t row;
			bool hasCell;
		};

		std::vector<Pending> pending;

		for (uint32_t viewMode = 1; viewMode <= MaxViewMode; viewMode++)
		{
			const uint32_t maxisButton = MaxisButtonForViewMode(viewMode);
			cIGZWin* button = grid->GetChildWindowFromID(maxisButton ? maxisButton : CustomButtonBase + viewMode);
			int32_t column = 0;
			int32_t row = 0;
			const bool hasCell = ReadButtonCell(resources, viewMode, column, row);

			if (hasCell)
			{
				listed++;
			}

			if (button)
			{
				pending.push_back({ viewMode, button, column, row, hasCell });
			}
			else if (hasCell)
			{
				missing++;
				Logger::GetInstance().WriteLineFormatted(
					LogLevel::Error,
					"Data view 0x%08X has a Data View Button Cell but ExtendedDataViewUI.dat has no button 0x%04X for it.",
					DataViewInstanceBase + viewMode,
					CustomButtonBase + viewMode);
			}
		}

		if (listed == 0)
		{
			return false;
		}

		// Cells inside the grid first, then the rest in the next free cell.
		for (Pending& item : pending)
		{
			if (!item.hasCell)
			{
				continue;
			}

			if (item.column >= 0 && item.column < GridColumns && item.row >= 0 && item.row < MaxPages * PageRows
				&& !taken[item.row][item.column])
			{
				taken[item.row][item.column] = true;
			}
			else
			{
				item.row = -1;
			}
		}

		for (Pending& item : pending)
		{
			if (item.hasCell && item.row < 0)
			{
				for (int32_t row = 0; row < MaxPages * PageRows && item.row < 0; row++)
				{
					for (int32_t column = 0; column < GridColumns; column++)
					{
						if (!taken[row][column])
						{
							taken[row][column] = true;
							item.row = row;
							item.column = column;
							moved++;
							break;
						}
					}
				}
			}
		}

		for (const Pending& item : pending)
		{
			const uint32_t buttonID = item.button->GetID();

			if (item.hasCell && item.row >= 0)
			{
				const int32_t top = 1 + RowHeight * (item.row % PageRows + 1);
				item.button->SetArea(ColumnLeft[item.column], top, ColumnRight[item.column], top + RowHeight);

				const int32_t page = item.row / PageRows + 1;
				placedButtons.push_back({ buttonID, page });
				pageCount = std::max(pageCount, page);
				placed++;
			}
			else if (buttonID < CustomButtonBase)
			{
				// A Maxis button whose data view has no cell (overridden by another mod): it stays on page 1, where
				// the UI has it.
				placedButtons.push_back({ buttonID, 1 });
				pageCount = std::max(pageCount, 1);
				kept++;
				Logger::GetInstance().WriteLineFormatted(
					LogLevel::Error,
					"Data view 0x%08X has no Data View Button Cell (another plugin overrides it?): its Maxis button 0x%04X stays on page 1.",
					DataViewInstanceBase + item.viewMode,
					buttonID);
			}
			else
			{
				item.button->HideWindow();
			}
		}

		if (!layoutLogged)
		{
			layoutLogged = true;
			Logger::GetInstance().WriteLineFormatted(
				LogLevel::Info,
				"Data Views panel: %u data views with a button cell, %u buttons placed (%u moved to a free cell), %u Maxis buttons kept, %u buttons missing; %d page(s).",
				listed,
				placed,
				moved,
				kept,
				missing,
				pageCount);
		}

		return true;
	}

	void ShowPage(cIGZWin* grid, int32_t page)
	{
		if (page < 1 || page > pageCount)
		{
			page = 1;
		}

		currentPage = page;

		for (const PlacedButton& item : placedButtons)
		{
			cIGZWin* button = grid->GetChildWindowFromID(item.buttonID);

			if (button)
			{
				if (item.page == page)
				{
					button->ShowWindow();
				}
				else
				{
					button->HideWindow();
				}
			}
		}

		const uint32_t navigationIDs[] = { PreviousPageButtonID, NextPageButtonID, PageLabelID };

		for (const uint32_t id : navigationIDs)
		{
			cIGZWin* window = grid->GetChildWindowFromID(id);

			if (window)
			{
				if (pageCount > 1)
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
			std::snprintf(text, sizeof(text), "%d / %d", page, pageCount);
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

			if (grid && pageCount > 1)
			{
				int32_t page = currentPage;

				if (buttonID == NextPageButtonID)
				{
					page = page >= pageCount ? 1 : page + 1;
				}
				else
				{
					page = page <= 1 ? pageCount : page - 1;
				}

				ShowPage(grid, page);
			}

			return true;
		}

		if (buttonID > CustomButtonBase && buttonID <= CustomButtonBase + MaxViewMode)
		{
			OnViewModeButtonSelected(esi - 8, buttonID, buttonID - CustomButtonBase);
			return true;
		}

		for (const LegacyDataViewButton& button : LegacyDataViewButtons)
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
			for (uint32_t viewMode = 1; viewMode <= MaxViewMode; viewMode++)
			{
				SetNotificationTarget(parent, pTargetWin, CustomButtonBase + viewMode);
			}

			for (const LegacyDataViewButton& button : LegacyDataViewButtons)
			{
				SetNotificationTarget(parent, pTargetWin, button.id1);
				SetNotificationTarget(parent, pTargetWin, button.id2);
			}

			for (const uint32_t& buttonID : PageButtons)
			{
				SetNotificationTarget(parent, pTargetWin, buttonID);
			}
		}

		cIGZWin* grid = parent->GetChildWindowFromIDRecursive(ButtonGridID);

		if (grid && LayOutButtons(grid))
		{
			ShowPage(grid, currentPage);
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

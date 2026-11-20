# sc4-data-view-extensions

A DLL Plugin for SimCity 4 that extends the game's data views.

This plugin includes a modified data view UI that adds dedicated radio buttons
for Moisture, Park Aura, Landmark Aura and Transient Aura data views.
It also gives a button to the four zone desirability views of the game, which had none (Crime and Flammability,
duplicates of the Crime and Fire Hazard views, get none; the per-wealth desirability and traffic volume views stay in the Desirability and Traffic sub views).
Each button is placed by the Data View Button Cell of its data view, the Data Views panel keeps the size and position of
the Maxis panel and its buttons are split in pages (see [Data View Button Cell](#data-view-button-cell)). The Maxis
buttons keep their IDs and cells, so the UI will work with existing mods that re-purpose the Maxis data view radio
buttons.

The Transient Aura data view has its own button (page 2) and no longer replaces the Garbage Pollution data view.

## Differences from Cori's Park Aura Mod

This mod adds a new radio button instead of replacing the Garbage Pollution data view.
The highlighted items on the Park Aura (and Landmark Aura) views are Parks/Landmarks
instead of Garbage facilities.

## References

See [That Unused Moisture Data View](https://community.simtropolis.com/forums/topic/70765-that-unused-moisture-data-view/) for a description of how the moisture data view works.    
See [Cori's Park Aura](https://community.simtropolis.com/files/file/33412-coris-parks-aura/) mod page
for a description of how the park aura data view works.

## Maxis Transit Switch Highlight Mode

This data view highlight mode uses the value 9, but Maxis didn't document it in Ingred.ini.
It is used in the NAM Transit Station Data View plugin.

# New Features for Data View Modders

## Data View Sources

The DLL adds the following new data sources. The Data Source Property Value item corresponds
to the numeric value of the game's "DataView: Data source" property (0x4A0B47E5).

| Name | Data Source Property Value | Description |
|------|----------------------------|-------------|
| Landmark Aura | 13 | A data source using the game's landmark aura data. |
| Transient Aura | 14 | A data source using the game's transient aura data. |

## New Data View Highlight Modes

The DLL adds the following new highlight modes. The Highlight Mode Property Value item corresponds
to the numeric value of the game's "DataView: Highlight mode" property (0x4A0B47E9).

| Name | Highlight Mode Property Value | Description |
|------|-------------------------------|-------------|
| Park | 10 | Buildings that have a Park Effect exemplar property are highlighted. |
| Landmark | 11 | Buildings that have a Landmark Effect exemplar property are highlighted. |

## Data View Button Cell

`dat/DataViews.dat` holds every data view of the game (the 47 exemplars of `SimCity_1.dat`, type `0x6534284A`,
group `0x690F693F`, with a *DataView: Data source*) and the ones of this DLL (Moisture, Park Aura, Landmark Aura,
Transient Aura). The data views that get a button carry the property **Data View Button Cell** (`0x5C4DF150`, Uint32 x 2:
column 0-3, row 0-63), similar to SC4SIM's Graph Button Cell. The view mode of a data view is its instance minus
`0x4A0B6800`.

When the Data Views panel opens, the DLL reads the cell of every data view and places its button there:

* The Maxis data views use the Maxis buttons, which keep their IDs (mods that re-purpose them keep working);
  Desirability and Traffic take the cells of the R$ Desirability and Traffic data views and still open their sub views.
* Every other data view uses the button `0x5200 + view mode`. A click selects the view mode, nothing to add to the DLL.
* A data view with a cell whose button is not in `ExtendedDataViewUI.dat` (for example one added by another plugin)
  gets its button created at run time from the button template UI `0x00000000 / 0x96A006B0 / 0x5C4DF300` (a copy of the
  Maxis Fire Hazard button, ID `0x5C4DF301`, in `ExtendedDataViewUI.dat`). The DLL gives it the ID `0x5200 + view mode`,
  a caption and the map view as notification target, like the buttons of the UI, so its clicks select the view mode.
  The caption is the LTEXT `0x6A554AFD / 0x5C4DF200 + view mode` when there is one, else the exemplar name without
  `DataView: `. The log line of the panel counts the buttons created at run time.
* A cell outside the grid or already taken sends the button to the next free cell. A custom button whose data view has
  no cell is hidden; a Maxis button whose data view has no cell (overridden by a plugin without the property) stays on
  page 1, with a line in `SC4DataViewExtensions.log`. When no data view has the property, the panel is left as the UI
  builds it.

The panel keeps the size and position of the Maxis panel. Row 0 of the button grid (window `0x8A2871D6`) holds All Off
and the page controls `< Previous`, page number, `Next >` (`0x5C4DF1F0`, `0x5C4DF1F2`, `0x5C4DF1F1`) on every page;
cell row r is grid row r % 4 + 1 of page r / 4 + 1. Page 1 is the Maxis layout; page 2 holds Moisture, the four zone desirability
data views (data views of the game without a button), Park Aura, Landmark Aura and Transient Aura. The game's Crime
(`0x04`) and Flammability (`0x06`) data views get no button: they read the same grids as the Crime and Fire Hazard
buttons, with no legend, and Flammability's colour ramp is fully transparent. The page controls are shown only when there is more than one page.

Both DAT files are built by `tools/make_data_view_ui.py` (cells in `tools/data_view_layout.py`) from the game's
`SimCity_1.dat` and the 1.0 DAT files in `dat/source`:

    python tools/make_data_view_ui.py "<SimCity 4>/SimCity_1.dat" "dat/source/ExtendedDataViewUI-1.0.dat" "dat/source/z_DataView - TransientAura.dat" dat

To add a data view: give its exemplar a Data View Button Cell (and, for its caption, an LTEXT
`0x6A554AFD / 0x5C4DF200 + view mode`); the DLL creates the button. A line in `tools/data_view_layout.py` also writes the
button into the UI, as for the data views of this DLL.

# System Requirements

* SimCity 4 version 641
* Windows 10 or later

The plugin may work on Windows 7 or later, but I do not have the ability to test that.

# Installation

1. Close SimCity 4.
2. Copy `SC4DataViewExtensions.dll`, `dat/ExtendedDataViewUI.dat` and `dat/DataViews.dat` into the top-level of the Plugins folder in the SimCity 4 installation directory or Documents/SimCity 4 directory.
3. Remove `z_DataView - TransientAura.dat` if it is installed: the Transient Aura data view is now in `DataViews.dat`, and the old file would still replace the Garbage Pollution data view.
4. Start SimCity 4.

# Troubleshooting

The plugin should write a `SC4DataViewExtensions.log` file in the same folder as the plugin.    
The log contains status information for the most recent run of the plugin.

# License

This project is licensed under the terms of the MIT License.    
See [LICENSE.txt](LICENSE.txt) for more information.

## 3rd party code

[gzcom-dll](https://github.com/nsgomez/gzcom-dll/tree/master) Located in the vendor folder, MIT License.    
[Windows Implementation Library](https://github.com/microsoft/wil) Headers located in the vendor/wil folder (no vcpkg needed), MIT License.    
[SC4Fix](https://github.com/nsgomez/sc4fix) - MIT License    

## Mod Credits

The Moisture view data panel is based on [Cori's Moisture Data View](https://community.simtropolis.com/files/file/31154-coris-dataview-moisture/).    
The Parks Aura, Landmarks Aura and Transient Aura data panels are based on the [Cori's Parks Aura Data View](https://community.simtropolis.com/files/file/33412-coris-parks-aura/). 

# Source Code

## Prerequisites

* Visual Studio 2022

## Building the plugin

* Open the solution in the `src` folder
* Update the post build events to copy the build output to you SimCity 4 application plugins folder.
* Build the solution

## Debugging the plugin

Visual Studio can be configured to launch SimCity 4 on the Debugging page of the project properties.
I configured the debugger to launch the game in a window with the following command line:    
`-intro:off -CPUCount:1 -CPUPriority:high -w -CustomResolution:enabled -r1920x1080x32`

You may need to adjust the resolution for your screen.

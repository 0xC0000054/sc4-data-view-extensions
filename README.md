# sc4-data-view-extensions

A DLL Plugin for SimCity 4 that extends the game's data views.

This plugin includes a modified data view UI that adds dedicated radio buttons
for Moisture, Park Aura, Landmark Aura and Transient Aura data views.
The Data Views panel keeps the size and position of the Maxis panel and its buttons are split in pages
(see [Data View Pages](#data-view-pages)). The Maxis buttons keep their IDs and cells, so the UI will work with
existing mods that re-purpose the Maxis data view radio buttons.

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

## Data View Pages

The button grid of the Data Views panel (window `0x8A2871D6`) has 4 columns and 5 rows, all of them used by the
Maxis buttons and the new ones. Instead of making the panel taller (the game keeps the panel's bottom edge, so a taller
panel would cover the city view), the rows below the first are split in pages:

* Row 0 is the same on every page: All Off, Moisture, Park Aura and the page controls `<`, page number, `>`
  (`0x5C4DF1F0`, `0x5C4DF1F2`, `0x5C4DF1F1`), in the cell of the right column.
* Rows 1-4 are the pages: container windows `0x5C4DF100 + page` (`0x5C4DF101`, `0x5C4DF102`, ...). Page 1 holds the 16
  Maxis data views in their Maxis cells, page 2 Landmark Aura and Transient Aura.

The DLL counts the page containers, shows one at a time and wraps around from the last page to the first. The page
controls are shown only when there is more than one page. A UI without page containers (the Maxis UI or the 1.0
`ExtendedDataViewUI.dat`) is left as it is.

`ExtendedDataViewUI.dat` is built by `tools/make_data_view_ui.py` from the 1.0 UI and the 1.0 Transient Aura DAT
(`dat/source`):

    python tools/make_data_view_ui.py "dat/source/ExtendedDataViewUI-1.0.dat" "dat/source/z_DataView - TransientAura.dat" dat

To add a data view: put its button (a copy of a radio button, a free ID up to `0x5015` or a new one with a matching
alternate `0x51xx` ID) in a page container, add the button and its view mode to `ExtraDataViewButtons` in
`cSC4WinMapViewHooks.cpp` (raise `RadioButtonCount` if the ID is above `0x5015`) and ship the data view exemplar
`0x6534284A / 0x690F693F / 0x4A0B6800 + view mode`. A new page is a new container `0x5C4DF100 + page` with the same
area as the others.

# System Requirements

* SimCity 4 version 641
* Windows 10 or later

The plugin may work on Windows 7 or later, but I do not have the ability to test that.

# Installation

1. Close SimCity 4.
2. Copy `SC4DataViewExtensions.dll` and `dat/ExtendedDataViewUI.dat` into the top-level of the Plugins folder in the SimCity 4 installation directory or Documents/SimCity 4 directory.
3. Remove `z_DataView - TransientAura.dat` if it is installed: the Transient Aura data view is now in `ExtendedDataViewUI.dat`, and the old file would still replace the Garbage Pollution data view.
4. Start SimCity 4.

# Troubleshooting

The plugin should write a `SC4DataViewExtensions.log` file in the same folder as the plugin.    
The log contains status information for the most recent run of the plugin.

# License

This project is licensed under the terms of the MIT License.    
See [LICENSE.txt](LICENSE.txt) for more information.

## 3rd party code

[gzcom-dll](https://github.com/nsgomez/gzcom-dll/tree/master) Located in the vendor folder, MIT License.    
[Windows Implementation Library](https://github.com/microsoft/wil) - MIT License    
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

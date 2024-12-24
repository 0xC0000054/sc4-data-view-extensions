# sc4-data-view-extensions

A DLL Plugin for SimCity 4 that extends the game's data views.

## Features

### New data view sources

The DLL adds the following new data sources. The Data Source Property Value item corresponds
to the numeric value of the game's "DataView: Data source" property (0x4A0B47E5).

| Name | Data Source Property Value | Description |
|-------------------------------------------------|
| Landmark Aura | 13 | A data source using the game's landmark aura data. |
| Transient Aura | 14 | A data source using the game's transient aura data. |

### New data view highlight modes

The DLL adds the following new highlight modes. The Highlight Mode Property Value item corresponds
to the numeric value of the game's "DataView: Highlight mode" property (0x4A0B47E9).

| Name | Highlight Mode Property Value | Description |
|----------------------------------------------------|
| Park | 10 | Buildings that have a Park Effect exemplar property are highlighted. |
| Landmark | 11 | Buildings that have a Landmark Effect exemplar property are highlighted. |

## System Requirements

* SimCity 4 version 641
* Windows 10 or later

The plugin may work on Windows 7 or later, but I do not have the ability to test that.

## Installation

1. Close SimCity 4.
2. Copy `SC4DataViewExtensions.dll` and `ExtendedDataViewUI.dat` into the top-level of the Plugins folder in the SimCity 4 installation directory or Documents/SimCity 4 directory.
3. Start SimCity 4.

## Troubleshooting

The plugin should write a `SC4DataViewExtensions.log` file in the same folder as the plugin.    
The log contains status information for the most recent run of the plugin.

# License

This project is licensed under the terms of the MIT License.    
See [LICENSE.txt](LICENSE.txt) for more information.

## 3rd party code

[gzcom-dll](https://github.com/nsgomez/gzcom-dll/tree/master) Located in the vendor folder, MIT License.    
[Windows Implementation Library](https://github.com/microsoft/wil) - MIT License    
[SC4Fix](https://github.com/nsgomez/sc4fix) - MIT License    

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

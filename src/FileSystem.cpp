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

#include "FileSystem.h"

#include <Windows.h>
#include "wil/resource.h"
#include "wil/win32_helpers.h"

using namespace std::string_view_literals;

namespace
{
	std::filesystem::path GetModulePath()
	{
		wil::unique_cotaskmem_string modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

		return std::filesystem::path(modulePath.get());
	}

	std::filesystem::path GetDllFolderPath()
	{
		static std::filesystem::path dllFolderPath = GetModulePath().parent_path();

		return dllFolderPath;
	}
}

std::filesystem::path FileSystem::GetLogFilePath()
{
	std::filesystem::path path = GetDllFolderPath();
	path /= L"SC4DataViewExtensions.log"sv;

	return path;
}

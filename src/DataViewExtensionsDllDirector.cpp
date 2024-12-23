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
#include "FileSystem.h"
#include "GlobalPointers.h"
#include "Logger.h"
#include "SC4VersionDetection.h"
#include "version.h"
#include "cIGZCOM.h"
#include "cIGZFrameWork.h"
#include "cIGZMessage2Standard.h"
#include "cIGZMessageServer2.h"
#include "cISC4City.h"
#include "cISC4SimGrid.h"
#include "cRZMessage2COMDirector.h"
#include "GZServPtrs.h"
#include "wil/result.h"
#include <array>

static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

static constexpr std::array<uint32_t, 2> RequiredNotifications
{
	kSC4MessagePostCityInit,
	kSC4MessagePreCityShutdown,
};

static constexpr uint32_t kDataViewExtensionsDllDirector = 0xEFB723C6;

cISC4AuraSimulator* spAura = nullptr;
cISC4OccupantManager* spOccupantManager = nullptr;

class DataViewExtensionsDllDirector final : public cRZMessage2COMDirector
{
public:
	DataViewExtensionsDllDirector()
	{
		Logger& logger = Logger::GetInstance();
		logger.Init(FileSystem::GetLogFilePath(), LogLevel::Error);
		logger.WriteLogFileHeader("SC4DataViewExtensions v" PLUGIN_VERSION_STR);
	}

	uint32_t GetDirectorID() const
	{
		return kDataViewExtensionsDllDirector;
	}

	bool OnStart(cIGZCOM* pCOM)
	{
		Logger& logger = Logger::GetInstance();

		const uint16_t gameVersion = SC4VersionDetection::GetGameVersion();

		if (gameVersion == 641)
		{
			try
			{
				cSC4WinMapViewHooks::Install();
				logger.WriteLine(LogLevel::Info, "Installed the data view patches.");
				mpFrameWork->AddHook(this);
			}
			catch (const std::exception& e)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to install the data view patches: %s",
					e.what());
			}
		}
		else
		{
			logger.WriteLineFormatted(LogLevel::Error, "Unsupported game version %u", gameVersion);
		}

		return true;
	}

	bool PostAppInit()
	{
		cIGZMessageServer2Ptr pMsgServ;
		if (pMsgServ)
		{
			for (uint32_t messageID : RequiredNotifications)
			{
				pMsgServ->AddNotification(this, messageID);
			}
		}

		return true;
	}

	bool DoMessage(cIGZMessage2* pMsg)
	{
		switch (pMsg->GetType())
		{
		case kSC4MessagePostCityInit:
			PostCityInit(reinterpret_cast<cIGZMessage2Standard*>(pMsg));
			break;
		case kSC4MessagePreCityShutdown:
			PreCityShutdown();
			break;
		}

		return true;
	}

	void PostCityInit(cIGZMessage2Standard* pStandardMsg)
	{
		cISC4City* pCity = static_cast<cISC4City*>(pStandardMsg->GetVoid1());

		if (pCity)
		{
			spAura = pCity->GetAuraSimulator();
			spOccupantManager = pCity->GetOccupantManager();
		}
	}

	void PreCityShutdown()
	{
		spAura = nullptr;
		spOccupantManager = nullptr;
	}
};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static DataViewExtensionsDllDirector sDirector;
	return &sDirector;
}
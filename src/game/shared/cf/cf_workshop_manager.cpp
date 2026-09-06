//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Custom Fortress 2 Workshop Manager Implementation
//
//=============================================================================

#include "cbase.h"
#include "cf_workshop_manager.h"
#include "workshop/ugc_utils.h"
#include "filesystem.h"
#include "tier2/fileutils.h"
#include "tier1/convar.h"
#include "tier0/icommandline.h"
#include "tier0/threadtools.h"
#include "econ/econ_item_schema.h"
#include "tf_item_inventory.h"

#ifdef CLIENT_DLL
#include "c_tf_player.h"
#else
#include "tf_player.h"
#include "steam/steam_gameserver.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Global instance
static CCFWorkshopManager g_CFWorkshopManager;

// Global accessor
CCFWorkshopManager* CFWorkshop()
{
	return &g_CFWorkshopManager;
}

#ifdef CLIENT_DLL
// Request content reload - deferred to Update() to avoid crashes during Steam callbacks
static void RequestContentReload()
{
	CFWorkshop()->SetContentReloadPending(true);
}

// Request HUD reload - deferred to Update()
static void RequestHUDReload()
{
	CFWorkshop()->SetHUDReloadPending(true);
}

// Actually perform the content reload - only call this from Update()
static void PerformContentReload()
{
	// Flush model cache to force reload from new search paths
	engine->ExecuteClientCmd("r_flushlod");

	// Reload all materials
	engine->ExecuteClientCmd( "mat_reloadallmaterials" );
	
	// Restart sound system to reload sounds
	engine->ExecuteClientCmd("snd_restart");

	// Reload particle systems
	engine->ExecuteClientCmd("ent_fire info_particle_system stop;wait 120;ent_fire info_particle_system start");
	
	Msg("[CF Workshop] Content reloaded - models, materials, and sounds refreshed.\n");
}

// Actually perform the HUD reload - only call this from Update()
static void PerformHUDReload()
{
	// Reload the HUD scheme and layout
	engine->ExecuteClientCmd("hud_reloadscheme");
	
	Msg("[CF Workshop] HUD reloaded.\n");
}
#endif

// Workshop tag definitions - must match .res checkbox names and Steam Workshop tags
const CFWorkshopTagInfo_t g_CFWorkshopTags[CF_WORKSHOP_TAG_COUNT] = {
	// MOD TYPE category
	{ CF_WORKSHOP_TAGIDX_MAP,           CF_WORKSHOP_TAG_MOD_MAP,           "Mod Type" },
	{ CF_WORKSHOP_TAGIDX_SKIN,          CF_WORKSHOP_TAG_MOD_SKIN,          "Mod Type" },
	{ CF_WORKSHOP_TAGIDX_WEAPON,        CF_WORKSHOP_TAG_MOD_WEAPON,        "Mod Type" },
	{ CF_WORKSHOP_TAGIDX_GUIS,          CF_WORKSHOP_TAG_MOD_GUI,           "Mod Type" },
	{ CF_WORKSHOP_TAGIDX_PARTICLES,     CF_WORKSHOP_TAG_MOD_PARTICLES,     "Mod Type" },
	{ CF_WORKSHOP_TAGIDX_SOUNDS,        CF_WORKSHOP_TAG_MOD_SOUNDS,        "Mod Type" },
	{ CF_WORKSHOP_TAGIDX_SPRAYS,        CF_WORKSHOP_TAG_MOD_SPRAYS,        "Mod Type" },
	{ CF_WORKSHOP_TAGIDX_MISC,          CF_WORKSHOP_TAG_MOD_MISC,          "Mod Type" },
	
	// GAME MODE category
	{ CF_WORKSHOP_TAGIDX_CTF,           CF_WORKSHOP_TAG_CTF,               "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_CP,            CF_WORKSHOP_TAG_CP,                "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_PAYLOAD,       CF_WORKSHOP_TAG_PAYLOAD,           "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_PLR,           CF_WORKSHOP_TAG_PAYLOAD_RACE,      "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_ARENA,         CF_WORKSHOP_TAG_ARENA,             "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_KOTH,          CF_WORKSHOP_TAG_KOTH,              "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_AD,            CF_WORKSHOP_TAG_AD,                "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_SD,            CF_WORKSHOP_TAG_SD,                "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_RD,            CF_WORKSHOP_TAG_RD,                "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_MVM,           CF_WORKSHOP_TAG_MVM,               "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_MVM_VERSUS,    CF_WORKSHOP_TAG_MVM_VERSUS,        "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_MANNPOWER,     CF_WORKSHOP_TAG_MANNPOWER,         "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_MEDIEVAL,      CF_WORKSHOP_TAG_MEDIEVAL,          "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_PASS,          CF_WORKSHOP_TAG_PASSTIME,          "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_PD,            CF_WORKSHOP_TAG_PD,                "Game Mode" },
	{ CF_WORKSHOP_TAGIDX_CUSTOM,        CF_WORKSHOP_TAG_CUSTOM,            "Game Mode" },
	
	// CLASS category
	{ CF_WORKSHOP_TAGIDX_SCOUT,         CF_WORKSHOP_TAG_SCOUT,             "Class" },
	{ CF_WORKSHOP_TAGIDX_SOLDIER,       CF_WORKSHOP_TAG_SOLDIER,           "Class" },
	{ CF_WORKSHOP_TAGIDX_PYRO,          CF_WORKSHOP_TAG_PYRO,              "Class" },
	{ CF_WORKSHOP_TAGIDX_DEMOMAN,       CF_WORKSHOP_TAG_DEMOMAN,           "Class" },
	{ CF_WORKSHOP_TAGIDX_HEAVY,         CF_WORKSHOP_TAG_HEAVY,             "Class" },
	{ CF_WORKSHOP_TAGIDX_ENGINEER,      CF_WORKSHOP_TAG_ENGINEER,          "Class" },
	{ CF_WORKSHOP_TAGIDX_MEDIC,         CF_WORKSHOP_TAG_MEDIC,             "Class" },
	{ CF_WORKSHOP_TAGIDX_SNIPER,        CF_WORKSHOP_TAG_SNIPER,            "Class" },
	{ CF_WORKSHOP_TAGIDX_SPY,           CF_WORKSHOP_TAG_SPY,               "Class" },
	
	// ITEM SLOT category
	{ CF_WORKSHOP_TAGIDX_PRIMARY,       CF_WORKSHOP_TAG_WEAPON_PRIMARY,    "Item Slot" },
	{ CF_WORKSHOP_TAGIDX_SECONDARY,     CF_WORKSHOP_TAG_WEAPON_SECONDARY,  "Item Slot" },
	{ CF_WORKSHOP_TAGIDX_MELEE,         CF_WORKSHOP_TAG_WEAPON_MELEE,      "Item Slot" },
	{ CF_WORKSHOP_TAGIDX_PDA,           CF_WORKSHOP_TAG_WEAPON_PDA,        "Item Slot" },
	{ CF_WORKSHOP_TAGIDX_COSMETIC,      CF_WORKSHOP_TAG_COSMETIC,          "Item Slot" },
	{ CF_WORKSHOP_TAGIDX_ACTION,        CF_WORKSHOP_TAG_ACTION,            "Item Slot" },
	{ CF_WORKSHOP_TAGIDX_TAUNT,         CF_WORKSHOP_TAG_TAUNT,             "Item Slot" },
	{ CF_WORKSHOP_TAGIDX_SLOT_SKIN,     CF_WORKSHOP_TAG_SKIN,              "Item Slot" },
	
	// OTHER category
	{ CF_WORKSHOP_TAGIDX_HALLOWEEN,     CF_WORKSHOP_TAG_HALLOWEEN,         "Other" },
	{ CF_WORKSHOP_TAGIDX_CHRISTMAS,     CF_WORKSHOP_TAG_CHRISTMAS,         "Other" },
	{ CF_WORKSHOP_TAGIDX_SUMMER,		CF_WORKSHOP_TAG_SUMMER,			   "Other" },
	{ CF_WORKSHOP_TAGIDX_UNUSUAL,       CF_WORKSHOP_TAG_UNUSUAL,           "Other" },
	{ CF_WORKSHOP_TAGIDX_WARPAINT,      CF_WORKSHOP_TAG_WARPAINT,          "Other" },
	{ CF_WORKSHOP_TAGIDX_KILLSTREAK,    CF_WORKSHOP_TAG_KILLSTREAK,        "Other" },
	{ CF_WORKSHOP_TAGIDX_COMMUNITY_FIX, CF_WORKSHOP_TAG_COMMUNITY_FIX,     "Other" },
};

// Helper to check if a number is less than another for map sorting
bool PublishedFileId_Less(const PublishedFileId_t& a, const PublishedFileId_t& b)
{
	return a < b;
}

//-----------------------------------------------------------------------------
// CCFWorkshopItem Implementation
//-----------------------------------------------------------------------------

CCFWorkshopItem::CCFWorkshopItem(PublishedFileId_t fileID, CFWorkshopItemType_t type)
	: m_nFileID(fileID)
	, m_eType(type)
	, m_eState(CF_WORKSHOP_STATE_NONE)
	, m_nFileSize(0)
	, m_rtimeUpdated(0)
	, m_rtimeCreated(0)
	, m_ulSteamIDOwner(0)
	, m_bSubscribed(false)
	, m_bHighPriority(false)
{
	CFWorkshopDebug("Created workshop item %llu of type %d\n", fileID, type);
	Refresh();
}

CCFWorkshopItem::~CCFWorkshopItem()
{
}

void CCFWorkshopItem::Refresh(bool bHighPriority)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		CFWorkshopWarning("Failed to get Steam UGC context for item %llu\n", m_nFileID);
		m_eState = CF_WORKSHOP_STATE_ERROR;
		return;
	}

	m_eState = CF_WORKSHOP_STATE_QUERYING;
	m_bHighPriority = bHighPriority;

	// Cancel in-flight request
	if (m_callbackQueryDetails.IsActive())
	{
		m_callbackQueryDetails.Cancel();
	}

	// Create query for this specific item
	UGCQueryHandle_t hQuery = pUGC->CreateQueryUGCDetailsRequest(&m_nFileID, 1);
	if (hQuery == k_UGCQueryHandleInvalid)
	{
		CFWorkshopWarning("Failed to create query for item %llu\n", m_nFileID);
		m_eState = CF_WORKSHOP_STATE_ERROR;
		return;
	}

	// We want full details
	pUGC->SetReturnMetadata(hQuery, true);
	pUGC->SetReturnChildren(hQuery, true);
	pUGC->SetReturnAdditionalPreviews(hQuery, true);
	pUGC->SetReturnLongDescription(hQuery, true);

	// Send the query
	SteamAPICall_t hCall = pUGC->SendQueryUGCRequest(hQuery);
	m_callbackQueryDetails.Set(hCall, this, &CCFWorkshopItem::Steam_OnQueryDetails);
}

void CCFWorkshopItem::Steam_OnQueryDetails(SteamUGCQueryCompleted_t* pResult, bool bError)
{
	if (bError || pResult->m_eResult != k_EResultOK)
	{
		CFWorkshopWarning("Query failed for item %llu (result: %d)\n", m_nFileID, pResult->m_eResult);
		m_eState = CF_WORKSHOP_STATE_ERROR;
		return;
	}

	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		m_eState = CF_WORKSHOP_STATE_ERROR;
		return;
	}

	// Get the details
	SteamUGCDetails_t details;
	if (!pUGC->GetQueryUGCResult(pResult->m_handle, 0, &details))
	{
		CFWorkshopWarning("Failed to get query result for item %llu\n", m_nFileID);
		m_eState = CF_WORKSHOP_STATE_ERROR;
		return;
	}

	// Store the details
	m_strTitle = details.m_rgchTitle;
	m_strDescription = details.m_rgchDescription;
	m_strTags = details.m_rgchTags;
	m_nFileSize = details.m_nFileSize;
	m_rtimeUpdated = details.m_rtimeUpdated;
	m_rtimeCreated = details.m_rtimeCreated;
	m_ulSteamIDOwner = details.m_ulSteamIDOwner;
	
	// Update type from tags if we're still set to OTHER
	if (m_eType == CF_WORKSHOP_TYPE_OTHER)
	{
		m_eType = CFWorkshop_GetItemTypeFromTags(details.m_rgchTags);
	}

	// Get preview URL
	char szPreviewURL[256];
	if (pUGC->GetQueryUGCPreviewURL(pResult->m_handle, 0, szPreviewURL, sizeof(szPreviewURL)))
	{
		m_strPreviewURL = szPreviewURL;
	}

	// Get metadata (for maps, this contains the original BSP filename)
	char szMetadata[k_cchDeveloperMetadataMax];
	if (pUGC->GetQueryUGCMetadata(pResult->m_handle, 0, szMetadata, sizeof(szMetadata)))
	{
		m_strOriginalFilename = szMetadata;
		CFWorkshopDebug("Item metadata: %s\n", szMetadata);
	}

	// Check subscription status
	m_bSubscribed = IsSubscribed();

	// Check download state
	uint32 itemState = pUGC->GetItemState(m_nFileID);
	
	if (itemState & k_EItemStateInstalled)
	{
		m_eState = CF_WORKSHOP_STATE_DOWNLOADED;
	}
	else if (itemState & (k_EItemStateDownloading | k_EItemStateDownloadPending))
	{
		m_eState = CF_WORKSHOP_STATE_DOWNLOADING;
	}
	else if (itemState & k_EItemStateNeedsUpdate)
	{
		m_eState = CF_WORKSHOP_STATE_NEEDS_UPDATE;
		// Auto-download updates if subscribed
		if (m_bSubscribed)
		{
			Download(m_bHighPriority);
		}
	}
	else
	{
		m_eState = CF_WORKSHOP_STATE_NONE;
	}

	// Release the query
	pUGC->ReleaseQueryUGCRequest(pResult->m_handle);

	CFWorkshopMsg("Item details updated: %s (ID: %llu, Size: %llu bytes)\n", 
		m_strTitle.Get(), m_nFileID, m_nFileSize);
}

void CCFWorkshopItem::Download(bool bHighPriority)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		CFWorkshopWarning("Failed to get Steam UGC context\n");
		return;
	}

	m_bHighPriority = bHighPriority;
	
	if (pUGC->DownloadItem(m_nFileID, bHighPriority))
	{
		m_eState = CF_WORKSHOP_STATE_DOWNLOADING;
		CFWorkshopMsg("Started download for item %s (%llu)\n", m_strTitle.Get(), m_nFileID);
	}
	else
	{
		CFWorkshopWarning("Failed to start download for item %llu\n", m_nFileID);
	}
}

void CCFWorkshopItem::OnDownloadComplete(DownloadItemResult_t* pResult)
{
	if (pResult->m_eResult == k_EResultOK)
	{
		m_eState = CF_WORKSHOP_STATE_DOWNLOADED;
		CFWorkshopMsg("Download complete for item %s (%llu)\n", m_strTitle.Get(), m_nFileID);
		
		// Auto-install if it's a skin or effect
		if (m_eType == CF_WORKSHOP_TYPE_WEAPON_SKIN || 
			m_eType == CF_WORKSHOP_TYPE_CHARACTER_SKIN ||
			m_eType == CF_WORKSHOP_TYPE_PARTICLE_EFFECT)
		{
			Install();
		}
	}
	else
	{
		m_eState = CF_WORKSHOP_STATE_ERROR;
		CFWorkshopWarning("Download failed for item %llu (result: %d)\n", m_nFileID, pResult->m_eResult);
	}
}

void CCFWorkshopItem::OnItemInstalled(ItemInstalled_t* pResult)
{
	m_eState = CF_WORKSHOP_STATE_DOWNLOADED;
	CFWorkshopMsg("Item installed: %s (%llu)\n", m_strTitle.Get(), m_nFileID);
}

void CCFWorkshopItem::Install()
{
	if (m_eState != CF_WORKSHOP_STATE_DOWNLOADED)
	{
		CFWorkshopWarning("Cannot install item %llu - not downloaded\n", m_nFileID);
		return;
	}

	m_eState = CF_WORKSHOP_STATE_INSTALLING;
	
	// Get install info
	char szInstallPath[MAX_PATH];
	if (!GetInstallPath(szInstallPath, sizeof(szInstallPath)))
	{
		CFWorkshopWarning("Failed to get install path for item %llu\n", m_nFileID);
		m_eState = CF_WORKSHOP_STATE_ERROR;
		return;
	}

	// TODO: Copy files to appropriate game directories based on type
	// For now, just mark as installed
	m_eState = CF_WORKSHOP_STATE_INSTALLED;
	CFWorkshopMsg("Installed item %s to %s\n", m_strTitle.Get(), szInstallPath);
}

void CCFWorkshopItem::Uninstall()
{
	// TODO: Remove installed files
	m_eState = CF_WORKSHOP_STATE_DOWNLOADED;
	CFWorkshopMsg("Uninstalled item %llu\n", m_nFileID);
}

bool CCFWorkshopItem::GetInstallPath(char* pszPath, size_t pathSize) const
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return false;

	uint64 sizeOnDisk = 0;
	uint32 timestamp = 0;
	return pUGC->GetItemInstallInfo(m_nFileID, &sizeOnDisk, pszPath, pathSize, &timestamp);
}

bool CCFWorkshopItem::IsSubscribed() const
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return false;

	uint32 itemState = pUGC->GetItemState(m_nFileID);
	return (itemState & k_EItemStateSubscribed) != 0;
}

bool CCFWorkshopItem::IsDownloaded() const
{
	return m_eState == CF_WORKSHOP_STATE_DOWNLOADED || m_eState == CF_WORKSHOP_STATE_INSTALLED;
}

bool CCFWorkshopItem::IsInstalled() const
{
	return m_eState == CF_WORKSHOP_STATE_INSTALLED;
}

bool CCFWorkshopItem::IsOwnedByCurrentUser() const
{
	if (m_ulSteamIDOwner == 0)
		return false;
	
	ISteamUser* pUser = SteamUser();
	if (!pUser)
		return false;
	
	CSteamID currentUserID = pUser->GetSteamID();
	return currentUserID.IsValid() && currentUserID.IsValid() && currentUserID.GetAccountID() == (m_ulSteamIDOwner & 0xFFFFFFFF);
}

float CCFWorkshopItem::GetDownloadProgress() const
{
	if (m_eState != CF_WORKSHOP_STATE_DOWNLOADING)
		return (m_eState == CF_WORKSHOP_STATE_DOWNLOADED || m_eState == CF_WORKSHOP_STATE_INSTALLED) ? 1.0f : 0.0f;

	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return 0.0f;

	uint64 downloaded = 0;
	uint64 total = 0;
	
	if (pUGC->GetItemDownloadInfo(m_nFileID, &downloaded, &total) && total > 0)
	{
		return (float)downloaded / (float)total;
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// CCFWorkshopManager Implementation
//-----------------------------------------------------------------------------

CCFWorkshopManager::CCFWorkshopManager()
	: m_callbackDownloadItem(this, &CCFWorkshopManager::Steam_OnDownloadItem)
	, m_callbackItemInstalled(this, &CCFWorkshopManager::Steam_OnItemInstalled)
	, m_callbackDownloadItem_Server(this, &CCFWorkshopManager::Steam_OnDownloadItem)
	, m_callbackItemInstalled_Server(this, &CCFWorkshopManager::Steam_OnItemInstalled)
	, m_mapItems(0, 0, PublishedFileId_Less)
	, m_mapActiveSkins(DefLessFunc(CUtlString))
	, m_nPreparingMap(k_PublishedFileIdInvalid)
	, m_nAppID(0)
	, m_bInitialized(false)
	, m_bRefreshQueued(false)
	, m_bContentReloadPending(false)
	, m_bHUDReloadPending(false)
	, m_flNextContentReloadTime(0.0f)
	, m_hCurrentQuery(k_UGCQueryHandleInvalid)
	, m_bUploadInProgress(false)
	, m_bDeleteInProgress(false)
	, m_hCurrentUpdate(k_UGCUpdateHandleInvalid)
	, m_nLastUploadedFileID(0)
	, m_nPendingDeleteFileID(0)
	, m_ePendingType(CF_WORKSHOP_TYPE_OTHER)
	, m_ePendingVisibility(k_ERemoteStoragePublishedFileVisibilityPublic)
	, m_bConfigLoaded(false)
	, m_flNextConfigUpdateCheck(0.0f)
{
	m_szUploadStatus[0] = '\0';
}

CCFWorkshopManager::~CCFWorkshopManager()
{
}

bool CCFWorkshopManager::Init()
{
	CFWorkshopMsg("Initializing Custom Fortress 2 Workshop Manager...\n");
	
	m_nAppID = 3768450; // Custom Fortress 2 App ID
	
	// Initialize on client, or on listenserver
#ifdef CLIENT_DLL
	InitWorkshop();
#else
	// Server-side initializes later when Steam API is ready
#endif
	
	return true;
}

void CCFWorkshopManager::Shutdown()
{
	CFWorkshopMsg("Shutting down workshop manager...\n");
	
	// Clean up all items
	FOR_EACH_MAP_FAST(m_mapItems, i)
	{
		delete m_mapItems[i];
	}
	m_mapItems.Purge();
	
	m_vecSubscribedItems.Purge();
	m_mapActiveSkins.Purge();
	m_bInitialized = false;
}

void CCFWorkshopManager::Update(float frametime)
{
	// Check if we need to refresh subscriptions
	if (m_bRefreshQueued)
	{
		m_bRefreshQueued = false;
		RefreshSubscriptions();
	}
	
	// Periodic update checking (if configured)
	if (m_bConfigLoaded && m_config.m_nUpdateCheckInterval > 0)
	{
		float flCurrentTime = Plat_FloatTime();
		if (flCurrentTime >= m_flNextConfigUpdateCheck)
		{
			m_flNextConfigUpdateCheck = flCurrentTime + m_config.m_nUpdateCheckInterval;
			
			// Check for updates on all subscribed items
			FOR_EACH_VEC(m_vecSubscribedItems, i)
			{
				PublishedFileId_t fileID = m_vecSubscribedItems[i];
				CCFWorkshopItem* pItem = GetItem(fileID);
				if (pItem)
				{
					pItem->Refresh(m_config.m_bHighPriority);
				}
			}
			
			if (m_config.m_bLogUpdates)
			{
				CFWorkshopMsg("Checking for workshop item updates...\n");
			}
		}
	}
	
#ifdef CLIENT_DLL
	// Handle deferred content reload (must be done here, not in Steam callbacks)
	if (m_bContentReloadPending)
	{
		// Check cooldown to prevent rapid reloads which can crash the engine
		float flCurrentTime = Plat_FloatTime();
		if (flCurrentTime >= m_flNextContentReloadTime)
		{
			m_bContentReloadPending = false;
			m_flNextContentReloadTime = flCurrentTime + 2.0f;  // 2 second cooldown
			PerformContentReload();
		}
		// If on cooldown, keep pending flag set - will retry next frame
	}
	
	// Handle deferred HUD reload
	if (m_bHUDReloadPending)
	{
		float flCurrentTime = Plat_FloatTime();
		if (flCurrentTime >= m_flNextContentReloadTime)
		{
			m_bHUDReloadPending = false;
			m_flNextContentReloadTime = flCurrentTime + 1.0f;  // 1 second cooldown for HUD
			PerformHUDReload();
		}
	}
#endif
}

void CCFWorkshopManager::InitWorkshop()
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		CFWorkshopWarning("Failed to get Steam UGC interface\n");
		return;
	}

	// For dedicated servers, initialize BInitWorkshopForGameServer to enable automatic Workshop downloads
#ifdef GAME_DLL
	if (engine->IsDedicatedServer())
	{
		int i = CommandLine()->FindParm("-ugcpath");
		if (i)
		{
			const char* pUGCPath = CommandLine()->GetParm(i + 1);
			if (pUGCPath)
			{
				g_pFullFileSystem->CreateDirHierarchy(pUGCPath, "GAME");
				char szFullPath[MAX_PATH] = { 0 };
				g_pFullFileSystem->RelativePathToFullPath(pUGCPath, "GAME", szFullPath, sizeof(szFullPath));
				if (szFullPath[0])
				{
					// Use App ID 3768450 (Custom Fortress 2) and the specified path
					if (pUGC->BInitWorkshopForGameServer(m_nAppID, szFullPath))
					{
						CFWorkshopMsg("Initialized Workshop for dedicated server (UGC path: %s)\n", szFullPath);
					}
					else
					{
						CFWorkshopWarning("Failed to initialize Workshop for dedicated server\n");
					}
				}
				else
				{
					CFWorkshopWarning("Could not resolve -ugcpath to absolute path: %s\n", pUGCPath);
				}
			}
			else
			{
				CFWorkshopWarning("Empty -ugcpath passed, using default\n");
			}
		}
		else
		{
			CFWorkshopMsg("No -ugcpath specified for dedicated server, using default Workshop directory\n");
		}
	}
#endif

	CFWorkshopMsg("Workshop initialized successfully\n");
	m_bInitialized = true;
	
	// Load workshop configuration
	LoadConfig();
	
	// Apply sv_pure override if configured
	if (m_bConfigLoaded && m_config.m_nSvPureOverride >= 0)
	{
#ifndef CLIENT_DLL
		ConVar* sv_pure = cvar->FindVar("sv_pure");
		if (sv_pure)
		{
			sv_pure->SetValue(m_config.m_nSvPureOverride);
			if (m_config.m_bLoggingEnabled)
			{
				CFWorkshopMsg("Set sv_pure to %d (from config)\n", m_config.m_nSvPureOverride);
			}
		}
#endif
	}
	
	// Load disabled items from config
	LoadDisabledItems();
	
	// Download collection items if configured
	if (m_bConfigLoaded && m_config.m_bAutoDownload && m_config.m_nCollectionID != 0)
	{
		if (m_config.m_bLoggingEnabled)
		{
			CFWorkshopMsg("Auto-downloading collection %llu...\n", m_config.m_nCollectionID);
		}
		// Note: Collection download will be implemented via QueryCollection
		QueryCollection(m_config.m_nCollectionID);
	}
	
	// Subscribe and download specific map IDs from config
	if (m_bConfigLoaded && m_config.m_bAutoDownload)
	{
		FOR_EACH_VEC(m_config.m_vecMapIDs, i)
		{
			PublishedFileId_t mapID = m_config.m_vecMapIDs[i];
			if (mapID != 0)
			{
				CCFWorkshopItem* pItem = GetItem(mapID);
				if (!pItem)
				{
					pItem = AddItem(mapID, CF_WORKSHOP_TYPE_MAP);
				}
				
				if (pItem && !pItem->IsDownloaded())
				{
					if (m_config.m_bLogDownloads)
					{
						CFWorkshopMsg("Auto-downloading map %llu from config...\n", mapID);
					}
					pItem->Download(m_config.m_bHighPriority);
				}
			}
		}
		
		// Subscribe and download specific item IDs from config
		FOR_EACH_VEC(m_config.m_vecItemIDs, i)
		{
			PublishedFileId_t itemID = m_config.m_vecItemIDs[i];
			if (itemID != 0)
			{
				CCFWorkshopItem* pItem = GetItem(itemID);
				if (!pItem)
				{
					pItem = AddItem(itemID, CF_WORKSHOP_TYPE_OTHER);
				}
				
				if (pItem && !pItem->IsDownloaded())
				{
					if (m_config.m_bLogDownloads)
					{
						CFWorkshopMsg("Auto-downloading item %llu from config...\n", itemID);
					}
					pItem->Download(m_config.m_bHighPriority);
				}
			}
		}
	}

	// Queue a refresh of subscriptions
	m_bRefreshQueued = true;
}

void CCFWorkshopManager::RefreshSubscriptions()
{
	if (!m_bInitialized)
	{
		CFWorkshopWarning("Workshop not initialized\n");
		return;
	}

	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		CFWorkshopWarning("Failed to get Steam UGC interface\n");
		return;
	}

	// Don't refresh on dedicated servers (they use manual sync)
#ifdef GAME_DLL
	return;
#endif

	CFWorkshopMsg("Refreshing workshop subscriptions...\n");

	// Get subscribed items
	m_vecSubscribedItems.RemoveAll();
	uint32 numSubscribed = pUGC->GetNumSubscribedItems();
	
	if (numSubscribed > 0)
	{
		m_vecSubscribedItems.SetSize(numSubscribed);
		uint32 actual = pUGC->GetSubscribedItems(m_vecSubscribedItems.Base(), numSubscribed);
		
		if (actual < numSubscribed)
		{
			m_vecSubscribedItems.SetSize(actual);
		}

		CFWorkshopMsg("Found %u subscribed workshop items\n", actual);

		// Refresh or create items for each subscription
		FOR_EACH_VEC(m_vecSubscribedItems, i)
		{
			PublishedFileId_t fileID = m_vecSubscribedItems[i];
			
			// Skip items that were recently unsubscribed (Steam may still report them briefly)
			if (m_vecRecentlyUnsubscribed.Find(fileID) != m_vecRecentlyUnsubscribed.InvalidIndex())
			{
				CFWorkshopDebug("Skipping recently unsubscribed item %llu\n", fileID);
				continue;
			}
			
			// Check if we already have this item
			unsigned short index = m_mapItems.Find(fileID);
			if (index == m_mapItems.InvalidIndex())
			{
				// New item - we'll determine type from tags when query completes
				CCFWorkshopItem* pItem = new CCFWorkshopItem(fileID, CF_WORKSHOP_TYPE_OTHER);
				m_mapItems.Insert(fileID, pItem);
			}
			else
			{
				// Existing item - refresh it
				m_mapItems[index]->Refresh();
			}
			
			// Mount workshop content folder for each subscribed item
			MountWorkshopItem(fileID);
		}
	}
	else
	{
		CFWorkshopMsg("No subscribed workshop items found\n");
	}
	
	// Clear the recently unsubscribed list after a successful refresh
	m_vecRecentlyUnsubscribed.RemoveAll();
}

void CCFWorkshopManager::MountWorkshopItem(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;
	
	// Skip if item is disabled by user
	if (!IsItemEnabled(fileID))
	{
		CFWorkshopDebug("Item %llu is disabled, skipping mount\n", fileID);
		return;
	}
	
	// Check if item is installed
	uint32 itemState = pUGC->GetItemState(fileID);
	if ((itemState & k_EItemStateInstalled) == 0)
	{
		CFWorkshopDebug("Item %llu not installed, skipping mount\n", fileID);
		return;
	}
	
	// Get the install folder
	char szInstallPath[MAX_PATH];
	uint64 sizeOnDisk = 0;
	uint32 timestamp = 0;
	
	if (!pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
	{
		CFWorkshopWarning("Failed to get install info for item %llu\n", fileID);
		return;
	}
	
	// Add the workshop folder to search paths if not already there
	// This allows the engine to find maps, materials, models etc in workshop items
	if (!g_pFullFileSystem->IsDirectory(szInstallPath))
	{
		CFWorkshopWarning("Workshop path doesn't exist: %s\n", szInstallPath);
		return;
	}
	
	// Check item type - certain types need high priority mounting to override base files
	CCFWorkshopItem* pItem = GetItem(fileID);
	CFWorkshopItemType_t eType = pItem ? pItem->GetType() : CF_WORKSHOP_TYPE_OTHER;
	
	// Check for content replacement mods (weapon skins, sound mods, particle mods, HUDs)
	bool bIsContentReplacement = (eType == CF_WORKSHOP_TYPE_WEAPON_SKIN || 
	                               eType == CF_WORKSHOP_TYPE_SOUND_MOD ||
	                               eType == CF_WORKSHOP_TYPE_PARTICLE_EFFECT ||
	                               eType == CF_WORKSHOP_TYPE_HUD);
	
	// Track if this is a HUD mod for special reload handling
	bool bIsHUD = (eType == CF_WORKSHOP_TYPE_HUD);
	
	// Also detect content replacement mods by folder structure
	if (!bIsContentReplacement)
	{
		char szMapsPath[MAX_PATH];
		V_snprintf(szMapsPath, sizeof(szMapsPath), "%s/maps", szInstallPath);
		bool bHasMaps = g_pFullFileSystem->IsDirectory(szMapsPath);
		
		char szScriptsPath[MAX_PATH];
		V_snprintf(szScriptsPath, sizeof(szScriptsPath), "%s/scripts/items", szInstallPath);
		bool bHasItemScripts = g_pFullFileSystem->IsDirectory(szScriptsPath);
		
		char szModelsPath[MAX_PATH];
		V_snprintf(szModelsPath, sizeof(szModelsPath), "%s/models", szInstallPath);
		bool bHasModels = g_pFullFileSystem->IsDirectory(szModelsPath);
		
		char szMaterialsPath[MAX_PATH];
		V_snprintf(szMaterialsPath, sizeof(szMaterialsPath), "%s/materials", szInstallPath);
		bool bHasMaterials = g_pFullFileSystem->IsDirectory(szMaterialsPath);
		
		char szSoundPath[MAX_PATH];
		V_snprintf(szSoundPath, sizeof(szSoundPath), "%s/sound", szInstallPath);
		bool bHasSound = g_pFullFileSystem->IsDirectory(szSoundPath);
		
		char szParticlesPath[MAX_PATH];
		V_snprintf(szParticlesPath, sizeof(szParticlesPath), "%s/particles", szInstallPath);
		bool bHasParticles = g_pFullFileSystem->IsDirectory(szParticlesPath);
		
		// Check for HUD folders
		char szResourceUIPath[MAX_PATH];
		V_snprintf(szResourceUIPath, sizeof(szResourceUIPath), "%s/resource/ui", szInstallPath);
		bool bHasResourceUI = g_pFullFileSystem->IsDirectory(szResourceUIPath);
		
		char szScriptsHudPath[MAX_PATH];
		V_snprintf(szScriptsHudPath, sizeof(szScriptsHudPath), "%s/scripts/hudlayout.res", szInstallPath);
		bool bHasHudLayout = g_pFullFileSystem->FileExists(szScriptsHudPath);
		
		// Detect HUD mod: has resource/ui or scripts/hudlayout.res
		if (bHasResourceUI || bHasHudLayout)
		{
			// Check config restrictions for HUDs
			if (m_bConfigLoaded && !m_config.m_bAllowHUDs)
			{
				if (m_config.m_bLoggingEnabled)
				{
					CFWorkshopMsg("Skipping HUD mod %llu (disabled in config)\n", fileID);
				}
				return;
			}
			
			bIsContentReplacement = true;
			bIsHUD = true;
		}
		// Content replacement: has replacement content but no maps or item scripts
		// This includes models/materials (skins), sound, or particles
		else if (!bHasMaps && !bHasItemScripts)
		{
			// Apply config restrictions
			bool bAllowThisContent = true;
			
			if (bHasSound)
			{
				if (m_bConfigLoaded && !m_config.m_bAllowSounds)
				{
					if (m_config.m_bLoggingEnabled)
					{
						CFWorkshopMsg("Skipping sound mod %llu (disabled in config)\n", fileID);
					}
					bAllowThisContent = false;
				}
			}
			
			if (bHasParticles)
			{
				if (m_bConfigLoaded && !m_config.m_bAllowParticles)
				{
					if (m_config.m_bLoggingEnabled)
					{
						CFWorkshopMsg("Skipping particle mod %llu (disabled in config)\n", fileID);
					}
					bAllowThisContent = false;
				}
			}
			
			if (bHasModels || bHasMaterials)
			{
				if (m_bConfigLoaded && !m_config.m_bAllowSkins)
				{
					if (m_config.m_bLoggingEnabled)
					{
						CFWorkshopMsg("Skipping skin mod %llu (disabled in config)\n", fileID);
					}
					bAllowThisContent = false;
				}
			}
			
			if (!bAllowThisContent)
			{
				return; // Skip mounting this item
			}
			
			if (bHasModels || bHasMaterials || bHasSound || bHasParticles)
			{
				bIsContentReplacement = true;
			}
		}
	}
	
	if (bIsContentReplacement)
	{
		// Mount with HIGH priority to override base game files (like custom folder)
		g_pFullFileSystem->AddSearchPath(szInstallPath, "GAME", PATH_ADD_TO_HEAD);
		g_pFullFileSystem->AddSearchPath(szInstallPath, "mod", PATH_ADD_TO_HEAD);
		
		// Track as mounted content replacement if not already tracked
		if (!IsWeaponSkinMounted(fileID))
		{
			m_vecMountedWeaponSkins.AddToTail(fileID);
		}
		
		const char* pszModType = "content";
		if (eType == CF_WORKSHOP_TYPE_SOUND_MOD) pszModType = "sound";
		else if (eType == CF_WORKSHOP_TYPE_PARTICLE_EFFECT) pszModType = "particle";
		else if (eType == CF_WORKSHOP_TYPE_WEAPON_SKIN) pszModType = "skin";
		else if (bIsHUD) pszModType = "HUD";
		
		CFWorkshopMsg("Mounted %s mod %llu from: %s (HIGH PRIORITY)\n", pszModType, fileID, szInstallPath);
		
#ifdef CLIENT_DLL
		// Request content reload to apply the mod (deferred to avoid callback crash)
		RequestContentReload();
		
		// For HUD mods, also reload the HUD scheme
		if (bIsHUD)
		{
			RequestHUDReload();
		}
#endif
	}
	else
	{
		// Add as a game search path with normal priority
		g_pFullFileSystem->AddSearchPath(szInstallPath, "GAME", PATH_ADD_TO_TAIL);
		g_pFullFileSystem->AddSearchPath(szInstallPath, "mod", PATH_ADD_TO_TAIL);
		
		CFWorkshopMsg("Mounted workshop item %llu from: %s\n", fileID, szInstallPath);
	}
	
	// Check for maps in the mounted folder and notify user
	char szMapsPath[MAX_PATH];
	V_snprintf(szMapsPath, sizeof(szMapsPath), "%s/maps", szInstallPath);
	if (g_pFullFileSystem->IsDirectory(szMapsPath))
	{
		// Find .bsp files in the maps folder
		FileFindHandle_t hFind;
		char szSearchPath[MAX_PATH];
		V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.bsp", szMapsPath);
		
		const char* pszFilename = g_pFullFileSystem->FindFirstEx(szSearchPath, NULL, &hFind);
		while (pszFilename)
		{
			// Remove .bsp extension to get map name
			char szMapName[256];
			V_StripExtension(pszFilename, szMapName, sizeof(szMapName));
			Msg("[CF Workshop] Map '%s' is now available! Use 'map %s' to play.\n", szMapName, szMapName);
			pszFilename = g_pFullFileSystem->FindNext(hFind);
		}
		g_pFullFileSystem->FindClose(hFind);
	}
}

void CCFWorkshopManager::UnmountWorkshopItem(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;
	
	// Check if this was a mounted content replacement mod
	bool bWasContentMod = IsWeaponSkinMounted(fileID);
	if (bWasContentMod)
	{
		m_vecMountedWeaponSkins.FindAndRemove(fileID);
	}
	
	// Check if this is a HUD mod for special reload handling
	CCFWorkshopItem* pItem = GetItem(fileID);
	bool bIsHUD = pItem && pItem->GetType() == CF_WORKSHOP_TYPE_HUD;
	
	// Get the install folder (may still exist even after unsubscribe)
	char szInstallPath[MAX_PATH];
	uint64 sizeOnDisk = 0;
	uint32 timestamp = 0;
	
	if (!pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
	{
		// Item might already be deleted
		if (!pItem)
		{
			CFWorkshopDebug("Cannot unmount item %llu - install path unknown\n", fileID);
			return;
		}
	}
	
	// Also detect HUD by folder structure if type isn't set
	if (!bIsHUD && szInstallPath[0])
	{
		char szResourceUIPath[MAX_PATH];
		V_snprintf(szResourceUIPath, sizeof(szResourceUIPath), "%s/resource/ui", szInstallPath);
		bool bHasResourceUI = g_pFullFileSystem->IsDirectory(szResourceUIPath);
		
		char szScriptsHudPath[MAX_PATH];
		V_snprintf(szScriptsHudPath, sizeof(szScriptsHudPath), "%s/scripts/hudlayout.res", szInstallPath);
		bool bHasHudLayout = g_pFullFileSystem->FileExists(szScriptsHudPath);
		
		if (bHasResourceUI || bHasHudLayout)
		{
			bIsHUD = true;
		}
	}
	
	// Check for maps before removing and notify user
	char szMapsPath[MAX_PATH];
	V_snprintf(szMapsPath, sizeof(szMapsPath), "%s/maps", szInstallPath);
	if (g_pFullFileSystem->IsDirectory(szMapsPath))
	{
		FileFindHandle_t hFind;
		char szSearchPath[MAX_PATH];
		V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.bsp", szMapsPath);
		
		const char* pszFilename = g_pFullFileSystem->FindFirstEx(szSearchPath, NULL, &hFind);
		while (pszFilename)
		{
			char szMapName[256];
			V_StripExtension(pszFilename, szMapName, sizeof(szMapName));
			Msg("[CF Workshop] Map '%s' has been removed.\n", szMapName);
			pszFilename = g_pFullFileSystem->FindNext(hFind);
		}
		g_pFullFileSystem->FindClose(hFind);
	}
	
	// Remove from search paths
	g_pFullFileSystem->RemoveSearchPath(szInstallPath, "GAME");
	g_pFullFileSystem->RemoveSearchPath(szInstallPath, "mod");
	
	CFWorkshopMsg("Unmounted workshop item %llu from: %s\n", fileID, szInstallPath);
	
#ifdef CLIENT_DLL
	// If this was a weapon skin or content mod, reload content to revert to originals
	if (bWasContentMod)
	{
		RequestContentReload();
	}
	
	// If this was a HUD, reload HUD to revert to default
	if (bIsHUD)
	{
		RequestHUDReload();
	}
#endif
}

CCFWorkshopItem* CCFWorkshopManager::GetItem(PublishedFileId_t fileID)
{
	unsigned short index = m_mapItems.Find(fileID);
	if (index != m_mapItems.InvalidIndex())
	{
		return m_mapItems[index];
	}
	return NULL;
}

CCFWorkshopItem* CCFWorkshopManager::AddItem(PublishedFileId_t fileID, CFWorkshopItemType_t type)
{
	unsigned short index = m_mapItems.Find(fileID);
	if (index != m_mapItems.InvalidIndex())
	{
		return m_mapItems[index];
	}

	CCFWorkshopItem* pItem = new CCFWorkshopItem(fileID, type);
	m_mapItems.Insert(fileID, pItem);
	return pItem;
}

void CCFWorkshopManager::RemoveItem(PublishedFileId_t fileID)
{
	unsigned short index = m_mapItems.Find(fileID);
	if (index != m_mapItems.InvalidIndex())
	{
		CCFWorkshopItem* pItem = m_mapItems[index];
		
		// Also remove from browseable items list
		m_vecBrowseableItems.FindAndRemove(pItem);
		
		delete pItem;
		m_mapItems.RemoveAt(index);
	}
}

// Check if an item is currently enabled (not in disabled list)
bool CCFWorkshopManager::IsItemEnabled(PublishedFileId_t fileID) const
{
	// Item is enabled if it's NOT in the disabled list
	return m_vecDisabledItems.Find(fileID) == m_vecDisabledItems.InvalidIndex();
}

// Enable or disable an item (mount/unmount with content reload)
void CCFWorkshopManager::SetItemEnabled(PublishedFileId_t fileID, bool bEnabled)
{
	bool bCurrentlyEnabled = IsItemEnabled(fileID);
	
	if (bEnabled == bCurrentlyEnabled)
		return;  // No change needed
	
	if (bEnabled)
	{
		// Remove from disabled list
		m_vecDisabledItems.FindAndRemove(fileID);
		// Mount the item
		MountWorkshopItem(fileID);
	}
	else
	{
		// Add to disabled list
		if (m_vecDisabledItems.Find(fileID) == m_vecDisabledItems.InvalidIndex())
		{
			m_vecDisabledItems.AddToTail(fileID);
		}
		
		// Unmount the item
		UnmountWorkshopItem(fileID);
	}
	
	// Save the disabled items list
	SaveDisabledItems();
	
#ifdef CLIENT_DLL
	// Always request content reload when toggling
	RequestContentReload();
#endif
}

// Save disabled items to config file
void CCFWorkshopManager::SaveDisabledItems()
{
	KeyValues* pKV = new KeyValues("DisabledWorkshopItems");
	
	for (int i = 0; i < m_vecDisabledItems.Count(); i++)
	{
		char szKey[32];
		V_snprintf(szKey, sizeof(szKey), "%d", i);
		pKV->SetUint64(szKey, m_vecDisabledItems[i]);
	}
	
	pKV->SaveToFile(g_pFullFileSystem, "cfg/workshop_disabled.txt", "MOD");
	pKV->deleteThis();
}

// Load disabled items from config file
void CCFWorkshopManager::LoadDisabledItems()
{
	m_vecDisabledItems.RemoveAll();
	
	KeyValues* pKV = new KeyValues("DisabledWorkshopItems");
	if (pKV->LoadFromFile(g_pFullFileSystem, "cfg/workshop_disabled.txt", "MOD"))
	{
		for (KeyValues* pSubKey = pKV->GetFirstValue(); pSubKey; pSubKey = pSubKey->GetNextValue())
		{
			PublishedFileId_t fileID = pSubKey->GetUint64();
			if (fileID != 0)
			{
				m_vecDisabledItems.AddToTail(fileID);
			}
		}
		CFWorkshopMsg("Loaded %d disabled workshop items from config\n", m_vecDisabledItems.Count());
	}
	pKV->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Load workshop configuration from workshop_config.txt
//-----------------------------------------------------------------------------
void CCFWorkshopManager::LoadConfig()
{
	m_config.Reset();
	m_bConfigLoaded = false;
	
	KeyValues* pKV = new KeyValues("WorkshopConfig");
	if (!pKV->LoadFromFile(g_pFullFileSystem, "cfg/workshop_config.txt", "MOD"))
	{
		if (m_config.m_bLoggingEnabled)
		{
			CFWorkshopMsg("No workshop_config.txt found, using default settings\n");
		}
		pKV->deleteThis();
		return;
	}
	
	// Load collection ID
	m_config.m_nCollectionID = pKV->GetUint64("collection_id", 0);
	
	// Load auto-download setting
	m_config.m_bAutoDownload = pKV->GetBool("auto_download", true);
	
	// Load map IDs
	KeyValues* pMapsSection = pKV->FindKey("maps");
	if (pMapsSection)
	{
		for (KeyValues* pMapKey = pMapsSection->GetFirstValue(); pMapKey; pMapKey = pMapKey->GetNextValue())
		{
			PublishedFileId_t mapID = Q_atoui64(pMapKey->GetString());
			if (mapID != 0)
			{
				m_config.m_vecMapIDs.AddToTail(mapID);
			}
		}
	}
	
	// Load item IDs
	KeyValues* pItemsSection = pKV->FindKey("items");
	if (pItemsSection)
	{
		for (KeyValues* pItemKey = pItemsSection->GetFirstValue(); pItemKey; pItemKey = pItemKey->GetNextValue())
		{
			PublishedFileId_t itemID = Q_atoui64(pItemKey->GetString());
			if (itemID != 0)
			{
				m_config.m_vecItemIDs.AddToTail(itemID);
			}
		}
	}
	
	// Load download priority
	const char* pszPriority = pKV->GetString("download_priority", "high");
	m_config.m_bHighPriority = (V_stricmp(pszPriority, "high") == 0);
	
	// Load update check interval
	m_config.m_nUpdateCheckInterval = pKV->GetInt("update_check_interval", 3600);
	
	// Load content path
	const char* pszContentPath = pKV->GetString("content_path", "workshop_content");
	V_strcpy_safe(m_config.m_szContentPath, pszContentPath);
	
	// Load allow workshop in rotation
	m_config.m_bAllowWorkshopInRotation = pKV->GetBool("allow_workshop_in_rotation", true);
	
	// Load restrictions
	KeyValues* pRestrictionsSection = pKV->FindKey("restrictions");
	if (pRestrictionsSection)
	{
		m_config.m_nMaxFileSizeMB = pRestrictionsSection->GetInt("max_file_size", 100);
		m_config.m_bAllowSkins = pRestrictionsSection->GetBool("allow_skins", true);
		m_config.m_bAllowParticles = pRestrictionsSection->GetBool("allow_particles", true);
		m_config.m_bAllowSounds = pRestrictionsSection->GetBool("allow_sounds", false);
		m_config.m_bAllowHUDs = pRestrictionsSection->GetBool("allow_huds", false);
	}
	
	// Load sv_pure override
	m_config.m_nSvPureOverride = pKV->GetInt("sv_pure_override", -1);
	
	// Load logging settings
	KeyValues* pLoggingSection = pKV->FindKey("logging");
	if (pLoggingSection)
	{
		m_config.m_bLoggingEnabled = pLoggingSection->GetBool("enabled", true);
		m_config.m_bVerboseLogging = pLoggingSection->GetBool("verbose", false);
		m_config.m_bLogDownloads = pLoggingSection->GetBool("log_downloads", true);
		m_config.m_bLogUpdates = pLoggingSection->GetBool("log_updates", true);
	}
	
	pKV->deleteThis();
	
	m_bConfigLoaded = true;
	
	if (m_config.m_bLoggingEnabled)
	{
		CFWorkshopMsg("Loaded workshop configuration from workshop_config.txt\n");
		if (m_config.m_bVerboseLogging)
		{
			CFWorkshopMsg("  Collection ID: %llu\n", m_config.m_nCollectionID);
			CFWorkshopMsg("  Auto-download: %s\n", m_config.m_bAutoDownload ? "Yes" : "No");
			CFWorkshopMsg("  Map IDs: %d\n", m_config.m_vecMapIDs.Count());
			CFWorkshopMsg("  Item IDs: %d\n", m_config.m_vecItemIDs.Count());
			CFWorkshopMsg("  High Priority: %s\n", m_config.m_bHighPriority ? "Yes" : "No");
			CFWorkshopMsg("  Update Check Interval: %u seconds\n", m_config.m_nUpdateCheckInterval);
			CFWorkshopMsg("  Allow Skins: %s\n", m_config.m_bAllowSkins ? "Yes" : "No");
			CFWorkshopMsg("  Allow Particles: %s\n", m_config.m_bAllowParticles ? "Yes" : "No");
			CFWorkshopMsg("  Allow Sounds: %s\n", m_config.m_bAllowSounds ? "Yes" : "No");
			CFWorkshopMsg("  Allow HUDs: %s\n", m_config.m_bAllowHUDs ? "Yes" : "No");
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reload workshop configuration
//-----------------------------------------------------------------------------
void CCFWorkshopManager::ReloadConfig()
{
	LoadConfig();
	
	if (m_bConfigLoaded && m_config.m_bLoggingEnabled)
	{
		CFWorkshopMsg("Workshop configuration reloaded\n");
	}
}

void CCFWorkshopManager::GetItemsByType(CFWorkshopItemType_t type, CUtlVector<CCFWorkshopItem*>& items)
{
	items.RemoveAll();
	
	FOR_EACH_MAP_FAST(m_mapItems, i)
	{
		if (m_mapItems[i]->GetType() == type)
		{
			items.AddToTail(m_mapItems[i]);
		}
	}
}

PublishedFileId_t CCFWorkshopManager::GetSubscribedItem(uint32 index) const
{
	if (index < (uint32)m_vecSubscribedItems.Count())
	{
		return m_vecSubscribedItems[index];
	}
	return k_PublishedFileIdInvalid;
}

// Map-specific functions
bool CCFWorkshopManager::IsWorkshopMap(const char* pszMapName)
{
	if (!pszMapName)
		return false;

	// Workshop maps start with "workshop/"
	return (V_strnicmp(pszMapName, "workshop/", 9) == 0);
}

PublishedFileId_t CCFWorkshopManager::GetMapIDFromName(const char* pszMapName)
{
	if (!IsWorkshopMap(pszMapName))
		return k_PublishedFileIdInvalid;

	// Parse the ID from the map name
	// Format: workshop/1234567890 or workshop/mapname.ugc1234567890
	const char* pszID = pszMapName + 9; // Skip "workshop/"
	
	// Look for .ugc prefix
	const char* pszUGC = V_strstr(pszID, ".ugc");
	if (pszUGC)
	{
		pszID = pszUGC + 4; // Skip ".ugc"
	}

	// Parse the numeric ID
	PublishedFileId_t fileID = 0;
	sscanf(pszID, "%llu", &fileID);
	
	return fileID;
}

bool CCFWorkshopManager::GetCanonicalMapName(PublishedFileId_t fileID, const char* pszOriginalName, char* pszOut, size_t outSize)
{
	if (!pszOriginalName || !pszOut)
		return false;

	// Format: workshop/mapname.ugcFILEID
	char szBaseName[MAX_PATH];
	V_FileBase(pszOriginalName, szBaseName, sizeof(szBaseName));
	
	V_snprintf(pszOut, outSize, "workshop/%s.ugc%llu", szBaseName, fileID);
	return true;
}

void CCFWorkshopManager::PrepareMap(PublishedFileId_t fileID)
{
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (!pItem)
	{
		pItem = AddItem(fileID, CF_WORKSHOP_TYPE_MAP);
	}

	if (pItem && !pItem->IsDownloaded())
	{
		pItem->Download(true); // High priority for maps
	}
}

// Skin functions
bool CCFWorkshopManager::HasInstalledSkin(const char* pszItemClass)
{
	unsigned short index = m_mapActiveSkins.Find(pszItemClass);
	return (index != m_mapActiveSkins.InvalidIndex());
}

const char* CCFWorkshopManager::GetInstalledSkinPath(const char* pszItemClass)
{
	unsigned short index = m_mapActiveSkins.Find(pszItemClass);
	if (index != m_mapActiveSkins.InvalidIndex())
	{
		PublishedFileId_t fileID = m_mapActiveSkins[index];
		CCFWorkshopItem* pItem = GetItem(fileID);
		if (pItem && pItem->IsInstalled())
		{
			static char szPath[MAX_PATH];
			if (pItem->GetInstallPath(szPath, sizeof(szPath)))
			{
				return szPath;
			}
		}
	}
	return NULL;
}

void CCFWorkshopManager::ApplySkin(PublishedFileId_t fileID)
{
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (!pItem)
	{
		CFWorkshopWarning("Cannot apply skin - item %llu not found\n", fileID);
		return;
	}

	if (!pItem->IsInstalled())
	{
		CFWorkshopWarning("Cannot apply skin - item not installed\n");
		return;
	}

	// TODO: Determine item class from metadata and apply skin
	CFWorkshopMsg("Applied skin: %s\n", pItem->GetTitle());
}

void CCFWorkshopManager::RemoveSkin(const char* pszItemClass)
{
	unsigned short index = m_mapActiveSkins.Find(pszItemClass);
	if (index != m_mapActiveSkins.InvalidIndex())
	{
		m_mapActiveSkins.RemoveAt(index);
		CFWorkshopMsg("Removed skin for %s\n", pszItemClass);
	}
}

//-----------------------------------------------------------------------------
// Weapon Mod Functions
//-----------------------------------------------------------------------------

// Mount all subscribed weapon mods
void CCFWorkshopManager::MountAllWeaponMods()
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;
	
	CFWorkshopMsg("Mounting all subscribed weapon mods...\n");
	
	for (int i = 0; i < m_vecSubscribedItems.Count(); i++)
	{
		PublishedFileId_t fileID = m_vecSubscribedItems[i];
		CCFWorkshopItem* pItem = GetItem(fileID);
		
		if (pItem && pItem->GetType() == CF_WORKSHOP_TYPE_WEAPON_MOD)
		{
			MountWeaponMod(fileID);
		}
	}
}

// Mount a single weapon mod
bool CCFWorkshopManager::MountWeaponMod(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return false;
	
	// Check if already mounted
	if (IsWeaponModMounted(fileID))
	{
		CFWorkshopDebug("Weapon mod %llu already mounted\n", fileID);
		return true;
	}
	
	// Check if item is installed
	uint32 itemState = pUGC->GetItemState(fileID);
	if ((itemState & k_EItemStateInstalled) == 0)
	{
		CFWorkshopWarning("Weapon mod %llu not installed, skipping mount\n", fileID);
		return false;
	}
	
	// Get the install folder
	char szInstallPath[MAX_PATH];
	uint64 sizeOnDisk = 0;
	uint32 timestamp = 0;
	
	if (!pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
	{
		CFWorkshopWarning("Failed to get install info for weapon mod %llu\n", fileID);
		return false;
	}
	
	if (!g_pFullFileSystem->IsDirectory(szInstallPath))
	{
		CFWorkshopWarning("Workshop path doesn't exist: %s\n", szInstallPath);
		return false;
	}
	
	// Look for a weapon script file (*.txt files in scripts/items/)
	char szScriptsPath[MAX_PATH];
	V_snprintf(szScriptsPath, sizeof(szScriptsPath), "%s/scripts/items", szInstallPath);
	
	bool bFoundWeaponScript = false;
	CUtlVector<CUtlString> vecWeaponScripts;
	
	if (g_pFullFileSystem->IsDirectory(szScriptsPath))
	{
		// Search for .txt files that contain weapon definitions
		FileFindHandle_t hFind;
		char szSearchPath[MAX_PATH];
		V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.txt", szScriptsPath);
		
		const char* pszFilename = g_pFullFileSystem->FindFirstEx(szSearchPath, NULL, &hFind);
		while (pszFilename)
		{
			char szFullPath[MAX_PATH];
			V_snprintf(szFullPath, sizeof(szFullPath), "%s/%s", szScriptsPath, pszFilename);
			
			// Validate this is a weapon definition file by checking for "items" section
			KeyValues* pKVWeapons = new KeyValues("WeaponMod");
			if (pKVWeapons->LoadFromFile(g_pFullFileSystem, szFullPath, NULL))
			{
				KeyValues* pKVItems = pKVWeapons->FindKey("items");
				if (pKVItems)
				{
					// Count the items in the file
					int nItemCount = 0;
					FOR_EACH_TRUE_SUBKEY(pKVItems, pKVItem)
					{
						nItemCount++;
					}
					
					if (nItemCount > 0)
					{
						CFWorkshopMsg("Found weapon script with %d items: %s\n", nItemCount, pszFilename);
						vecWeaponScripts.AddToTail(pszFilename);
						bFoundWeaponScript = true;
					}
				}
			}
			pKVWeapons->deleteThis();
			
			pszFilename = g_pFullFileSystem->FindNext(hFind);
		}
		g_pFullFileSystem->FindClose(hFind);
	}
	
	if (bFoundWeaponScript)
	{
		// Add the workshop folder to search paths for models, materials, and scripts
		g_pFullFileSystem->AddSearchPath(szInstallPath, "GAME", PATH_ADD_TO_TAIL);
		g_pFullFileSystem->AddSearchPath(szInstallPath, "mod", PATH_ADD_TO_TAIL);
		
		m_vecMountedWeaponMods.AddToTail(fileID);
		
		CCFWorkshopItem* pItem = GetItem(fileID);
		Msg("[CF Workshop] Weapon mod '%s' mounted with %d script file(s).\n", 
			pItem ? pItem->GetTitle() : "Unknown", vecWeaponScripts.Count());
		Msg("[CF Workshop] Use 'cl_reload_item_schema' to load the new weapons.\n");
		
		return true;
	}
	else
	{
		// Still mount as a content pack even without weapon scripts (might have models/materials only)
		bool bHasContent = false;
		
		// Check for models folder
		char szModelsPath[MAX_PATH];
		V_snprintf(szModelsPath, sizeof(szModelsPath), "%s/models", szInstallPath);
		if (g_pFullFileSystem->IsDirectory(szModelsPath))
			bHasContent = true;
		
		// Check for materials folder
		char szMaterialsPath[MAX_PATH];
		V_snprintf(szMaterialsPath, sizeof(szMaterialsPath), "%s/materials", szInstallPath);
		if (g_pFullFileSystem->IsDirectory(szMaterialsPath))
			bHasContent = true;
		
		if (bHasContent)
		{
			g_pFullFileSystem->AddSearchPath(szInstallPath, "GAME", PATH_ADD_TO_TAIL);
			g_pFullFileSystem->AddSearchPath(szInstallPath, "mod", PATH_ADD_TO_TAIL);
			
			m_vecMountedWeaponMods.AddToTail(fileID);
			
			CCFWorkshopItem* pItem = GetItem(fileID);
			Msg("[CF Workshop] Weapon mod '%s' mounted (content only, no weapon scripts).\n", 
				pItem ? pItem->GetTitle() : "Unknown");
			
			return true;
		}
		
		CFWorkshopWarning("No valid weapon scripts or content found in mod %llu\n", fileID);
		return false;
	}
}

// Unmount a weapon mod
void CCFWorkshopManager::UnmountWeaponMod(PublishedFileId_t fileID)
{
	int index = m_vecMountedWeaponMods.Find(fileID);
	if (index == m_vecMountedWeaponMods.InvalidIndex())
	{
		CFWorkshopDebug("Weapon mod %llu not mounted\n", fileID);
		return;
	}
	
	ISteamUGC* pUGC = GetSteamUGC();
	if (pUGC)
	{
		char szInstallPath[MAX_PATH];
		uint64 sizeOnDisk = 0;
		uint32 timestamp = 0;
		
		if (pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
		{
			g_pFullFileSystem->RemoveSearchPath(szInstallPath, "GAME");
			g_pFullFileSystem->RemoveSearchPath(szInstallPath, "mod");
		}
	}
	
	m_vecMountedWeaponMods.Remove(index);
	
	CCFWorkshopItem* pItem = GetItem(fileID);
	Msg("[CF Workshop] Weapon mod '%s' unmounted.\n", pItem ? pItem->GetTitle() : "Unknown");
	
	// Note: Items already in the schema won't be removed - would require schema reload
	CFWorkshopWarning("Note: Weapon definitions remain in schema until restart.\n");
}

// Check if a weapon mod is mounted
bool CCFWorkshopManager::IsWeaponModMounted(PublishedFileId_t fileID) const
{
	return m_vecMountedWeaponMods.Find(fileID) != m_vecMountedWeaponMods.InvalidIndex();
}

// Get a mounted weapon mod by index
PublishedFileId_t CCFWorkshopManager::GetMountedWeaponMod(int index) const
{
	if (index >= 0 && index < m_vecMountedWeaponMods.Count())
		return m_vecMountedWeaponMods[index];
	return 0;
}

// Refresh all weapon mods - reload the schema and remount
void CCFWorkshopManager::RefreshWeaponMods()
{
#ifdef CLIENT_DLL
	CFWorkshopMsg("Refreshing weapon mods...\n");
	
	// Unmount all currently mounted weapon mods (just the search paths)
	for (int i = m_vecMountedWeaponMods.Count() - 1; i >= 0; i--)
	{
		PublishedFileId_t fileID = m_vecMountedWeaponMods[i];
		ISteamUGC* pUGC = GetSteamUGC();
		if (pUGC)
		{
			char szInstallPath[MAX_PATH];
			uint64 sizeOnDisk = 0;
			uint32 timestamp = 0;
			
			if (pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
			{
				g_pFullFileSystem->RemoveSearchPath(szInstallPath, "GAME");
				g_pFullFileSystem->RemoveSearchPath(szInstallPath, "mod");
			}
		}
	}
	m_vecMountedWeaponMods.Purge();
	
	// Trigger a schema reload (this will pick up workshop weapon scripts)
	engine->ClientCmd_Unrestricted("cl_reload_item_schema");
	
	// Remount all weapon mods
	MountAllWeaponMods();
#endif
}

//-----------------------------------------------------------------------------
// Weapon Skin Functions (file replacement like custom folder)
//-----------------------------------------------------------------------------

// Mount all subscribed weapon skins
void CCFWorkshopManager::MountAllWeaponSkins()
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;
	
	CFWorkshopMsg("Mounting all subscribed weapon skins...\n");
	
	for (int i = 0; i < m_vecSubscribedItems.Count(); i++)
	{
		PublishedFileId_t fileID = m_vecSubscribedItems[i];
		CCFWorkshopItem* pItem = GetItem(fileID);
		
		if (pItem && pItem->GetType() == CF_WORKSHOP_TYPE_WEAPON_SKIN)
		{
			MountWeaponSkin(fileID);
		}
	}
}

// Mount a single weapon skin - adds to search path with high priority to override base files
bool CCFWorkshopManager::MountWeaponSkin(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return false;
	
	// Check if already mounted
	if (IsWeaponSkinMounted(fileID))
	{
		CFWorkshopDebug("Weapon skin %llu already mounted\n", fileID);
		return true;
	}
	
	// Check if item is installed
	uint32 itemState = pUGC->GetItemState(fileID);
	if ((itemState & k_EItemStateInstalled) == 0)
	{
		CFWorkshopWarning("Weapon skin %llu not installed, skipping mount\n", fileID);
		return false;
	}
	
	// Get the install folder
	char szInstallPath[MAX_PATH];
	uint64 sizeOnDisk = 0;
	uint32 timestamp = 0;
	
	if (!pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
	{
		CFWorkshopWarning("Failed to get install info for weapon skin %llu\n", fileID);
		return false;
	}
	
	if (!g_pFullFileSystem->IsDirectory(szInstallPath))
	{
		CFWorkshopWarning("Workshop path doesn't exist: %s\n", szInstallPath);
		return false;
	}
	
	// Check for content that can override base game files
	bool bHasContent = false;
	CUtlVector<CUtlString> vecContentTypes;
	
	// Check for models folder
	char szModelsPath[MAX_PATH];
	V_snprintf(szModelsPath, sizeof(szModelsPath), "%s/models", szInstallPath);
	if (g_pFullFileSystem->IsDirectory(szModelsPath))
	{
		bHasContent = true;
		vecContentTypes.AddToTail("models");
	}
	
	// Check for materials folder
	char szMaterialsPath[MAX_PATH];
	V_snprintf(szMaterialsPath, sizeof(szMaterialsPath), "%s/materials", szInstallPath);
	if (g_pFullFileSystem->IsDirectory(szMaterialsPath))
	{
		bHasContent = true;
		vecContentTypes.AddToTail("materials");
	}
	
	// Check for sound folder
	char szSoundPath[MAX_PATH];
	V_snprintf(szSoundPath, sizeof(szSoundPath), "%s/sound", szInstallPath);
	if (g_pFullFileSystem->IsDirectory(szSoundPath))
	{
		bHasContent = true;
		vecContentTypes.AddToTail("sound");
	}
	
	// Check for particles folder
	char szParticlesPath[MAX_PATH];
	V_snprintf(szParticlesPath, sizeof(szParticlesPath), "%s/particles", szInstallPath);
	if (g_pFullFileSystem->IsDirectory(szParticlesPath))
	{
		bHasContent = true;
		vecContentTypes.AddToTail("particles");
	}
	
	if (!bHasContent)
	{
		CFWorkshopWarning("Weapon skin %llu has no content to mount (no models, materials, sound, or particles)\n", fileID);
		return false;
	}
	
	// Add the workshop folder to search paths with HIGH PRIORITY (PATH_ADD_TO_HEAD)
	// This makes it override base game files, like the custom folder
	g_pFullFileSystem->AddSearchPath(szInstallPath, "GAME", PATH_ADD_TO_HEAD);
	g_pFullFileSystem->AddSearchPath(szInstallPath, "mod", PATH_ADD_TO_HEAD);
	
	m_vecMountedWeaponSkins.AddToTail(fileID);
	
	// Build content type string for message
	char szContentTypes[256] = {0};
	for (int i = 0; i < vecContentTypes.Count(); i++)
	{
		if (i > 0) V_strcat_safe(szContentTypes, ", ");
		V_strcat_safe(szContentTypes, vecContentTypes[i].Get());
	}
	
	CCFWorkshopItem* pItem = GetItem(fileID);
	Msg("[CF Workshop] Weapon skin '%s' mounted with: %s\n", 
		pItem ? pItem->GetTitle() : "Unknown", szContentTypes);
	
#ifdef CLIENT_DLL
	// Request content reload to apply the new skin (deferred to avoid callback crash)
	RequestContentReload();
#endif
	
	return true;
}

// Unmount a weapon skin
void CCFWorkshopManager::UnmountWeaponSkin(PublishedFileId_t fileID)
{
	int index = m_vecMountedWeaponSkins.Find(fileID);
	if (index == m_vecMountedWeaponSkins.InvalidIndex())
	{
		CFWorkshopDebug("Weapon skin %llu not mounted\n", fileID);
		return;
	}
	
	ISteamUGC* pUGC = GetSteamUGC();
	if (pUGC)
	{
		char szInstallPath[MAX_PATH];
		uint64 sizeOnDisk = 0;
		uint32 timestamp = 0;
		
		if (pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
		{
			g_pFullFileSystem->RemoveSearchPath(szInstallPath, "GAME");
			g_pFullFileSystem->RemoveSearchPath(szInstallPath, "mod");
		}
	}
	
	m_vecMountedWeaponSkins.Remove(index);
	
	CCFWorkshopItem* pItem = GetItem(fileID);
	Msg("[CF Workshop] Weapon skin '%s' unmounted.\n", 
		pItem ? pItem->GetTitle() : "Unknown");
	
#ifdef CLIENT_DLL
	// Request content reload to revert to original models (deferred to avoid callback crash)
	RequestContentReload();
#endif
}

// Check if a weapon skin is mounted
bool CCFWorkshopManager::IsWeaponSkinMounted(PublishedFileId_t fileID) const
{
	return m_vecMountedWeaponSkins.Find(fileID) != m_vecMountedWeaponSkins.InvalidIndex();
}

// Get a mounted weapon skin by index
PublishedFileId_t CCFWorkshopManager::GetMountedWeaponSkin(int index) const
{
	if (index >= 0 && index < m_vecMountedWeaponSkins.Count())
		return m_vecMountedWeaponSkins[index];
	return 0;
}

// Refresh all weapon skins - remount all
void CCFWorkshopManager::RefreshWeaponSkins()
{
	CFWorkshopMsg("Refreshing weapon skins...\n");
	
	// Unmount all currently mounted weapon skins
	for (int i = m_vecMountedWeaponSkins.Count() - 1; i >= 0; i--)
	{
		PublishedFileId_t fileID = m_vecMountedWeaponSkins[i];
		ISteamUGC* pUGC = GetSteamUGC();
		if (pUGC)
		{
			char szInstallPath[MAX_PATH];
			uint64 sizeOnDisk = 0;
			uint32 timestamp = 0;
			
			if (pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
			{
				g_pFullFileSystem->RemoveSearchPath(szInstallPath, "GAME");
				g_pFullFileSystem->RemoveSearchPath(szInstallPath, "mod");
			}
		}
	}
	m_vecMountedWeaponSkins.Purge();
	
	// Remount all weapon skins
	MountAllWeaponSkins();
	
	Msg("[CF Workshop] Weapon skins refreshed. Restart or reload map to see changes.\n");
}

// Query functions
void CCFWorkshopManager::QueryPopularItems(CFWorkshopItemType_t type, uint32 page)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;

	CFWorkshopMsg("Querying popular workshop items (type %d, page %u)...\n", type, page);

	UGCQueryHandle_t hQuery = pUGC->CreateQueryAllUGCRequest(
		k_EUGCQuery_RankedByTrend,
		k_EUGCMatchingUGCType_Items,
		m_nAppID,
		m_nAppID,
		page
	);

	if (hQuery != k_UGCQueryHandleInvalid)
	{
		// Add filters based on type
		// TODO: Add tag filtering

		SteamAPICall_t hCall = pUGC->SendQueryUGCRequest(hQuery);
		m_callbackQueryCompleted.Set(hCall, this, &CCFWorkshopManager::Steam_OnQueryCompleted);
		m_hCurrentQuery = hQuery;
	}
}

void CCFWorkshopManager::QueryUserItems(uint32 page)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;

#ifdef CLIENT_DLL
	ISteamUser* pUser = steamapicontext ? steamapicontext->SteamUser() : NULL;
	if (!pUser)
		return;

	AccountID_t accountID = pUser->GetSteamID().GetAccountID();
	
	CFWorkshopMsg("Querying user's published items...\n");

	UGCQueryHandle_t hQuery = pUGC->CreateQueryUserUGCRequest(
		accountID,
		k_EUserUGCList_Published,
		k_EUGCMatchingUGCType_Items,
		k_EUserUGCListSortOrder_CreationOrderDesc,
		m_nAppID,
		m_nAppID,
		page
	);

	if (hQuery != k_UGCQueryHandleInvalid)
	{
		pUGC->SetReturnLongDescription(hQuery, true);
		
		SteamAPICall_t hCall = pUGC->SendQueryUGCRequest(hQuery);
		m_callbackQueryCompleted.Set(hCall, this, &CCFWorkshopManager::Steam_OnQueryCompleted);
		m_hCurrentQuery = hQuery;
	}
#endif
}

void CCFWorkshopManager::QueryItemDetails(PublishedFileId_t fileID)
{
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (pItem)
	{
		pItem->Refresh();
	}
	else
	{
		AddItem(fileID, CF_WORKSHOP_TYPE_OTHER);
	}
}

void CCFWorkshopManager::SearchWorkshop(const char* pszSearchText, CFWorkshopItemType_t type)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC || !pszSearchText)
		return;

	CFWorkshopMsg("Searching workshop for: %s\n", pszSearchText);

	UGCQueryHandle_t hQuery = pUGC->CreateQueryAllUGCRequest(
		k_EUGCQuery_RankedByTrend,
		k_EUGCMatchingUGCType_Items,
		m_nAppID,
		m_nAppID,
		1
	);

	if (hQuery != k_UGCQueryHandleInvalid)
	{
		pUGC->SetSearchText(hQuery, pszSearchText);
		
		SteamAPICall_t hCall = pUGC->SendQueryUGCRequest(hQuery);
		m_callbackQueryCompleted.Set(hCall, this, &CCFWorkshopManager::Steam_OnQueryCompleted);
		m_hCurrentQuery = hQuery;
	}
}

void CCFWorkshopManager::QueryRecentItems(uint32 numItems)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;

	// Don't start a new query if one is in progress
	if (m_hCurrentQuery != k_UGCQueryHandleInvalid)
		return;

	CFWorkshopMsg("Querying %u most recent workshop items...\n", numItems);

	UGCQueryHandle_t hQuery = pUGC->CreateQueryAllUGCRequest(
		k_EUGCQuery_RankedByPublicationDate,  // Most recent first
		k_EUGCMatchingUGCType_Items,
		m_nAppID,
		m_nAppID,
		1  // Page 1
	);

	if (hQuery != k_UGCQueryHandleInvalid)
	{
		// Limit results to requested number
		pUGC->SetReturnLongDescription(hQuery, true);
		pUGC->SetReturnMetadata(hQuery, true);
		
		SteamAPICall_t hCall = pUGC->SendQueryUGCRequest(hQuery);
		m_callbackQueryCompleted.Set(hCall, this, &CCFWorkshopManager::Steam_OnQueryCompleted);
		m_hCurrentQuery = hQuery;
	}
}

void CCFWorkshopManager::QueryCollection(PublishedFileId_t collectionID)
{
	if (collectionID == 0)
		return;
		
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;

	// Don't start a new query if one is in progress
	if (m_hCurrentQuery != k_UGCQueryHandleInvalid)
		return;

	if (m_bConfigLoaded && m_config.m_bLoggingEnabled)
	{
		CFWorkshopMsg("Querying Workshop collection %llu...\n", collectionID);
	}

	// Query the collection details to get the list of items
	PublishedFileId_t collectionArray[1] = { collectionID };
	UGCQueryHandle_t hQuery = pUGC->CreateQueryUGCDetailsRequest(collectionArray, 1);

	if (hQuery != k_UGCQueryHandleInvalid)
	{
		pUGC->SetReturnChildren(hQuery, true); // Get the items in the collection
		pUGC->SetReturnLongDescription(hQuery, true);
		pUGC->SetReturnMetadata(hQuery, true);
		
		SteamAPICall_t hCall = pUGC->SendQueryUGCRequest(hQuery);
		m_callbackQueryCompleted.Set(hCall, this, &CCFWorkshopManager::Steam_OnQueryCompleted);
		m_hCurrentQuery = hQuery;
	}
}

CCFWorkshopItem* CCFWorkshopManager::GetBrowseableItem(uint32 index) const
{
	if (index >= (uint32)m_vecBrowseableItems.Count())
		return NULL;
	return m_vecBrowseableItems[index];
}

void CCFWorkshopManager::SubscribeItem(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;

	CFWorkshopMsg("Subscribing to item %llu...\n", fileID);
	
	SteamAPICall_t hCall = pUGC->SubscribeItem(fileID);
	m_callbackSubscribe.Set(hCall, this, &CCFWorkshopManager::Steam_OnSubscribeItem);
}

void CCFWorkshopManager::UnsubscribeItem(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;

	CFWorkshopMsg("Unsubscribing from item %llu...\n", fileID);
	
	SteamAPICall_t hCall = pUGC->UnsubscribeItem(fileID);
	m_callbackUnsubscribe.Set(hCall, this, &CCFWorkshopManager::Steam_OnUnsubscribeItem);
}

void CCFWorkshopManager::DeleteItem(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return;
	
	if (m_bDeleteInProgress)
	{
		CFWorkshopWarning("Delete already in progress\n");
		return;
	}
	
	// Verify the user owns this item
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (pItem && !pItem->IsOwnedByCurrentUser())
	{
		CFWorkshopWarning("Cannot delete item %llu - not owned by current user\n", fileID);
		return;
	}
	
	CFWorkshopMsg("Deleting workshop item %llu...\n", fileID);
	
	m_bDeleteInProgress = true;
	m_nPendingDeleteFileID = fileID;
	
	SteamAPICall_t hCall = pUGC->DeleteItem(fileID);
	m_callbackDeleteItem.Set(hCall, this, &CCFWorkshopManager::Steam_OnDeleteItem);
}

void CCFWorkshopManager::Steam_OnDeleteItem(DeleteItemResult_t* pResult, bool bError)
{
	m_bDeleteInProgress = false;
	
	if (bError || pResult->m_eResult != k_EResultOK)
	{
		CFWorkshopWarning("Failed to delete item %llu (error: %d)\n", 
			m_nPendingDeleteFileID, pResult->m_eResult);
		V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Delete failed: error %d", pResult->m_eResult);
		return;
	}
	
	CFWorkshopMsg("Successfully deleted workshop item %llu\n", pResult->m_nPublishedFileId);
	V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Item deleted successfully");
	
	// Remove from our tracking
	RemoveItem(pResult->m_nPublishedFileId);
	
	m_nPendingDeleteFileID = 0;
}

bool CCFWorkshopManager::StartMetadataUpdate(
	PublishedFileId_t fileID,
	const char* pszTitle,
	const char* pszDescription,
	ERemoteStoragePublishedFileVisibility visibility,
	const char* pszTags)
{
	if (m_bUploadInProgress)
	{
		CFWorkshopWarning("Upload already in progress\n");
		return false;
	}
	
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		CFWorkshopWarning("Steam UGC not available\n");
		return false;
	}
	
	// Verify the user owns this item
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (pItem && !pItem->IsOwnedByCurrentUser())
	{
		CFWorkshopWarning("Cannot update item %llu - not owned by current user\n", fileID);
		V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Error: You don't own this item");
		return false;
	}
	
	CFWorkshopMsg("Starting metadata update for item %llu...\n", fileID);
	
	// Start an update handle
	m_hCurrentUpdate = pUGC->StartItemUpdate(m_nAppID, fileID);
	if (m_hCurrentUpdate == k_UGCUpdateHandleInvalid)
	{
		CFWorkshopWarning("Failed to start update for item %llu\n", fileID);
		V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Failed to start update");
		return false;
	}
	
	// Set title
	if (pszTitle && pszTitle[0])
	{
		pUGC->SetItemTitle(m_hCurrentUpdate, pszTitle);
	}
	
	// Set description
	if (pszDescription && pszDescription[0])
	{
		pUGC->SetItemDescription(m_hCurrentUpdate, pszDescription);
	}
	
	// Set visibility
	pUGC->SetItemVisibility(m_hCurrentUpdate, visibility);
	
	// Set tags if provided
	if (pszTags && pszTags[0])
	{
		// Parse comma-separated tags
		SteamParamStringArray_t tagArray;
		CUtlVector<const char*> tagPtrs;
		CUtlVector<CUtlString> tagStrings;
		
		char szTagsCopy[1024];
		V_strncpy(szTagsCopy, pszTags, sizeof(szTagsCopy));
		
		char* pszToken = strtok(szTagsCopy, ",");
		while (pszToken)
		{
			// Trim whitespace
			while (*pszToken == ' ') pszToken++;
			char* pszEnd = pszToken + V_strlen(pszToken) - 1;
			while (pszEnd > pszToken && *pszEnd == ' ') *pszEnd-- = '\0';
			
			if (*pszToken)
			{
				tagStrings.AddToTail(pszToken);
			}
			pszToken = strtok(NULL, ",");
		}
		
		for (int i = 0; i < tagStrings.Count(); i++)
		{
			tagPtrs.AddToTail(tagStrings[i].Get());
		}
		
		tagArray.m_ppStrings = tagPtrs.Base();
		tagArray.m_nNumStrings = tagPtrs.Count();
		
		pUGC->SetItemTags(m_hCurrentUpdate, &tagArray);
	}
	
	// Submit the update (no content or preview changes)
	m_bUploadInProgress = true;
	V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Updating item metadata...");
	
	SteamAPICall_t hCall = pUGC->SubmitItemUpdate(m_hCurrentUpdate, "Metadata update");
	m_callbackSubmitUpdate.Set(hCall, this, &CCFWorkshopManager::Steam_OnSubmitUpdate);
	
	return true;
}

void CCFWorkshopManager::ClearBrowseableItems()
{
	// Clear the browseable items cache
	m_vecBrowseableItems.RemoveAll();
	
	// Also clear the item map to force fresh data
	FOR_EACH_MAP_FAST(m_mapItems, i)
	{
		delete m_mapItems[i];
	}
	m_mapItems.RemoveAll();
	
	CFWorkshopMsg("Cleared workshop item cache\n");
}

//-----------------------------------------------------------------------------
// Upload Functions
//-----------------------------------------------------------------------------
bool CCFWorkshopManager::StartUpload(
	const char* pszTitle,
	const char* pszDescription,
	const char* pszContentPath,
	const char* pszPreviewImagePath,
	CFWorkshopItemType_t type,
	ERemoteStoragePublishedFileVisibility visibility,
	PublishedFileId_t existingFileID,
	const char* pszTags,
	const char* pszScreenshotsPath)
{
	if (m_bUploadInProgress)
	{
		CFWorkshopWarning("Upload already in progress\n");
		return false;
	}

	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		CFWorkshopWarning("Steam UGC not available\n");
		return false;
	}

	// Validate inputs
	if (!pszTitle || !pszTitle[0])
	{
		CFWorkshopWarning("Title is required\n");
		return false;
	}

	if (!pszContentPath || !pszContentPath[0])
	{
		CFWorkshopWarning("Content path is required\n");
		return false;
	}

	// Convert content path to absolute path
	char szAbsContentPath[MAX_PATH];
	V_MakeAbsolutePath(szAbsContentPath, sizeof(szAbsContentPath), pszContentPath, NULL);
	
	// Check if content path exists
	if (!g_pFullFileSystem->IsDirectory(szAbsContentPath))
	{
		CFWorkshopWarning("Content path does not exist: %s\n", szAbsContentPath);
		return false;
	}

	// Convert preview path to absolute if provided
	char szAbsPreviewPath[MAX_PATH] = {0};
	if (pszPreviewImagePath && pszPreviewImagePath[0])
	{
		V_MakeAbsolutePath(szAbsPreviewPath, sizeof(szAbsPreviewPath), pszPreviewImagePath, NULL);
		
		// Check if preview image exists
		if (!g_pFullFileSystem->FileExists(szAbsPreviewPath))
		{
			CFWorkshopWarning("Preview image does not exist: %s\n", szAbsPreviewPath);
			// Don't fail, just warn and proceed without preview
			szAbsPreviewPath[0] = '\0';
		}
	}

	// Convert screenshots path to absolute if provided
	char szAbsScreenshotsPath[MAX_PATH] = {0};
	if (pszScreenshotsPath && pszScreenshotsPath[0])
	{
		V_MakeAbsolutePath(szAbsScreenshotsPath, sizeof(szAbsScreenshotsPath), pszScreenshotsPath, NULL);
		
		// Check if screenshots path exists
		if (!g_pFullFileSystem->IsDirectory(szAbsScreenshotsPath))
		{
			CFWorkshopWarning("Screenshots path does not exist: %s\n", szAbsScreenshotsPath);
			// Don't fail, just warn and proceed without screenshots
			szAbsScreenshotsPath[0] = '\0';
		}
	}

	// Store pending upload info with absolute paths
	m_strPendingTitle = pszTitle;
	m_strPendingDescription = pszDescription ? pszDescription : "";
	m_strPendingContentPath = szAbsContentPath;
	m_strPendingPreviewPath = szAbsPreviewPath;
	m_strPendingScreenshotsPath = szAbsScreenshotsPath;
	m_strPendingTags = pszTags ? pszTags : "";
	m_ePendingType = type;
	m_ePendingVisibility = visibility;
	m_bUploadInProgress = true;

	V_strcpy_safe(m_szUploadStatus, "Starting upload...");

	if (existingFileID != 0)
	{
		// Update existing item
		CFWorkshopMsg("Updating existing workshop item %llu...\n", existingFileID);
		m_nLastUploadedFileID = existingFileID;
		
		// Start the update directly
		StartItemUpdate(existingFileID);
	}
	else
	{
		// Create new item
		CFWorkshopMsg("Creating new workshop item...\n");
		
		SteamAPICall_t hCall = pUGC->CreateItem(m_nAppID, k_EWorkshopFileTypeCommunity);
		m_callbackCreateItem.Set(hCall, this, &CCFWorkshopManager::Steam_OnCreateItem);
	}

	return true;
}

void CCFWorkshopManager::StartItemUpdate(PublishedFileId_t fileID)
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		m_bUploadInProgress = false;
		V_strcpy_safe(m_szUploadStatus, "Error: Steam UGC not available");
		return;
	}

	m_hCurrentUpdate = pUGC->StartItemUpdate(m_nAppID, fileID);
	if (m_hCurrentUpdate == k_UGCUpdateHandleInvalid)
	{
		m_bUploadInProgress = false;
		V_strcpy_safe(m_szUploadStatus, "Error: Failed to start update");
		return;
	}

	// Set item properties
	pUGC->SetItemTitle(m_hCurrentUpdate, m_strPendingTitle.Get());
	
	if (m_strPendingDescription.Length() > 0)
	{
		pUGC->SetItemDescription(m_hCurrentUpdate, m_strPendingDescription.Get());
	}

	// Build tags array from user-selected tags
	CUtlVector<const char*> tagList;
	CUtlVector<CUtlString> tagStorage;  // Keep strings alive
	
	// Check if user has already selected a mod type tag
	bool bHasModTypeTag = false;
	
	// Parse and add user-selected tags from comma-separated string
	if (m_strPendingTags.Length() > 0)
	{
		CUtlString tagsCopy = m_strPendingTags;
		char* pszContext = NULL;
#ifdef _WIN32
		char* pszToken = strtok_s(tagsCopy.Access(), ",", &pszContext);
#else
		char* pszToken = strtok_r(tagsCopy.Access(), ",", &pszContext);
#endif
		while (pszToken)
		{
			// Trim whitespace
			while (*pszToken == ' ') pszToken++;
			char* pszEnd = pszToken + V_strlen(pszToken) - 1;
			while (pszEnd > pszToken && *pszEnd == ' ') { *pszEnd = '\0'; pszEnd--; }
			
			if (*pszToken)
			{
				// Check if this is a mod type tag
				if (V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_MAP) == 0 ||
					V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_SKIN) == 0 ||
					V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_WEAPON) == 0 ||
					V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_GUI) == 0 ||
					V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_PARTICLES) == 0 ||
					V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_SOUNDS) == 0 ||
					V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_SPRAYS) == 0 ||
					V_stricmp(pszToken, CF_WORKSHOP_TAG_MOD_MISC) == 0)
				{
					bHasModTypeTag = true;
				}
				
				// Store the string and add pointer to list
				int idx = tagStorage.AddToTail();
				tagStorage[idx] = pszToken;
				tagList.AddToTail(tagStorage[idx].Get());
			}
#ifdef _WIN32
			pszToken = strtok_s(NULL, ",", &pszContext);
#else
			pszToken = strtok_r(NULL, ",", &pszContext);
#endif
		}
	}
	
	// Only add automatic type tag if user hasn't selected a mod type tag
	if (!bHasModTypeTag)
	{
		const char* pszTypeTag = NULL;
		switch (m_ePendingType)
		{
			case CF_WORKSHOP_TYPE_MAP: pszTypeTag = CF_WORKSHOP_TAG_MOD_MAP; break;
			case CF_WORKSHOP_TYPE_WEAPON_SKIN: pszTypeTag = CF_WORKSHOP_TAG_MOD_SKIN; break;
			case CF_WORKSHOP_TYPE_WEAPON_MOD: pszTypeTag = CF_WORKSHOP_TAG_MOD_WEAPON; break;
			case CF_WORKSHOP_TYPE_CHARACTER_SKIN: pszTypeTag = CF_WORKSHOP_TAG_MOD_SKIN; break;
			case CF_WORKSHOP_TYPE_PARTICLE_EFFECT: pszTypeTag = CF_WORKSHOP_TAG_MOD_PARTICLES; break;
			case CF_WORKSHOP_TYPE_SOUND_MOD: pszTypeTag = CF_WORKSHOP_TAG_MOD_SOUNDS; break;
			case CF_WORKSHOP_TYPE_HUD: pszTypeTag = CF_WORKSHOP_TAG_MOD_GUI; break;
			default: pszTypeTag = CF_WORKSHOP_TAG_MOD_MISC; break;
		}
		
		if (pszTypeTag)
		{
			// Add to beginning of list
			int idx = tagStorage.AddToTail();
			tagStorage[idx] = pszTypeTag;
			tagList.InsertBefore(0, tagStorage[idx].Get());
		}
	}

	SteamParamStringArray_t tagArray;
	tagArray.m_ppStrings = tagList.Base();
	tagArray.m_nNumStrings = tagList.Count();
	pUGC->SetItemTags(m_hCurrentUpdate, &tagArray);

	// Set content folder
	pUGC->SetItemContent(m_hCurrentUpdate, m_strPendingContentPath.Get());

	// Set preview image if provided
	if (m_strPendingPreviewPath.Length() > 0)
	{
		pUGC->SetItemPreview(m_hCurrentUpdate, m_strPendingPreviewPath.Get());
	}

	// Add additional screenshots if folder provided
	if (m_strPendingScreenshotsPath.Length() > 0)
	{
		FileFindHandle_t findHandle;
		char szSearchPath[MAX_PATH];
		V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.*", m_strPendingScreenshotsPath.Get());
		
		const char* pszFileName = g_pFullFileSystem->FindFirstEx(szSearchPath, "GAME", &findHandle);
		int nScreenshotCount = 0;
		
		while (pszFileName && nScreenshotCount < 10)  // Limit to 10 additional screenshots
		{
			if (!g_pFullFileSystem->FindIsDirectory(findHandle))
			{
				// Check if it's an image file
				const char* pszExt = V_GetFileExtension(pszFileName);
				if (pszExt && (V_stricmp(pszExt, "jpg") == 0 || V_stricmp(pszExt, "jpeg") == 0 || 
							  V_stricmp(pszExt, "png") == 0))
				{
					char szFullPath[MAX_PATH];
					V_snprintf(szFullPath, sizeof(szFullPath), "%s/%s", m_strPendingScreenshotsPath.Get(), pszFileName);
					
					// Add as additional preview (k_EItemPreviewType_Image = 0)
					if (pUGC->AddItemPreviewFile(m_hCurrentUpdate, szFullPath, k_EItemPreviewType_Image))
					{
						CFWorkshopMsg("Added screenshot: %s\n", pszFileName);
						nScreenshotCount++;
					}
					else
					{
						CFWorkshopWarning("Failed to add screenshot: %s\n", pszFileName);
					}
				}
			}
			
			pszFileName = g_pFullFileSystem->FindNext(findHandle);
		}
		
		g_pFullFileSystem->FindClose(findHandle);
		
		if (nScreenshotCount > 0)
		{
			CFWorkshopMsg("Added %d screenshot(s) to workshop item\n", nScreenshotCount);
		}
	}

	// Set visibility
	pUGC->SetItemVisibility(m_hCurrentUpdate, m_ePendingVisibility);

	V_strcpy_safe(m_szUploadStatus, "Uploading content...");

	// Submit the update
	SteamAPICall_t hCall = pUGC->SubmitItemUpdate(m_hCurrentUpdate, "Uploaded via Custom Fortress 2");
	m_callbackSubmitUpdate.Set(hCall, this, &CCFWorkshopManager::Steam_OnSubmitUpdate);
}

float CCFWorkshopManager::GetUploadProgress() const
{
	if (!m_bUploadInProgress || m_hCurrentUpdate == k_UGCUpdateHandleInvalid)
		return 0.0f;

	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
		return 0.0f;

	uint64 bytesProcessed = 0;
	uint64 bytesTotal = 0;
	EItemUpdateStatus status = pUGC->GetItemUpdateProgress(m_hCurrentUpdate, &bytesProcessed, &bytesTotal);

	if (bytesTotal > 0)
	{
		return (float)bytesProcessed / (float)bytesTotal;
	}

	// Return rough progress based on status
	switch (status)
	{
		case k_EItemUpdateStatusPreparingConfig: return 0.1f;
		case k_EItemUpdateStatusPreparingContent: return 0.2f;
		case k_EItemUpdateStatusUploadingContent: return 0.5f;
		case k_EItemUpdateStatusUploadingPreviewFile: return 0.8f;
		case k_EItemUpdateStatusCommittingChanges: return 0.95f;
		default: return 0.0f;
	}
}

void CCFWorkshopManager::PrintStatus()
{
	Msg("=== Custom Fortress 2 Workshop Status ===\n");
	Msg("Subscribed Items: %u\n", m_vecSubscribedItems.Count());
	Msg("Tracked Items: %u\n", m_mapItems.Count());
	Msg("\n");

	FOR_EACH_MAP_FAST(m_mapItems, i)
	{
		CCFWorkshopItem* pItem = m_mapItems[i];
		const char* pszState = "Unknown";
		
		switch (pItem->GetState())
		{
			case CF_WORKSHOP_STATE_NONE: pszState = "Not Downloaded"; break;
			case CF_WORKSHOP_STATE_QUERYING: pszState = "Querying"; break;
			case CF_WORKSHOP_STATE_DOWNLOADING: pszState = "Downloading"; break;
			case CF_WORKSHOP_STATE_DOWNLOADED: pszState = "Downloaded"; break;
			case CF_WORKSHOP_STATE_INSTALLING: pszState = "Installing"; break;
			case CF_WORKSHOP_STATE_INSTALLED: pszState = "Installed"; break;
			case CF_WORKSHOP_STATE_ERROR: pszState = "Error"; break;
			case CF_WORKSHOP_STATE_NEEDS_UPDATE: pszState = "Needs Update"; break;
		}

		Msg("  [%llu] %s - %s (%.1f MB)\n", 
			pItem->GetFileID(),
			pItem->GetTitle(),
			pszState,
			pItem->GetFileSize() / (1024.0f * 1024.0f)
		);
	}
}

void CCFWorkshopManager::OnMapLoad(const char* pszMapName)
{
	if (IsWorkshopMap(pszMapName))
	{
		PublishedFileId_t fileID = GetMapIDFromName(pszMapName);
		if (fileID != k_PublishedFileIdInvalid)
		{
			PrepareMap(fileID);
			
#ifndef CLIENT_DLL
			// Server: Advertise in Steam server info (visible in server browser)
			AdvertiseWorkshopMapID(fileID);
			
			// Server: Broadcast workshop map ID to all clients
			BroadcastWorkshopMapID(fileID);
#endif
		}
	}
	else
	{
#ifndef CLIENT_DLL
		// Not a workshop map, clear the server info
		AdvertiseWorkshopMapID(0);
#endif
	}
}

#ifndef CLIENT_DLL
// Server: Broadcast the workshop map ID to all clients
void CCFWorkshopManager::BroadcastWorkshopMapID(PublishedFileId_t fileID)
{
	if (fileID == 0 || fileID == k_PublishedFileIdInvalid)
		return;
	
	CFWorkshopMsg("Broadcasting workshop map ID %llu to clients\\n", fileID);
	
	// Send to all players
	CRecipientFilter filter;
	filter.AddAllPlayers();
	
	UserMessageBegin( filter, "WorkshopMapID" );
		WRITE_LONG( (uint32)(fileID & 0xFFFFFFFF) );  // Low 32 bits
		WRITE_LONG( (uint32)(fileID >> 32) );          // High 32 bits
	MessageEnd();
}

// Server: Advertise workshop map ID in Steam server info
void CCFWorkshopManager::AdvertiseWorkshopMapID(PublishedFileId_t fileID)
{
#ifndef NO_STEAM
	if (!steamgameserverapicontext || !steamgameserverapicontext->SteamGameServer())
		return;
	
	ISteamGameServer* pServer = steamgameserverapicontext->SteamGameServer();
	
	if (fileID != 0 && fileID != k_PublishedFileIdInvalid)
	{
		// Set the Workshop map ID as a server key-value pair
		// This will be visible in server browser queries (A2S_RULES)
		char szFileID[32];
		V_snprintf(szFileID, sizeof(szFileID), "%llu", fileID);
		pServer->SetKeyValue("workshop_map_id", szFileID);
		
		CFWorkshopMsg("Advertising workshop map ID %llu in server info\\n", fileID);
	}
	else
	{
		// Clear the Workshop map ID if map is not from workshop
		pServer->SetKeyValue("workshop_map_id", "0");
	}
#endif
}

// Server: Broadcast list of subscribed addons to a client (for listen server auto-download)
void CCFWorkshopManager::BroadcastAddonList(edict_t* pClient)
{
	// Only send on listen servers (not dedicated servers)
	if (!pClient || engine->IsDedicatedServer())
		return;
	
	// Get list of subscribed items (excluding maps, since those are handled separately)
	CUtlVector<PublishedFileId_t> addonList;
	
	// Iterate through all subscribed items
	for (int i = 0; i < m_vecSubscribedItems.Count(); i++)
	{
		PublishedFileId_t fileID = m_vecSubscribedItems[i];
		CCFWorkshopItem* pItem = GetItem(fileID);
		
		if (!pItem)
			continue;
		
		// Skip maps - they're handled by the workshop map system
		if (pItem->GetType() == CF_WORKSHOP_TYPE_MAP)
			continue;
		
		// Only send subscribed and downloaded items
		if (pItem->IsSubscribed() && pItem->IsDownloaded())
		{
			addonList.AddToTail(fileID);
		}
	}
	
	// Limit to 255 items to fit in a byte
	int numToSend = MIN(addonList.Count(), 255);
	
	if (numToSend == 0)
	{
		CFWorkshopDebug("No addons to broadcast to client\\n");
		// Still send an empty list so client knows we tried
	}
	else
	{
		CFWorkshopMsg("Broadcasting %d workshop addons to client\\n", numToSend);
	}
	
	// Send to the specific client
	CBasePlayer* pPlayer = (CBasePlayer*)CBaseEntity::Instance(pClient);
	if (!pPlayer)
		return;
	
	CRecipientFilter filter;
	filter.AddRecipient(pPlayer);
	filter.MakeReliable();
	
	UserMessageBegin(filter, "AddonList");
		WRITE_BYTE(numToSend);
		
		// Write each addon's file ID (as two 32-bit values)
		for (int i = 0; i < numToSend; i++)
		{
			PublishedFileId_t fileID = addonList[i];
			WRITE_LONG((uint32)(fileID & 0xFFFFFFFF));  // Low 32 bits
			WRITE_LONG((uint32)(fileID >> 32));          // High 32 bits
		}
	MessageEnd();
}
#endif

#ifdef CLIENT_DLL
// Client: Handle workshop map ID from server
void CCFWorkshopManager::OnWorkshopMapIDReceived(PublishedFileId_t fileID)
{
	if (fileID == 0 || fileID == k_PublishedFileIdInvalid)
		return;
	
	CFWorkshopMsg("Received workshop map ID %llu from server\\n", fileID);
	
	// Check if we already have this map
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (pItem)
	{
		if (pItem->IsDownloaded() || pItem->IsInstalled())
		{
			CFWorkshopMsg("Map already downloaded/installed\\n");
			return;
		}
	}
	
	// Check if we're subscribed
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		CFWorkshopWarning("Steam UGC not available, cannot download map\\n");
		return;
	}
	
	uint32 numSubscribed = pUGC->GetNumSubscribedItems();
	PublishedFileId_t* pSubscribed = new PublishedFileId_t[numSubscribed];
	pUGC->GetSubscribedItems(pSubscribed, numSubscribed);
	
	bool bIsSubscribed = false;
	for (uint32 i = 0; i < numSubscribed; i++)
	{
		if (pSubscribed[i] == fileID)
		{
			bIsSubscribed = true;
			break;
		}
	}
	delete[] pSubscribed;
	
	if (!bIsSubscribed)
	{
		// Not subscribed - subscribe now
		CFWorkshopMsg("Not subscribed to map %llu, subscribing...\\n", fileID);
		SubscribeItem(fileID);
	}
	else
	{
		CFWorkshopMsg("Already subscribed to map %llu\\n", fileID);
	}
	
	// Add or update the item
	if (!pItem)
	{
		pItem = AddItem(fileID, CF_WORKSHOP_TYPE_MAP);
	}
	
	// Download the map if not already downloaded
	if (pItem && !pItem->IsDownloaded())
	{
		CFWorkshopMsg("Downloading map %llu...\\n", fileID);
		pItem->Download(true);  // High priority
	}
}

// Client: Parse Workshop map ID from server info string
PublishedFileId_t CCFWorkshopManager::ParseWorkshopMapIDFromServerInfo(const char* pszValue)
{
	if (!pszValue || !pszValue[0])
		return k_PublishedFileIdInvalid;
	
	// Parse the 64-bit file ID from string
	return (PublishedFileId_t)V_atoui64(pszValue);
}

// Client: Check if we need to download map before connecting
bool CCFWorkshopManager::CheckAndDownloadMapBeforeConnect(const char* pszServerIP, const char* pszMapName, PublishedFileId_t fileID)
{
	if (fileID == 0 || fileID == k_PublishedFileIdInvalid)
	{
		// Not a workshop map, normal connection
		return true;
	}
	
	CFWorkshopMsg("Server requires workshop map %llu (%s)\\n", fileID, pszMapName);
	
	// Check if we have the map file locally
	char szMapPath[MAX_PATH];
	V_snprintf(szMapPath, sizeof(szMapPath), "maps/%s.bsp", pszMapName);
	
	if (filesystem->FileExists(szMapPath, "GAME"))
	{
		CFWorkshopMsg("Map file already exists locally\\n");
		return true;
	}
	
	// Check if we have the item
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (pItem && pItem->IsDownloaded())
	{
		CFWorkshopMsg("Workshop item already downloaded\\n");
		return true;
	}
	
	// We need to download this map
	Warning("Downloading required workshop map before connecting...\\n");
	Warning("Workshop ID: %llu\\n", fileID);
	Warning("This may take a moment. Please wait...\\n");
	
	// Subscribe and download
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		Warning("Steam Workshop not available!\\n");
		return false;
	}
	
	// Check if subscribed
	uint32 numSubscribed = pUGC->GetNumSubscribedItems();
	bool bIsSubscribed = false;
	
	if (numSubscribed > 0)
	{
		PublishedFileId_t* pSubscribed = new PublishedFileId_t[numSubscribed];
		pUGC->GetSubscribedItems(pSubscribed, numSubscribed);
		
		for (uint32 i = 0; i < numSubscribed; i++)
		{
			if (pSubscribed[i] == fileID)
			{
				bIsSubscribed = true;
				break;
			}
		}
		delete[] pSubscribed;
	}
	
	if (!bIsSubscribed)
	{
		SubscribeItem(fileID);
	}
	
	// Add to tracking
	if (!pItem)
	{
		pItem = AddItem(fileID, CF_WORKSHOP_TYPE_MAP);
	}
	
	// Start download
	if (pItem)
	{
		pItem->Download(true);  // High priority
	}
	
	// Return false to indicate download is in progress
	// The connection should be retried after download completes
	return false;
}
#endif

void CCFWorkshopManager::GameServerSteamAPIActivated()
{
#ifdef GAME_DLL
	CFWorkshopMsg("Dedicated server Steam API activated\n");
	InitWorkshop();
#endif
}

// Steam callbacks
void CCFWorkshopManager::Steam_OnDownloadItem(DownloadItemResult_t* pResult)
{
	PublishedFileId_t fileID = pResult->m_nPublishedFileId;
	
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (pItem)
	{
		pItem->OnDownloadComplete(pResult);
		
		// Mount the item immediately so maps are available without restart
		if (pResult->m_eResult == k_EResultOK)
		{
			MountWorkshopItem(fileID);
		}
	}
}

void CCFWorkshopManager::Steam_OnItemInstalled(ItemInstalled_t* pResult)
{
	PublishedFileId_t fileID = pResult->m_nPublishedFileId;
	
	CCFWorkshopItem* pItem = GetItem(fileID);
	if (pItem)
	{
		pItem->OnItemInstalled(pResult);
	}
	
	// Mount the item immediately so maps are available without restart
	MountWorkshopItem(fileID);
}

void CCFWorkshopManager::Steam_OnQueryCompleted(SteamUGCQueryCompleted_t* pResult, bool bError)
{
	if (bError || pResult->m_eResult != k_EResultOK)
	{
		CFWorkshopWarning("Query failed (result: %d)\n", pResult->m_eResult);
		m_hCurrentQuery = k_UGCQueryHandleInvalid;
		return;
	}

	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		m_hCurrentQuery = k_UGCQueryHandleInvalid;
		return;
	}

	CFWorkshopMsg("Query completed: %u results\n", pResult->m_unNumResultsReturned);

	// Clear old browseable items
	m_vecBrowseableItems.RemoveAll();

	// Process results and populate browseable items
	for (uint32 i = 0; i < pResult->m_unNumResultsReturned && i < 50; i++)
	{
		SteamUGCDetails_t details;
		if (pUGC->GetQueryUGCResult(pResult->m_handle, i, &details))
		{
			// Check if this is a collection - if so, download all children
			if (details.m_eFileType == k_EWorkshopFileTypeCollection)
			{
				CFWorkshopMsg("Found collection: %s (%llu) with %u children\n", details.m_rgchTitle, details.m_nPublishedFileId, details.m_unNumChildren);
				
				// Get child items from the collection
				uint32 numChildren = details.m_unNumChildren;
				if (numChildren > 0)
				{
					PublishedFileId_t* pChildIDs = new PublishedFileId_t[numChildren];
					bool bSuccess = pUGC->GetQueryUGCChildren(pResult->m_handle, i, pChildIDs, numChildren);
					
					if (bSuccess)
					{
						CFWorkshopMsg("Successfully retrieved %u items from collection, subscribing and downloading...\n", numChildren);
						
						// Subscribe to and download each child item
						for (uint32 j = 0; j < numChildren; j++)
						{
							PublishedFileId_t childID = pChildIDs[j];
							
							if (childID == 0)
								continue;
								
							Msg("[CF Workshop] Collection item %d/%d: ID %llu\n", j + 1, numChildren, childID);
							
							// Add or get the item
							CCFWorkshopItem* pChildItem = GetItem(childID);
							if (!pChildItem)
							{
								pChildItem = AddItem(childID, CF_WORKSHOP_TYPE_OTHER);
								Msg("[CF Workshop]   Created new item entry\n");
							}
							
							// Subscribe if not already subscribed
							if (pChildItem && !pChildItem->IsSubscribed())
							{
								Msg("[CF Workshop]   Subscribing...\n");
								SubscribeItem(childID);
							}
							else if (pChildItem)
							{
								Msg("[CF Workshop]   Already subscribed\n");
							}
							
							// Download if auto-download is enabled
							if (pChildItem && m_bConfigLoaded && m_config.m_bAutoDownload)
							{
								if (!pChildItem->IsDownloaded())
								{
									Msg("[CF Workshop]   Downloading (priority: %s)...\n", m_config.m_bHighPriority ? "high" : "normal");
									pChildItem->Download(m_config.m_bHighPriority);
								}
								else
								{
									Msg("[CF Workshop]   Already downloaded\n");
								}
							}
						}
					}
					else
					{
						Warning("[CF Workshop] Failed to retrieve collection children!\n");
					}
					
					delete[] pChildIDs;
				}
				else
				{
					Warning("[CF Workshop] Collection has 0 children!\n");
				}
				
				continue; // Skip adding the collection itself to browseable items
			}
			
			// Skip deleted, banned, or otherwise invalid items
			if (details.m_eResult != k_EResultOK)
			{
				CFWorkshopMsg("  Skipping item %llu (result: %d)\n", details.m_nPublishedFileId, details.m_eResult);
				continue;
			}
			
			// Skip banned items
			if (details.m_bBanned)
			{
				CFWorkshopMsg("  Skipping banned item %llu\n", details.m_nPublishedFileId);
				continue;
			}
			
			// Skip items with empty titles (often indicates deleted items)
			if (details.m_rgchTitle[0] == '\0')
			{
				CFWorkshopMsg("  Skipping item %llu with empty title\n", details.m_nPublishedFileId);
				continue;
			}
			
			// Check if the item actually exists on disk or is downloadable
			uint32 itemState = pUGC->GetItemState(details.m_nPublishedFileId);
			
			// If item state indicates it doesn't exist at all (state is 0 means not tracked)
			// But we also need to check if it's downloadable - deleted items won't be
			// Skip items that have no state and no file size (likely deleted)
			if (itemState == 0 && details.m_nFileSize == 0)
			{
				CFWorkshopMsg("  Skipping item %llu - appears deleted (no state, no size)\n", details.m_nPublishedFileId);
				continue;
			}
			
			CFWorkshopMsg("  Result %u: %s (ID: %llu, State: %u)\n", i, details.m_rgchTitle, details.m_nPublishedFileId, itemState);
			
			// Get or create item for this result
			CCFWorkshopItem* pItem = GetItem(details.m_nPublishedFileId);
			if (!pItem)
			{
				// Determine type from tags
				CFWorkshopItemType_t type = CFWorkshop_GetItemTypeFromTags(details.m_rgchTags);
				pItem = AddItem(details.m_nPublishedFileId, type);
			}
			
			if (pItem)
			{
				// Update item with query details
				pItem->m_strTitle.Set(details.m_rgchTitle);
				pItem->m_strDescription.Set(details.m_rgchDescription);
				pItem->m_strTags.Set(details.m_rgchTags);
				pItem->m_nFileSize = details.m_nFileSize;
				pItem->m_rtimeUpdated = details.m_rtimeUpdated;
				pItem->m_rtimeCreated = details.m_rtimeCreated;
				
				// Get preview URL
				char szPreviewURL[256];
				if (pUGC->GetQueryUGCPreviewURL(pResult->m_handle, i, szPreviewURL, sizeof(szPreviewURL)))
				{
					pItem->m_strPreviewURL.Set(szPreviewURL);
				}
				
				// Check subscription state
				uint32 state = pUGC->GetItemState(details.m_nPublishedFileId);
				pItem->m_bSubscribed = (state & k_EItemStateSubscribed) != 0;
				
				m_vecBrowseableItems.AddToTail(pItem);
			}
		}
	}

	// Release the query
	pUGC->ReleaseQueryUGCRequest(pResult->m_handle);
	m_hCurrentQuery = k_UGCQueryHandleInvalid;
}

void CCFWorkshopManager::Steam_OnCreateItem(CreateItemResult_t* pResult, bool bError)
{
	if (bError || pResult->m_eResult != k_EResultOK)
	{
		CFWorkshopWarning("Failed to create workshop item (result: %d)\n", pResult->m_eResult);
		m_bUploadInProgress = false;
		V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Error: Failed to create item (code %d)", pResult->m_eResult);
		return;
	}

	CFWorkshopMsg("Workshop item created successfully! ID: %llu\n", pResult->m_nPublishedFileId);
	
	if (pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement)
	{
		CFWorkshopMsg("User needs to accept Steam Workshop legal agreement\n");
	}

	// Store the new file ID and continue with upload
	m_nLastUploadedFileID = pResult->m_nPublishedFileId;
	
	// Now start the actual content upload
	StartItemUpdate(pResult->m_nPublishedFileId);
}

void CCFWorkshopManager::Steam_OnSubmitUpdate(SubmitItemUpdateResult_t* pResult, bool bError)
{
	m_hCurrentUpdate = k_UGCUpdateHandleInvalid;
	
	if (bError || pResult->m_eResult != k_EResultOK)
	{
		CFWorkshopWarning("Failed to update workshop item (result: %d)\n", pResult->m_eResult);
		m_bUploadInProgress = false;
		V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Error: Upload failed (code %d)", pResult->m_eResult);
		return;
	}

	m_bUploadInProgress = false;
	V_snprintf(m_szUploadStatus, sizeof(m_szUploadStatus), "Upload complete! Item ID: %llu", m_nLastUploadedFileID);
	
	CFWorkshopMsg("Workshop item uploaded successfully! ID: %llu\n", m_nLastUploadedFileID);
	
	if (pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement)
	{
		CFWorkshopMsg("User needs to accept Steam Workshop legal agreement\n");
		// Open the legal agreement page
		if (steamapicontext && steamapicontext->SteamFriends())
		{
			steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage(
				"https://steamcommunity.com/workshop/workshoplegalagreement/?appid=3768450");
		}
	}
}

void CCFWorkshopManager::Steam_OnSubscribeItem(RemoteStorageSubscribePublishedFileResult_t* pResult, bool bError)
{
	if (bError || pResult->m_eResult != k_EResultOK)
	{
		CFWorkshopWarning("Failed to subscribe to item (result: %d)\n", pResult->m_eResult);
		return;
	}

	CFWorkshopMsg("Successfully subscribed to item %llu\n", pResult->m_nPublishedFileId);
	
	// Start downloading the item immediately
	ISteamUGC* pUGC = GetSteamUGC();
	if (pUGC)
	{
		// Check if item needs to be downloaded
		uint32 itemState = pUGC->GetItemState(pResult->m_nPublishedFileId);
		if ((itemState & k_EItemStateInstalled) == 0)
		{
			CFWorkshopMsg("Starting download for newly subscribed item %llu\n", pResult->m_nPublishedFileId);
			pUGC->DownloadItem(pResult->m_nPublishedFileId, true);  // High priority
		}
		else
		{
			// Already installed, mount it now
			MountWorkshopItem(pResult->m_nPublishedFileId);
		}
	}
	
	// Queue a refresh to pick up the new subscription
	m_bRefreshQueued = true;
}

void CCFWorkshopManager::Steam_OnUnsubscribeItem(RemoteStorageUnsubscribePublishedFileResult_t* pResult, bool bError)
{
	if (bError || pResult->m_eResult != k_EResultOK)
	{
		CFWorkshopWarning("Failed to unsubscribe from item (result: %d)\n", pResult->m_eResult);
		return;
	}

	CFWorkshopMsg("Successfully unsubscribed from item %llu\n", pResult->m_nPublishedFileId);
	
	// Track this as recently unsubscribed to prevent re-mounting during refresh
	m_vecRecentlyUnsubscribed.AddToTail(pResult->m_nPublishedFileId);
	
	// Unmount the item so maps are no longer available
	UnmountWorkshopItem(pResult->m_nPublishedFileId);
	
	// Remove from our tracked items
	RemoveItem(pResult->m_nPublishedFileId);
	
	// Remove from subscribed items list
	m_vecSubscribedItems.FindAndRemove(pResult->m_nPublishedFileId);
	
	// Don't queue a refresh - we've already handled the removal
}

// Helper functions
bool CFWorkshop_IsValidMapName(const char* pszMapName)
{
	if (!pszMapName || !pszMapName[0])
		return false;

	// Must be lowercase alphanumeric with underscores
	for (const char* p = pszMapName; *p; p++)
	{
		char c = *p;
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
			return false;
	}

	return true;
}

bool CFWorkshop_ParseWorkshopURL(const char* pszURL, PublishedFileId_t& fileIDOut)
{
	if (!pszURL)
		return false;

	// Look for ?id= or &id= in URL
	const char* pszID = V_strstr(pszURL, "?id=");
	if (!pszID)
		pszID = V_strstr(pszURL, "&id=");
	
	if (pszID)
	{
		pszID += 4; // Skip "?id=" or "&id="
		sscanf(pszID, "%llu", &fileIDOut);
		return (fileIDOut != 0);
	}

	return false;
}

CFWorkshopItemType_t CFWorkshop_GetItemTypeFromTags(const char* pszTags)
{
	if (!pszTags)
		return CF_WORKSHOP_TYPE_OTHER;

	// Parse comma-separated tags and determine type
	if (V_stristr(pszTags, "Weapon Mod"))
		return CF_WORKSHOP_TYPE_WEAPON_MOD;
	if (V_stristr(pszTags, CF_WORKSHOP_TAG_MOD_MAP))
		return CF_WORKSHOP_TYPE_MAP;
	if (V_stristr(pszTags, "weapon") || V_stristr(pszTags, "skin"))
		return CF_WORKSHOP_TYPE_WEAPON_SKIN;
	if (V_stristr(pszTags, "character") || V_stristr(pszTags, "model"))
		return CF_WORKSHOP_TYPE_CHARACTER_SKIN;
	if (V_stristr(pszTags, "particle") || V_stristr(pszTags, "effect"))
		return CF_WORKSHOP_TYPE_PARTICLE_EFFECT;
	if (V_stristr(pszTags, "sound") || V_stristr(pszTags, "audio"))
		return CF_WORKSHOP_TYPE_SOUND_MOD;
	if (V_stristr(pszTags, "hud") || V_stristr(pszTags, "ui"))
		return CF_WORKSHOP_TYPE_HUD;

	return CF_WORKSHOP_TYPE_OTHER;
}

//-----------------------------------------------------------------------------
// Console Commands
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL

CON_COMMAND(cf_workshop_refresh, "Refresh workshop subscriptions")
{
	CFWorkshop()->RefreshSubscriptions();
}

CON_COMMAND(cf_workshop_clear_cache, "Clear workshop browse cache and refresh")
{
	CFWorkshop()->ClearBrowseableItems();
	CFWorkshop()->QueryRecentItems(50);
	Msg("Workshop cache cleared, refreshing...\n");
}

CON_COMMAND(cf_workshop_status, "Print workshop status")
{
	CFWorkshop()->PrintStatus();
}

CON_COMMAND(cf_workshop_subscribe, "Subscribe to a workshop item")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_subscribe <fileID>\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID != 0)
	{
		CFWorkshop()->SubscribeItem(fileID);
	}
}

CON_COMMAND(cf_workshop_unsubscribe, "Unsubscribe from a workshop item")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_unsubscribe <fileID>\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID != 0)
	{
		CFWorkshop()->UnsubscribeItem(fileID);
	}
}

CON_COMMAND(cf_workshop_download, "Download a workshop item")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_download <fileID>\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID != 0)
	{
		CCFWorkshopItem* pItem = CFWorkshop()->GetItem(fileID);
		if (!pItem)
		{
			pItem = CFWorkshop()->AddItem(fileID, CF_WORKSHOP_TYPE_OTHER);
		}
		
		if (pItem)
		{
			pItem->Download(true);
		}
	}
}

CON_COMMAND(cf_workshop_search, "Search the workshop")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_search <search terms>\n");
		return;
	}

	CFWorkshop()->SearchWorkshop(args[1]);
}

CON_COMMAND(cf_workshop_maps, "List subscribed workshop maps and their paths")
{
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		Warning("[CF Workshop] Steam API not available.\n");
		return;
	}
	
	uint32 numSubscribed = pUGC->GetNumSubscribedItems();
	if (numSubscribed == 0)
	{
		Msg("[CF Workshop] No subscribed workshop items.\n");
		return;
	}
	
	CUtlVector<PublishedFileId_t> items;
	items.SetSize(numSubscribed);
	pUGC->GetSubscribedItems(items.Base(), numSubscribed);
	
	Msg("[CF Workshop] Subscribed workshop maps:\n");
	bool bFoundMaps = false;
	
	FOR_EACH_VEC(items, i)
	{
		PublishedFileId_t fileID = items[i];
		//uint32 itemState = pUGC->GetItemState(fileID);
		
		char szInstallPath[MAX_PATH];
		uint64 sizeOnDisk = 0;
		uint32 timestamp = 0;
		
		if (pUGC->GetItemInstallInfo(fileID, &sizeOnDisk, szInstallPath, sizeof(szInstallPath), &timestamp))
		{
			// Check if there's a maps folder
			char szMapPath[MAX_PATH];
			V_snprintf(szMapPath, sizeof(szMapPath), "%s/maps", szInstallPath);
			
			if (g_pFullFileSystem->IsDirectory(szMapPath))
			{
				bFoundMaps = true;
				Msg("  [%llu] Path: %s\n", fileID, szInstallPath);
				
				// List .bsp files in the maps folder
				FileFindHandle_t hFind;
				char szSearchPath[MAX_PATH];
				V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/maps/*.bsp", szInstallPath);
				
				const char* pszFileName = g_pFullFileSystem->FindFirst(szSearchPath, &hFind);
				while (pszFileName)
				{
					Msg("    - maps/%s\n", pszFileName);
					pszFileName = g_pFullFileSystem->FindNext(hFind);
				}
				g_pFullFileSystem->FindClose(hFind);
			}
			else
			{
				// Check if there's a .bsp directly in the install folder
				FileFindHandle_t hFind;
				char szSearchPath[MAX_PATH];
				V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.bsp", szInstallPath);
				
				const char* pszFileName = g_pFullFileSystem->FindFirst(szSearchPath, &hFind);
				if (pszFileName)
				{
					bFoundMaps = true;
					Msg("  [%llu] Path: %s\n", fileID, szInstallPath);
					while (pszFileName)
					{
						Msg("    - %s\n", pszFileName);
						pszFileName = g_pFullFileSystem->FindNext(hFind);
					}
				}
				g_pFullFileSystem->FindClose(hFind);
			}
		}
	}
	
	if (!bFoundMaps)
	{
		Msg("[CF Workshop] No workshop maps found in subscribed items.\n");
	}
}

CON_COMMAND(cf_workshop_upload, "Open the in-game Workshop upload dialog")
{
	// Open the upload dialog via the workshop browser
	engine->ClientCmd_Unrestricted("cf_workshop_open_upload");
}

CON_COMMAND(cf_workshop_browse, "Open Steam Workshop browse page in overlay")
{
	if (steamapicontext && steamapicontext->SteamFriends())
	{
		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage("https://steamcommunity.com/app/3768450/workshop/");
		Msg("[CF Workshop] Opening Steam Workshop in overlay...\n");
	}
	else
	{
		Warning("[CF Workshop] Steam API not available. Cannot open overlay.\n");
	}
}

CON_COMMAND(cf_workshop_myitems, "Open your Workshop items page")
{
	if (steamapicontext && steamapicontext->SteamFriends())
	{
		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage("https://steamcommunity.com/my/myworkshopfiles/?appid=3768450");
		Msg("[CF Workshop] Opening your Workshop items in overlay...\n");
	}
	else
	{
		Warning("[CF Workshop] Steam API not available. Cannot open overlay.\n");
	}
}

CON_COMMAND(cf_workshop_mount_weapon, "Mount a weapon mod by file ID")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_mount_weapon <fileID>\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID != 0)
	{
		if (CFWorkshop()->MountWeaponMod(fileID))
		{
			Msg("[CF Workshop] Weapon mod %llu mounted successfully.\n", fileID);
		}
		else
		{
			Warning("[CF Workshop] Failed to mount weapon mod %llu.\n", fileID);
		}
	}
}

CON_COMMAND(cf_workshop_unmount_weapon, "Unmount a weapon mod by file ID")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_unmount_weapon <fileID>\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID != 0)
	{
		CFWorkshop()->UnmountWeaponMod(fileID);
	}
}

CON_COMMAND(cf_workshop_mount_all_weapons, "Mount all subscribed weapon mods")
{
	CFWorkshop()->MountAllWeaponMods();
	Msg("[CF Workshop] Mounted all weapon mods. Count: %d\n", CFWorkshop()->GetMountedWeaponModCount());
}

CON_COMMAND(cf_workshop_weapon_status, "List mounted weapon mods")
{
	int count = CFWorkshop()->GetMountedWeaponModCount();
	if (count == 0)
	{
		Msg("[CF Workshop] No weapon mods currently mounted.\n");
		return;
	}
	
	Msg("[CF Workshop] Mounted weapon mods (%d):\n", count);
	for (int i = 0; i < count; i++)
	{
		PublishedFileId_t fileID = CFWorkshop()->GetMountedWeaponMod(i);
		CCFWorkshopItem* pItem = CFWorkshop()->GetItem(fileID);
		Msg("  %d. [%llu] %s\n", i + 1, fileID, pItem ? pItem->GetTitle() : "Unknown");
	}
}

CON_COMMAND(cf_workshop_refresh_weapons, "Refresh all weapon mods (requires schema reload)")
{
	CFWorkshop()->RefreshWeaponMods();
}

CON_COMMAND(cf_workshop_mount_skin, "Mount a weapon skin by file ID")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_mount_skin <fileID>\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID != 0)
	{
		if (CFWorkshop()->MountWeaponSkin(fileID))
		{
			Msg("[CF Workshop] Weapon skin %llu mounted successfully.\n", fileID);
		}
		else
		{
			Warning("[CF Workshop] Failed to mount weapon skin %llu.\n", fileID);
		}
	}
}

CON_COMMAND(cf_workshop_unmount_skin, "Unmount a weapon skin by file ID")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: cf_workshop_unmount_skin <fileID>\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID != 0)
	{
		CFWorkshop()->UnmountWeaponSkin(fileID);
	}
}

CON_COMMAND(cf_workshop_mount_all_skins, "Mount all subscribed weapon skins")
{
	CFWorkshop()->MountAllWeaponSkins();
	Msg("[CF Workshop] Mounted all weapon skins. Count: %d\n", CFWorkshop()->GetMountedWeaponSkinCount());
}

CON_COMMAND(cf_workshop_skin_status, "List mounted weapon skins")
{
	int count = CFWorkshop()->GetMountedWeaponSkinCount();
	if (count == 0)
	{
		Msg("[CF Workshop] No weapon skins currently mounted.\n");
		return;
	}
	
	Msg("[CF Workshop] Mounted weapon skins (%d):\n", count);
	for (int i = 0; i < count; i++)
	{
		PublishedFileId_t fileID = CFWorkshop()->GetMountedWeaponSkin(i);
		CCFWorkshopItem* pItem = CFWorkshop()->GetItem(fileID);
		Msg("  %d. [%llu] %s\n", i + 1, fileID, pItem ? pItem->GetTitle() : "Unknown");
	}
}

CON_COMMAND(cf_workshop_refresh_skins, "Refresh all weapon skins")
{
	CFWorkshop()->RefreshWeaponSkins();
}

CON_COMMAND(cf_workshop_config_reload, "Reload workshop configuration from workshop_config.txt")
{
	CFWorkshop()->ReloadConfig();
	Msg("[CF Workshop] Configuration reloaded.\n");
}

CON_COMMAND(cf_workshop_config_status, "Display current workshop configuration")
{
	if (!CFWorkshop()->IsConfigEnabled())
	{
		Msg("[CF Workshop] Configuration not loaded or workshop_config.txt not found.\n");
		Msg("[CF Workshop] Using default settings.\n");
		return;
	}
	
	const CFWorkshopConfig_t& config = CFWorkshop()->GetConfig();
	
	Msg("========================================\n");
	Msg(" Custom Fortress 2 Workshop Config\n");
	Msg("========================================\n");
	Msg("Collection ID: %llu\n", config.m_nCollectionID);
	Msg("Auto-download: %s\n", config.m_bAutoDownload ? "Enabled" : "Disabled");
	Msg("High Priority Downloads: %s\n", config.m_bHighPriority ? "Yes" : "No");
	Msg("Update Check Interval: %u seconds\n", config.m_nUpdateCheckInterval);
	Msg("Content Path: %s\n", config.m_szContentPath);
	Msg("Allow Workshop in Rotation: %s\n", config.m_bAllowWorkshopInRotation ? "Yes" : "No");
	Msg("\nConfigured Map IDs: %d\n", config.m_vecMapIDs.Count());
	FOR_EACH_VEC(config.m_vecMapIDs, i)
	{
		Msg("  %d. %llu\n", i + 1, config.m_vecMapIDs[i]);
	}
	Msg("\nConfigured Item IDs: %d\n", config.m_vecItemIDs.Count());
	FOR_EACH_VEC(config.m_vecItemIDs, i)
	{
		Msg("  %d. %llu\n", i + 1, config.m_vecItemIDs[i]);
	}
	Msg("\nRestrictions:\n");
	Msg("  Max File Size: %u MB\n", config.m_nMaxFileSizeMB);
	Msg("  Allow Skins: %s\n", config.m_bAllowSkins ? "Yes" : "No");
	Msg("  Allow Particles: %s\n", config.m_bAllowParticles ? "Yes" : "No");
	Msg("  Allow Sounds: %s\n", config.m_bAllowSounds ? "Yes" : "No");
	Msg("  Allow HUDs: %s\n", config.m_bAllowHUDs ? "Yes" : "No");
	Msg("\nsv_pure Override: ");
	if (config.m_nSvPureOverride < 0)
		Msg("None\n");
	else
		Msg("%d\n", config.m_nSvPureOverride);
	Msg("\nLogging:\n");
	Msg("  Enabled: %s\n", config.m_bLoggingEnabled ? "Yes" : "No");
	Msg("  Verbose: %s\n", config.m_bVerboseLogging ? "Yes" : "No");
	Msg("  Log Downloads: %s\n", config.m_bLogDownloads ? "Yes" : "No");
	Msg("  Log Updates: %s\n", config.m_bLogUpdates ? "Yes" : "No");
	Msg("========================================\n");
}

CON_COMMAND(cf_workshop_download_config_items, "Download all items specified in workshop_config.txt")
{
	if (!CFWorkshop()->IsConfigEnabled())
	{
		Warning("[CF Workshop] Configuration not loaded. Cannot download config items.\n");
		return;
	}
	
	const CFWorkshopConfig_t& config = CFWorkshop()->GetConfig();
	
	int downloadCount = 0;
	
	// Download configured maps
	FOR_EACH_VEC(config.m_vecMapIDs, i)
	{
		PublishedFileId_t mapID = config.m_vecMapIDs[i];
		if (mapID != 0)
		{
			CCFWorkshopItem* pItem = CFWorkshop()->GetItem(mapID);
			if (!pItem)
			{
				pItem = CFWorkshop()->AddItem(mapID, CF_WORKSHOP_TYPE_MAP);
			}
			
			if (pItem && !pItem->IsDownloaded())
			{
				Msg("[CF Workshop] Downloading map %llu...\n", mapID);
				pItem->Download(config.m_bHighPriority);
				downloadCount++;
			}
		}
	}
	
	// Download configured items
	FOR_EACH_VEC(config.m_vecItemIDs, i)
	{
		PublishedFileId_t itemID = config.m_vecItemIDs[i];
		if (itemID != 0)
		{
			CCFWorkshopItem* pItem = CFWorkshop()->GetItem(itemID);
			if (!pItem)
			{
				pItem = CFWorkshop()->AddItem(itemID, CF_WORKSHOP_TYPE_OTHER);
			}
			
			if (pItem && !pItem->IsDownloaded())
			{
				Msg("[CF Workshop] Downloading item %llu...\n", itemID);
				pItem->Download(config.m_bHighPriority);
				downloadCount++;
			}
		}
	}
	
	if (downloadCount == 0)
	{
		Msg("[CF Workshop] All configured items are already downloaded.\n");
	}
	else
	{
		Msg("[CF Workshop] Started downloading %d items.\n", downloadCount);
	}
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Workshop Map Support (IServerGameDLL hooks)
//-----------------------------------------------------------------------------

// Parse Workshop map ID from name (e.g., "workshop/1234567" or "workshop/cp_mymap.ugc1234567")
PublishedFileId_t CCFWorkshopManager::MapIDFromName(const char* pszMapName)
{
	if (!pszMapName || !pszMapName[0])
		return k_PublishedFileIdInvalid;

	// Convert to lowercase for consistent parsing
	CUtlString mapName = pszMapName;
	mapName.ToLower();

	const char szWorkshopPrefix[] = "workshop/";
	const size_t nPrefixLen = sizeof(szWorkshopPrefix) - 1;

	if (V_strnicmp(mapName.Get(), szWorkshopPrefix, nPrefixLen) != 0)
	{
		CFWorkshopDebug("Map '%s' does not appear to be a Workshop map -- no workshop/ prefix\n", pszMapName);
		return k_PublishedFileIdInvalid;
	}

	// Check canonical format: workshop/cp_anyname.ugc1234
	const char szUGCSuffix[] = ".ugc";
	const size_t nSuffixLen = sizeof(szUGCSuffix) - 1;

	CUtlString strID;
	const char* pszUGCSuffix = V_strstr(mapName.Get(), szUGCSuffix);
	if (pszUGCSuffix && V_strlen(pszUGCSuffix) >= nSuffixLen + 1)
	{
		// Found .ugc suffix, extract ID after it
		strID = pszUGCSuffix + nSuffixLen;

		// Validate that the base name is a legal workshop map name
		CUtlString baseMapName;
		baseMapName.SetDirect(mapName.Get() + nPrefixLen, (int)(pszUGCSuffix - (mapName.Get() + nPrefixLen)));
		if (!IsValidDisplayNameForMap(baseMapName.Get()))
		{
			CFWorkshopDebug("Map '%s' looks like a Workshop map, but '%s' is not a legal Workshop map name\n",
				pszMapName, baseMapName.Get());
			return k_PublishedFileIdInvalid;
		}
	}
	else
	{
		// Assume workshop/12345 shorthand
		strID = mapName.Get() + nPrefixLen;
	}

	// Verify it's all digits
	for (int i = 0; i < strID.Length(); i++)
	{
		if (strID[i] < '0' || strID[i] > '9')
			return k_PublishedFileIdInvalid;
	}

	// Parse the ID
	PublishedFileId_t nMapID = k_PublishedFileIdInvalid;
	sscanf(strID.Get(), "%llu", &nMapID);
	return nMapID;
}

// Generate canonical name for a Workshop map (e.g., "workshop/cp_mymap.ugc1234567")
bool CCFWorkshopManager::CanonicalNameForMap(PublishedFileId_t fileID, const char* pszOriginalFileName, char* pszCanonicalName, size_t nMaxLen)
{
	if (!IsValidOriginalFileNameForMap(pszOriginalFileName))
	{
		CFWorkshopWarning("Invalid Workshop map name %llu [ %s ]\n", fileID, pszOriginalFileName);
		return false;
	}

	// Extract base name without extension
	char szBase[MAX_PATH];
	V_FileBase(pszOriginalFileName, szBase, sizeof(szBase));

	// Format as workshop/basename.ugcID
	int len = V_snprintf(pszCanonicalName, nMaxLen, "workshop/%s.ugc%llu", szBase, fileID);
	if (len >= (int)nMaxLen || len >= MAX_PATH)
	{
		Assert(len < MAX_PATH);
		return false;
	}

	return true;
}

// Validate original filename for Workshop map
bool CCFWorkshopManager::IsValidOriginalFileNameForMap(const char* pszFileName)
{
	if (!pszFileName || !pszFileName[0])
		return false;

	// Must end with .bsp
	const char* pExt = V_GetFileExtension(pszFileName);
	if (!pExt || V_stricmp(pExt, "bsp") != 0)
		return false;

	// Get filename without extension
	char szBase[MAX_PATH];
	V_FileBase(pszFileName, szBase, sizeof(szBase));

	return IsValidDisplayNameForMap(szBase);
}

// Validate display name for Workshop map (base filename without extension)
bool CCFWorkshopManager::IsValidDisplayNameForMap(const char* pszMapName)
{
	if (!pszMapName || !pszMapName[0])
		return false;

	// Check length (reasonable limits)
	int len = V_strlen(pszMapName);
	if (len < 3 || len > 64)
		return false;

	// Must start with alphanumeric
	if (!V_isalnum(pszMapName[0]))
		return false;

	// Only allow alphanumeric, underscore, and hyphen
	for (int i = 0; i < len; i++)
	{
		char c = pszMapName[i];
		if (!V_isalnum(c) && c != '_' && c != '-')
			return false;
	}

	return true;
}

// Async preparation of Workshop map resources (called by engine before map load)
int CCFWorkshopManager::AsyncPrepareLevelResources(char* pszMapName, size_t nMapNameSize,
	char* pszMapFile, size_t nMapFileSize, float* flProgress)
{
	// Parse Workshop map ID from name
	PublishedFileId_t nMapID = MapIDFromName(pszMapName);

	// Not a Workshop map
	if (nMapID == k_PublishedFileIdInvalid)
	{
		if (flProgress)
			*flProgress = 1.f;
		m_nPreparingMap = k_PublishedFileIdInvalid;
		return 0; // ePrepareLevelResources_Prepared
	}

	bool bNewPrepare = (m_nPreparingMap != nMapID);
	m_nPreparingMap = nMapID;

	CFWorkshopDebug("AsyncPrepareLevelResources for [ %s ] (ID: %llu)\n", pszMapName, nMapID);

	// Find or create Workshop item tracking
	unsigned short nIndex = m_mapItems.Find(nMapID);
	CCFWorkshopItem* pItem = NULL;

	if (nIndex == m_mapItems.InvalidIndex())
	{
		CFWorkshopMsg("Map ID %llu isn't tracked, adding\n", nMapID);
		pItem = new CCFWorkshopItem(nMapID, CF_WORKSHOP_TYPE_MAP);
		m_mapItems.Insert(nMapID, pItem);
	}
	else
	{
		pItem = m_mapItems[nIndex];
	}

	if (bNewPrepare)
	{
		// Start fresh query for new prepare
		pItem->Refresh();
		pItem->m_bHighPriority = true;
	}

	// Check item state
	CFWorkshopItemState_t eState = pItem->GetState();

	if (eState == CF_WORKSHOP_STATE_REFRESHING || eState == CF_WORKSHOP_STATE_NONE)
	{
		if (flProgress)
			*flProgress = 0.f;
		return 1; // ePrepareLevelResources_InProgress
	}

	if (eState == CF_WORKSHOP_STATE_DOWNLOADING || eState == CF_WORKSHOP_STATE_DOWNLOAD_PENDING)
	{
		if (flProgress)
			*flProgress = pItem->GetDownloadProgress();
		return 1; // ePrepareLevelResources_InProgress
	}

	if (eState == CF_WORKSHOP_STATE_DOWNLOADED || eState == CF_WORKSHOP_STATE_INSTALLED)
	{
		// Get install info
		ISteamUGC* pUGC = GetSteamUGC();
		if (!pUGC)
		{
			CFWorkshopWarning("Failed to get Steam UGC interface\n");
			if (flProgress)
				*flProgress = 1.f;
			m_nPreparingMap = k_PublishedFileIdInvalid;
			return 0; // ePrepareLevelResources_Prepared (will fail)
		}

		uint64 nUGCSize = 0;
		uint32 nTimestamp = 0;
		char szFolder[MAX_PATH] = { 0 };
		if (!pUGC->GetItemInstallInfo(nMapID, &nUGCSize, szFolder, sizeof(szFolder), &nTimestamp))
		{
			CFWorkshopWarning("GetItemInstallInfo failed for Workshop map %llu\n", nMapID);
			if (flProgress)
				*flProgress = 1.f;
			m_nPreparingMap = k_PublishedFileIdInvalid;
			return 0; // ePrepareLevelResources_Prepared (will fail)
		}

		// Get the original filename from item metadata/tags
		const char* pszOriginalName = pItem->GetOriginalFilename();
		if (!pszOriginalName || !pszOriginalName[0])
		{
			CFWorkshopWarning("Workshop map %llu has no original filename metadata\n", nMapID);
			if (flProgress)
				*flProgress = 1.f;
			m_nPreparingMap = k_PublishedFileIdInvalid;
			return 0; // ePrepareLevelResources_Prepared (will fail)
		}

		// Build full path to BSP file
		char szFullPath[MAX_PATH];
		V_MakeAbsolutePath(szFullPath, sizeof(szFullPath), pszOriginalName, szFolder);

		// Generate canonical name
		char szCanonicalName[MAX_PATH];
		if (!CanonicalNameForMap(nMapID, pszOriginalName, szCanonicalName, sizeof(szCanonicalName)))
		{
			CFWorkshopWarning("Failed to generate canonical name for Workshop map %llu\n", nMapID);
			if (flProgress)
				*flProgress = 1.f;
			m_nPreparingMap = k_PublishedFileIdInvalid;
			return 0; // ePrepareLevelResources_Prepared (will fail)
		}

		// Return the canonical name and file path
		V_strncpy(pszMapName, szCanonicalName, nMapNameSize);
		V_strncpy(pszMapFile, szFullPath, nMapFileSize);

		CFWorkshopMsg("Successfully prepared Workshop map from file ID %llu: %s\n", nMapID, szCanonicalName);

		if (flProgress)
			*flProgress = 1.f;
		m_nPreparingMap = k_PublishedFileIdInvalid;
		return 0; // ePrepareLevelResources_Prepared
	}

	// Error state
	CFWorkshopWarning("Workshop map %llu is in error state\n", nMapID);
	if (flProgress)
		*flProgress = 1.f;
	m_nPreparingMap = k_PublishedFileIdInvalid;
	return 0; // ePrepareLevelResources_Prepared (will fail)
}

// Synchronous preparation wrapper (blocks until ready)
void CCFWorkshopManager::PrepareLevelResources(char* pszMapName, size_t nMapNameSize,
	char* pszMapFile, size_t nMapFileSize)
{
	PublishedFileId_t nWorkshopID = MapIDFromName(pszMapName);
	if (nWorkshopID == k_PublishedFileIdInvalid)
		return;

#ifdef GAME_DLL
	// If dedicated server, wait for Steam connection
	if (engine->IsDedicatedServer())
	{
		if (!steamgameserverapicontext || !steamgameserverapicontext->SteamGameServer())
		{
			CFWorkshopWarning("No Steam connection in PrepareLevelResources, Workshop map loads will fail\n");
			return;
		}

		// Wait for login to finish
		if (!steamgameserverapicontext->SteamGameServer()->BLoggedOn())
		{
			CFWorkshopMsg("Waiting for Steam connection...\n");
			while (!steamgameserverapicontext->SteamGameServer()->BLoggedOn())
			{
				ThreadSleep(10);
			}
		}
	}
#endif

	CFWorkshopMsg("Preparing Workshop map ID %llu...\n", nWorkshopID);

	// Loop until async prepare returns ready
	while (AsyncPrepareLevelResources(pszMapName, nMapNameSize, pszMapFile, nMapFileSize) == 1)
	{
		ThreadSleep(10);
#ifdef GAME_DLL
		if (engine->IsDedicatedServer())
		{
			SteamGameServer_RunCallbacks();
		}
		else
#endif
		{
			SteamAPI_RunCallbacks();
		}
	}
}

// Check if we can provide a Workshop map
int CCFWorkshopManager::OnCanProvideLevel(char* pMapName, int nMapNameMax)
{
	PublishedFileId_t nWorkshopID = MapIDFromName(pMapName);
	if (nWorkshopID == k_PublishedFileIdInvalid)
		return 2; // eCanProvideLevel_CannotProvide

	unsigned short nIndex = m_mapItems.Find(nWorkshopID);
	if (nIndex == m_mapItems.InvalidIndex())
	{
		// Looks like a Workshop map, but not currently available
		return 1; // eCanProvideLevel_Possibly
	}

	CCFWorkshopItem* pItem = m_mapItems[nIndex];
	const char* pszOriginalName = pItem->GetOriginalFilename();

	// Generate canonical name if known
	if (pszOriginalName && pszOriginalName[0])
	{
		char szCanonicalName[MAX_PATH];
		if (CanonicalNameForMap(nWorkshopID, pszOriginalName, szCanonicalName, sizeof(szCanonicalName)))
		{
			V_strncpy(pMapName, szCanonicalName, nMapNameMax);
		}
	}

	if (pItem->GetState() != CF_WORKSHOP_STATE_DOWNLOADED && pItem->GetState() != CF_WORKSHOP_STATE_INSTALLED)
	{
		return 1; // eCanProvideLevel_Possibly
	}

	if (!pszOriginalName || !pszOriginalName[0])
	{
		CFWorkshopWarning("Map is marked available but has no proper name configured [ %llu ]\n", nWorkshopID);
		return 1; // eCanProvideLevel_Possibly
	}

	return 0; // eCanProvideLevel_CanProvide
}

// Get Workshop map description for server browser
bool CCFWorkshopManager::GetWorkshopMapDesc(uint32 uIndex, void* pDesc)
{
	// This would be used for listing Workshop maps in server browser
	// For now, return false (not implemented yet)
	return false;
}

//-----------------------------------------------------------------------------
// Console command to host a Workshop map
//-----------------------------------------------------------------------------
CON_COMMAND(host_workshop_map, "Load a Workshop map by its file ID")
{
	if (args.ArgC() < 2)
	{
		Msg("Usage: host_workshop_map <fileID>\n");
		Msg("Example: host_workshop_map 1234567890\n");
		return;
	}

	PublishedFileId_t fileID = 0;
	sscanf(args[1], "%llu", &fileID);
	
	if (fileID == 0)
	{
		Warning("Invalid Workshop file ID\n");
		return;
	}

	// Find or create the Workshop item
	CCFWorkshopItem* pItem = CFWorkshop()->GetItem(fileID);
	if (!pItem)
	{
		Msg("Workshop item %llu not found, adding to tracking...\n", fileID);
		pItem = CFWorkshop()->AddItem(fileID, CF_WORKSHOP_TYPE_MAP);
	}

	// Make sure it's downloaded
	if (pItem->GetState() != CF_WORKSHOP_STATE_DOWNLOADED && pItem->GetState() != CF_WORKSHOP_STATE_INSTALLED)
	{
		Msg("Workshop map %llu is not downloaded yet. Current state: %d\n", fileID, pItem->GetState());
		Msg("Initiating download...\n");
		pItem->Download(true); // High priority
		return;
	}

	// Get install info
	ISteamUGC* pUGC = GetSteamUGC();
	if (!pUGC)
	{
		Warning("Failed to get Steam UGC interface\n");
		return;
	}

	uint64 nUGCSize = 0;
	uint32 nTimestamp = 0;
	char szInstallPath[MAX_PATH];
	if (!pUGC->GetItemInstallInfo(fileID, &nUGCSize, szInstallPath, sizeof(szInstallPath), &nTimestamp))
	{
		Warning("Failed to get install info for Workshop map %llu\n", fileID);
		return;
	}

	// Mount the Workshop item
	CFWorkshop()->MountWorkshopItem(fileID);

	// Get the original filename
	const char* pszOriginalName = pItem->GetOriginalFilename();
	if (!pszOriginalName || !pszOriginalName[0])
	{
		Warning("Workshop map %llu has no original filename metadata\n", fileID);
		// Try to find a BSP file in the install directory
		char szSearchPath[MAX_PATH];
		V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/*.bsp", szInstallPath);
		FileFindHandle_t hFind;
		const char* pszBSP = g_pFullFileSystem->FindFirstEx(szSearchPath, "GAME", &hFind);
		if (pszBSP)
		{
			pszOriginalName = pszBSP;
			g_pFullFileSystem->FindClose(hFind);
		}
		else
		{
			// Try maps subfolder
			V_snprintf(szSearchPath, sizeof(szSearchPath), "%s/maps/*.bsp", szInstallPath);
			pszBSP = g_pFullFileSystem->FindFirstEx(szSearchPath, "GAME", &hFind);
			if (pszBSP)
			{
				pszOriginalName = pszBSP;
				g_pFullFileSystem->FindClose(hFind);
			}
			else
			{
				Warning("Could not find BSP file in Workshop item %llu\n", fileID);
				return;
			}
		}
	}

	// Generate canonical name
	char szCanonicalName[MAX_PATH];
	if (!CFWorkshop()->CanonicalNameForMap(fileID, pszOriginalName, szCanonicalName, sizeof(szCanonicalName)))
	{
		Warning("Failed to generate canonical name for Workshop map %llu\n", fileID);
		return;
	}

	Msg("Loading Workshop map: %s (ID: %llu)\n", szCanonicalName, fileID);
	
	// Execute the map command with the canonical name
#ifdef CLIENT_DLL
	engine->ClientCmd_Unrestricted(CFmtStr("map %s", szCanonicalName));
#else
	engine->ServerCommand(CFmtStr("changelevel %s\n", szCanonicalName));
#endif
}


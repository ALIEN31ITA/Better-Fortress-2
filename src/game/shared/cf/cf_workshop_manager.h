//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Custom Fortress 2 Workshop Manager
// Handles Steam Workshop integration for maps and item reskins
//
//=============================================================================

#ifndef CF_WORKSHOP_MANAGER_H
#define CF_WORKSHOP_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "steam/steam_api.h"
#include "utlvector.h"
#include "utlmap.h"
#include "utlstring.h"

// Enable verbose debug spew to DevMsg
// #define CF_WORKSHOP_DEBUG

//-----------------------------------------------------------------------------
// Workshop Configuration Structure
//-----------------------------------------------------------------------------
struct CFWorkshopConfig_t
{
	// Collection ID for bulk downloads
	PublishedFileId_t m_nCollectionID;
	
	// Auto-download settings
	bool m_bAutoDownload;
	bool m_bAllowWorkshopInRotation;
	
	// Specific map IDs to keep synced
	CUtlVector<PublishedFileId_t> m_vecMapIDs;
	
	// Specific item IDs to download
	CUtlVector<PublishedFileId_t> m_vecItemIDs;
	
	// Priority settings
	bool m_bHighPriority;
	
	// Update checking (in seconds, 0 = only on server start)
	uint32 m_nUpdateCheckInterval;
	
	// Content path (relative to game directory)
	char m_szContentPath[MAX_PATH];
	
	// Restrictions
	uint32 m_nMaxFileSizeMB;
	bool m_bAllowSkins;
	bool m_bAllowParticles;
	bool m_bAllowSounds;
	bool m_bAllowHUDs;
	
	// sv_pure override
	int m_nSvPureOverride;
	
	// Logging
	bool m_bLoggingEnabled;
	bool m_bVerboseLogging;
	bool m_bLogDownloads;
	bool m_bLogUpdates;
	
	CFWorkshopConfig_t()
	{
		Reset();
	}
	
	void Reset()
	{
		m_nCollectionID = 0;
		m_bAutoDownload = true;
		m_bAllowWorkshopInRotation = true;
		m_vecMapIDs.RemoveAll();
		m_vecItemIDs.RemoveAll();
		m_bHighPriority = true;
		m_nUpdateCheckInterval = 3600;
		V_strcpy_safe(m_szContentPath, "workshop_content");
		m_nMaxFileSizeMB = 100;
		m_bAllowSkins = true;
		m_bAllowParticles = true;
		m_bAllowSounds = false;
		m_bAllowHUDs = false;
		m_nSvPureOverride = -1; // -1 means don't override
		m_bLoggingEnabled = true;
		m_bVerboseLogging = false;
		m_bLogDownloads = true;
		m_bLogUpdates = true;
	}
};

//-----------------------------------------------------------------------------
// Workshop Tag Categories and Tags
// These must match the Steam Workshop configuration
//-----------------------------------------------------------------------------

// RES Files
#define RES_BROWSER				"resource/UI/custom/workshop/WS_Browser.res"
#define RES_UPDATE				"resource/UI/custom/workshop/WS_Update.res"
#define RES_UPLOAD				"resource/UI/custom/workshop/WS_Upload.res"
#define RES_WEAPONTOOL			"resource/UI/custom/workshop/WS_WeaponTool.res"
#define RES_DELETECONFIRM		"resource/UI/custom/workshop/WS_DeleteConfirm.res"

// CLASS category tags
#define CF_WORKSHOP_TAG_SCOUT		"Scout"
#define CF_WORKSHOP_TAG_SOLDIER		"Soldier"
#define CF_WORKSHOP_TAG_PYRO		"Pyro"
#define CF_WORKSHOP_TAG_DEMOMAN		"Demoman"
#define CF_WORKSHOP_TAG_HEAVY		"Heavy"
#define CF_WORKSHOP_TAG_ENGINEER	"Engineer"
#define CF_WORKSHOP_TAG_MEDIC		"Medic"
#define CF_WORKSHOP_TAG_SNIPER		"Sniper"
#define CF_WORKSHOP_TAG_SPY			"Spy"

// ITEM SLOT category tags
#define CF_WORKSHOP_TAG_WEAPON_PRIMARY		"Weapon (Primary)"
#define CF_WORKSHOP_TAG_WEAPON_SECONDARY	"Weapon (Secondary)"
#define CF_WORKSHOP_TAG_WEAPON_MELEE		"Weapon (Melee)"
#define CF_WORKSHOP_TAG_WEAPON_PDA			"Weapon (PDA)"
#define CF_WORKSHOP_TAG_COSMETIC			"Cosmetic"
#define CF_WORKSHOP_TAG_ACTION				"Action"
#define CF_WORKSHOP_TAG_SKIN				"Skin"
#define CF_WORKSHOP_TAG_TAUNT				"Taunt"

// GAME MODE category tags
#define CF_WORKSHOP_TAG_CTF				"Capture The Flag"
#define CF_WORKSHOP_TAG_CP				"Control Point"
#define CF_WORKSHOP_TAG_PAYLOAD			"Payload"
#define CF_WORKSHOP_TAG_PAYLOAD_RACE	"Payload Race"
#define CF_WORKSHOP_TAG_ARENA			"Arena"
#define CF_WORKSHOP_TAG_KOTH			"King of the Hill"
#define CF_WORKSHOP_TAG_AD				"Attack / Defense"
#define CF_WORKSHOP_TAG_SD				"Special Delivery"
#define CF_WORKSHOP_TAG_RD				"Robot Destruction"
#define CF_WORKSHOP_TAG_MVM				"Mann Vs. Machine"
#define CF_WORKSHOP_TAG_MVM_VERSUS		"Mann Vs. Machine Versus"
#define CF_WORKSHOP_TAG_MANNPOWER		"Mannpower"
#define CF_WORKSHOP_TAG_MEDIEVAL		"Medieval"
#define CF_WORKSHOP_TAG_PASSTIME		"PASS Time"
#define CF_WORKSHOP_TAG_PD				"Player Destruction"
#define CF_WORKSHOP_TAG_CUSTOM			"Custom"

// MOD TYPE category tags
#define CF_WORKSHOP_TAG_MOD_SKIN		"Skin"
#define CF_WORKSHOP_TAG_MOD_WEAPON		"Weapon Mod"
#define CF_WORKSHOP_TAG_MOD_GUI			"GUIs"
#define CF_WORKSHOP_TAG_MOD_PARTICLES	"Particles"
#define CF_WORKSHOP_TAG_MOD_SOUNDS		"Sounds"
#define CF_WORKSHOP_TAG_MOD_SPRAYS		"Sprays"
#define CF_WORKSHOP_TAG_MOD_MAP			"Map"
#define CF_WORKSHOP_TAG_MOD_MISC		"Misc."

// OTHER category tags
#define CF_WORKSHOP_TAG_HALLOWEEN		"Halloween"
#define CF_WORKSHOP_TAG_CHRISTMAS		"Christmas"
#define CF_WORKSHOP_TAG_SUMMER			"Summer"
#define CF_WORKSHOP_TAG_UNUSUAL			"Unusual Effect"
#define CF_WORKSHOP_TAG_WARPAINT		"War Paint"
#define CF_WORKSHOP_TAG_KILLSTREAK		"Killstreak Effect"
#define CF_WORKSHOP_TAG_COMMUNITY_FIX	"Community Fix"

//-----------------------------------------------------------------------------
// Workshop tag indices for checkbox arrays and tag info lookup
// Use TAGIDX prefix to avoid conflict with the string macros above
//-----------------------------------------------------------------------------
enum CFWorkshopTagIndex_t
{
	// MOD TYPE category
	CF_WORKSHOP_TAGIDX_MAP = 0,
	CF_WORKSHOP_TAGIDX_SKIN,
	CF_WORKSHOP_TAGIDX_WEAPON,
	CF_WORKSHOP_TAGIDX_GUIS,
	CF_WORKSHOP_TAGIDX_PARTICLES,
	CF_WORKSHOP_TAGIDX_SOUNDS,
	CF_WORKSHOP_TAGIDX_SPRAYS,
	CF_WORKSHOP_TAGIDX_MISC,
	
	// GAME MODE category
	CF_WORKSHOP_TAGIDX_CTF,
	CF_WORKSHOP_TAGIDX_CP,
	CF_WORKSHOP_TAGIDX_PAYLOAD,
	CF_WORKSHOP_TAGIDX_PLR,
	CF_WORKSHOP_TAGIDX_ARENA,
	CF_WORKSHOP_TAGIDX_KOTH,
	CF_WORKSHOP_TAGIDX_AD,
	CF_WORKSHOP_TAGIDX_SD,
	CF_WORKSHOP_TAGIDX_RD,
	CF_WORKSHOP_TAGIDX_MVM,
	CF_WORKSHOP_TAGIDX_MVM_VERSUS,
	CF_WORKSHOP_TAGIDX_MANNPOWER,
	CF_WORKSHOP_TAGIDX_MEDIEVAL,
	CF_WORKSHOP_TAGIDX_PASS,
	CF_WORKSHOP_TAGIDX_PD,
	CF_WORKSHOP_TAGIDX_CUSTOM,
	
	// CLASS category
	CF_WORKSHOP_TAGIDX_SCOUT,
	CF_WORKSHOP_TAGIDX_SOLDIER,
	CF_WORKSHOP_TAGIDX_PYRO,
	CF_WORKSHOP_TAGIDX_DEMOMAN,
	CF_WORKSHOP_TAGIDX_HEAVY,
	CF_WORKSHOP_TAGIDX_ENGINEER,
	CF_WORKSHOP_TAGIDX_MEDIC,
	CF_WORKSHOP_TAGIDX_SNIPER,
	CF_WORKSHOP_TAGIDX_SPY,
	
	// ITEM SLOT category
	CF_WORKSHOP_TAGIDX_PRIMARY,
	CF_WORKSHOP_TAGIDX_SECONDARY,
	CF_WORKSHOP_TAGIDX_MELEE,
	CF_WORKSHOP_TAGIDX_PDA,
	CF_WORKSHOP_TAGIDX_COSMETIC,
	CF_WORKSHOP_TAGIDX_ACTION,
	CF_WORKSHOP_TAGIDX_TAUNT,
	CF_WORKSHOP_TAGIDX_SLOT_SKIN,
	
	// OTHER category
	CF_WORKSHOP_TAGIDX_HALLOWEEN,
	CF_WORKSHOP_TAGIDX_CHRISTMAS,
	CF_WORKSHOP_TAGIDX_SUMMER,
	CF_WORKSHOP_TAGIDX_UNUSUAL,
	CF_WORKSHOP_TAGIDX_WARPAINT,
	CF_WORKSHOP_TAGIDX_KILLSTREAK,
	CF_WORKSHOP_TAGIDX_COMMUNITY_FIX,

	CF_WORKSHOP_TAG_COUNT
};

// Workshop tag info structure
struct CFWorkshopTagInfo_t
{
	CFWorkshopTagIndex_t index;
	const char* pszTagName;
	const char* pszCategory;
};

// Global array of workshop tag info
extern const CFWorkshopTagInfo_t g_CFWorkshopTags[CF_WORKSHOP_TAG_COUNT];

#define CFWorkshopMsg(...) Msg("[CF Workshop] " __VA_ARGS__)
#define CFWorkshopWarning(...) Warning("[CF Workshop] " __VA_ARGS__)

#ifdef CF_WORKSHOP_DEBUG
#define CFWorkshopDebug(...) DevMsg("[CF Workshop Debug] " __VA_ARGS__)
#else
#define CFWorkshopDebug(...)
#endif

// Forward declarations
class CCFWorkshopItem;
class CCFWorkshopManager;

// Global accessor
CCFWorkshopManager *CFWorkshop();

// Workshop item types
enum CFWorkshopItemType_t
{
	CF_WORKSHOP_TYPE_MAP = 0,
	CF_WORKSHOP_TYPE_WEAPON_SKIN,
	CF_WORKSHOP_TYPE_WEAPON_MOD,      // Full weapon mod with models, materials, and weapon script
	CF_WORKSHOP_TYPE_CHARACTER_SKIN,
	CF_WORKSHOP_TYPE_PARTICLE_EFFECT,
	CF_WORKSHOP_TYPE_SOUND_MOD,
	CF_WORKSHOP_TYPE_HUD,
	CF_WORKSHOP_TYPE_OTHER,
};

// Workshop item state
enum CFWorkshopItemState_t
{
	CF_WORKSHOP_STATE_NONE = 0,
	CF_WORKSHOP_STATE_REFRESHING,
	CF_WORKSHOP_STATE_QUERYING,
	CF_WORKSHOP_STATE_DOWNLOAD_PENDING,
	CF_WORKSHOP_STATE_DOWNLOADING,
	CF_WORKSHOP_STATE_DOWNLOADED,
	CF_WORKSHOP_STATE_INSTALLING,
	CF_WORKSHOP_STATE_INSTALLED,
	CF_WORKSHOP_STATE_ERROR,
	CF_WORKSHOP_STATE_NEEDS_UPDATE,
};

// Represents a single workshop item
class CCFWorkshopItem
{
public:
	CCFWorkshopItem(PublishedFileId_t fileID, CFWorkshopItemType_t type);
	~CCFWorkshopItem();

	// Accessors
	PublishedFileId_t GetFileID() const { return m_nFileID; }
	CFWorkshopItemType_t GetType() const { return m_eType; }
	CFWorkshopItemState_t GetState() const { return m_eState; }
	const char* GetTitle() const { return m_strTitle.Get(); }
	const char* GetDescription() const { return m_strDescription.Get(); }
	const char* GetTags() const { return m_strTags.Get(); }
	const char* GetPreviewURL() const { return m_strPreviewURL.Get(); }
	const char* GetOriginalFilename() const { return m_strOriginalFilename.Get(); }
	uint64 GetFileSize() const { return m_nFileSize; }
	uint32 GetTimeUpdated() const { return m_rtimeUpdated; }
	uint64 GetOwnerSteamID() const { return m_ulSteamIDOwner; }
	float GetDownloadProgress() const;
	
	// Check if the current user is the owner of this item
	bool IsOwnedByCurrentUser() const;

	// State management
	void Refresh(bool bHighPriority = false);
	void Download(bool bHighPriority = false);
	void Install();
	void Uninstall();
	
	// Get local installation path
	bool GetInstallPath(char* pszPath, size_t pathSize) const;
	
	// Check if item is subscribed
	bool IsSubscribed() const;
	
	// Check if item is downloaded
	bool IsDownloaded() const;
	
	// Check if item is installed
	bool IsInstalled() const;

private:
	friend class CCFWorkshopManager;

	// Steam callbacks
	CCallResult<CCFWorkshopItem, SteamUGCQueryCompleted_t> m_callbackQueryDetails;
	void Steam_OnQueryDetails(SteamUGCQueryCompleted_t* pResult, bool bError);
	
	void OnDownloadComplete(DownloadItemResult_t* pResult);
	void OnItemInstalled(ItemInstalled_t* pResult);

	PublishedFileId_t m_nFileID;
	CFWorkshopItemType_t m_eType;
	CFWorkshopItemState_t m_eState;
	
	CUtlString m_strTitle;
	CUtlString m_strDescription;
	CUtlString m_strTags;
	CUtlString m_strPreviewURL;
	CUtlString m_strOriginalFilename;  // For maps: original BSP filename (stored in metadata)
	
	uint64 m_nFileSize;
	uint32 m_rtimeUpdated;
	uint32 m_rtimeCreated;
	uint64 m_ulSteamIDOwner;
	
	bool m_bSubscribed;
	bool m_bHighPriority;
};

// Main workshop manager
class CCFWorkshopManager : public CAutoGameSystemPerFrame
{
public:
	CCFWorkshopManager();
	virtual ~CCFWorkshopManager();

	// CAutoGameSystem overrides
	virtual bool Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	void Update(float frametime);
	virtual const char* Name() { return "CFWorkshopManager"; }

	// Workshop initialization
	void InitWorkshop();
	
	// Subscription management
	void RefreshSubscriptions();
	void MountWorkshopItem(PublishedFileId_t fileID);
	void UnmountWorkshopItem(PublishedFileId_t fileID);
	uint32 GetSubscribedItemCount() const { return m_vecSubscribedItems.Count(); }
	PublishedFileId_t GetSubscribedItem(uint32 index) const;
	
	// Enable/Disable items (mount/unmount with content reload)
	bool IsItemEnabled(PublishedFileId_t fileID) const;
	void SetItemEnabled(PublishedFileId_t fileID, bool bEnabled);
	void SaveDisabledItems();
	void LoadDisabledItems();
	
	// Item management
	CCFWorkshopItem* GetItem(PublishedFileId_t fileID);
	CCFWorkshopItem* AddItem(PublishedFileId_t fileID, CFWorkshopItemType_t type);
	void RemoveItem(PublishedFileId_t fileID);
	
	// Find items by type
	void GetItemsByType(CFWorkshopItemType_t type, CUtlVector<CCFWorkshopItem*>& items);
	
	// Map-specific functions
	bool IsWorkshopMap(const char* pszMapName);
	PublishedFileId_t GetMapIDFromName(const char* pszMapName);
	bool GetCanonicalMapName(PublishedFileId_t fileID, const char* pszOriginalName, char* pszOut, size_t outSize);
	void PrepareMap(PublishedFileId_t fileID);
	
	// Item skin functions
	bool HasInstalledSkin(const char* pszItemClass);
	const char* GetInstalledSkinPath(const char* pszItemClass);
	void ApplySkin(PublishedFileId_t fileID);
	void RemoveSkin(const char* pszItemClass);
	
	// Weapon mod functions
	void MountAllWeaponMods();
	bool MountWeaponMod(PublishedFileId_t fileID);
	void UnmountWeaponMod(PublishedFileId_t fileID);
	bool IsWeaponModMounted(PublishedFileId_t fileID) const;
	int GetMountedWeaponModCount() const { return m_vecMountedWeaponMods.Count(); }
	PublishedFileId_t GetMountedWeaponMod(int index) const;
	void RefreshWeaponMods();  // Reload all weapon mods
	
	// Weapon skin functions (file replacement like custom folder)
	void MountAllWeaponSkins();
	bool MountWeaponSkin(PublishedFileId_t fileID);
	void UnmountWeaponSkin(PublishedFileId_t fileID);
	bool IsWeaponSkinMounted(PublishedFileId_t fileID) const;
	int GetMountedWeaponSkinCount() const { return m_vecMountedWeaponSkins.Count(); }
	PublishedFileId_t GetMountedWeaponSkin(int index) const;
	void RefreshWeaponSkins();  // Remount all weapon skins
	
	// Query workshop for items
	void QueryPopularItems(CFWorkshopItemType_t type, uint32 page = 1);
	void QueryUserItems(uint32 page = 1);
	void QueryItemDetails(PublishedFileId_t fileID);
	void SearchWorkshop(const char* pszSearchText, CFWorkshopItemType_t type = CF_WORKSHOP_TYPE_OTHER);
	void QueryRecentItems(uint32 numItems = 35);
	void QueryCollection(PublishedFileId_t collectionID);
	
	// Get browseable items (from queries)
	uint32 GetBrowseableItemCount() const { return m_vecBrowseableItems.Count(); }
	CCFWorkshopItem* GetBrowseableItem(uint32 index) const;
	bool IsQueryInProgress() const { return m_hCurrentQuery != k_UGCQueryHandleInvalid; }
	void ClearBrowseableItems();  // Clear cached browse results
	
	// Uploading (for content creators)
	void CreateNewItem(CFWorkshopItemType_t type);
	void UpdateItem(PublishedFileId_t fileID, const char* pszContentPath);
	
	// Full upload with all parameters
	bool StartUpload(
		const char* pszTitle,
		const char* pszDescription,
		const char* pszContentPath,
		const char* pszPreviewImagePath,
		CFWorkshopItemType_t type,
		ERemoteStoragePublishedFileVisibility visibility,
		PublishedFileId_t existingFileID = 0,  // 0 = create new
		const char* pszTags = NULL,  // Comma-separated tags
		const char* pszScreenshotsPath = NULL  // Path to folder containing additional screenshots
	);
	
	// Check upload status
	bool IsUploading() const { return m_bUploadInProgress; }
	float GetUploadProgress() const;
	const char* GetUploadStatus() const { return m_szUploadStatus; }
	PublishedFileId_t GetLastUploadedFileID() const { return m_nLastUploadedFileID; }
	
	// Subscribe/Unsubscribe
	void SubscribeItem(PublishedFileId_t fileID);
	void UnsubscribeItem(PublishedFileId_t fileID);
	
	// Delete item from workshop (owner only)
	void DeleteItem(PublishedFileId_t fileID);
	bool IsDeleteInProgress() const { return m_bDeleteInProgress; }
	
	// Update item metadata (owner only, no content/preview change)
	bool StartMetadataUpdate(
		PublishedFileId_t fileID,
		const char* pszTitle,
		const char* pszDescription,
		ERemoteStoragePublishedFileVisibility visibility,
		const char* pszTags = NULL
	);
	
	// Clear the upload/delete status message
	void ClearStatus() { m_szUploadStatus[0] = '\0'; }
	
	// Get our app ID
	AppId_t GetAppID() const { return m_nAppID; }
	
	// Console commands
	void PrintStatus();
	
	// Configuration
	void LoadConfig();
	void ReloadConfig();
	const CFWorkshopConfig_t& GetConfig() const { return m_config; }
	bool IsConfigEnabled() const { return m_bConfigLoaded; }
	
	// Content reload (deferred to avoid crashes during Steam callbacks)
	void SetContentReloadPending(bool bPending) { m_bContentReloadPending = bPending; }
	void SetHUDReloadPending(bool bPending) { m_bHUDReloadPending = bPending; }
	
#ifdef CLIENT_DLL
	// Handle workshop map ID from server
	void OnWorkshopMapIDReceived(PublishedFileId_t fileID);
	
	// Check if we need to download map before connecting (called from server browser)
	// Returns true if we have the map, false if download is needed
	bool CheckAndDownloadMapBeforeConnect(const char* pszServerIP, const char* pszMapName, PublishedFileId_t fileID);
	
	// Parse Workshop map ID from server rules/info
	PublishedFileId_t ParseWorkshopMapIDFromServerInfo(const char* pszValue);
#endif
	
	// Server-side hooks
	void OnMapLoad(const char* pszMapName);
	void GameServerSteamAPIActivated();
	
	// Workshop map preparation (IServerGameDLL hooks)
	void PrepareLevelResources(char* pszMapName, size_t nMapNameSize, char* pszMapFile, size_t nMapFileSize);
	int AsyncPrepareLevelResources(char* pszMapName, size_t nMapNameSize, char* pszMapFile, size_t nMapFileSize, float* flProgress = NULL);
	int OnCanProvideLevel(char* pMapName, int nMapNameMax);
	bool GetWorkshopMapDesc(uint32 uIndex, void* pDesc);
	
	// Workshop map helpers
	PublishedFileId_t MapIDFromName(const char* pszMapName);
	bool CanonicalNameForMap(PublishedFileId_t fileID, const char* pszOriginalFileName, char* pszCanonicalName, size_t nMaxLen);
	bool IsValidOriginalFileNameForMap(const char* pszFileName);
	bool IsValidDisplayNameForMap(const char* pszMapName);
	
#ifndef CLIENT_DLL
	// Broadcast workshop map ID to clients
	void BroadcastWorkshopMapID(PublishedFileId_t fileID);
	
	// Advertise workshop map ID in server info (for server browser)
	void AdvertiseWorkshopMapID(PublishedFileId_t fileID);
	
	// Broadcast list of subscribed addons to a specific client (for listen server auto-download)
	void BroadcastAddonList(edict_t* pClient);
#endif

private:
	// Steam callbacks - client
	CCallback<CCFWorkshopManager, DownloadItemResult_t, false> m_callbackDownloadItem;
	CCallback<CCFWorkshopManager, ItemInstalled_t, false> m_callbackItemInstalled;
	
	// Steam callbacks - server
	CCallback<CCFWorkshopManager, DownloadItemResult_t, true> m_callbackDownloadItem_Server;
	CCallback<CCFWorkshopManager, ItemInstalled_t, true> m_callbackItemInstalled_Server;
	
	void Steam_OnDownloadItem(DownloadItemResult_t* pResult);
	void Steam_OnItemInstalled(ItemInstalled_t* pResult);
	
	// Query callbacks
	CCallResult<CCFWorkshopManager, SteamUGCQueryCompleted_t> m_callbackQueryCompleted;
	void Steam_OnQueryCompleted(SteamUGCQueryCompleted_t* pResult, bool bError);
	
	// Create/Update callbacks
	CCallResult<CCFWorkshopManager, CreateItemResult_t> m_callbackCreateItem;
	void Steam_OnCreateItem(CreateItemResult_t* pResult, bool bError);
	
	CCallResult<CCFWorkshopManager, SubmitItemUpdateResult_t> m_callbackSubmitUpdate;
	void Steam_OnSubmitUpdate(SubmitItemUpdateResult_t* pResult, bool bError);
	
	// Helper for continuing upload after item creation
	void StartItemUpdate(PublishedFileId_t fileID);
	
	// Subscribe callbacks
	CCallResult<CCFWorkshopManager, RemoteStorageSubscribePublishedFileResult_t> m_callbackSubscribe;
	void Steam_OnSubscribeItem(RemoteStorageSubscribePublishedFileResult_t* pResult, bool bError);
	
	CCallResult<CCFWorkshopManager, RemoteStorageUnsubscribePublishedFileResult_t> m_callbackUnsubscribe;
	void Steam_OnUnsubscribeItem(RemoteStorageUnsubscribePublishedFileResult_t* pResult, bool bError);
	
	// Delete callback
	CCallResult<CCFWorkshopManager, DeleteItemResult_t> m_callbackDeleteItem;
	void Steam_OnDeleteItem(DeleteItemResult_t* pResult, bool bError);

	// Tracked items
	CUtlMap<PublishedFileId_t, CCFWorkshopItem*> m_mapItems;
	CUtlVector<PublishedFileId_t> m_vecSubscribedItems;
	CUtlVector<CCFWorkshopItem*> m_vecBrowseableItems;  // Items from recent query for browsing
	
	// Active skins
	CUtlMap<CUtlString, PublishedFileId_t> m_mapActiveSkins;
	
	// Mounted weapon mods
	CUtlVector<PublishedFileId_t> m_vecMountedWeaponMods;
	
	// Mounted weapon skins (file replacement)
	CUtlVector<PublishedFileId_t> m_vecMountedWeaponSkins;
	
	// Disabled items (persisted to config)
	CUtlVector<PublishedFileId_t> m_vecDisabledItems;
	
	// Recently unsubscribed items (to prevent re-mounting during refresh)
	CUtlVector<PublishedFileId_t> m_vecRecentlyUnsubscribed;
	
	// Map being prepared for loading
	PublishedFileId_t m_nPreparingMap;
	
	// App ID
	AppId_t m_nAppID;
	
	// Configuration
	CFWorkshopConfig_t m_config;
	bool m_bConfigLoaded;
	float m_flNextConfigUpdateCheck;
	
	// Initialization state
	bool m_bInitialized;
	bool m_bRefreshQueued;
	bool m_bContentReloadPending;  // Deferred content reload to avoid crash during callbacks
	bool m_bHUDReloadPending;      // Deferred HUD reload for custom HUDs
	float m_flNextContentReloadTime;  // Cooldown to prevent rapid reloads causing crashes
	
	// Current query handle
	UGCQueryHandle_t m_hCurrentQuery;
	
	// Upload state
	bool m_bUploadInProgress;
	bool m_bDeleteInProgress;
	UGCUpdateHandle_t m_hCurrentUpdate;
	PublishedFileId_t m_nLastUploadedFileID;
	PublishedFileId_t m_nPendingDeleteFileID;
	char m_szUploadStatus[256];
	CUtlString m_strPendingTitle;
	CUtlString m_strPendingDescription;
	CUtlString m_strPendingContentPath;
	CUtlString m_strPendingPreviewPath;
	CUtlString m_strPendingScreenshotsPath;  // Path to folder containing additional screenshots
	CUtlString m_strPendingTags;  // Comma-separated additional tags
	CFWorkshopItemType_t m_ePendingType;
	ERemoteStoragePublishedFileVisibility m_ePendingVisibility;
};

// Helper functions
bool CFWorkshop_IsValidMapName(const char* pszMapName);
bool CFWorkshop_ParseWorkshopURL(const char* pszURL, PublishedFileId_t& fileIDOut);
CFWorkshopItemType_t CFWorkshop_GetItemTypeFromTags(const char* pszTags);

#endif // CF_WORKSHOP_MANAGER_H

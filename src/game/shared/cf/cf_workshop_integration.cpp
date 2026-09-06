//====== Copyright Custom Fortress 2, All rights reserved. =================
//
// Integration layer between CF Workshop Manager and existing TF Workshop
//
//=============================================================================

#include "cbase.h"
#include "cf_workshop_manager.h"
#ifdef GAME_DLL
#include "tf/workshop/maps_workshop.h"
#endif

#ifdef CLIENT_DLL
#include "c_tf_player.h"
#else
#include "tf_player.h"
#include "tf_gamerules.h"
#endif

// Forward TF workshop map calls to CF workshop manager
void CF_IntegrateWithTFWorkshop()
{
	// This function can be called to ensure both systems work together
	// The CF workshop manager extends the TF workshop with more features
	
#ifdef GAME_DLL
	// On server, make sure TF workshop knows about CF workshop maps
	if (TFMapsWorkshop())
	{
		// The existing TF maps workshop will continue to handle map downloads
		// CF workshop manager adds support for items, skins, and other content
	}
#endif
}

// Helper to convert between TF and CF workshop systems
PublishedFileId_t CF_GetMapWorkshopID(const char* pszMapName)
{
	// Try CF workshop first
	if (CFWorkshop()->IsWorkshopMap(pszMapName))
	{
		return CFWorkshop()->GetMapIDFromName(pszMapName);
	}
	
#ifndef CLIENT_DLL
	// Fall back to TF workshop
	if (TFMapsWorkshop())
	{
		return TFMapsWorkshop()->MapIDFromName(pszMapName);
	}
#endif
	
	return k_PublishedFileIdInvalid;
}

// Ensure workshop map is prepared
void CF_PrepareWorkshopMap(const char* pszMapName)
{
	PublishedFileId_t fileID = CF_GetMapWorkshopID(pszMapName);
	if (fileID != k_PublishedFileIdInvalid)
	{
		// Use CF workshop if available
		if (CFWorkshop())
		{
			CFWorkshop()->PrepareMap(fileID);
		}
	}
}

#ifndef CLIENT_DLL
// Server-side integration with game rules
void CF_WorkshopOnMapLoad()
{
	const char* pszMapName = STRING(gpGlobals->mapname);
	if (pszMapName && CFWorkshop())
	{
		CFWorkshop()->OnMapLoad(pszMapName);
	}
}

// Called when Steam API becomes available on dedicated server
void CF_WorkshopServerActivated()
{
	if (CFWorkshop())
	{
		CFWorkshop()->GameServerSteamAPIActivated();
	}
}
#endif

// Console command to show combined workshop status
#ifdef CLIENT_DLL
CON_COMMAND(cf_workshop_fullstatus, "Show complete workshop status (maps + items)")
{
	Msg("=== Custom Fortress 2 Complete Workshop Status ===\n\n");
	
	// CF Workshop items
	if (CFWorkshop())
	{
		CFWorkshop()->PrintStatus();
	}
	
	Msg("\n");
}
#endif

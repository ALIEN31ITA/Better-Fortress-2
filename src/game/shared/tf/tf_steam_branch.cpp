//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Steam branch detection utilities
//
//=============================================================================

#include "cbase.h"
#include "tf_steam_branch.h"

#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif

#ifdef GAME_DLL
ConVar sv_steam_branch( "sv_steam_branch", "default", FCVAR_HIDDEN | FCVAR_PROTECTED | FCVAR_DONTRECORD, "Steam branch this server is running on (automatically set by the game)" );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Get the current Steam branch name
//-----------------------------------------------------------------------------
const char *GetSteamBranchName()
{
#ifndef NO_STEAM
	static char s_szBranchName[128] = { 0 };
	static bool s_bInitialized = false;

	if ( !s_bInitialized )
	{
		s_bInitialized = true;
		
#ifdef CLIENT_DLL
		// On client, detect the branch via Steam API
		ISteamApps *pSteamApps = steamapicontext ? steamapicontext->SteamApps() : NULL;
		if ( pSteamApps )
		{
			// GetCurrentBetaName returns the beta branch name
			// Returns false and empty string if on the default "public" branch
			if ( !pSteamApps->GetCurrentBetaName( s_szBranchName, sizeof( s_szBranchName ) ) ||
			     s_szBranchName[0] == '\0' )
			{
				// Not on a beta branch, use "public" as the default
				V_strncpy( s_szBranchName, "public", sizeof( s_szBranchName ) );
			}
		}
		else
		{
			// Steam not available, default to "public"
			V_strncpy( s_szBranchName, "public", sizeof( s_szBranchName ) );
		}
#else
		// On server, use the ConVar setting
		V_strncpy( s_szBranchName, sv_steam_branch.GetString(), sizeof( s_szBranchName ) );
#endif

		ConColorMsg(Color(220, 220, 0, 255), "Steam branch detected: %s\n", s_szBranchName );
	}

	return s_szBranchName;
#else
	return "public";
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Check if we're running on a beta branch
//-----------------------------------------------------------------------------
bool IsRunningOnBetaBranch()
{
	const char *pszBranch = GetSteamBranchName();
	return ( V_stricmp( pszBranch, "public" ) != 0 );
}

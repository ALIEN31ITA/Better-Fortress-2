//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sentry.io integration implementation
//
//=============================================================================//

#include "cbase.h"
#include "cf_sentry.h"

#if USE_SENTRY

#ifdef _WIN32
#pragma warning ( disable : 4005 )
#endif // _WIN32

#include "sentry.h"

#include "tier1/convar.h"
#include "steam/steam_api.h"
#include "cdll_client_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
ConVar cf_sentry_dsn( "cf_sentry_dsn", "https://db48123d5c5d429832bc0e06daa30c3e@o4510461085745152.ingest.de.sentry.io/4510461087907920", FCVAR_NONE,
	"Sentry.io DSN (Data Source Name). Leave empty to disable Sentry." );

ConVar cf_sentry_enabled( "cf_sentry_enabled", "1", FCVAR_ARCHIVE,
	"Enable Sentry.io crash reporting." );

ConVar cf_sentry_environment( "cf_sentry_environment", "production", FCVAR_NONE,
	"Sentry environment (production, development, staging)." );

ConVar cf_sentry_debug( "cf_sentry_debug", "0", FCVAR_NONE,
	"Enable Sentry debug logging." );

//-----------------------------------------------------------------------------
// Static members
//-----------------------------------------------------------------------------
static bool s_bSentryInitialized = false;

//-----------------------------------------------------------------------------
// Purpose: Initialize Sentry SDK
//-----------------------------------------------------------------------------
void SentryIntegration::Init( const char *pszDSN, const char *pszRelease, const char *pszEnvironment )
{
	if ( s_bSentryInitialized )
	{
		Warning( "[Sentry] Already initialized.\n" );
		return;
	}
	
	if ( !cf_sentry_enabled.GetBool() )
	{
		Msg( "[Sentry] Disabled via ConVar.\n" );
		return;
	}
	
	// Use ConVar DSN if provided, otherwise use parameter
	const char *pszActualDSN = cf_sentry_dsn.GetString();
	if ( !pszActualDSN || !pszActualDSN[0] )
		pszActualDSN = pszDSN;
	
	if ( !pszActualDSN || !pszActualDSN[0] )
	{
		Warning( "[Sentry] No DSN provided. Crash reporting disabled.\n" );
		return;
	}
	
	// Configure Sentry options
	sentry_options_t *options = sentry_options_new();
	sentry_options_set_dsn( options, pszActualDSN );
	
	// Set release version
	if ( pszRelease && pszRelease[0] )
	{
		sentry_options_set_release( options, pszRelease );
	}
	else
	{
		// Use engine version
		char szRelease[128];
		V_snprintf( szRelease, sizeof( szRelease ), "customfortress2@%s", engine->GetProductVersionString() );
		sentry_options_set_release( options, szRelease );
	}
	
	// Set environment
	const char *pszActualEnv = cf_sentry_environment.GetString();
	if ( !pszActualEnv || !pszActualEnv[0] )
		pszActualEnv = pszEnvironment;
	if ( pszActualEnv && pszActualEnv[0] )
		sentry_options_set_environment( options, pszActualEnv );
	
	// Set database path for offline caching
	char szDbPath[MAX_PATH];
	V_snprintf( szDbPath, sizeof( szDbPath ), "%s/.sentry-native", engine->GetGameDirectory() );
	sentry_options_set_database_path( options, szDbPath );
	
	// Enable debug if requested
	if ( cf_sentry_debug.GetBool() )
	{
		sentry_options_set_debug( options, 1 );
	}
	
	// Set handler path (for minidumps on Windows)
#if defined( _WIN32 )
	char szHandlerPath[MAX_PATH];
	V_snprintf( szHandlerPath, sizeof( szHandlerPath ), "%s/bin/x64/crashpad_handler.exe", engine->GetGameDirectory() );
	sentry_options_set_handler_path( options, szHandlerPath );
	Msg( "[Sentry] Handler path: %s\n", szHandlerPath );
#endif
	
	// Initialize Sentry
	int nResult = sentry_init( options );
	if ( nResult != 0 )
	{
		Warning( "[Sentry] Failed to initialize (error %d).\n", nResult );
		return;
	}
	
	s_bSentryInitialized = true;
	Msg( "[Sentry] Initialized successfully.\n" );
	Msg( "[Sentry] Release: %s\n", pszRelease ? pszRelease : "auto" );
	Msg( "[Sentry] Environment: %s\n", pszActualEnv ? pszActualEnv : "default" );
	
	// Set initial context
	SetTag( "game", "Custom Fortress 2" );
	SetTag( "engine", "Source Engine" );
	
	// Set user ID (hashed Steam ID)
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		uint64 id = steamID.ConvertToUint64();
		
		// Hash for privacy
		unsigned int hash = 5381;
		for ( int i = 0; i < 8; i++ )
		{
			unsigned char byte = ( id >> ( i * 8 ) ) & 0xFF;
			hash = ( ( hash << 5 ) + hash ) + byte;
		}
		
		char szUserID[32];
		V_snprintf( szUserID, sizeof( szUserID ), "user_%08x", hash );
		SetUser( szUserID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown Sentry SDK
//-----------------------------------------------------------------------------
void SentryIntegration::Shutdown()
{
	if ( !s_bSentryInitialized )
		return;
	
	Msg( "[Sentry] Shutting down...\n" );
	
	// Close Sentry (will flush any pending events)
	sentry_close();
	
	s_bSentryInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Set user context
//-----------------------------------------------------------------------------
void SentryIntegration::SetUser( const char *pszUserID, const char *pszUsername )
{
	if ( !s_bSentryInitialized )
		return;
	
	sentry_value_t user = sentry_value_new_object();
	
	if ( pszUserID && pszUserID[0] )
		sentry_value_set_by_key( user, "id", sentry_value_new_string( pszUserID ) );
	
	if ( pszUsername && pszUsername[0] )
		sentry_value_set_by_key( user, "username", sentry_value_new_string( pszUsername ) );
	
	sentry_set_user( user );
}

//-----------------------------------------------------------------------------
// Purpose: Add breadcrumb (for tracking user actions)
//-----------------------------------------------------------------------------
void SentryIntegration::AddBreadcrumb( const char *pszMessage, const char *pszCategory )
{
	if ( !s_bSentryInitialized || !pszMessage )
		return;
	
	sentry_value_t crumb = sentry_value_new_breadcrumb( "default", pszMessage );
	
	if ( pszCategory && pszCategory[0] )
		sentry_value_set_by_key( crumb, "category", sentry_value_new_string( pszCategory ) );
	
	sentry_add_breadcrumb( crumb );
}

//-----------------------------------------------------------------------------
// Purpose: Set custom tag
//-----------------------------------------------------------------------------
void SentryIntegration::SetTag( const char *pszKey, const char *pszValue )
{
	if ( !s_bSentryInitialized || !pszKey || !pszValue )
		return;
	
	sentry_set_tag( pszKey, pszValue );
}

//-----------------------------------------------------------------------------
// Purpose: Set extra context data
//-----------------------------------------------------------------------------
void SentryIntegration::SetContext( const char *pszKey, const char *pszValue )
{
	if ( !s_bSentryInitialized || !pszKey || !pszValue )
		return;
	
	sentry_set_extra( pszKey, sentry_value_new_string( pszValue ) );
}

//-----------------------------------------------------------------------------
// Purpose: Manually capture exception
//-----------------------------------------------------------------------------
void SentryIntegration::CaptureException( const char *pszMessage )
{
	if ( !s_bSentryInitialized || !pszMessage )
		return;
	
	sentry_value_t event = sentry_value_new_message_event(
		SENTRY_LEVEL_ERROR,
		"exception",
		pszMessage
	);
	
	sentry_capture_event( event );
}

//-----------------------------------------------------------------------------
// Purpose: Manually capture message
//-----------------------------------------------------------------------------
void SentryIntegration::CaptureMessage( const char *pszMessage, int nLevel )
{
	if ( !s_bSentryInitialized || !pszMessage )
		return;
	
	sentry_level_t level = SENTRY_LEVEL_INFO;
	switch ( nLevel )
	{
		case 0: level = SENTRY_LEVEL_DEBUG; break;
		case 1: level = SENTRY_LEVEL_INFO; break;
		case 2: level = SENTRY_LEVEL_WARNING; break;
		case 3: level = SENTRY_LEVEL_ERROR; break;
		case 4: level = SENTRY_LEVEL_FATAL; break;
	}
	
	sentry_value_t event = sentry_value_new_message_event( level, "message", pszMessage );
	sentry_capture_event( event );
}

//-----------------------------------------------------------------------------
// Purpose: Check if initialized
//-----------------------------------------------------------------------------
bool SentryIntegration::IsInitialized()
{
	return s_bSentryInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: Flush pending events
//-----------------------------------------------------------------------------
void SentryIntegration::Flush( int nTimeoutMs )
{
	if ( !s_bSentryInitialized )
		return;
	
	Msg( "[Sentry] Flushing pending events (timeout: %dms)...\n", nTimeoutMs );
	sentry_flush( nTimeoutMs );
	Msg( "[Sentry] Flush complete.\n" );
}

#endif // USE_SENTRY

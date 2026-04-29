//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Custom Fortress 2 Crash Handler Implementation
//
//=============================================================================//

#include "cbase.h"
#include "cf_crashhandler.h"
#include "tier0/minidump.h"
#include "tier0/dbg.h"
#include "tier1/convar.h"
#include "filesystem.h"
#include "cdll_client_int.h"
#include "engine/IEngineSound.h"
#include "steam/steam_api.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/ILocalize.h"
#include <time.h>

#if defined( _WIN32 )
#pragma warning ( disable : 4005 )
#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
ConVar cf_crashreporting_enabled( "cf_crashreporting_enabled", "1", FCVAR_ARCHIVE,
	"Enable crash reporting and minidump generation. Set to 0 to disable." );

ConVar cf_crashreporting_upload( "cf_crashreporting_upload", "1", FCVAR_ARCHIVE,
	"Automatically upload crash reports to developers (requires cf_crashreporting_enabled)." );

ConVar cf_crashreporting_fullminidump( "cf_crashreporting_fullminidump", "0", FCVAR_ARCHIVE,
	"Generate full minidumps with more memory information. Creates larger files." );

ConVar cf_crashreporting_server( "cf_crashreporting_server", "https://crashes.customfortress2.com/api/v1/crash/upload", FCVAR_NONE,
	"Server URL for crash report uploads." );

//-----------------------------------------------------------------------------
// Purpose: Test crash command
//-----------------------------------------------------------------------------
void CCrashHandler::TestCrash()
{
	Msg( "[CrashHandler] Triggering test crash...\n" );
	
#if USE_SENTRY
	// Add context to Sentry before crash
	if ( SentryIntegration::IsInitialized() )
	{
		SentryIntegration::AddBreadcrumb( "User executed 'crash' command", "test" );
		SentryIntegration::SetTag( "crash_type", "intentional_test" );
		SentryIntegration::SetContext( "test_command", "crash" );
		Msg( "[CrashHandler] Added Sentry context for test crash.\n" );
		
		// Capture a message before crashing so Sentry has something to send
		SentryIntegration::CaptureMessage( "Intentional crash test triggered via 'crash' command", 3 );
		Msg( "[CrashHandler] Sent test event to Sentry.\n" );
		
		// Flush to ensure it gets uploaded before we crash
		SentryIntegration::Flush( 2000 );
	}
#endif
	
	// Manually trigger crash handling BEFORE actually crashing
	if ( s_bEnabled && s_bInitialized )
	{
		Msg( "[CrashHandler] Collecting crash data...\n" );
		
		// Create crashes directory
		filesystem->CreateDirHierarchy( "crashes", "MOD" );
		
		// Collect metadata
		CollectMetadata();
		
		// Generate a crash report manually
		char szCrashPath[MAX_PATH];
		char szFullPath[MAX_PATH];
		V_snprintf( szCrashPath, sizeof(szCrashPath), "crashes/%s.txt", s_Metadata.szCrashID );
		filesystem->RelativePathToFullPath( szCrashPath, "MOD", szFullPath, sizeof(szFullPath) );
		
		Msg( "[CrashHandler] Writing test crash report to: %s\n", szFullPath );
		
		FileHandle_t hFile = filesystem->Open( szCrashPath, "w", "MOD" );
		if ( hFile )
		{
			filesystem->FPrintf( hFile, "=== CUSTOM FORTRESS 2 TEST CRASH REPORT ===\n\n" );
			filesystem->FPrintf( hFile, "Crash ID: %s\n", s_Metadata.szCrashID );
			filesystem->FPrintf( hFile, "Version: %s\n", s_Metadata.szVersion );
			filesystem->FPrintf( hFile, "Map: %s\n", s_Metadata.szMap );
			filesystem->FPrintf( hFile, "Timestamp: %s\n", s_Metadata.szTimestamp );
			filesystem->FPrintf( hFile, "Memory Usage: %d MB\n", s_Metadata.nMemoryUsageMB );
			filesystem->FPrintf( hFile, "Uptime: %d seconds\n", s_Metadata.nUptime );
			filesystem->FPrintf( hFile, "\nThis was a test crash triggered by the 'crash' command.\n" );
			filesystem->Close( hFile );
			
			Msg( "[CrashHandler] Test crash report written successfully!\n" );
			Msg( "[CrashHandler] Check: %s\n", szFullPath );
		}
		else
		{
			Warning( "[CrashHandler] Failed to write test crash report!\n" );
		}
	}
	
	// Now actually crash
	Msg( "[CrashHandler] Crashing in 1 second...\n" );
	
	// Dereference null pointer to cause access violation
	int *pNull = NULL;
	*pNull = 42;
}

void CC_CrashTest()
{
	CCrashHandler::TestCrash();
}

static ConCommand crash( "crash", CC_CrashTest, "Test crash handler by triggering an intentional crash.", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Static members
//-----------------------------------------------------------------------------
CrashMetadata_t CCrashHandler::s_Metadata;
bool CCrashHandler::s_bEnabled = false;
bool CCrashHandler::s_bInitialized = false;

//-----------------------------------------------------------------------------
// Purpose: Initialize crash handler
//-----------------------------------------------------------------------------
void CCrashHandler::Init( bool bUseSentry )
{
	if ( s_bInitialized )
	{
		Msg( "[CrashHandler] Already initialized.\n" );
		return;
	}
	
	Msg( "[CrashHandler] Initializing crash reporting system...\n" );
	
	s_bInitialized = true;
	s_bEnabled = cf_crashreporting_enabled.GetBool();
	
#if USE_SENTRY
	// Initialize Sentry for automated crash reporting
	if ( bUseSentry && s_bEnabled )
	{
		char szRelease[128];
		V_snprintf( szRelease, sizeof( szRelease ), "customfortress2@%s", engine->GetProductVersionString() );
		
		// Get DSN from environment or ConVar
		// For production, set this via: cf_sentry_dsn "https://your-public-key@sentry.io/your-project-id"
		SentryIntegration::Init( nullptr, szRelease, "production" );
	}
#endif
	
	if ( !s_bEnabled )
	{
		Msg( "[CrashHandler] Crash reporting is disabled.\n" );
		return;
	}
	
	// Initialize metadata
	V_memset( &s_Metadata, 0, sizeof( s_Metadata ) );
	
	// Set up exception handler (only if Sentry is not active)
#if defined( _WIN32 )
#if USE_SENTRY
	if ( !SentryIntegration::IsInitialized() )
#endif
	{
		SetUnhandledExceptionFilter( (LPTOP_LEVEL_EXCEPTION_FILTER)CustomExceptionFilter );
		Msg( "[CrashHandler] Exception filter installed.\n" );
	}
#if USE_SENTRY
	else
	{
		Msg( "[CrashHandler] Using Sentry crash handler.\n" );
	}
#endif
#endif
	
	// Collect initial system information
	CollectSystemInfo();
	
	// Set version
	V_snprintf( s_Metadata.szVersion, sizeof( s_Metadata.szVersion ), 
		"Custom Fortress 2 v%s", engine->GetProductVersionString() );
	
	Msg( "[CrashHandler] Initialized. Version: %s\n", s_Metadata.szVersion );
	Msg( "[CrashHandler] Upload enabled: %s\n", cf_crashreporting_upload.GetBool() ? "Yes" : "No" );
	Msg( "[CrashHandler] Type 'crash' to test crash handler.\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown crash handler
//-----------------------------------------------------------------------------
void CCrashHandler::Shutdown()
{
#if USE_SENTRY
	SentryIntegration::Shutdown();
#endif
	
	s_bInitialized = false;
	s_bEnabled = false;
}

//-----------------------------------------------------------------------------
// Purpose: Enable/disable crash reporting
//-----------------------------------------------------------------------------
void CCrashHandler::SetEnabled( bool bEnabled )
{
	s_bEnabled = bEnabled;
	cf_crashreporting_enabled.SetValue( bEnabled );
}

bool CCrashHandler::IsEnabled()
{
	return s_bEnabled && s_bInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: Set current map
//-----------------------------------------------------------------------------
void CCrashHandler::SetCurrentMap( const char *pszMapName )
{
	if ( pszMapName )
	{
		V_strncpy( s_Metadata.szMap, pszMapName, sizeof( s_Metadata.szMap ) );
		
#if USE_SENTRY
		SentryIntegration::SetTag( "map", pszMapName );
		
		char szMessage[256];
		V_snprintf( szMessage, sizeof( szMessage ), "Map changed to: %s", pszMapName );
		SentryIntegration::AddBreadcrumb( szMessage, "navigation" );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set game mode
//-----------------------------------------------------------------------------
void CCrashHandler::SetGameMode( const char *pszGameMode )
{
	if ( pszGameMode )
	{
		V_strncpy( s_Metadata.szGameMode, pszGameMode, sizeof( s_Metadata.szGameMode ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update metrics
//-----------------------------------------------------------------------------
void CCrashHandler::UpdateMetrics( int nPlayers, int nFPS )
{
	s_Metadata.nPlayerCount = nPlayers;
	s_Metadata.nFPS = nFPS;
}

//-----------------------------------------------------------------------------
// Purpose: Collect all metadata
//-----------------------------------------------------------------------------
void CCrashHandler::CollectMetadata()
{
	// Generate unique crash ID
	GenerateCrashID();
	
	// Get timestamp
	time_t rawtime;
	time( &rawtime );
	struct tm timeinfo;
#ifdef _WIN32
	gmtime_s( &timeinfo, &rawtime );
#else
	gmtime_r( &rawtime, &timeinfo );
#endif
	strftime( s_Metadata.szTimestamp, sizeof( s_Metadata.szTimestamp ), 
		"%Y-%m-%d %H:%M:%S UTC", &timeinfo );
	
	// Get Steam ID hash
	HashSteamID( s_Metadata.szSteamIDHash, sizeof( s_Metadata.szSteamIDHash ) );
	
	// Get memory usage
#if defined( _WIN32 )
	PROCESS_MEMORY_COUNTERS pmc;
	if ( GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) ) )
	{
		s_Metadata.nMemoryUsageMB = (int)( pmc.WorkingSetSize / ( 1024 * 1024 ) );
	}
#endif
	
	// Get uptime
	s_Metadata.nUptime = (int)Plat_FloatTime();
	
	// Get multiplayer status
	s_Metadata.bMultiplayer = engine->GetMaxClients() > 1;
	s_Metadata.bDedicated = false; // Client DLL is never a dedicated server
	
	// Collect console history
	CollectConsoleHistory();
}

//-----------------------------------------------------------------------------
// Purpose: Collect system information
//-----------------------------------------------------------------------------
void CCrashHandler::CollectSystemInfo()
{
#if defined( _WIN32 )
	// Get OS version
	OSVERSIONINFOEX osvi;
	ZeroMemory( &osvi, sizeof( OSVERSIONINFOEX ) );
	osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEX );
	
	if ( GetVersionEx( (OSVERSIONINFO*)&osvi ) )
	{
		V_snprintf( s_Metadata.szOS, sizeof( s_Metadata.szOS ),
			"Windows %d.%d Build %d", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber );
	}
	
	// Get CPU info
	SYSTEM_INFO siSysInfo;
	GetSystemInfo( &siSysInfo );
	V_snprintf( s_Metadata.szCPU, sizeof( s_Metadata.szCPU ),
		"%d cores", siSysInfo.dwNumberOfProcessors );
#else
	V_strncpy( s_Metadata.szOS, "Unknown", sizeof( s_Metadata.szOS ) );
	V_strncpy( s_Metadata.szCPU, "Unknown", sizeof( s_Metadata.szCPU ) );
#endif
	
	// Get GPU info (from material system)
	if ( materials )
	{
		MaterialAdapterInfo_t adapterInfo;
		materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), adapterInfo );
		V_strncpy( s_Metadata.szVideoCard, adapterInfo.m_pDriverName, sizeof( s_Metadata.szVideoCard ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Collect console history
//-----------------------------------------------------------------------------
void CCrashHandler::CollectConsoleHistory()
{
	// This would need engine support to get console buffer
	// For now, just mark it as available
	V_strncpy( s_Metadata.szLastConsoleLines, "[Console history not yet implemented]", 
		sizeof( s_Metadata.szLastConsoleLines ) );
}

//-----------------------------------------------------------------------------
// Purpose: Generate unique crash ID (UUID-like)
//-----------------------------------------------------------------------------
void CCrashHandler::GenerateCrashID()
{
	// Generate a simple unique ID based on timestamp and random number
	time_t rawtime;
	time( &rawtime );
	
	V_snprintf( s_Metadata.szCrashID, sizeof( s_Metadata.szCrashID ),
		"%08x-%04x-%04x", (unsigned int)rawtime, rand() % 0xFFFF, rand() % 0xFFFF );
}

//-----------------------------------------------------------------------------
// Purpose: Hash Steam ID for privacy
//-----------------------------------------------------------------------------
void CCrashHandler::HashSteamID( char *pszOutput, int nSize )
{
	if ( !steamapicontext || !steamapicontext->SteamUser() )
	{
		V_strncpy( pszOutput, "unknown", nSize );
		return;
	}
	
	CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
	uint64 id = steamID.ConvertToUint64();
	
	// Simple hash (in production, use SHA256 or similar)
	unsigned int hash = 5381;
	for ( int i = 0; i < 8; i++ )
	{
		unsigned char byte = ( id >> ( i * 8 ) ) & 0xFF;
		hash = ( ( hash << 5 ) + hash ) + byte;
	}
	
	V_snprintf( pszOutput, nSize, "user_%08x", hash );
}

//-----------------------------------------------------------------------------
// Purpose: Write metadata to JSON file
//-----------------------------------------------------------------------------
bool CCrashHandler::WriteCrashMetadata( const char *pszCrashID )
{
	char szMetadataPath[MAX_PATH];
	V_snprintf( szMetadataPath, sizeof( szMetadataPath ), 
		"crashes/%s_metadata.txt", pszCrashID );
	
	FileHandle_t hFile = filesystem->Open( szMetadataPath, "w", "MOD" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return false;
	
	// Write metadata in JSON-like format
	filesystem->FPrintf( hFile, "{\n" );
	filesystem->FPrintf( hFile, "  \"crash_id\": \"%s\",\n", s_Metadata.szCrashID );
	filesystem->FPrintf( hFile, "  \"version\": \"%s\",\n", s_Metadata.szVersion );
	filesystem->FPrintf( hFile, "  \"timestamp\": \"%s\",\n", s_Metadata.szTimestamp );
	filesystem->FPrintf( hFile, "  \"map\": \"%s\",\n", s_Metadata.szMap );
	filesystem->FPrintf( hFile, "  \"gamemode\": \"%s\",\n", s_Metadata.szGameMode );
	filesystem->FPrintf( hFile, "  \"user_hash\": \"%s\",\n", s_Metadata.szSteamIDHash );
	filesystem->FPrintf( hFile, "  \"memory_mb\": %d,\n", s_Metadata.nMemoryUsageMB );
	filesystem->FPrintf( hFile, "  \"uptime_sec\": %d,\n", s_Metadata.nUptime );
	filesystem->FPrintf( hFile, "  \"player_count\": %d,\n", s_Metadata.nPlayerCount );
	filesystem->FPrintf( hFile, "  \"fps\": %d,\n", s_Metadata.nFPS );
	filesystem->FPrintf( hFile, "  \"os\": \"%s\",\n", s_Metadata.szOS );
	filesystem->FPrintf( hFile, "  \"cpu\": \"%s\",\n", s_Metadata.szCPU );
	filesystem->FPrintf( hFile, "  \"gpu\": \"%s\",\n", s_Metadata.szVideoCard );
	filesystem->FPrintf( hFile, "  \"multiplayer\": %s,\n", s_Metadata.bMultiplayer ? "true" : "false" );
	filesystem->FPrintf( hFile, "  \"dedicated\": %s\n", s_Metadata.bDedicated ? "true" : "false" );
	filesystem->FPrintf( hFile, "}\n" );
	
	filesystem->Close( hFile );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Queue crash for upload
//-----------------------------------------------------------------------------
void CCrashHandler::QueueForUpload( const char *pszMinidumpPath, const char *pszMetadataPath )
{
	if ( !cf_crashreporting_upload.GetBool() )
		return;
	
	// Create upload queue file
	char szQueuePath[MAX_PATH];
	V_snprintf( szQueuePath, sizeof( szQueuePath ), "crashes/upload_queue.txt" );
	
	FileHandle_t hFile = filesystem->Open( szQueuePath, "a", "MOD" );
	if ( hFile != FILESYSTEM_INVALID_HANDLE )
	{
		filesystem->FPrintf( hFile, "%s|%s\n", pszMinidumpPath, pszMetadataPath );
		filesystem->Close( hFile );
		
		Msg( "[CrashHandler] Crash queued for upload: %s\n", s_Metadata.szCrashID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Custom exception filter
//-----------------------------------------------------------------------------
#ifdef _WIN32
long __stdcall CCrashHandler::CustomExceptionFilter( struct _EXCEPTION_POINTERS *pExceptionInfo )
{
	Msg( "[CrashHandler] Exception filter called!\n" );
	
	if ( !s_bEnabled || !s_bInitialized )
	{
		Msg( "[CrashHandler] Disabled or not initialized (enabled=%d, init=%d)\n", s_bEnabled, s_bInitialized );
		return EXCEPTION_CONTINUE_SEARCH;
	}
	
	Msg( "\n========================================\n" );
	Msg( "[CrashHandler] CRASH DETECTED!\n" );
	Msg( "========================================\n" );
	
	// Collect all metadata
	CollectMetadata();
	
	Msg( "[CrashHandler] Crash ID: %s\n", s_Metadata.szCrashID );
	Msg( "[CrashHandler] Map: %s\n", s_Metadata.szMap );
	Msg( "[CrashHandler] Memory: %d MB\n", s_Metadata.nMemoryUsageMB );
	
	// Create crashes directory
	filesystem->CreateDirHierarchy( "crashes", "MOD" );
	Msg( "[CrashHandler] Created crashes directory.\n" );
	
	// Write minidump
	char szMinidumpPath[MAX_PATH];
	V_snprintf( szMinidumpPath, sizeof( szMinidumpPath ), 
		"crashes/%s.dmp", s_Metadata.szCrashID );
	Msg( "[CrashHandler] Writing minidump to: %s\n", szMinidumpPath );
	
	int nDumpType = MiniDumpNormal;
	if ( cf_crashreporting_fullminidump.GetBool() )
	{
		nDumpType = MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory;
	}
	
	bool bWroteMinidump = WriteMiniDumpUsingExceptionInfo(
		pExceptionInfo->ExceptionRecord->ExceptionCode,
		pExceptionInfo,
		(MINIDUMP_TYPE)nDumpType,
		s_Metadata.szCrashID
	);
	
	if ( bWroteMinidump )
	{
		Msg( "[CrashHandler] Minidump written successfully.\n" );
		
		// Write metadata
		WriteCrashMetadata( s_Metadata.szCrashID );
		
		// Queue for upload
		char szMetadataPath[MAX_PATH];
		V_snprintf( szMetadataPath, sizeof( szMetadataPath ), 
			"crashes/%s_metadata.txt", s_Metadata.szCrashID );
		QueueForUpload( szMinidumpPath, szMetadataPath );
		
		Msg( "[CrashHandler] Crash report saved. Thank you for helping improve Custom Fortress 2!\n" );
	}
	else
	{
		Warning( "[CrashHandler] Failed to write minidump.\n" );
	}
	
	Msg( "========================================\n\n" );
	
	// Continue with default crash handling
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif // _WIN32

//-----------------------------------------------------------------------------
// Purpose: Get metadata
//-----------------------------------------------------------------------------
const CrashMetadata_t& CCrashHandler::GetMetadata()
{
	return s_Metadata;
}

//-----------------------------------------------------------------------------
// Purpose: Handle crash (called externally)
//-----------------------------------------------------------------------------
void CCrashHandler::HandleCrash( unsigned int uStructuredExceptionCode, void *pExceptionInfo )
{
#ifdef _WIN32
	CustomExceptionFilter( (struct _EXCEPTION_POINTERS *)pExceptionInfo );
#else
	// Linux crash handling - just collect metadata for now
	CollectMetadata();
#endif
}

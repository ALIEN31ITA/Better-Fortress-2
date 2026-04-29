//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Async crash report uploader implementation
//
//=============================================================================//

#include "cbase.h"
#include "cf_crashupload.h"
#include "cf_crashhandler.h"
#include "filesystem.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "tier1/strtools.h"

#if defined( _WIN32 )
#pragma warning ( disable : 4005 )
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
#define CRASH_QUEUE_FILE "crashes/upload_queue.txt"
#define MAX_UPLOAD_RETRIES 3
#define RETRY_DELAY 60.0f		// Retry after 60 seconds
#define PROCESS_INTERVAL 5.0f	// Process queue every 5 seconds
#define MAX_CRASH_AGE_DAYS 30	// Delete crashes older than 30 days

//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
ConVar cf_crashreporting_upload_timeout( "cf_crashreporting_upload_timeout", "30", FCVAR_NONE,
	"HTTP timeout for crash uploads (seconds)." );

ConVar cf_crashreporting_max_queue( "cf_crashreporting_max_queue", "10", FCVAR_NONE,
	"Maximum number of crashes to keep in upload queue." );

//-----------------------------------------------------------------------------
// Static members
//-----------------------------------------------------------------------------
CUtlVector<CrashUploadEntry_t> CCrashUploadManager::s_Queue;
bool CCrashUploadManager::s_bInitialized = false;
bool CCrashUploadManager::s_bProcessing = false;
float CCrashUploadManager::s_flNextProcessTime = 0.0f;
int CCrashUploadManager::s_nSuccessCount = 0;
int CCrashUploadManager::s_nFailedCount = 0;

//-----------------------------------------------------------------------------
// Purpose: Initialize upload manager
//-----------------------------------------------------------------------------
void CCrashUploadManager::Init()
{
	if ( s_bInitialized )
		return;
	
	s_bInitialized = true;
	s_Queue.Purge();
	s_nSuccessCount = 0;
	s_nFailedCount = 0;
	
	// Load existing queue
	LoadQueue();
	
	// Remove old crashes
	RemoveOldCrashes();
	
	Msg( "[CrashUpload] Initialized. %d crashes in queue.\n", s_Queue.Count() );
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown upload manager
//-----------------------------------------------------------------------------
void CCrashUploadManager::Shutdown()
{
	if ( !s_bInitialized )
		return;
	
	// Save queue state
	SaveQueue();
	
	s_Queue.Purge();
	s_bInitialized = false;
	
	Msg( "[CrashUpload] Shutdown. Uploaded: %d, Failed: %d\n", s_nSuccessCount, s_nFailedCount );
}

//-----------------------------------------------------------------------------
// Purpose: Think - process queue periodically
//-----------------------------------------------------------------------------
void CCrashUploadManager::Think()
{
	if ( !s_bInitialized || s_bProcessing )
		return;
	
	if ( !CCrashHandler::IsEnabled() || !cf_crashreporting_upload.GetBool() )
		return;
	
	float flTime = Plat_FloatTime();
	if ( flTime >= s_flNextProcessTime )
	{
		s_flNextProcessTime = flTime + PROCESS_INTERVAL;
		ProcessQueue();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Queue a crash for upload
//-----------------------------------------------------------------------------
void CCrashUploadManager::QueueCrash( const char *pszCrashID, const char *pszMinidumpPath, const char *pszMetadataPath )
{
	if ( !s_bInitialized )
		return;
	
	// Check if already in queue
	for ( int i = 0; i < s_Queue.Count(); i++ )
	{
		if ( s_Queue[i].strCrashID == pszCrashID )
			return; // Already queued
	}
	
	// Add to queue
	CrashUploadEntry_t entry;
	entry.strCrashID = pszCrashID;
	entry.strMinidumpPath = pszMinidumpPath;
	entry.strMetadataPath = pszMetadataPath;
	entry.status = CRASH_UPLOAD_PENDING;
	entry.nRetryCount = 0;
	entry.flNextRetryTime = 0.0f;
	
	s_Queue.AddToTail( entry );
	
	// Enforce max queue size
	int nMaxQueue = cf_crashreporting_max_queue.GetInt();
	while ( s_Queue.Count() > nMaxQueue )
	{
		// Remove oldest pending entry
		for ( int i = 0; i < s_Queue.Count(); i++ )
		{
			if ( s_Queue[i].status == CRASH_UPLOAD_PENDING || 
				 s_Queue[i].status == CRASH_UPLOAD_FAILED )
			{
				s_Queue.Remove( i );
				break;
			}
		}
	}
	
	SaveQueue();
	Msg( "[CrashUpload] Queued crash: %s\n", pszCrashID );
}

//-----------------------------------------------------------------------------
// Purpose: Load queue from disk
//-----------------------------------------------------------------------------
void CCrashUploadManager::LoadQueue()
{
	s_Queue.Purge();
	
	FileHandle_t hFile = filesystem->Open( CRASH_QUEUE_FILE, "r", "MOD" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;
	
	char szLine[512];
	while ( filesystem->ReadLine( szLine, sizeof( szLine ), hFile ) )
	{
		// Parse line: crashid|minidump|metadata|status|retries
		CUtlVector<char*> tokens;
		V_SplitString( szLine, "|", tokens );
		
		if ( tokens.Count() >= 3 )
		{
			CrashUploadEntry_t entry;
			entry.strCrashID = tokens[0];
			entry.strMinidumpPath = tokens[1];
			entry.strMetadataPath = tokens[2];
			entry.status = ( tokens.Count() >= 4 ) ? (CrashUploadStatus_t)atoi( tokens[3] ) : CRASH_UPLOAD_PENDING;
			entry.nRetryCount = ( tokens.Count() >= 5 ) ? atoi( tokens[4] ) : 0;
			entry.flNextRetryTime = 0.0f;
			
			s_Queue.AddToTail( entry );
		}
		
		tokens.PurgeAndDeleteElements();
	}
	
	filesystem->Close( hFile );
}

//-----------------------------------------------------------------------------
// Purpose: Save queue to disk
//-----------------------------------------------------------------------------
void CCrashUploadManager::SaveQueue()
{
	FileHandle_t hFile = filesystem->Open( CRASH_QUEUE_FILE, "w", "MOD" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;
	
	for ( int i = 0; i < s_Queue.Count(); i++ )
	{
		const CrashUploadEntry_t &entry = s_Queue[i];
		
		// Don't save completed entries
		if ( entry.status == CRASH_UPLOAD_SUCCESS )
			continue;
		
		filesystem->FPrintf( hFile, "%s|%s|%s|%d|%d\n",
			entry.strCrashID.Get(),
			entry.strMinidumpPath.Get(),
			entry.strMetadataPath.Get(),
			(int)entry.status,
			entry.nRetryCount );
	}
	
	filesystem->Close( hFile );
}

//-----------------------------------------------------------------------------
// Purpose: Process upload queue
//-----------------------------------------------------------------------------
void CCrashUploadManager::ProcessQueue()
{
	s_bProcessing = true;
	
	float flTime = Plat_FloatTime();
	bool bDidUpload = false;
	
	for ( int i = 0; i < s_Queue.Count(); i++ )
	{
		CrashUploadEntry_t &entry = s_Queue[i];
		
		// Skip if not ready
		if ( entry.status == CRASH_UPLOAD_SUCCESS || 
			 entry.status == CRASH_UPLOAD_CANCELLED ||
			 entry.status == CRASH_UPLOAD_IN_PROGRESS )
			continue;
		
		// Check retry timer
		if ( entry.status == CRASH_UPLOAD_FAILED && entry.flNextRetryTime > flTime )
			continue;
		
		// Check retry limit
		if ( entry.nRetryCount >= MAX_UPLOAD_RETRIES )
		{
			Warning( "[CrashUpload] Giving up on crash %s after %d retries.\n", 
				entry.strCrashID.Get(), entry.nRetryCount );
			s_nFailedCount++;
			continue;
		}
		
		// Try to upload
		Msg( "[CrashUpload] Uploading crash: %s (attempt %d/%d)\n", 
			entry.strCrashID.Get(), entry.nRetryCount + 1, MAX_UPLOAD_RETRIES );
		
		entry.status = CRASH_UPLOAD_IN_PROGRESS;
		
		if ( UploadCrash( entry ) )
		{
			entry.status = CRASH_UPLOAD_SUCCESS;
			s_nSuccessCount++;
			Msg( "[CrashUpload] Successfully uploaded: %s\n", entry.strCrashID.Get() );
		}
		else
		{
			entry.status = CRASH_UPLOAD_FAILED;
			entry.nRetryCount++;
			entry.flNextRetryTime = flTime + RETRY_DELAY * entry.nRetryCount;
			Warning( "[CrashUpload] Failed to upload: %s\n", entry.strCrashID.Get() );
		}
		
		bDidUpload = true;
		break; // Only upload one per cycle to avoid blocking
	}
	
	if ( bDidUpload )
	{
		SaveQueue();
	}
	
	s_bProcessing = false;
}

//-----------------------------------------------------------------------------
// Purpose: Upload a single crash
//-----------------------------------------------------------------------------
bool CCrashUploadManager::UploadCrash( CrashUploadEntry_t &entry )
{
	// Verify files exist
	if ( !filesystem->FileExists( entry.strMinidumpPath.Get(), "MOD" ) )
	{
		Warning( "[CrashUpload] Minidump file not found: %s\n", entry.strMinidumpPath.Get() );
		return false;
	}
	
	if ( !filesystem->FileExists( entry.strMetadataPath.Get(), "MOD" ) )
	{
		Warning( "[CrashUpload] Metadata file not found: %s\n", entry.strMetadataPath.Get() );
		return false;
	}
	
	// Get server URL
	const char *pszURL = cf_crashreporting_server.GetString();
	if ( !pszURL || !pszURL[0] )
	{
		Warning( "[CrashUpload] No server URL configured.\n" );
		return false;
	}
	
	// Send HTTP POST
	CUtlString strResponse;
	return SendHTTPPost( pszURL, entry.strMinidumpPath.Get(), entry.strMetadataPath.Get(), strResponse );
}

//-----------------------------------------------------------------------------
// Purpose: Send HTTP POST with files
//-----------------------------------------------------------------------------
bool CCrashUploadManager::SendHTTPPost( const char *pszURL, const char *pszMinidumpPath, 
	const char *pszMetadataPath, CUtlString &outResponse )
{
#if defined( _WIN32 )
	// Read files into memory
	CUtlBuffer minidumpData;
	CUtlBuffer metadataData;
	
	if ( !filesystem->ReadFile( pszMinidumpPath, "MOD", minidumpData ) )
		return false;
	
	if ( !filesystem->ReadFile( pszMetadataPath, "MOD", metadataData ) )
		return false;
	
	// Parse URL
	char szHostName[256];
	char szUrlPath[512];
	int nPort = 443; // HTTPS
	bool bSecure = true;
	
	if ( V_strnicmp( pszURL, "https://", 8 ) == 0 )
	{
		const char *pszHost = pszURL + 8;
		const char *pszPath = V_strstr( pszHost, "/" );
		
		if ( pszPath )
		{
			V_strncpy( szHostName, pszHost, MIN( sizeof( szHostName ), pszPath - pszHost + 1 ) );
			V_strncpy( szUrlPath, pszPath, sizeof( szUrlPath ) );
		}
		else
		{
			V_strncpy( szHostName, pszHost, sizeof( szHostName ) );
			V_strncpy( szUrlPath, "/", sizeof( szUrlPath ) );
		}
	}
	else if ( V_strnicmp( pszURL, "http://", 7 ) == 0 )
	{
		bSecure = false;
		nPort = 80;
		// Parse similarly...
		return false; // For now, require HTTPS
	}
	else
	{
		return false;
	}
	
	// Open Internet connection
	HINTERNET hInternet = InternetOpen( "CustomFortress2/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if ( !hInternet )
		return false;
	
	HINTERNET hConnect = InternetConnect( hInternet, szHostName, nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0 );
	if ( !hConnect )
	{
		InternetCloseHandle( hInternet );
		return false;
	}
	
	DWORD dwFlags = INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;
	if ( bSecure )
		dwFlags |= INTERNET_FLAG_SECURE;
	
	HINTERNET hRequest = HttpOpenRequest( hConnect, "POST", szUrlPath, NULL, NULL, NULL, dwFlags, 0 );
	if ( !hRequest )
	{
		InternetCloseHandle( hConnect );
		InternetCloseHandle( hInternet );
		return false;
	}
	
	// Build multipart form data (simplified - in production use proper multipart encoding)
	const char *pszBoundary = "----CustomFortress2CrashBoundary";
	char szHeaders[512];
	V_snprintf( szHeaders, sizeof( szHeaders ), 
		"Content-Type: multipart/form-data; boundary=%s\r\n", pszBoundary );
	
	// For now, just send metadata as JSON in body
	// In production, properly encode minidump + metadata as multipart/form-data
	bool bResult = HttpSendRequest( hRequest, szHeaders, static_cast<DWORD>(-1),
		(void*)metadataData.Base(), metadataData.TellPut() );
	
	if ( bResult )
	{
		DWORD dwStatusCode = 0;
		DWORD dwSize = sizeof( dwStatusCode );
		HttpQueryInfo( hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, 
			&dwStatusCode, &dwSize, NULL );
		
		bResult = ( dwStatusCode >= 200 && dwStatusCode < 300 );
	}
	
	InternetCloseHandle( hRequest );
	InternetCloseHandle( hConnect );
	InternetCloseHandle( hInternet );
	
	return bResult;
#else
	// Non-Windows platforms would use libcurl or similar
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Remove crashes older than MAX_CRASH_AGE_DAYS
//-----------------------------------------------------------------------------
void CCrashUploadManager::RemoveOldCrashes()
{
	// TODO: Implement file age checking and deletion
	// For now, just clean up successfully uploaded crashes
	
	for ( int i = s_Queue.Count() - 1; i >= 0; i-- )
	{
		if ( s_Queue[i].status == CRASH_UPLOAD_SUCCESS )
		{
			// Delete files
			filesystem->RemoveFile( s_Queue[i].strMinidumpPath.Get(), "MOD" );
			filesystem->RemoveFile( s_Queue[i].strMetadataPath.Get(), "MOD" );
			s_Queue.Remove( i );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get statistics
//-----------------------------------------------------------------------------
int CCrashUploadManager::GetPendingCount()
{
	int nCount = 0;
	for ( int i = 0; i < s_Queue.Count(); i++ )
	{
		if ( s_Queue[i].status == CRASH_UPLOAD_PENDING )
			nCount++;
	}
	return nCount;
}

int CCrashUploadManager::GetSuccessCount()
{
	return s_nSuccessCount;
}

int CCrashUploadManager::GetFailedCount()
{
	return s_nFailedCount;
}

//-----------------------------------------------------------------------------
// Purpose: Retry all failed uploads
//-----------------------------------------------------------------------------
void CCrashUploadManager::RetryFailed()
{
	for ( int i = 0; i < s_Queue.Count(); i++ )
	{
		if ( s_Queue[i].status == CRASH_UPLOAD_FAILED )
		{
			s_Queue[i].status = CRASH_UPLOAD_PENDING;
			s_Queue[i].nRetryCount = 0;
			s_Queue[i].flNextRetryTime = 0.0f;
		}
	}
	SaveQueue();
}

//-----------------------------------------------------------------------------
// Purpose: Clear entire queue
//-----------------------------------------------------------------------------
void CCrashUploadManager::ClearQueue()
{
	s_Queue.Purge();
	SaveQueue();
}

//-----------------------------------------------------------------------------
// Purpose: Cancel all uploads
//-----------------------------------------------------------------------------
void CCrashUploadManager::CancelAll()
{
	for ( int i = 0; i < s_Queue.Count(); i++ )
	{
		s_Queue[i].status = CRASH_UPLOAD_CANCELLED;
	}
	SaveQueue();
}

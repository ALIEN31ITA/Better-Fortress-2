// @PracticeMedicine: 
// they said i get drywall if i rewrite their Discord RPC implementaion.
// if the whole Better Fortress 2 team gets sued, you know why. ;)
#include "cbase.h"
#include "tf_discord_rpc.h"
#include "discord_register.h"
#include "tf_gamerules.h"
#include "c_tf_playerresource.h"
#include <ctime>
#include "inetchannelinfo.h"
#include "KeyValues.h"
#include "filesystem.h"

// memdbgon.h must be the last include in a cpp file!!!!
#include "tier0/memdbgon.h"

#define DISCORD_APP_ID "1378157979815120916"

//#define DISCORD_UPDATE_RATE 10.0f

// a lil logging
#define DISCORD_LOG_MSG(...) ConColorMsg( Color( 70, 130, 255, 255 ), "[ Rich Presence ] " __VA_ARGS__);

ConVar tf_discord_rpc("tf_discord_rpc", "1", FCVAR_ARCHIVE, "If enabled, you can brag about idk.. Oh and you might need to restart the game for the changes to apply :3");
ConVar tf_discord_rpc_verbose("tf_discord_rpc_verbose", "0", FCVAR_ARCHIVE, "Controls the max verbose level of TF Discord RPC.", true, 0, true, 5.00);
ConVar tf_discord_rpc_updaterate("tf_discord_rpc_updaterate", "5.00", FCVAR_ARCHIVE, "Controls the update rate of running callbacks to Discord.", true, 5.00f, true, 15.00f);

#define DISCORD_LOG_VERBOSE(level, ...) \
	if (tf_discord_rpc_verbose.GetInt() >= level) \
		DISCORD_LOG_MSG(__VA_ARGS__);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDiscordJoinRequestNotification::Accept()
{
	if (!m_szUserId)
	{
		MarkForDeletion();
		return;
	}

	Discord_Respond(m_szUserId, DISCORD_REPLY_YES);
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDiscordJoinRequestNotification::Decline()
{
	if (!m_szUserId)
	{
		MarkForDeletion();
		return;
	}

	Discord_Respond(m_szUserId, DISCORD_REPLY_NO);
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDiscordJoinRequestNotification::UpdateTick()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFDiscordRPC::CTFDiscordRPC()
	: CAutoGameSystemPerFrame("tf_discord_rpc")
	, m_pRpc()
	, m_flLastUpdateTime(0.0f)
{
	Q_memset(m_szMapName, 0, MAX_MAP_NAME);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFDiscordRPC::Init()
{
	if (!tf_discord_rpc.GetBool())
		return true;

	DiscordEventHandlers handlers{};
	handlers.ready = &CTFDiscordRPC::DiscordReady;
	handlers.disconnected = &CTFDiscordRPC::DiscordDisconnected;
	handlers.errored = &CTFDiscordRPC::DiscordError;
	handlers.joinGame = &CTFDiscordRPC::DiscordJoinGame;
	handlers.joinRequest = &CTFDiscordRPC::DiscordJoinRequest;
	handlers.spectateGame = &CTFDiscordRPC::DiscordSpectateGame;

	char appIdStr[128];
	Q_snprintf(appIdStr, sizeof(appIdStr), "%d", engine->GetAppID());

	char gameCommand[256];
	Q_snprintf(gameCommand, sizeof(gameCommand), "%s -game \"%s\" -novid -steam\n", CommandLine()->GetParm(0), engine->GetGameDirectory());
	Discord_Register(DISCORD_APP_ID, gameCommand);
	Discord_Initialize(DISCORD_APP_ID, &handlers, 0, appIdStr);
	DISCORD_LOG_MSG("Discord initialized!\n");
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::Shutdown()
{
	DISCORD_LOG_MSG("CTFDiscordRPC::Shutdown\n");
	Discord_ClearPresence();
	Discord_Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::SetMapImage()
{
	KeyValues* kv = new KeyValues("discord_rpc_assets");
	if (!kv->LoadFromFile(g_pFullFileSystem, "scripts/discord_rpc_assets.txt", "MOD"))
		return;

	const char* imageKey = NULL;
	const char* imageText = NULL;

	for (KeyValues* pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey())
	{
		// key name doesnt match current map name, continue iterating.
		if (Q_stricmp(pData->GetName(), m_szMapName)) 
		{
			DISCORD_LOG_VERBOSE(3, "Warning: Expected %s but got %s. Ignoring...\n", m_szMapName, pData->GetName());
			continue;
		}
		imageKey = pData->GetString("image");
		imageText = pData->GetString("image_text");
	}

	m_pRpc.largeImageKey = imageKey;
	m_pRpc.largeImageText = imageText;

	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::SetGameTypeImage(const char *gameType)
{
	KeyValues* kv = new KeyValues("discord_rpc_assets");
	if (!kv->LoadFromFile(g_pFullFileSystem, "scripts/discord_rpc_assets.txt", "MOD"))
		return;

	const char* imageKey = NULL;
	const char* imageText = NULL;

	for (KeyValues* pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey())
	{
		// key name doesnt match current short gamemode name, continue iterating.
		if (Q_stricmp(pData->GetName(), gameType)) 
		{
			DISCORD_LOG_VERBOSE(3, "Warning: Expected %s but got %s. Ignoring...\n", gameType, pData->GetName());
			continue;
		}
		imageKey = pData->GetString("image");
		imageText = pData->GetString("image_text");
	}

	m_pRpc.smallImageKey = imageKey;
	m_pRpc.smallImageText = imageText;

	kv->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::Update(float frametime)
{
	UpdateRPC();

	Discord_RunCallbacks();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFDiscordRPC::ShouldReallyUpdate()
{
	return gpGlobals->realtime >= m_flLastUpdateTime + /*DISCORD_UPDATE_RATE*/tf_discord_rpc_updaterate.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::UpdateRPC()
{
	// wait dumbass
	if (!ShouldReallyUpdate())
		return;

	DISCORD_LOG_VERBOSE(1, "CTFDiscordRPC::UpdateRPC\n");

	m_flLastUpdateTime = gpGlobals->realtime;

	Q_memset(&m_pRpc, 0, sizeof(m_pRpc));

	if (engine->IsDrawingLoadingImage() == true)
	{
		m_pRpc.state = "";
		m_pRpc.details = "Currently loading...";
	}
	else 
	{
		if (engine->IsConnected())
		{
			UpdateServerInfo();
		}
		else
		{
			m_pRpc.details = "";
			m_pRpc.state = "In main menu";
			m_pRpc.endTimestamp;
		}
	}

	Discord_UpdatePresence(&m_pRpc);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::UpdateServerInfo()
{
	if (!engine->IsConnected())
		return;

	time_t iSysTime;
	time(&iSysTime);
	struct tm* tStartTime = NULL;
	tStartTime = localtime(&iSysTime);
	tStartTime->tm_sec += 0 - gpGlobals->curtime;

	// update our network details
	INetChannelInfo* ni = engine->GetNetChannelInfo();

	char partyId[128];
	Q_snprintf(partyId, sizeof(partyId), "%s-party", ni->GetAddress());

	Q_memset(&m_pRpc, 0, sizeof(m_pRpc));

	m_pRpc.partyId = partyId;
	m_pRpc.joinSecret = ni->GetAddress();

	const char* pszGameType = NULL;
	const char* pszGameTypeShort = NULL;

	// update our actual server details lol
	if ( TFGameRules() )
	{
		switch ( TFGameRules()->GetGameType() )
		{
		case TF_GAMETYPE_CP:
			pszGameType = "Control Point";
			pszGameTypeShort = "cp";
			break;
		case TF_GAMETYPE_ARENA:
			pszGameType = "Arena";
			pszGameTypeShort = "arena";
			break;
		case TF_GAMETYPE_CTF:
			pszGameType = "Capture The Flag";
			pszGameTypeShort = "ctf";
			break;
		case TF_GAMETYPE_ESCORT:
			pszGameType = "Payload";
			pszGameTypeShort = "pl";
			break;
		case TF_GAMETYPE_MVM:
			pszGameType = "Mann vs Machine";
			pszGameTypeShort = "mvm";
			break;
		case TF_GAMETYPE_RD:
			pszGameType = "Robot Destruction";
			pszGameTypeShort = "rd";
			break;
		case TF_GAMETYPE_PD:
			pszGameType = "Player Destruction";
			pszGameTypeShort = "pd";
			break;
		case TF_GAMETYPE_PASSTIME:
			pszGameType = "Passtime";
			pszGameTypeShort = "pass";
			break;
		case TF_GAMETYPE_RAID:
			pszGameType = "Raid";
			pszGameTypeShort = "raid";
			break;
		default:
			pszGameType = "Unknown";
			pszGameTypeShort = "unknown";
			break;
		}

		if (TFGameRules()->IsInKothMode())
		{
			pszGameType = "King of the Hill";
			pszGameTypeShort = "koth";
		}
	}

	char szState[256];
	Q_snprintf(szState, sizeof(szState), "Map: %s", m_szMapName);
	m_pRpc.state = szState;
	SetMapImage();
	SetGameTypeImage(pszGameTypeShort);
	m_pRpc.details = pszGameType;

	// alright now update our player info
	if (g_TF_PR)
	{
		int maxPlayers = gpGlobals->maxClients;
		int curPlayers = 0;

		for (int i = 1; i < maxPlayers; i++)
		{
			if (g_TF_PR->IsConnected(i))
			{
				curPlayers++;
			}
		}

		m_pRpc.partyMax = maxPlayers;
		m_pRpc.partySize = curPlayers;
	}

	m_pRpc.startTimestamp = mktime(tStartTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::Reset()
{
	DISCORD_LOG_VERBOSE(1, "CTFDiscordRPC::Reset\n");
	Q_memset(&m_pRpc, 0, sizeof(m_pRpc));
	m_pRpc.details = "";
	m_pRpc.state = "In main menu";
	m_pRpc.endTimestamp;

	Discord_UpdatePresence(&m_pRpc);
	DISCORD_LOG_MSG("Rich presence resetted.\n");
}

#include "tier0/valve_minmax_on.h"
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::LevelInitPreEntity()
{
	DISCORD_LOG_VERBOSE(1, "CTFDiscordRPC::LevelInitPreEntity\n");
	m_flLastUpdateTime = max(0, gpGlobals->realtime - /*DISCORD_UPDATE_RATE*/tf_discord_rpc_updaterate.GetFloat());

	Q_strcpy(m_szMapName, MapName());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::LevelShutdownPreEntity()
{
	DISCORD_LOG_VERBOSE(1, "CTFDiscordRPC::LevelShutdownPreEntity\n");
	m_flLastUpdateTime = max(0, gpGlobals->realtime - /*DISCORD_UPDATE_RATE*/tf_discord_rpc_updaterate.GetFloat());
	Reset();
}
#include "tier0/valve_minmax_off.h"

// the bloody discord events

//-----------------------------------------------------------------------------
// Purpose: What? Who is this guy? *chokes*
//-----------------------------------------------------------------------------
void CTFDiscordRPC::DiscordReady(const DiscordUser* connectedBastard)
{
	DISCORD_LOG_MSG("Ready!\n");
	DISCORD_LOG_MSG("The user's user ID: %s\n", connectedBastard->userId);

	GetDiscordRPC()->Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::DiscordDisconnected(int code, const char* message)
{
	DISCORD_LOG_MSG("Disconnected from Discord, too bad! - %s\n", message);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::DiscordError(int code, const char* message)
{
	DISCORD_LOG_MSG("An error occured! Too bad! - %s\n", message);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::DiscordJoinGame(const char* secret)
{
	char connectCmd[512];
	Q_snprintf(connectCmd, sizeof(connectCmd), "connect %s\n", secret);

	engine->ClientCmd_Unrestricted(connectCmd);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::DiscordJoinRequest(const DiscordUser* theBastard)
{
	DISCORD_LOG_MSG("User %s requesting to join, showing notification to player...\n", theBastard->username);
	CDiscordJoinRequestNotification* popUp = new CDiscordJoinRequestNotification(theBastard->userId);
	char popUpMessage[512];
	Q_snprintf(popUpMessage, sizeof(popUpMessage), "Discord user \"%s\" is requesting to join.", theBastard->username);
	popUp->SetText(popUpMessage);
	popUp->SetLifetime(345.5f);
	NotificationQueue_Add(popUp);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFDiscordRPC::DiscordSpectateGame(const char* secret)
{
	// TODO -- get the SourceTV IP address. :/
}

// source engine has too much spaghetti code that i had to add this
// wtf valve? (the auto game system wont init CTFDiscordRPC with out defining this at least from my side)
static CTFDiscordRPC s_DscMgr;
CTFDiscordRPC* GetDiscordRPC() { return &s_DscMgr; }

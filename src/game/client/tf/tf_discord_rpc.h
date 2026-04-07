#ifndef TF_DISCORDRPC_H
#define TF_DISCORDRPC_H
#ifdef _WIN32
#pragma once
#endif

#include "econ_notifications.h"
#include "igamesystem.h"
#include "discord_rpc.h"

class CDiscordJoinRequestNotification : public CEconNotification
{
public:
	CDiscordJoinRequestNotification(const char* userid) : CEconNotification()
	{
		m_szUserId = userid;
		m_bHasTriggered = false;
	}

	~CDiscordJoinRequestNotification()
	{
		if (!m_bHasTriggered)
		{
			m_bHasTriggered = true;
		}
	}

	virtual void MarkForDeletion()
	{
		m_bHasTriggered = true;
		CEconNotification::MarkForDeletion();
	}

	virtual EType NotificationType() { return eType_AcceptDecline; }
	virtual bool BShowInGameElements() const { return true; }

	virtual void Accept();
	virtual void Trigger() { Accept(); }
	virtual void Decline();
	virtual void UpdateTick();

	static bool IsNotificationType(CEconNotification* pNotification) { return dynamic_cast<CDiscordJoinRequestNotification*>(pNotification) != NULL; }

private:
	bool m_bHasTriggered;
	const char* m_szUserId;
};

/**
 * @brief Discord RPC implementation.
 */
class CTFDiscordRPC : public CAutoGameSystemPerFrame
{
public:
	CTFDiscordRPC();

	virtual bool Init();
	virtual void Shutdown();

	virtual void Update(float frametime);

	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPreEntity();

protected:
	// stinky events
	static void DiscordReady(const DiscordUser* user);
	static void DiscordDisconnected(int code, const char* message);
	static void DiscordError(int code, const char* message);
	static void DiscordJoinGame(const char* secret);
	static void DiscordJoinRequest(const DiscordUser* theBastard);
	static void DiscordSpectateGame(const char* secret);

public:
	void Reset();
	void UpdateRPC();
	void UpdateServerInfo();

private:
	bool ShouldReallyUpdate();
	void SetMapImage();
	void SetGameTypeImage(const char *gameType);
	DiscordRichPresence m_Rpc;

	char m_szMapName[MAX_MAP_NAME];
	float m_flLastUpdateTime;
};

// Accessor.
CTFDiscordRPC *GetDiscordRPC();

#endif // !TF_DISCORDRPC_H

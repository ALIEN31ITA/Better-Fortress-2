//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "usermessages.h"
#include "shake.h"
#include "voice_gamemgr.h"

// NVNT include to register in haptic user messages
#include "haptics/haptic_msgs.h"

#ifdef CLIENT_DLL
#include "cf/cf_workshop_manager.h"

// Client-side handler for WorkshopMapID message
void MsgFunc_WorkshopMapID( bf_read &msg )
{
	// Read the 64-bit file ID (sent as two 32-bit values)
	uint32 lowBits = msg.ReadLong();
	uint32 highBits = msg.ReadLong();
	PublishedFileId_t fileID = ((uint64)highBits << 32) | lowBits;
	
	// Pass to workshop manager
	if (CFWorkshop())
	{
		CFWorkshop()->OnWorkshopMapIDReceived(fileID);
	}
}

// Client-side handler for AddonList message
void MsgFunc_AddonList( bf_read &msg )
{
	// Read the number of addons in the list
	uint8 numAddons = msg.ReadByte();
	
	if (numAddons == 0)
	{
		Msg("[CF Addon Sync] Host has no workshop items subscribed.\n");
		return;
	}
	
	Msg("[CF Addon Sync] Host has %d workshop items. Checking subscriptions...\n", numAddons);
	
	// Read each addon file ID
	CUtlVector<PublishedFileId_t> hostAddons;
	for (uint8 i = 0; i < numAddons; i++)
	{
		uint32 lowBits = msg.ReadLong();
		uint32 highBits = msg.ReadLong();
		PublishedFileId_t fileID = ((uint64)highBits << 32) | lowBits;
		hostAddons.AddToTail(fileID);
	}
	
	// Check which addons we don't have and subscribe to them
	if (CFWorkshop())
	{
		int numSubscribed = 0;
		int numAlreadyHave = 0;
		
		for (int i = 0; i < hostAddons.Count(); i++)
		{
			PublishedFileId_t fileID = hostAddons[i];
			
			// Check if we already have this addon
			CCFWorkshopItem* pItem = CFWorkshop()->GetItem(fileID);
			if (pItem && pItem->IsSubscribed())
			{
				numAlreadyHave++;
				continue;
			}
			
			// Subscribe to the addon
			Msg("[CF Addon Sync] Subscribing to workshop item %llu...\n", fileID);
			CFWorkshop()->SubscribeItem(fileID);
			numSubscribed++;
		}
		
		if (numSubscribed > 0)
		{
			Msg("[CF Addon Sync] Subscribed to %d new workshop items. They will download automatically.\n", numSubscribed);
			Msg("[CF Addon Sync] You may need to restart the game after downloads complete.\n");
		}
		else if (numAlreadyHave > 0)
		{
			Msg("[CF Addon Sync] You already have all of the host's workshop items!\n");
		}
	}
}
#endif

void RegisterUserMessages()
{
	usermessages->Register( "Geiger", 1 );		// geiger info data
	usermessages->Register( "Train", 1 );		// train control data
	usermessages->Register( "HudText", -1 );	
	usermessages->Register( "SayText", -1 );	
	usermessages->Register( "SayText2", -1 );
	usermessages->Register( "TextMsg", -1 );
	usermessages->Register( "ResetHUD", 1 );	// called every respawn
	usermessages->Register( "GameTitle", 0 );	// show game title
	usermessages->Register( "ItemPickup", -1 );	// for item history on screen
	usermessages->Register( "ShowMenu", -1 );	// show hud menu
	usermessages->Register( "Shake", 13 );		// shake view
	usermessages->Register( "Fade", 10 );		// fade HUD in/out
	usermessages->Register( "VGUIMenu", -1 );	// Show VGUI menu
	usermessages->Register( "Rumble", 3 );	// Send a rumble to a controller
	usermessages->Register( "CloseCaption", -1 ); // Show a caption (by string id number)(duration in 10th of a second)

	usermessages->Register( "SendAudio", -1 );	// play radion command

	usermessages->Register( "VoiceMask", VOICE_MAX_PLAYERS_DW*4 * 2 + 1 );
	usermessages->Register( "RequestState", 0 );

	usermessages->Register( "Damage", -1 );		// for HUD damage indicators
	usermessages->Register( "HintText", -1 );	// Displays hint text display
	usermessages->Register( "KeyHintText", -1 );	// Displays hint text display
	
	usermessages->Register( "HudMsg", -1 );
	usermessages->Register( "AmmoDenied", 2 );
	usermessages->Register( "AchievementEvent", -1 );

	usermessages->Register( "UpdateRadar", -1 );

	usermessages->Register( "VoiceSubtitle", 3 );

	usermessages->Register( "HudNotify", 2 );	// Type, bForceDisplay
	usermessages->Register( "HudNotifyCustom", -1 );

	usermessages->Register( "PlayerStatsUpdate", -1 );
	usermessages->Register( "MapStatsUpdate", -1 );

	usermessages->Register( "PlayerIgnited", 3 );
	usermessages->Register( "PlayerIgnitedInv", 3 );

	usermessages->Register( "HudArenaNotify", 2 );

	usermessages->Register( "UpdateAchievement", -1 );
	
//=============================================================================
// HPE_BEGIN:
// [msmith]	Training Messages
//=============================================================================
	usermessages->Register( "TrainingMsg", -1 );	// Displays a training message
	usermessages->Register( "TrainingObjective", -1 );	// Displays a training objective
//=============================================================================
// HPE_END
//=============================================================================
// 
//=============================================================================
// HPE_BEGIN:
// CUSTOM FORTRESS MESSAGES
//=============================================================================
	usermessages->Register( "VS_SendNotification", -1 );	// Displays a notification
	usermessages->Register( "WorkshopMapID", 8 );	// Workshop map file ID (64-bit)
	usermessages->Register( "AddonList", -1 );		// List of host's workshop addons for auto-download
//=============================================================================
// HPE_END
//=============================================================================

#ifdef CLIENT_DLL
	// Hook the WorkshopMapID message on client
	usermessages->HookMessage( "WorkshopMapID", MsgFunc_WorkshopMapID );
	// Hook the AddonList message on client
	usermessages->HookMessage( "AddonList", MsgFunc_AddonList );
#endif

	usermessages->Register( "DamageDodged", -1 );

	usermessages->Register( "PlayerJarated", 2 );
	usermessages->Register( "PlayerExtinguished", 2 );
	usermessages->Register( "PlayerJaratedFade", 2 );
	usermessages->Register( "PlayerShieldBlocked", 2 );

	usermessages->Register( "BreakModel", -1 );
	usermessages->Register( "CheapBreakModel", -1 );
	usermessages->Register( "BreakModel_Pumpkin", -1 );
	usermessages->Register( "BreakModelRocketDud", -1 );

	// Voting
	usermessages->Register( "CallVoteFailed", -1 );
	usermessages->Register( "VoteStart", -1 );
	usermessages->Register( "VotePass", -1 );
	usermessages->Register( "VoteFailed", 6 );
	usermessages->Register( "VoteSetup", -1 );  // Initiates client-side voting UI

	usermessages->Register( "PlayerBonusPoints", 3 );
	usermessages->Register( "RDTeamPointsChanged", 4 );

	usermessages->Register( "SpawnFlyingBird", -1 );
	usermessages->Register( "PlayerGodRayEffect", -1 );
	usermessages->Register( "PlayerTeleportHomeEffect", -1 );

	usermessages->Register( "MVMStatsReset", -1 );
	usermessages->Register( "MVMPlayerEvent", -1 );
	usermessages->Register( "MVMResetPlayerStats", -1 );
	usermessages->Register( "MVMWaveFailed", 0 );
	usermessages->Register( "MVMAnnouncement", 2 );	// Send an enumerated message
	usermessages->Register( "MVMPlayerUpgradedEvent", 9 );	// PlayerIdx(1), WaveIdx(1), ItemDef(2), AttributeDef(2), Quality(1), cost(2)
	usermessages->Register( "MVMVictory", 2 );	// IsKicking(1), time(1) (seconds)
	usermessages->Register( "MVMWaveChange", 15 ); // ServerWaveID(2), deaths(1), damageBot(4), damageGiant(4), damageTank(4)
	usermessages->Register( "MVMLocalPlayerUpgradesClear", 1 );	// Count(1)
	usermessages->Register( "MVMLocalPlayerUpgradesValue", 6 );	// Class(1), ItemDef(2), Upgrade(1), cost(2)
	usermessages->Register( "MVMResetPlayerWaveSpendingStats", 1 );	// Wave(1)
	usermessages->Register( "MVMLocalPlayerWaveSpendingValue", 12 );	// PlayerIdx(8), Wave(1), Type(1), Cost(2)
	usermessages->Register( "MVMResetPlayerUpgradeSpending", -1 );
	usermessages->Register( "MVMServerKickTimeUpdate", 1 );	// time(1) (seconds)

	usermessages->Register( "PlayerLoadoutUpdated", -1 );

	usermessages->Register( "PlayerTauntSoundLoopStart", -1 );
	usermessages->Register( "PlayerTauntSoundLoopEnd", -1 );

	usermessages->Register( "ForcePlayerViewAngles", -1 );
	usermessages->Register( "BonusDucks", 2 );	// ent index, ignoretimer
	usermessages->Register( "EOTLDuckEvent", 7 ); // IsCreated (vs IsPickedUp), ID of Creator, ID of Victim, ID of Toucher, iDuckTeam, Count, IsGolden

	usermessages->Register( "PlayerPickupWeapon", -1 );

	usermessages->Register( "QuestObjectiveCompleted", 8 + 1 + 1 + 1 + 2 + 1 ); // QuestID(8) + Points0(1) + Points1(1) + Points2(1) + ObjectiveDef(2) + ScorerUserID(1)

	usermessages->Register( "SdkRequestEquipment", -1 );

	usermessages->Register( "BuiltObject", 3 ); // object type, object mode (entrance vs. exit), index

	// NVNT register haptic user messages
	RegisterHapticMessages();
	RegisterScriptMessages();
}


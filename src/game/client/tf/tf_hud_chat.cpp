//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "vgui/ILocalize.h"
#include "engine/IEngineSound.h"
#include "c_tf_team.h"
#include "c_playerresource.h"
#include "c_tf_player.h"
#include "tf_gamerules.h"
#include "ihudlcd.h"
#include "tf_hud_freezepanel.h"
#include "tier1/strtools.h"
#if defined( REPLAY_ENABLED )
#include "replay/ienginereplay.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar is_dedicated;
//-----------------------------------------------------------------------------
// Client-side role detection functions
//-----------------------------------------------------------------------------
int ClientUTIL_PlayerIsModDev( int clientIndex )
{
	if ( !g_PR )
		return 0;
		
	// Get the player's Steam ID using the client function
	CSteamID steamID = GetSteamIDForPlayerIndex( clientIndex );
	uint64 steamid = steamID.ConvertToUint64();
	
	switch(steamid)
	{
		//Main Devs
		case 76561198130175522: // Alien31
		case 76561198886303174: // main_thing
		case 76561199004586557: // Vvis
		case 76561198302570978: // GabenZone
		case 76561198269305264: // Loner
			return 1;
		break;
		//Publishers
		case 76561198087658491: // MixerRules
			return 2;
		break;
		//Contributors
		case 76561198813329543: // Grub - it's grubbin time.
		case 76561198423023261: // Alieneer/MOTS0
			return 3;
		break;
		//None
		default:
			return 0;
		break;
	}
}

bool ClientUTIL_IsListenServerHost( int clientIndex )
{
	// On dedicated servers, there's no listen server host
	if ( engine->IsPlayingDemo() || engine->IsPlayingTimeDemo() || is_dedicated.GetBool() )
		return false;
		
	// The listen server host is always player index 1
	return (clientIndex == 1);
}

DECLARE_HUDELEMENT( CHudChat );
DECLARE_HUD_MESSAGE( CHudChat, SayText );
DECLARE_HUD_MESSAGE( CHudChat, SayText2 );
DECLARE_HUD_MESSAGE( CHudChat, TextMsg );
DECLARE_HUD_MESSAGE( CHudChat, VoiceSubtitle );

extern ConVar hud_saytext_time;
extern ConVar cf_sound_chatping;
extern ConVar cf_sound_chatping_file;

void RenderPartyChatMessage( const ChatMessage_t& message,
							 RichText* pRichText,
							 const Color& colorSystemMessage,
							 const Color& colorPlayerName, 
							 const Color& colorText, bool shouldPrint )
{
	CSteamID localSteamID;
	if ( SteamUser() )
	{
		localSteamID = SteamUser()->GetSteamID();
	}

	switch ( message.m_eType )
	{
	default:
		Assert( !"Unknown chat message type" );
		break;

		// System messages
	case k_eTFPartyChatType_Synthetic_MemberJoin:
	case k_eTFPartyChatType_Synthetic_MemberLeave:
	case k_eTFPartyChatType_Synthetic_MemberOffline:
	case k_eTFPartyChatType_Synthetic_MemberOnline:
	{
		// Don't show system messages about ourselves
		if ( localSteamID == message.m_steamID )
			return;

		const char *pSystemMessageType = nullptr;
		switch ( message.m_eType )
		{
			case k_eTFPartyChatType_Synthetic_MemberJoin:
				pSystemMessageType = "#TF_Matchmaking_PlayerJoinedPartyChat";
				break;
			case k_eTFPartyChatType_Synthetic_MemberLeave:
				pSystemMessageType = "#TF_Matchmaking_PlayerLeftPartyChat";
				break;
			case k_eTFPartyChatType_Synthetic_MemberOffline:
				pSystemMessageType = "#TF_Matchmaking_PlayerOfflinePartyChat";
				break;
			case k_eTFPartyChatType_Synthetic_MemberOnline:
				pSystemMessageType = "#TF_Matchmaking_PlayerOnlinePartyChat";
				break;
		}

		wchar_t wCharPlayerName[ 128 ] = { 0 };
		GetPlayerNameForSteamID( wCharPlayerName, sizeof(wCharPlayerName), message.m_steamID );

		const char *pTokenUTF8 = g_pVGuiLocalize->FindAsUTF8( pSystemMessageType );
		const char szParam1[] = "%s1";
		const char *pSplit = V_strstr( pTokenUTF8, szParam1 );
		if ( !pSplit )
		{
			AssertMsg( false, "Missing token in localization string" );
			return;
		}

		// Before name
		if ( pSplit != pTokenUTF8 )
		{
			char szBefore[128] = { 0 };
			V_strncpy( szBefore, pTokenUTF8, ( pSplit - pTokenUTF8 ) + 1 );
			pRichText->InsertColorChange( colorSystemMessage );
			pRichText->InsertString( szBefore );
		}

		// Name token
		pRichText->InsertColorChange( colorPlayerName );
		pRichText->InsertString( wCharPlayerName );

		// After name
		const char *pAfterSplit = ( pSplit + sizeof( szParam1 ) - 1 );
		if ( *pAfterSplit != '\0' )
		{
			pRichText->InsertColorChange( colorSystemMessage );
			pRichText->InsertString( pAfterSplit );
		}
	}
	break;

	case k_eTFPartyChatType_Synthetic_SendFailed:
	{
		const wchar_t *pToken = g_pVGuiLocalize->Find( "#TF_Matchmaking_SendFailedPartyChat" );
		pRichText->InsertColorChange( colorSystemMessage );
		pRichText->InsertString( pToken );
	}
	break;
	case k_eTFPartyChatType_MemberChat:
	{
		wchar_t wCharPlayerName[ 128 ];
		GetPlayerNameForSteamID( wCharPlayerName, sizeof(wCharPlayerName), message.m_steamID );
		pRichText->InsertColorChange( colorPlayerName );
		pRichText->InsertString( wCharPlayerName );
		pRichText->InsertString( ": " );
		pRichText->InsertColorChange( colorText );
		pRichText->InsertString( message.m_pwszText );
		if(shouldPrint)
		{
			char ansiName[ 128 ];
			char ansiText[ 256 ];
			g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( wCharPlayerName ), ansiName, sizeof( ansiName ) );
			g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( message.m_pwszText ), ansiText, sizeof( ansiText ) );
			Msg( "(PARTY) %s : %s\n", ansiName, ansiText);
		}
	}
	break;
	}
}

//=====================
//CHudChatLine
//=====================

void CHudChatLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hFont = pScheme->GetFont( "ChatFont" );
	SetBorder( NULL );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetFgColor( Color( 0, 0, 0, 0 ) );

	SetFont( m_hFont );
}

CHudChatLine::CHudChatLine( vgui::Panel *parent, const char *panelName ) : CBaseHudChatLine( parent, panelName )
{
	m_text = NULL;
}


//=====================
//CHudChatInputLine
//=====================

void CHudChatInputLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
}



//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
	ListenForGameEvent( "party_chat" );
#if defined ( _X360 )
	RegisterForRenderGroup( "mid" );
#endif
}

void CHudChat::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_colorPartyEvent = pScheme->GetColor( "Green", Color( 255, 255, 255, 255 ) );
	m_colorPartyMessage = pScheme->GetColor( "Green", Color( 255, 255, 255, 255 ) );
}

void CHudChat::CreateChatInputLine( void )
{
	m_pChatInput = new CHudChatInputLine( this, "ChatInputLine" );
	m_pChatInput->SetVisible( false );
}

void CHudChat::CreateChatLines( void )
{
#ifndef _XBOX
	m_ChatLine = new CHudChatLine( this, "ChatLine1" );
	m_ChatLine->SetVisible( false );		

#endif
}

void CHudChat::Init( void )
{
	BaseClass::Init();

	HOOK_HUD_MESSAGE( CHudChat, SayText );
	HOOK_HUD_MESSAGE( CHudChat, SayText2 );
	HOOK_HUD_MESSAGE( CHudChat, TextMsg );
	HOOK_HUD_MESSAGE( CHudChat, VoiceSubtitle );
}

void CHudChat::FireGameEvent( IGameEvent *event )
{
	if ( FStrEq( event->GetName(), "party_chat" ) )
	{
		// When we get a party_chat event, someone in our party said something.  We don't want
		// to do a chat popup if we're playing, so instead we'll pipe their message into the
		// chat history and prepend the message with (PARTY) and give the message a distinct
		// color.
		CSteamID steamID = SteamIDFromDecimalString( event->GetString( "steamid", "0" ) );

		auto eType = (ETFPartyChatType)event->GetInt( "type", k_eTFPartyChatType_Invalid );
		const char *pszText = event->GetString( "text", "" );
		wchar_t *wText = NULL;
		int l = V_strlen( pszText );
		int nBufSize = ( l *sizeof(wchar_t) ) + 4;
		wText = (wchar_t *)stackalloc( nBufSize );
		V_UTF8ToUnicode( pszText, wText, nBufSize );

		// Manually insert a linebreak
		GetChatHistory()->InsertChar( L'\n' );

		// If someone said something, prepend "(PARTY)" like how we put "(TEAM)" for team messages
		if ( eType == k_eTFPartyChatType_MemberChat )
		{
			GetChatHistory()->InsertColorChange( GetTextColorForClient( COLOR_NORMAL, 0 ) );
			GetChatHistory()->InsertString( g_pVGuiLocalize->Find( "#TF_Chat_Party" ) );
			GetChatHistory()->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
		}

		// Put the message and fade
		RenderPartyChatMessage( { eType, wText, steamID }, GetChatHistory(), m_colorPartyEvent, m_colorPartyMessage, m_colorPartyMessage );
		GetChatHistory()->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );

		return;
	}

	BaseClass::FireGameEvent( event );
}

//-----------------------------------------------------------------------------
// Purpose: Hides us when our render group is hidden
// move render group to base chat when its safer
//-----------------------------------------------------------------------------
bool CHudChat::ShouldDraw( void )
{
#if defined( REPLAY_ENABLED )
	extern IEngineClientReplay *g_pEngineClientReplay;
	if ( g_pEngineClientReplay->IsPlayingReplayDemo() )
		return false;
#endif

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: Overrides base reset to not cancel chat at round restart
//-----------------------------------------------------------------------------
void CHudChat::Reset( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHudChat::GetChatInputOffset( void )
{
	if ( m_pChatInput->IsVisible() )
	{
		return m_iFontHeight;
	}
	else
	{
		return 0;
	}
}

int CHudChat::GetFilterForString( const char *pString )
{
	int iFilter = BaseClass::GetFilterForString( pString );

	if ( iFilter == CHAT_FILTER_NONE )
	{
		if ( !Q_stricmp( pString, "#TF_Name_Change" ) ) 
		{
			return CHAT_FILTER_NAMECHANGE;
		}
	}

	return iFilter;
}

//-----------------------------------------------------------------------------
Color CHudChat::GetClientColor( int clientIndex )
{
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	if ( pScheme == NULL )
		return Color( 255, 255, 255, 255 );

	if ( clientIndex == 0 ) // console msg
	{
		return g_ColorGreen;
	}
	else if( g_PR )
	{
		int iTeam = g_PR->GetTeam( clientIndex );

		C_TFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( clientIndex ) );
		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

		if ( IsVoiceSubtitle() == true )
		{
			// if this player is on the other team, disguised as my team, show disguised color
			if ( pPlayer && pLocalPlayer && pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) &&
				pPlayer->m_Shared.GetDisguiseTeam() == pLocalPlayer->GetTeamNumber() )
			{
				iTeam = pPlayer->m_Shared.GetDisguiseTeam();
			}
		}

		switch ( iTeam )
		{
		case TF_TEAM_RED	: return pScheme->GetColor( "TFColors.ChatTextRed", g_ColorRed );
		case TF_TEAM_BLUE	: return pScheme->GetColor( "TFColors.ChatTextBlue", g_ColorBlue );
		default	: return g_ColorGrey;
		}
	}

	return g_ColorYellow;
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudChat::IsVisible( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return BaseClass::IsVisible();
}

const char *CHudChat::GetDisplayedSubtitlePlayerName( int clientIndex )
{
	C_TFPlayer *pPlayer = ToTFPlayer( UTIL_PlayerByIndex( clientIndex ) );
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer || !pLocalPlayer )
		return BaseClass::GetDisplayedSubtitlePlayerName( clientIndex );

	// If they are disguised as the enemy, and not on our team
	if ( pPlayer->m_Shared.InCond( TF_COND_DISGUISED ) &&
		pPlayer->m_Shared.GetDisguiseTeam() != pPlayer->GetTeamNumber() && 
		!pLocalPlayer->InSameTeam( pPlayer ) )
	{
		C_TFPlayer *pDisguiseTarget = pPlayer->m_Shared.GetDisguiseTarget();

		Assert( pDisguiseTarget );

		if ( !pDisguiseTarget )
		{
			return BaseClass::GetDisplayedSubtitlePlayerName( clientIndex );
		}

		return pDisguiseTarget->GetPlayerName();
	}

	return BaseClass::GetDisplayedSubtitlePlayerName( clientIndex );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color CHudChat::GetTextColorForClient( TextColor colorNum, int clientIndex )
{
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	if ( pScheme == NULL )
		return Color( 255, 255, 255, 255 );

	Color c;
	switch ( colorNum )
	{
	case COLOR_CUSTOM:
		c = m_ColorCustom;
		break;

	case COLOR_PLAYERNAME:
		c = GetClientColor( clientIndex );
		break;

	case COLOR_LOCATION:
		c = g_ColorDarkGreen;
		break;

	case COLOR_ACHIEVEMENT:
		{
			IScheme *pSourceScheme = scheme()->GetIScheme( scheme()->GetScheme( "SourceScheme" ) ); 
			if ( pSourceScheme )
			{
				c = pSourceScheme->GetColor( "SteamLightGreen", GetBgColor() );
			}
			else
			{
				c = pScheme->GetColor( "TFColors.ChatTextYellow", GetBgColor() );
			}
		}
		break;

	default:
		c = pScheme->GetColor( "TFColors.ChatTextYellow", GetBgColor() );
	}

	return Color( c[0], c[1], c[2], 255 );
}

int CHudChat::GetFilterFlags( void )
{
//=============================================================================
// HPE_BEGIN:
// [msmith]	We don't want to be displaying these chat messages when we're in training.
//			This is because we don't want the player seeing when bots join etc.
//=============================================================================
	if ( TFGameRules() && TFGameRules()->IsInTraining() )
		return CHAT_FILTER_PUBLICCHAT;
//=============================================================================
// HPE_END
//=============================================================================

	int iFlags = BaseClass::GetFilterFlags();

	if ( TFGameRules() && TFGameRules()->IsInArenaMode() == true )
	{
		return iFlags &= ~CHAT_FILTER_TEAMCHANGE;
	}
	
	return iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Override to add custom tags with colors
//-----------------------------------------------------------------------------
void CHudChat::MsgFunc_SayText2( bf_read &msg )
{
	// Got message during connection
	if ( !g_PR )
		return;

	int client = msg.ReadByte();
	bool bWantsToChat = msg.ReadByte();

	wchar_t szBuf[6][256];
	char untranslated_msg_text[256];
	wchar_t *msg_text = ReadLocalizedString( msg, szBuf[0], sizeof( szBuf[0] ), false, untranslated_msg_text, sizeof( untranslated_msg_text ) );

	// Read chat components
	ReadChatTextString ( msg, szBuf[1], sizeof( szBuf[1] ) );		// player name
	ReadChatTextString ( msg, szBuf[2], sizeof( szBuf[2] ) );		// chat text
	ReadLocalizedString( msg, szBuf[3], sizeof( szBuf[3] ), true );
	ReadLocalizedString( msg, szBuf[4], sizeof( szBuf[4] ), true );

	if ( bWantsToChat )
	{
		int iFilter = CHAT_FILTER_NONE;

		if ( client > 0 && (g_PR->GetTeam( client ) != g_PR->GetTeam( GetLocalPlayerIndex() )) )
		{
			iFilter = CHAT_FILTER_PUBLICCHAT;
		}

		// Build player name with custom tags
		wchar_t szPlainPlayerName[512];
		Q_wcsncpy( szPlainPlayerName, L"", sizeof(szPlainPlayerName) );
		
		// Add HOST tag
		if ( client > 0 && ClientUTIL_IsListenServerHost( client ) )
		{
			V_wcsncat( szPlainPlayerName, g_pVGuiLocalize->Find( "#TF_Tag_Host" ), ARRAYSIZE(szPlainPlayerName) );
		}
		
		// Add special tag
		if ( client > 0 )
		{
			int bModDev = ClientUTIL_PlayerIsModDev( client );
			switch(bModDev)
			{
				case 1:
					V_wcsncat( szPlainPlayerName, g_pVGuiLocalize->Find( "#TF_Tag_Dev" ), ARRAYSIZE(szPlainPlayerName) );
					break;
				case 2:
					V_wcsncat( szPlainPlayerName, g_pVGuiLocalize->Find( "#TF_Tag_Publisher" ), ARRAYSIZE(szPlainPlayerName) );
					break;
				case 3:
					V_wcsncat( szPlainPlayerName, g_pVGuiLocalize->Find( "#TF_Tag_Contributor" ), ARRAYSIZE(szPlainPlayerName) );
					break;
			}
		}
		
		// Add player name
		V_wcsncat( szPlainPlayerName, szBuf[1], ARRAYSIZE(szPlainPlayerName) );

		// Use the format string to construct the complete message with DEAD/TEAM prefixes
		g_pVGuiLocalize->ConstructString_safe( szBuf[5], msg_text, 4, szPlainPlayerName, szBuf[2], szBuf[3], szBuf[4] );

		// Build custom colored chat message for history display
		CHudChatHistory *pChatHistory = GetChatHistory();
		if ( pChatHistory )
		{
			// Parse the constructed message to colorize it properly
			wchar_t *pszMessage = szBuf[5];
			
			// Insert linebreak first
			pChatHistory->InsertString( L"\n" );
			
			// First, display special tags (HOST, DEV, PUBLISHER, CONTRIBUTOR) before any prefixes
			const wchar_t *pszHostTagText = g_pVGuiLocalize->Find( "#TF_Tag_Host" );
			const wchar_t *pszDevTagText = g_pVGuiLocalize->Find( "#TF_Tag_Dev" );
			const wchar_t *pszPubTagText = g_pVGuiLocalize->Find( "#TF_Tag_Publisher" );
			const wchar_t *pszContribTagText = g_pVGuiLocalize->Find( "#TF_Tag_Contributor" );
			
			// Display HOST tag if present
			if ( client > 0 && ClientUTIL_IsListenServerHost( client ) )
			{
				pChatHistory->InsertColorChange( Color( 240, 240, 240, 255 ) ); // #F0F0F0
				pChatHistory->InsertString( pszHostTagText );
				pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
			}
			
			// Display special role tag if present
			if ( client > 0 )
			{
				int bModDev = ClientUTIL_PlayerIsModDev( client );
				switch(bModDev)
				{
					case 1:
						pChatHistory->InsertColorChange( Color( 240, 135, 43, 255 ) ); // #F0872B
						pChatHistory->InsertString( pszDevTagText );
						pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
						break;
					case 2:
						pChatHistory->InsertColorChange( Color( 46, 143, 191, 255 ) ); // #2E8FBF
						pChatHistory->InsertString( pszPubTagText );
						pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
						break;
					case 3:
						pChatHistory->InsertColorChange( Color( 95, 154, 63, 255 ) ); // #5F9A3F
						pChatHistory->InsertString( pszContribTagText );
						pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
						break;
				}
			}
			
			// Look for prefixes in the message and colorize them
			//wchar_t *pszStart = pszMessage;
			wchar_t *pszPlayerStart = NULL;
			
			// Get localized prefix strings
			const wchar_t *pszDeadPrefix = g_pVGuiLocalize->Find( "#TF_Chat_Dead" );
			const wchar_t *pszSpecPrefix = g_pVGuiLocalize->Find( "#TF_Chat_Spec" );
			const wchar_t *pszTeamPrefix = g_pVGuiLocalize->Find( "#TF_Chat_TeamPrefix" );
			const wchar_t *pszCoachPrefix = g_pVGuiLocalize->Find( "#TF_Chat_CoachPrefix" );
			
			// Find where the player name starts (after any prefixes)
			if ( wcsstr( pszMessage, pszDeadPrefix ) )
			{
				pChatHistory->InsertColorChange( Color( 255, 255, 255, 255 ) );  // White for DEAD
				
				// Check for combined DEAD+TEAM prefix
				wchar_t szDeadTeamPrefix[64];
				V_swprintf_safe( szDeadTeamPrefix, L"%s%s", pszDeadPrefix, pszTeamPrefix );
				if ( wcsstr( pszMessage, szDeadTeamPrefix ) )
				{
					pChatHistory->InsertString( pszDeadPrefix );
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
					pChatHistory->InsertColorChange( GetClientColor( client ) );  // Team colored for TEAM
					pChatHistory->InsertString( pszTeamPrefix );
					pChatHistory->InsertString( L" " );
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
					pszPlayerStart = wcsstr( pszMessage, szDeadTeamPrefix ) + wcslen( szDeadTeamPrefix ) + 1; // +1 for space
				}
				else
				{
					pChatHistory->InsertString( pszDeadPrefix );
					pChatHistory->InsertString( L" " );
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
					pszPlayerStart = wcsstr( pszMessage, pszDeadPrefix ) + wcslen( pszDeadPrefix ) + 1; // +1 for space
				}
			}
			else if ( wcsstr( pszMessage, pszSpecPrefix ) )
			{
				pChatHistory->InsertColorChange( Color( 160, 160, 160, 255 ) );  // Gray for SPEC
				
				// Check for combined SPEC+TEAM prefix
				wchar_t szSpecTeamPrefix[64];
				V_swprintf_safe( szSpecTeamPrefix, L"%s%s", pszSpecPrefix, pszTeamPrefix );
				if ( wcsstr( pszMessage, szSpecTeamPrefix ) )
				{
					pChatHistory->InsertString( pszSpecPrefix );
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
					pChatHistory->InsertColorChange( GetClientColor( client ) );  // Team colored for TEAM
					pChatHistory->InsertString( pszTeamPrefix );
					pChatHistory->InsertString( L" " );
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
					pszPlayerStart = wcsstr( pszMessage, szSpecTeamPrefix ) + wcslen( szSpecTeamPrefix ) + 1; // +1 for space
				}
				else
				{
					pChatHistory->InsertString( pszSpecPrefix );
					pChatHistory->InsertString( L" " );
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
					pszPlayerStart = wcsstr( pszMessage, pszSpecPrefix ) + wcslen( pszSpecPrefix ) + 1; // +1 for space
				}
			}
			else if ( wcsstr( pszMessage, pszTeamPrefix ) )
			{
				pChatHistory->InsertColorChange( GetClientColor( client ) );  // Team colored for TEAM
				pChatHistory->InsertString( pszTeamPrefix );
				pChatHistory->InsertString( L" " );
				pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
				pszPlayerStart = wcsstr( pszMessage, pszTeamPrefix ) + wcslen( pszTeamPrefix ) + 1; // +1 for space
			}
			else if ( wcsstr( pszMessage, pszCoachPrefix ) )
			{
				pChatHistory->InsertColorChange( Color( 153, 204, 255, 255 ) );  // Blue for COACH
				pChatHistory->InsertString( pszCoachPrefix );
				pChatHistory->InsertString( L" " );
				pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
				pszPlayerStart = wcsstr( pszMessage, pszCoachPrefix ) + wcslen( pszCoachPrefix ) + 1; // +1 for space
			}
			else
			{
				pszPlayerStart = pszMessage;
			}

			if ( pszPlayerStart )
			{
				// Find the player name and message separator
				wchar_t *pszColon = wcsstr( pszPlayerStart, L" :" );
				if ( pszColon )
				{
					// Find the actual player name (after removing the tags we already added)
					wchar_t *pszNameStart = pszPlayerStart;
					
					// Skip past any HOST tag in the message
					wchar_t *pszHostTag = wcsstr( pszNameStart, pszHostTagText );
					if ( pszHostTag )
					{
						pszNameStart = pszHostTag + wcslen( pszHostTagText );
					}
					
					// Skip past any role tag in the message
					wchar_t *pszDevTag = wcsstr( pszNameStart, pszDevTagText );
					wchar_t *pszPubTag = wcsstr( pszNameStart, pszPubTagText );
					wchar_t *pszContribTag = wcsstr( pszNameStart, pszContribTagText );
					
					if ( pszDevTag )
					{
						pszNameStart = pszDevTag + wcslen( pszDevTagText );
					}
					else if ( pszPubTag )
					{
						pszNameStart = pszPubTag + wcslen( pszPubTagText );
					}
					else if ( pszContribTag )
					{
						pszNameStart = pszContribTag + wcslen( pszContribTagText );
					}
					
					// Display player name with team color
					wchar_t *pszNameSection = pszNameStart;
					*pszColon = L'\0';  // Temporarily null-terminate at colon
					
					if ( *pszNameSection )
					{
						pChatHistory->InsertColorChange( GetClientColor( client ) );
						pChatHistory->InsertString( pszNameSection );
					}
					
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
					*pszColon = L' ';  // Restore the space
					
					// Add separator and chat text
					pChatHistory->InsertColorChange( GetTextColorForClient( COLOR_NORMAL, client ) );
					pChatHistory->InsertString( L" : " );
					pChatHistory->InsertString( pszColon + 3 );  // Skip past " : "
					pChatHistory->InsertFade( hud_saytext_time.GetFloat(), CHAT_HISTORY_IDLE_FADE_TIME );
				}
			}
		}

		char ansiString[512];
		g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[ 5 ] ), ansiString, sizeof( ansiString ) );

		UTIL_GetFilteredChatText( client, ansiString, sizeof( ansiString ) );
		if ( !ansiString[ 0 ] )
		{
			// This user has been ignored at the Steam level
			return;
		}

		Msg( "%s\n", (const char*)RemoveColorMarkup(ansiString) );


		if ( cf_sound_chatping.GetBool() )
		{ 
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, cf_sound_chatping_file.GetString() );
		}
	}
	else
	{
		// For non-chat messages, use the original approach
		g_pVGuiLocalize->ConstructString_safe( szBuf[5], msg_text, 4, szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		char ansiString[512];
		g_pVGuiLocalize->ConvertUnicodeToANSI( ConvertCRtoNL( szBuf[ 5 ] ), ansiString, sizeof( ansiString ) );
		
		// print raw chat text
		ChatPrintf( client, GetFilterForString( untranslated_msg_text ), "%s", ansiString );
	}
}

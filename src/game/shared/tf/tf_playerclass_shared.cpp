//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================

#include "cbase.h"
#include "KeyValues.h"
#include "tf_playerclass_shared.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "filesystem.h"
#include "tier2/tier2.h"

//=============================================================================
//
// Shared player class data.
//

//=============================================================================
//
// Tables.
//

#define CLASSMODEL_PARITY_BITS		3
#define CLASSMODEL_PARITY_MASK		((1<<CLASSMODEL_PARITY_BITS)-1)


// Client specific.
#ifdef CLIENT_DLL

BEGIN_RECV_TABLE_NOBASE( CTFPlayerClassShared, DT_TFPlayerClassShared )
	RecvPropInt( RECVINFO( m_iClass ) ),
	RecvPropString( RECVINFO( m_iszClassIcon ) ),
	RecvPropString( RECVINFO( m_iszCustomModel ) ),
	RecvPropString( RECVINFO( m_iszCustomHandsModel ) ),
	RecvPropVector( RECVINFO( m_vecCustomModelOffset ) ),
	RecvPropQAngles( RECVINFO( m_angCustomModelRotation ) ),
	RecvPropBool( RECVINFO( m_bCustomModelRotates ) ),
	RecvPropBool( RECVINFO( m_bCustomModelRotationSet ) ),
	RecvPropBool( RECVINFO( m_bCustomModelVisibleToSelf ) ),
	RecvPropBool( RECVINFO( m_bUseClassAnimations ) ),
	RecvPropInt( RECVINFO(m_iClassModelParity) ),
END_RECV_TABLE()

// Server specific.
#else

BEGIN_SEND_TABLE_NOBASE( CTFPlayerClassShared, DT_TFPlayerClassShared )
	SendPropInt( SENDINFO( m_iClass ), Q_log2( TF_CLASS_COUNT_ALL )+1, SPROP_UNSIGNED ),
	SendPropStringT( SENDINFO( m_iszClassIcon ) ),
	SendPropStringT( SENDINFO( m_iszCustomModel ) ),
	SendPropStringT( SENDINFO( m_iszCustomHandsModel ) ),
	SendPropVector( SENDINFO( m_vecCustomModelOffset ) ),
	SendPropQAngles( SENDINFO( m_angCustomModelRotation ) ),
	SendPropBool( SENDINFO( m_bCustomModelRotates ) ),
	SendPropBool( SENDINFO( m_bCustomModelRotationSet ) ),
	SendPropBool( SENDINFO( m_bCustomModelVisibleToSelf ) ),
	SendPropBool( SENDINFO( m_bUseClassAnimations ) ),
	SendPropInt( SENDINFO(m_iClassModelParity), CLASSMODEL_PARITY_BITS, SPROP_UNSIGNED ),
END_SEND_TABLE()

#endif


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFPlayerClassShared::CTFPlayerClassShared()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerClassShared::Reset( void )
{
	m_iClass.Set( TF_CLASS_UNDEFINED );
#ifdef CLIENT_DLL
	m_iszClassIcon[0] = '\0';
	m_iszCustomModel[0] = '\0';
	m_iszCustomHandsModel[0] = '\0';
#else
	m_iszClassIcon.Set( NULL_STRING );
	m_iszCustomModel.Set( NULL_STRING );
	m_iszCustomHandsModel.Set( NULL_STRING );
#endif
	m_vecCustomModelOffset = vec3_origin;
	m_angCustomModelRotation = vec3_angle;
	m_bCustomModelRotates = true;
	m_bCustomModelRotationSet = false;
	m_bCustomModelVisibleToSelf = true;
	m_bUseClassAnimations = false;
	m_iClassModelParity = 0;
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerClassShared::SetCustomModel( const char *pszModelName, bool isUsingClassAnimations )
{
	if ( pszModelName && pszModelName[0] )
	{
		bool bAllowPrecache = CBaseEntity::IsPrecacheAllowed();
		CBaseEntity::SetAllowPrecache( true );
		CBaseEntity::PrecacheModel( pszModelName );
		CBaseEntity::SetAllowPrecache( bAllowPrecache );

		m_iszCustomModel.Set( AllocPooledString( pszModelName ) );

		m_bUseClassAnimations = isUsingClassAnimations;
	}
	else
	{
		m_iszCustomModel.Set( NULL_STRING );
		m_vecCustomModelOffset = vec3_origin;
		m_angCustomModelRotation = vec3_angle;
	}

	m_iClassModelParity = (m_iClassModelParity + 1) & CLASSMODEL_PARITY_MASK;
}

void CTFPlayerClassShared::SetCustomHandsModel( const char *pszModelName )
{
	if ( pszModelName && pszModelName[0] )
	{
		bool bAllowPrecache = CBaseEntity::IsPrecacheAllowed();
		CBaseEntity::SetAllowPrecache( true );
		CBaseEntity::PrecacheModel( pszModelName );
		CBaseEntity::SetAllowPrecache( bAllowPrecache );

		m_iszCustomHandsModel.Set( AllocPooledString( pszModelName ) );
	}
	else
	{
		m_iszCustomHandsModel.Set( NULL_STRING );
	}

}
#endif // #ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerClassShared::CustomModelHasChanged( void )
{
	if ( m_iClassModelParity != m_iOldClassModelParity )
	{
		m_iOldClassModelParity = m_iClassModelParity.Get();
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const char	*CTFPlayerClassShared::GetCustomHandsModel( void ) const						
{ 
	// Does this play have an overridden model?
#ifdef CLIENT_DLL
	if ( m_iszCustomHandsModel[0] )
		return m_iszCustomHandsModel;
#else
	if ( m_iszCustomHandsModel.Get() != NULL_STRING )
		return ( STRING( m_iszCustomHandsModel.Get() ) );
#endif

#define MAX_MODEL_FILENAME_LENGTH 256
	static char modelFilename[ MAX_MODEL_FILENAME_LENGTH ];

	Q_strncpy( modelFilename, GetPlayerClassData( m_iClass )->GetHandsModelName(), sizeof( modelFilename ) );


	return modelFilename;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

const char	*CTFPlayerClassShared::GetModelName( void ) const						
{ 
	// Does this play have an overridden model?
#ifdef CLIENT_DLL
	if ( m_iszCustomModel[0] )
		return m_iszCustomModel;
#else
	if ( m_iszCustomModel.Get() != NULL_STRING )
		return ( STRING( m_iszCustomModel.Get() ) );
#endif

	#define MAX_MODEL_FILENAME_LENGTH 256
	static char modelFilename[ MAX_MODEL_FILENAME_LENGTH ];

	Q_strncpy( modelFilename, GetPlayerClassData( m_iClass )->GetModelName(), sizeof( modelFilename ) );
	

	return modelFilename;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the player class.
//-----------------------------------------------------------------------------
const char *g_HACK_GunslingerEngineerArmsOverride = "models\\weapons\\c_models\\c_engineer_gunslinger.mdl";
const char *HACK_RobotGunsLinger = "models\\mvm\\weapons\\c_models\\c_engineer_bot_gunslinger.mdl";

const char *CTFPlayerClassShared::GetHandModelName( int iHandIndex = 0 ) const
{
	switch(iHandIndex)
	{
	case 1:
		return g_HACK_GunslingerEngineerArmsOverride;
		break;
	case 2:
		return g_szBotViewmodels[m_iClass];
		break;
	case 3:
		return g_szBotBossViewmodels[m_iClass];
		break;
	default:
		return GetPlayerClassData(m_iClass)->m_szHandModelName;
		break;
	}				
}
//-----------------------------------------------------------------------------
// Purpose: Initialize the player class.
//-----------------------------------------------------------------------------
bool CTFPlayerClassShared::Init( int iClass )
{
	Assert ( ( iClass >= TF_FIRST_NORMAL_CLASS ) && ( iClass <= TF_LAST_NORMAL_CLASS ) );

	Reset();
	m_iClass = iClass;

#ifdef CLIENT_DLL
	V_strncpy( m_iszCustomModel, g_aRawPlayerClassNamesShort[ m_iClass ], sizeof( m_iszCustomModel ) );
#else
	m_iszClassIcon.Set( AllocPooledString( g_aRawPlayerClassNamesShort[ m_iClass ] ) );
#endif

	return true;
}

// If needed, put this into playerclass scripts
bool CTFPlayerClassShared::CanBuildObject( int iObjectType )
{
	bool bFound = false;

	TFPlayerClassData_t  *pData = GetData();

	int i;
	for ( i=0;i<TF_PLAYER_BLUEPRINT_COUNT;i++ )
	{
		if ( iObjectType == pData->m_aBuildable[i] )
		{
			bFound = true;
			break;
		}
	}

	return bFound;
}

//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================
#include "cbase.h"
#include "tf_weapon_bmmh.h"
#include "tf_fx_shared.h"
#include "tf_gamerules.h"
#include "in_buttons.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_tf_gamestats.h"
#include "prediction.h"
// Server specific.
#else
#include "tf_player.h"
#include "tf_gamestats.h"
#include "tf_projectile_scrapball.h"
#endif

#define TF_WEAPON_BMMH_MODEL		"models/weapons/w_models/w_stickybomb_d.mdl"
#define POSEPARAM_METER				"weapon_meter"
#define TF_WEAPON_BMMH_CHARGE_SOUND	"Weapon_StickyBombLauncher.ChargeUp"
#define TF_WEAPON_BMMH_MIN_COST		30.0f
#define TF_WEAPON_BMMH_MAX_COST		75.0f

//=============================================================================
//
// Weapon tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFBMMH, DT_WeaponBMMH )

BEGIN_NETWORK_TABLE( CTFBMMH, DT_WeaponBMMH )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flChargeCancelTime ) ),
#else
	SendPropFloat( SENDINFO( m_flChargeCancelTime ), 0, SPROP_NOSCALE | SPROP_CHANGES_OFTEN ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFBMMH )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_flChargeBeginTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flChargeCancelTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_bmmh, CTFBMMH );
PRECACHE_WEAPON_REGISTER( tf_weapon_bmmh );

// Server specific.
#ifndef CLIENT_DLL
BEGIN_DATADESC( CTFBMMH )
	DEFINE_FIELD( m_flChargeCancelTime, FIELD_FLOAT ),
END_DATADESC()
#endif

//=============================================================================
//
// Weapon functions.
//
CTFBMMH::CTFBMMH()
{
	m_bReloadsSingly = false;
	m_flChargeCancelTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Cancel charge instead of detonating projectiles
//-----------------------------------------------------------------------------
void CTFBMMH::SecondaryAttack( void )
{
	if ( !CanAttack() )
		return;

	// Cancel the charge if we're currently charging
	if ( GetInternalChargeBeginTime() > 0 )
	{
		// Reset charge time
		SetInternalChargeBeginTime( 0 );
		
		// Set a delay before allowing charging again (2.5 seconds)
		m_flChargeCancelTime = gpGlobals->curtime + 1.5f;
		
		// Stop charging sound
#ifdef CLIENT_DLL
		StopSound( TF_WEAPON_BMMH_CHARGE_SOUND );
#endif
		
		// Play a deny sound to indicate charge was cancelled
		WeaponSound( SPECIAL2 );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculate ammo per shot based on charge
//-----------------------------------------------------------------------------
int CTFBMMH::GetAmmoPerShot( void )
{
	float flChargeTime = gpGlobals->curtime - GetInternalChargeBeginTime();
	float flMaxChargeTime = GetChargeMaxTime();
	if ( flChargeTime <= 0.0f || flChargeTime > ( flMaxChargeTime + 0.01f ) || flMaxChargeTime <= 0.0f )
		return TF_WEAPON_BMMH_MIN_COST;
	
	return (int)RemapValClamped( flChargeTime, 0.0f, flMaxChargeTime, TF_WEAPON_BMMH_MIN_COST, TF_WEAPON_BMMH_MAX_COST);
}

//-----------------------------------------------------------------------------
// Purpose: Override PrimaryAttack to respect charge cancel delay
//-----------------------------------------------------------------------------
void CTFBMMH::PrimaryAttack( void )
{
	int iMaxChargeLimit = clamp(GetOwner()->GetAmmoCount( m_iPrimaryAmmoType ), TF_WEAPON_BMMH_MIN_COST, TF_WEAPON_BMMH_MAX_COST);
	// Check if we're still in the charge cancel delay period
	if ( m_flChargeCancelTime > gpGlobals->curtime )
	{
		// Don't allow charging yet - ensure charge time is reset
		SetInternalChargeBeginTime( 0 );
		return;
	}
	
	// Check for ammunition.
	if (m_iClip1 <= 0 && m_iClip1 != -1 || GetOwner()->GetAmmoCount(m_iPrimaryAmmoType) < TF_WEAPON_BMMH_MIN_COST)
		return;

	// Are we capable of firing again?
	if (m_flNextPrimaryAttack > gpGlobals->curtime)
		return;

	if (!CanAttack())
	{
		SetInternalChargeBeginTime(0);
		return;
	}

	if (GetInternalChargeBeginTime() <= 0)
	{
		// Set the weapon mode.
		m_iWeaponMode = TF_WEAPON_PRIMARY_MODE;

		// save that we had the attack button down
		SetInternalChargeBeginTime ( gpGlobals->curtime );

		SendWeaponAnim(ACT_VM_PULLBACK);

#ifdef CLIENT_DLL
		EmitSound( TF_WEAPON_BMMH_CHARGE_SOUND );
#endif // CLIENT_DLL
	}
	else
	{
		float flTotalChargeTime = gpGlobals->curtime - GetInternalChargeBeginTime();

		if (flTotalChargeTime >= (GetChargeMaxTime() * ((iMaxChargeLimit - TF_WEAPON_BMMH_MIN_COST) / (TF_WEAPON_BMMH_MAX_COST - TF_WEAPON_BMMH_MIN_COST))))
		{
			LaunchGrenade();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Override ItemPostFrame to prevent firing during cancel delay
//-----------------------------------------------------------------------------
void CTFBMMH::ItemPostFrame( void )
{
	// Check if we're in the charge cancel delay period
	if ( m_flChargeCancelTime > gpGlobals->curtime )
	{
		// Ensure charge time stays at 0 during the delay
		SetInternalChargeBeginTime( 0 );
	}
	
	// Call base class implementation
	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Override LaunchGrenade to ensure charge is properly reset
//-----------------------------------------------------------------------------
void CTFBMMH::LaunchGrenade( void )
{
	// Call base class to do the actual firing
	BaseClass::LaunchGrenade();
	
	// Ensure charge time is reset (base class should do this, but be explicit)
	SetInternalChargeBeginTime( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Fire projectile and store the metal cost
// NOTE: We override completely because base class tries to add to pipebomb list
//-----------------------------------------------------------------------------
CBaseEntity *CTFBMMH::FireProjectile( CTFPlayer *pPlayer )
{
	if ( !pPlayer )
		return NULL;
	
	// Call the gun base class, NOT the pipebomb launcher base class
	// This avoids the pipebomb tracking code which doesn't apply to scrapballs
	CBaseEntity *pProjectile = CTFWeaponBaseGun::FireProjectile( pPlayer );
	
#ifndef CLIENT_DLL
	// Calculate and store the metal cost before firing
	int iMetalCost = GetAmmoPerShot();

	// Store the metal cost in the projectile
	if ( pProjectile )
	{
		CTFProjectile_ScrapBall *pScrapBall = dynamic_cast<CTFProjectile_ScrapBall*>( pProjectile );
		if ( pScrapBall )
		{
			pScrapBall->SetMetalCost( iMetalCost );
		}
	}
#endif
	
	return pProjectile;
}
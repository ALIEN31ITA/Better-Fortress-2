
//========= Copyright Valve Corporation & BetterFortress, All rights reserved. ============//
//
// Engineer's Bmmh Scrapball
//
//=============================================================================
#include "cbase.h"
#include "tf_weaponbase.h"
#include "tf_projectile_scrapball.h"
#include "tf_player.h"
#include "tf_gamerules.h"
#include "tf_fx.h"
#include "soundent.h"

//=============================================================================
//
// TF Scrapball functions (Server specific).
//
#define SCRAPBALL_MODEL "models/weapons/w_models/w_rocket.mdl"

LINK_ENTITY_TO_CLASS( tf_projectile_scrapball, CTFProjectile_ScrapBall );
PRECACHE_REGISTER( tf_projectile_scrapball );

IMPLEMENT_NETWORKCLASS_ALIASED( TFProjectile_ScrapBall, DT_TFProjectile_ScrapBall )

BEGIN_NETWORK_TABLE( CTFProjectile_ScrapBall, DT_TFProjectile_ScrapBall )
	SendPropBool( SENDINFO( m_bCritical ) ),
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFProjectile_ScrapBall *CTFProjectile_ScrapBall::Create( CBaseEntity *pLauncher, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, CBaseEntity *pScorer )
{
	CTFProjectile_ScrapBall *pRocket = static_cast<CTFProjectile_ScrapBall*>( CTFBaseRocket::Create( pLauncher, "tf_projectile_scrapball", vecOrigin, vecAngles, pOwner ) );

	if ( pRocket )
	{
		pRocket->SetScorer( pScorer );
	}

	return pRocket;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::Spawn()
{
	SetModel( SCRAPBALL_MODEL );
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	SetGravity( 1.25 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::Precache()
{
	int iModel = PrecacheModel( SCRAPBALL_MODEL );
	PrecacheGibsForModel( iModel );
	PrecacheParticleSystem( "critical_rocket_blue" );
	PrecacheParticleSystem( "critical_rocket_red" );
	PrecacheParticleSystem( "rockettrail" );
	PrecacheParticleSystem( "rockettrail_RocketJumper" );
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::SetScorer( CBaseEntity *pScorer )
{
	m_Scorer = pScorer;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBasePlayer *CTFProjectile_ScrapBall::GetScorer( void )
{
	return dynamic_cast<CBasePlayer *>( m_Scorer.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFProjectile_ScrapBall::GetDamageType() 
{ 
	int iDmgType = BaseClass::GetDamageType();
	if ( m_bCritical )
	{
		iDmgType |= DMG_CRITICAL;
	}

	return iDmgType;
}

//-----------------------------------------------------------------------------
// Purpose: Scrapball Touch
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::RocketTouch( CBaseEntity *pOther )
{
	BaseClass::RocketTouch( pOther );

	CTFPlayer *pScorer = ToTFPlayer( GetOwnerEntity() );
	Vector vecOrigin = GetAbsOrigin();
	float flRadius = GetRadius(); 

	//Scan for buildings
	CUtlVector<CBaseObject*> objVector;
	for ( int i = 0; i < IBaseObjectAutoList::AutoList().Count(); ++i )
	{
		CBaseObject* pObj = static_cast<CBaseObject*>( IBaseObjectAutoList::AutoList()[i] );
		if ( !pObj || pObj->GetTeamNumber() != GetTeamNumber() )
			continue;

		float flDist = (pObj->GetAbsOrigin() - vecOrigin).Length();
		if ( flDist <= flRadius )
		{
			objVector.AddToTail( pObj );
		}
	}

	//Scan for Players
	/*CUtlVector<CTFPlayer*> playerVector;
	for ( int i = 0; i < IBaseObjectAutoList::AutoList().Count(); ++i )
	{
		CTFPlayer* pPlayer = static_cast<CTFPlayer*>( IBaseObjectAutoList::AutoList()[i] );
		CTFPlayer *pPlayer = ToTFPlayer( C_BasePlayer::GetLocalPlayer() );
		if ( !pObj || pObj->GetTeamNumber() != GetTeamNumber() )
			continue;

		float flDist = (pObj->GetAbsOrigin() - vecOrigin).Length();
		if ( flDist <= flRadius )
		{
			playerVector.AddToTail( pObj );
		}
	}*/

	int iAmmoPerShot = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( GetLauncher(), iAmmoPerShot, mod_ammo_per_shot );
	
	int iValidObjCount = objVector.Count();
	if ( iValidObjCount > 0 )
	{
		FOR_EACH_VEC( objVector, i )
		{
			CBaseObject *pObj = objVector[i];

			if (pObj->GetHealth() == pObj->GetMaxHealth())
				return;

			bool bRepairHit = false;
			bool bUpgradeHit = false;

			bRepairHit = ( pObj->Command_Repair( pScorer, iAmmoPerShot, 1.f ) > 0 );

			if ( !bRepairHit )
			{
				bUpgradeHit = pObj->CheckUpgradeOnHit( pScorer );
			}
		}
		// Play an impact sound.
		EmitSound( "Weapon_Arrow.ImpactFleshCrossbowHeal" );
	}
}

int CTFProjectile_ScrapBall::GiveMetal( CTFPlayer *pPlayer )
{
	int iMetalToGive = 40;
	int iMetal = pPlayer->GiveAmmo( iMetalToGive, TF_AMMO_METAL, false, kAmmoSource_DispenserOrCart );

	return iMetal;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::Explode( trace_t *pTrace, CBaseEntity *pOther )
{
	if ( ShouldNotDetonate() )
	{
		Destroy( true );
		return;
	}

	// Save this entity as enemy, they will take 100% damage.
	m_hEnemy = pOther;

	// Invisible.
	SetModelName( NULL_STRING );
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage = DAMAGE_NO;

	// Pull out a bit.
	if ( pTrace->fraction != 1.0 )
	{
		SetAbsOrigin( pTrace->endpos + ( pTrace->plane.normal * 1.0f ) );
	}

	// Play explosion sound and effect.
	Vector vecOrigin = GetAbsOrigin();
	CPVSFilter filter( vecOrigin );
	
	// Halloween Spell Effect Check
	int iHalloweenSpell = 0;
	int iCustomParticleIndex = INVALID_STRING_INDEX;
	item_definition_index_t ownerWeaponDefIndex = INVALID_ITEM_DEF_INDEX;
	// if the owner is a Sentry, Check its owner
	CBaseEntity *pPlayerOwner = GetOwnerPlayer();

	if ( TF_IsHolidayActive( kHoliday_HalloweenOrFullMoon ) )
	{
		CALL_ATTRIB_HOOK_INT_ON_OTHER( pPlayerOwner, iHalloweenSpell, halloween_pumpkin_explosions );
		if ( iHalloweenSpell > 0 )
		{
			iCustomParticleIndex = GetParticleSystemIndex( "halloween_explosion" );
		}
	}

	int iNoSelfBlastDamage = 0;
	CTFWeaponBase *pWeapon = dynamic_cast< CTFWeaponBase * >( GetOriginalLauncher() );
	if ( pWeapon )
	{
		ownerWeaponDefIndex = pWeapon->GetAttributeContainer()->GetItem()->GetItemDefIndex();

		CALL_ATTRIB_HOOK_INT_ON_OTHER( pWeapon, iNoSelfBlastDamage, no_self_blast_dmg );
		if ( iNoSelfBlastDamage )
		{
			iCustomParticleIndex = GetParticleSystemIndex( "ExplosionCore_Wall_Jumper" );
		}
	}
	
	int iLargeExplosion = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( pPlayerOwner, iLargeExplosion, use_large_smoke_explosion );
	if ( iLargeExplosion > 0 )
	{
		DispatchParticleEffect( "explosionTrail_seeds_mvm", GetAbsOrigin(), GetAbsAngles() );
		DispatchParticleEffect( "fluidSmokeExpl_ring_mvm", GetAbsOrigin(), GetAbsAngles() );
	}

	TE_TFExplosion( filter, 0.0f, vecOrigin, pTrace->plane.normal, GetWeaponID(), pOther->entindex(), ownerWeaponDefIndex, SPECIAL1, iCustomParticleIndex );

	CSoundEnt::InsertSound ( SOUND_COMBAT, vecOrigin, 1024, 3.0 );

	// Damage.
	CBaseEntity *pAttacker = GetOwnerEntity();
	IScorer *pScorerInterface = dynamic_cast<IScorer*>( pAttacker );
	if ( pScorerInterface )
	{
		pAttacker = pScorerInterface->GetScorer();
	}
	else if ( pAttacker && pAttacker->GetOwnerEntity() )
	{
		pAttacker = pAttacker->GetOwnerEntity();
	}

	float flRadius = GetRadius();

	if ( pAttacker ) // No attacker, deal no damage. Otherwise we could potentially kill teammates.
	{
		CTFPlayer *pTarget = ToTFPlayer( GetEnemy() );
		if ( pTarget )
		{
			// Rocket Specialist
			CheckForStunOnImpact( pTarget );

			RecordEnemyPlayerHit( pTarget, true );
		}

		CTakeDamageInfo info( this, pAttacker, GetOriginalLauncher(), vec3_origin, vecOrigin, GetDamage(), GetDamageType(), GetDamageCustom() );
		CTFRadiusDamageInfo radiusinfo( &info, vecOrigin, flRadius, NULL, TF_ROCKET_RADIUS_FOR_RJS, GetDamageForceScale() );
		TFGameRules()->RadiusDamage( radiusinfo );
	}


	// Don't decal players with scorch.
	if ( ( iNoSelfBlastDamage == 0 ) )
	{
		UTIL_DecalTrace( pTrace, "Scorch" );
	}

	// Remove the rocket.
	UTIL_Remove( this );

	return;
}

//-----------------------------------------------------------------------------
// Purpose: Scrapball was deflected.
//-----------------------------------------------------------------------------
void CTFProjectile_ScrapBall::Deflected( CBaseEntity *pDeflectedBy, Vector &vecDir )
{
	CTFPlayer *pTFDeflector = ToTFPlayer( pDeflectedBy );
	if ( !pTFDeflector )
		return;

	ChangeTeam( pTFDeflector->GetTeamNumber() );
	SetLauncher( pTFDeflector->GetActiveWeapon() );

	CTFPlayer* pOldOwner = ToTFPlayer( GetOwnerEntity() );
	SetOwnerEntity( pTFDeflector );

	if ( pOldOwner )
	{
		pOldOwner->SpeakConceptIfAllowed( MP_CONCEPT_DEFLECTED, "projectile:1,victim:1" );
	}

	if ( pTFDeflector->m_Shared.IsCritBoosted() )
	{
		SetCritical( true );
	}

	CTFWeaponBase::SendObjectDeflectedEvent( pTFDeflector, pOldOwner, GetWeaponID(), this );

	IncrementDeflected();
	m_nSkin = ( GetTeamNumber() == TF_TEAM_BLUE ) ? 1 : 0;
}

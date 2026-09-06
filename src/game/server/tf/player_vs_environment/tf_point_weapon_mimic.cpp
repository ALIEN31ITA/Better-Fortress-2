//========= Copyright Valve Corporation, All rights reserved. ============//

#include "cbase.h"
#include "player_vs_environment/tf_point_weapon_mimic.h"
#include "te_effect_dispatch.h"
#ifdef GAME_DLL
#include "tf_fx.h"
#include "rope.h" 
#endif


LINK_ENTITY_TO_CLASS( tf_point_weapon_mimic, CTFPointWeaponMimic );

IMPLEMENT_SERVERCLASS_ST( CTFPointWeaponMimic, DT_CTFPointWeaponMimic )
	SendPropInt( SENDINFO ( m_nWeaponType ) )
END_SEND_TABLE()

// Data Description
BEGIN_DATADESC( CTFPointWeaponMimic )

	// Keyfields
	DEFINE_KEYFIELD( m_nWeaponType, FIELD_INTEGER, "WeaponType" ),
	DEFINE_KEYFIELD( m_pzsFireSound, FIELD_SOUNDNAME, "FireSound" ),
	DEFINE_KEYFIELD( m_pzsFireParticles, FIELD_STRING, "ParticleEffect" ),
	DEFINE_KEYFIELD( m_pzsModelOverride, FIELD_MODELNAME, "ModelOverride" ),
	DEFINE_KEYFIELD( m_flModelScale, FIELD_FLOAT, "ModelScale" ),
	DEFINE_KEYFIELD( m_flSpeedMin, FIELD_FLOAT, "SpeedMin" ),
	DEFINE_KEYFIELD( m_flSpeedMax, FIELD_FLOAT, "SpeedMax" ),
	DEFINE_KEYFIELD( m_flDamage, FIELD_FLOAT, "Damage" ),
	DEFINE_KEYFIELD( m_flSplashRadius, FIELD_FLOAT, "SplashRadius" ),
	DEFINE_KEYFIELD( m_flSpreadAngle, FIELD_FLOAT, "SpreadAngle" ),
	DEFINE_KEYFIELD( m_bCrits, FIELD_BOOLEAN, "Crits" ),
	DEFINE_KEYFIELD( m_flProjectileTimer, FIELD_FLOAT, "ProjectileTimer"),
	DEFINE_KEYFIELD( m_flProjectileGravity, FIELD_FLOAT, "ProjectileGravity"),
	DEFINE_KEYFIELD( m_nProjectileType, FIELD_INTEGER, "ProjectileType" ),
	DEFINE_KEYFIELD( m_iProjectileAmmo, FIELD_INTEGER, "ProjectileAmmo" ),

	//Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "FireOnce", InputFireOnce ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "FireMultiple", InputFireMultiple ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DetonateStickies", DetonateStickies ),
	DEFINE_INPUTFUNC(FIELD_VOID, "DetonateFlare", DetonateFlare),
	DEFINE_INPUTFUNC(FIELD_VOID, "DetachGrapplingHook", DetachGrapplingHook),

	// Outputs
	DEFINE_OUTPUT( m_OnGrapplingLatched, "OnGrapplingLatched" ),
	DEFINE_OUTPUT( m_OnGrapplingLatched, "OnGrapplingLatchedPlayer" ),

END_DATADESC()



CTFPointWeaponMimic::CTFPointWeaponMimic()
: m_pzsModelOverride( NULL )
{
}

CTFPointWeaponMimic::~CTFPointWeaponMimic()
{
	RemoveGrapplingHook();
}

void CTFPointWeaponMimic::RemoveGrapplingHook()
{
	if ( m_hRope.Get() )
	{ 
		UTIL_Remove( m_hRope );
	}
	if ( m_hGrapplingHook.Get() ) 
	{ 
		UTIL_Remove( m_hGrapplingHook );
	}
}


void CTFPointWeaponMimic::Spawn()
{
	BaseClass::Spawn();

	if( m_pzsModelOverride )
	{
		PrecacheModel( m_pzsModelOverride );
	}

	if ( m_pzsFireSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_pzsFireSound ) );
	}

	if ( STRING( m_pzsFireParticles ) && STRING( m_pzsFireParticles )[0] )
	{
		PrecacheParticleSystem( STRING( m_pzsFireParticles ) );
	}

}

void CTFPointWeaponMimic::SetProjectileHasGravity( CBaseEntity *pProj )
{
	pProj->SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	pProj->SetGravity( m_flProjectileGravity );
}

void CTFPointWeaponMimic::InputFireOnce( inputdata_t& inputdata )
{
	Fire();
}

void CTFPointWeaponMimic::InputFireMultiple( inputdata_t& inputdata )
{
	int nNumFires = Max( 1, abs(inputdata.value.Int()) );

	while( nNumFires-- )
	{
		Fire();
	}
}

void CTFPointWeaponMimic::DetonateStickies( inputdata_t& inputdata )
{
	int count = m_Pipebombs.Count();

	for ( int i = 0; i < count; i++ )
	{
		CTFGrenadePipebombProjectile *pTemp = m_Pipebombs[i];
		if ( pTemp )
		{
			//This guy will die soon enough.
			if ( pTemp->IsEffectActive( EF_NODRAW ) )
				continue;
	
			pTemp->Detonate();
		}
	}

	m_Pipebombs.Purge();
}

void CTFPointWeaponMimic::DetonateFlare( inputdata_t& inputdata )
{
	int count = m_Flares.Count();

	for ( int i = 0; i < count; i++ )
	{
		CTFProjectile_Flare *pTemp = m_Flares[i];
		if ( pTemp )
		{
			//This guy will die soon enough.
			if ( pTemp->IsEffectActive(EF_NODRAW) )
				continue;

			pTemp->Detonate();
		}
	}

	m_Flares.Purge();
}

void CTFPointWeaponMimic::DetachGrapplingHook( inputdata_t& inputdata )
{
	RemoveGrapplingHook();
}

void CTFPointWeaponMimic::Fire()
{
	Assert( m_nWeaponType >= 0 && m_nWeaponType < WEAPON_TYPES );

	switch( m_nWeaponType )
	{
	case WEAPON_STANDARD_ROCKET:
		FireRocket();
		break;
	case WEAPON_STANDARD_GRENADE:
		FireGrenade();
		break;
	case WEAPON_STANDARD_ARROW:
		FireArrow();
		break;
	case WEAPON_STICKY_GRENADE:
		FireStickyGrenade();
		break;
	case WEAPON_STANDARD_FLARE:
		FireFlare();
		break;
	case WEAPON_DRG_ENERGYBALL:
	case WEAPON_DRG_ENERGYRING_POMSON:
	case WEAPON_DRG_ENERGYRING_BISON:
		FireDRGEnergyProj();
		break;
	case WEAPON_BALL_STUN:
	case WEAPON_BALL_BLEED:
		FireBalls();
		break;
	case WEAPON_DRAGONS_FURY:
		FireFireBall();
		break;
	case WEAPON_SPECIAL_ARROW_RANGER:
	case WEAPON_SPECIAL_ARROW_CRUSADER:
	case WEAPON_SPECIAL_ARROW_GRAPPLING:
		FireSpecialArrows();
		break;
	}

	if ( m_pzsFireSound != NULL_STRING )
	{
		EmitSound( STRING(m_pzsFireSound) );
	}

	if ( STRING( m_pzsFireParticles ) && STRING( m_pzsFireParticles )[0] )
	{
		Vector vecOrigin = GetAbsOrigin();
		QAngle vecAngles = GetAbsAngles();
		CPVSFilter pvsFilter( vecOrigin );
		TE_TFParticleEffect( pvsFilter, 0.0f, STRING( m_pzsFireParticles ), vecOrigin, vecAngles );
	}
}

void CTFPointWeaponMimic::FireRocket()
{
	CTFProjectile_Rocket *pProjectile = CTFProjectile_Rocket::Create( this, GetAbsOrigin(), GetFiringAngles(), this, NULL);

	if ( pProjectile )
	{
		if( m_pzsModelOverride )
		{
			pProjectile->SetModel( m_pzsModelOverride );
		}
		pProjectile->ChangeTeam( GetTeamNumber() ); //TF_TEAM_BLUE
		pProjectile->SetCritical( m_bCrits );
		pProjectile->SetDamage( m_flDamage );
		Vector vVelocity = pProjectile->GetAbsVelocity().Normalized() * GetSpeed();
		pProjectile->SetAbsVelocity( vVelocity );	
		pProjectile->SetupInitialTransmittedGrenadeVelocity( vVelocity );
		pProjectile->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
		if ( m_flProjectileGravity )
		{ 
			SetProjectileHasGravity( pProjectile );
		}
	}
}

void CTFPointWeaponMimic::FireGrenade()
{
	QAngle vFireAngles = GetFiringAngles();
	Vector vForward, vUp;
	AngleVectors( vFireAngles, &vForward, NULL, &vUp );
	Vector vVelocity( vForward * GetSpeed() );

	CTFGrenadePipebombProjectile *pGrenade = static_cast<CTFGrenadePipebombProjectile*>( CBaseEntity::CreateNoSpawn( "tf_projectile_pipe", GetAbsOrigin(), vFireAngles, this ) );
	if ( pGrenade )
	{
		DispatchSpawn( pGrenade );
		if( m_pzsModelOverride )
		{
			pGrenade->SetModel( m_pzsModelOverride );
		}
		pGrenade->InitGrenade( vVelocity, AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ), NULL, m_flDamage, m_flSplashRadius );
		pGrenade->ChangeTeam( GetTeamNumber() ); //TF_TEAM_BLUE
		pGrenade->m_nSkin = ( GetTeamNumber() == TF_TEAM_BLUE ) ? 1 : 0;
		pGrenade->SetDetonateTimerLength( m_flProjectileTimer /*2.f*/ );
		pGrenade->SetModelScale( m_flModelScale );
		pGrenade->SetCollisionGroup( TFCOLLISION_GROUP_ROCKETS );  // we want to use collision_group_rockets so we don't ever collide with players
		pGrenade->SetDamage( m_flDamage );
		pGrenade->SetFullDamage( m_flDamage );
		pGrenade->SetDamageRadius( m_flSplashRadius );
		pGrenade->SetCritical( m_bCrits );
		vVelocity = pGrenade->GetAbsVelocity().Normalized() * GetSpeed();
		pGrenade->SetAbsVelocity( vVelocity );	
		pGrenade->SetupInitialTransmittedGrenadeVelocity( vVelocity );
	}
}

void CTFPointWeaponMimic::FireArrow()
{
	CTFProjectile_Arrow *pProjectile = CTFProjectile_Arrow::Create( GetAbsOrigin(), GetFiringAngles(), 2000, 0.7f, TF_PROJECTILE_ARROW, this, NULL );

	if ( pProjectile )
	{
		if( m_pzsModelOverride )
		{
			pProjectile->SetModel( m_pzsModelOverride );
		}
		pProjectile->ChangeTeam( GetTeamNumber() ); //TF_TEAM_BLUE
		pProjectile->SetCritical( m_bCrits );
		pProjectile->SetDamage( m_flDamage );
		Vector vVelocity = pProjectile->GetAbsVelocity().Normalized() * GetSpeed();
		pProjectile->SetAbsVelocity( vVelocity );	
		pProjectile->SetupInitialTransmittedGrenadeVelocity( vVelocity );
		pProjectile->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
	}
}

void CTFPointWeaponMimic::FireStickyGrenade()
{
	QAngle vFireAngles = GetFiringAngles();
	Vector vForward, vUp;
	AngleVectors( vFireAngles, &vForward, NULL, &vUp );
	Vector vVelocity( vForward * GetSpeed() );

	CTFGrenadePipebombProjectile *pGrenade = static_cast<CTFGrenadePipebombProjectile*>( CBaseEntity::CreateNoSpawn( "tf_projectile_pipe", GetAbsOrigin(), vFireAngles, this ) );
	if ( pGrenade )
	{
		pGrenade->m_bDefensiveBomb = true;

		pGrenade->SetPipebombMode( TF_GL_MODE_REMOTE_DETONATE );
		pGrenade->SetModelScale( m_flModelScale );
		pGrenade->SetCollisionGroup( TFCOLLISION_GROUP_ROCKETS );  // we want to use collision_group_rockets so we don't ever collide with players
		pGrenade->SetCanTakeDamage( false );
		DispatchSpawn( pGrenade );
		if( m_pzsModelOverride )
		{
			pGrenade->SetModel( m_pzsModelOverride );
		}
		else
		{
			pGrenade->SetModel( "models/weapons/w_models/w_stickybomb_d.mdl" );
		}

		pGrenade->InitGrenade( vVelocity, AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ), NULL, m_flDamage, m_flSplashRadius );
		vVelocity = pGrenade->GetAbsVelocity().Normalized() * GetSpeed();
		pGrenade->SetAbsVelocity( vVelocity );	
		pGrenade->SetupInitialTransmittedGrenadeVelocity( vVelocity );

		pGrenade->SetDamage( m_flDamage );
		pGrenade->SetFullDamage( m_flDamage );
		pGrenade->SetDamageRadius( m_flSplashRadius );
		pGrenade->SetCritical( m_bCrits );
		pGrenade->ChangeTeam( GetTeamNumber() ); //TF_TEAM_BLUE
		pGrenade->m_nSkin = ( GetTeamNumber() == TF_TEAM_BLUE ) ? 1 : 0;

		m_Pipebombs.AddToTail( pGrenade );
	}
}

void CTFPointWeaponMimic::FireFlare()
{
	QAngle vFireAngles = GetFiringAngles();
	Vector vForward, vUp;
	AngleVectors(vFireAngles, &vForward, NULL, &vUp);
	Vector vVelocity(vForward * GetSpeed());

	//TRY THIS
	CTFProjectile_Flare *pProjectile = CTFProjectile_Flare::Create( this, GetAbsOrigin(), vFireAngles, this, NULL );
	if ( pProjectile )
	{
		pProjectile->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
		pProjectile->SetLauncher( this );
		pProjectile->SetCritical( m_bCrits );
		pProjectile->SetDamage( m_flDamage );
		if ( m_flProjectileGravity )
		{
			SetProjectileHasGravity( pProjectile );
		}
		Vector vVelocity = pProjectile->GetAbsVelocity().Normalized() * GetSpeed();
		pProjectile->SetAbsVelocity( vVelocity );	
		pProjectile->SetupInitialTransmittedGrenadeVelocity( vVelocity );
	}
}

//TODO: Fix the ShouldPenetrate() Particles

void CTFPointWeaponMimic::FireDRGEnergyProj()
{
	QAngle vFireAngles = GetFiringAngles();
	Vector vForward, vUp;
	AngleVectors(vFireAngles, &vForward, NULL, &vUp);
	Vector vVelocity(vForward * GetSpeed());
	float fGravity = m_flProjectileGravity ? m_flProjectileGravity : 0;

	//TEMP
	Vector vProjColor1 = GetTeamNumber() == TF_TEAM_RED ? TF_PARTICLE_WEAPON_RED_1 : TF_PARTICLE_WEAPON_BLUE_1;
	Vector vProjColor2 = GetTeamNumber() == TF_TEAM_RED ? TF_PARTICLE_WEAPON_RED_2 : TF_PARTICLE_WEAPON_BLUE_2;

	if ( m_nWeaponType == WEAPON_DRG_ENERGYBALL )
	{
		CTFProjectile_EnergyBall* pProjectile = CTFProjectile_EnergyBall::Create( GetAbsOrigin(), vFireAngles, GetSpeed(), fGravity, this, NULL );
		if ( pProjectile )
		{
			pProjectile->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
			pProjectile->SetLauncher( this );
			pProjectile->SetCritical( m_bCrits );
			pProjectile->SetDamage( m_flDamage );
			if ( m_nProjectileType )
			{
				pProjectile->SetChargedShot( true );
			}
			pProjectile->SetColor( 1, vProjColor1 );
			pProjectile->SetColor( 2, vProjColor2 );
		}
	}
	else if ( m_nWeaponType == WEAPON_DRG_ENERGYRING_POMSON || m_nWeaponType == WEAPON_DRG_ENERGYRING_BISON )
	{ 
		CTFProjectile_EnergyRing *pProjectile = CTFProjectile_EnergyRing::Create( this, GetAbsOrigin(), vFireAngles,
			GetSpeed(), fGravity, this, NULL, vProjColor1, vProjColor2, m_bCrits );
		if ( pProjectile )
		{
			bool bPompson = m_nWeaponType == WEAPON_DRG_ENERGYRING_POMSON;
			pProjectile->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
			pProjectile->SetWeaponID( bPompson ? TF_WEAPON_DRG_POMSON : TF_WEAPON_RAYGUN );
			pProjectile->SetCritical( m_bCrits );
			pProjectile->SetDamage( m_flDamage );
		}
	}
}

//TODO: Fix the trail color

void CTFPointWeaponMimic::FireBalls()
{
	QAngle vFireAngles = GetFiringAngles();
	Vector vForward, vUp;
	AngleVectors(vFireAngles, &vForward, NULL, &vUp);
	Vector vVelocity(vForward * GetSpeed());

	bool bSandman = m_nWeaponType == WEAPON_BALL_STUN;

	CTFStunBall* pBall = static_cast<CTFStunBall*>(CBaseAnimating::CreateNoSpawn( bSandman ? "tf_projectile_stun_ball" : "tf_projectile_ball_ornament", GetAbsOrigin(), vFireAngles, this));
	if (pBall)
	{
		DispatchSpawn( pBall );
		pBall->InitGrenade( vVelocity, AngularImpulse( 600, random->RandomInt( -1200, 1200 ), 0 ), NULL, m_flDamage, m_flSplashRadius );
		pBall->ChangeTeam( GetTeamNumber() );
		vVelocity = pBall->GetAbsVelocity().Normalized() * GetSpeed();
		pBall->SetAbsVelocity( vVelocity );	
		pBall->SetupInitialTransmittedGrenadeVelocity( vVelocity );

		pBall->SetCritical( m_bCrits );
		pBall->SetDamage( m_flDamage );
		pBall->SetDamageRadius( m_flSplashRadius );
		pBall->SetLauncher( this );
		pBall->SetOwnerEntity( this );
		pBall->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
		pBall->m_nSkin = ( GetTeamNumber() == TF_TEAM_BLUE ) ? 1 : 0;
	}
}

//TODO: Fix the speed

void CTFPointWeaponMimic::FireFireBall()
{
	QAngle vFireAngles = GetFiringAngles();
	Vector vForward, vUp;
	AngleVectors(vFireAngles, &vForward, NULL, &vUp);
	Vector vVelocity(vForward * GetSpeed());
	Vector vecSrc = GetAbsOrigin();

	CTFProjectile_Rocket *pRocket = static_cast<CTFProjectile_Rocket*>( CBaseEntity::CreateNoSpawn( "tf_projectile_balloffire", vecSrc, vFireAngles, this));
	if (pRocket)
	{;
		pRocket->SetOwnerEntity( this );
		pRocket->SetLauncher( this );

		float flEndDist = 100;

		Vector vecProjForward = (vecSrc + vForward * flEndDist) - vecSrc;
		VectorNormalize( vecProjForward );

		Vector vVelocity = pRocket->GetAbsVelocity().Normalized() * GetSpeed();
		pRocket->SetAbsVelocity( vecProjForward * GetSpeed() );	
		pRocket->SetupInitialTransmittedGrenadeVelocity( vVelocity );

		pRocket->SetDamage( m_flDamage );
		pRocket->ChangeTeam( GetTeamNumber() );
		pRocket->SetCritical( m_bCrits );
		pRocket->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
		DispatchSpawn( pRocket );

	}
}

void CTFPointWeaponMimic::FireSpecialArrows()
{

	//Only 1 grappling hook.
	if ( m_hGrapplingHook.Get() && m_nWeaponType == WEAPON_SPECIAL_ARROW_GRAPPLING )
		return;

	ProjectileType_t iProjectile = TF_PROJECTILE_HEALING_BOLT;
	switch ( m_nWeaponType )
	{
	case WEAPON_SPECIAL_ARROW_RANGER:
		iProjectile = TF_PROJECTILE_BUILDING_REPAIR_BOLT;
		break;
	case WEAPON_SPECIAL_ARROW_CRUSADER:
		iProjectile = TF_PROJECTILE_HEALING_BOLT;
		break;
	case WEAPON_SPECIAL_ARROW_GRAPPLING:
		iProjectile = TF_PROJECTILE_GRAPPLINGHOOK;
		break;
	}

	CTFProjectile_Arrow *pArrow = CTFProjectile_Arrow::Create( GetAbsOrigin(), GetFiringAngles(), GetSpeed(), m_flProjectileGravity ? m_flProjectileGravity : 0.7f, iProjectile, this, NULL);
	if ( pArrow )
	{
		pArrow->SetLauncher( this );
		pArrow->SetOwnerEntity( this );
		pArrow->ChangeTeam( GetTeamNumber() ); 
		pArrow->SetCritical( m_bCrits );
		pArrow->SetDamage( m_flDamage );
		Vector vVelocity = pArrow->GetAbsVelocity().Normalized() * GetSpeed();
		pArrow->SetAbsVelocity( vVelocity );	
		pArrow->SetupInitialTransmittedGrenadeVelocity( vVelocity );
		pArrow->SetCollisionGroup( TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS );
		if( m_pzsModelOverride )
		{
			pArrow->SetModel( m_pzsModelOverride );
		}
	}

	// We only need 1 grappling hook active.
	if ( iProjectile == TF_PROJECTILE_GRAPPLINGHOOK )
	{
		const char *pszMaterialName = "cable/cable";
		switch ( GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			pszMaterialName = "cable/cable_blue";
			break;
		case TF_TEAM_RED:
			pszMaterialName = "cable/cable_red";
			break;
		}

		int iHookAttachment = pArrow->LookupAttachment( "rope_locator" );
		m_hRope = CRopeKeyframe::Create( this, pArrow, -1, iHookAttachment, 2, pszMaterialName);
		m_hGrapplingHook = pArrow;
	}
}

QAngle CTFPointWeaponMimic::GetFiringAngles() const
{
	// No spread?  Straight along our angles, then
	QAngle angles = GetAbsAngles();
	if( m_flSpreadAngle == 0 )
		return angles;

	Vector vForward, vRight, vUp;
	AngleVectors( angles, &vForward, &vRight, &vUp );

	// Rotate around up by half the spread input, then rotate around the original forward by +-180
	float flHalfSpread = m_flSpreadAngle / 2.f;
	VMatrix mtxRotateAroundUp		= SetupMatrixAxisRot( vUp,		RandomFloat( -flHalfSpread, flHalfSpread ) );
	VMatrix mtxRotateAroundForward	= SetupMatrixAxisRot( vForward,	RandomFloat( -180, 180 ) );

	// Rotate forward
	VMatrix mtxSpreadRot;
	MatrixMultiply( mtxRotateAroundForward, mtxRotateAroundUp, mtxSpreadRot );
	vForward = mtxSpreadRot * vForward;

	// Back to angles
	VectorAngles( vForward, vUp, angles );

	return angles;

}

float CTFPointWeaponMimic::GetSpeed() const
{
	return RandomFloat( m_flSpeedMin, m_flSpeedMax );
}
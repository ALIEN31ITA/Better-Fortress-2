//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Weapon Mimic
//
//=============================================================================
#ifndef TF_POINT_WEAPON_MIMIC_H
#define TF_POINT_WEAPON_MIMIC_H

#include "tf_projectile_rocket.h"
#include "tf_projectile_arrow.h"
#include "tf_weapon_grenade_pipebomb.h"
#include "tf_projectile_flare.h"
#include "tf_projectile_energy_ball.h"
#include "tf_projectile_energy_ring.h"
#include "tf_weapon_bat.h"

// Move the class declaration from tf_point_weapon_mimic.cpp into here
class CTFPointWeaponMimic : public CPointEntity
{
	DECLARE_CLASS( CTFPointWeaponMimic, CPointEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CTFPointWeaponMimic();
	~CTFPointWeaponMimic();
	virtual void Spawn();
	virtual void RemoveGrapplingHook();

	virtual int		GetWeaponProjectile() { return m_nWeaponType; }
	virtual int		GetProjectileType()   { return m_nProjectileType; }
	virtual float	GetProjectileDamage() { return m_flDamage; }
	virtual float	GetProjectileTimer()  { return m_flProjectileTimer; }
	virtual int	    GetProjectileAmmo()   { return m_iProjectileAmmo; }

	QAngle GetFiringAngles() const;
	float GetSpeed() const;

	enum eWeaponType
	{
		WEAPON_STANDARD_ROCKET,
		WEAPON_STANDARD_GRENADE,
		WEAPON_STANDARD_ARROW,
		WEAPON_STICKY_GRENADE,
		WEAPON_STANDARD_FLARE,
		WEAPON_DRG_ENERGYBALL,			//Cowmangler
		WEAPON_DRG_ENERGYRING_POMSON,	
		WEAPON_DRG_ENERGYRING_BISON,
		WEAPON_BALL_STUN,
		WEAPON_BALL_BLEED,	//Warp Assasin
		WEAPON_DRAGONS_FURY,
		WEAPON_SPECIAL_ARROW_RANGER,
		WEAPON_SPECIAL_ARROW_CRUSADER,
		WEAPON_SPECIAL_ARROW_GRAPPLING,


		WEAPON_TYPES
	};

	void InputFireOnce( inputdata_t& inputdata );
	void InputFireMultiple( inputdata_t& inputdata );
	void DetonateStickies( inputdata_t& inputdata );
	void DetonateFlare(inputdata_t& inputdata);
	void DetachGrapplingHook(inputdata_t& inputdata);

	COutputEvent m_OnGrapplingLatched;
	COutputEvent m_OnGrapplingLatchedPlayer;

	// Grapplin gHook
	CHandle<CTFProjectile_Arrow>	 m_hGrapplingHook;
	CHandle<CRopeKeyframe>					  m_hRope;

	CNetworkVar( int, m_nWeaponType );

private:
	void Fire();
	void SetProjectileHasGravity( CBaseEntity *pProj );

	void FireRocket();
	void FireGrenade();
	void FireArrow();
	void FireStickyGrenade();
	void FireFlare();
	void FireDRGEnergyProj();
	void FireEnergyRing();
	void FireBalls();
	void FireFireBall();
	void FireSpecialArrows();


	//int m_nWeaponType;
	bool m_bContinousFire;
	int m_nProjectileType;
	int m_iProjectileAmmo;

	// Effects for firing
	string_t m_pzsFireSound;
	string_t m_pzsFireParticles;

	// Override/defaults for the projectile/bullets
	const char* m_pzsModelOverride;
	float		m_flModelScale;
	float		m_flSpeedMin;
	float		m_flSpeedMax;
	float		m_flDamage;
	float		m_flSplashRadius;
	float       m_flProjectileTimer;
	float       m_flProjectileGravity;
	float		m_flSpreadAngle;
	bool		m_bCrits;

	// List of active projectiles
	typedef CHandle<CTFGrenadePipebombProjectile>	PipebombHandle;
	CUtlVector<PipebombHandle>		m_Pipebombs;
	// Flares
	typedef CHandle<CTFProjectile_Flare>	FlareHandle;
	CUtlVector<FlareHandle>			m_Flares;
};

#endif // TF_POINT_WEAPON_MIMIC_H
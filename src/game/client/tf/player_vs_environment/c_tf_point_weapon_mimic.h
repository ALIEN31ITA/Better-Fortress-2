//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef C_TF_POINT_WEAPON_MIMIC_H
#define C_TF_POINT_WEAPON_MIMIC_H

#include "cbase.h"

class C_CTFPointWeaponMimic : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_CTFPointWeaponMimic, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_CTFPointWeaponMimic();
	virtual int		GetWeaponProjectile() { return m_nWeaponType; }

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
private:
	int m_nWeaponType;
};

#endif // C_TF_POINT_WEAPON_MIMIC_H

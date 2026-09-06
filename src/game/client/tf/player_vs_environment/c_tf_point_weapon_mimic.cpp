//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "c_tf_point_weapon_mimic.h"

IMPLEMENT_CLIENTCLASS_DT(C_CTFPointWeaponMimic, DT_CTFPointWeaponMimic, CTFPointWeaponMimic)
	RecvPropInt( RECVINFO( m_nWeaponType ) ),
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( tf_point_weapon_mimic, C_CTFPointWeaponMimic );

C_CTFPointWeaponMimic::C_CTFPointWeaponMimic()
{
}

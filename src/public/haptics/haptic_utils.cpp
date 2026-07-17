#include "cbase.h"
#include "usermessages.h"
#include "haptics/haptic_utils.h"

void RegisterHapticMessages(void)
{
	usermessages->Register( "SPHapWeapEvent", 4 );
	usermessages->Register( "HapDmg", -1 );
	usermessages->Register( "HapPunch", -1 );
	usermessages->Register( "HapSetDrag", -1 );
	usermessages->Register( "HapSetConst", -1 );
	usermessages->Register( "HapMeleeContact", 0);
}

void HapticSendWeaponAnim(CBaseCombatWeapon* weapon, int iActivity)
{
#ifndef CLIENT_DLL
	if(iActivity == ACT_VM_IDLE)
		return;

	if( !weapon->IsPredicted() && weapon->GetOwner() && weapon->GetOwner()->IsPlayer())
	{
		CSingleUserRecipientFilter user( ToBasePlayer(weapon->GetOwner()) );
		user.MakeReliable();
		UserMessageBegin( user, "SPHapWeapEvent" );
		WRITE_LONG(iActivity);
		MessageEnd();
	}
#endif // !CLIENT_DLL
}

void HapticSetDrag(CBasePlayer* pPlayer, float drag)
{
#ifndef CLIENT_DLL
	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();
	UserMessageBegin( user, "HapSetDrag" );
	WRITE_FLOAT(drag);
	MessageEnd();
#endif // !CLIENT_DLL
}

void HapticSetConstantForce(CBasePlayer* pPlayer, Vector force)
{
#ifndef CLIENT_DLL
	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();
	UserMessageBegin( user, "HapSetConst" );
	WRITE_SHORT(force.x);
	WRITE_SHORT(force.y);
	WRITE_SHORT(force.z);
	MessageEnd();
#endif // !CLIENT_DLL
}

void HapticsDamage(CBasePlayer* pPlayer, const CTakeDamageInfo &info)
{
}

void HapticPunch(CBasePlayer* pPlayer, float x, float y, float z)
{
#ifndef CLIENT_DLL
	CSingleUserRecipientFilter user(pPlayer);
	user.MakeReliable();
	UserMessageBegin(user,"HapPunch");
	WRITE_FLOAT(x);
	WRITE_FLOAT(y);
	WRITE_FLOAT(z);
	MessageEnd();
#endif // !CLIENT_DLL
}

void HapticMeleeContact(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL
	CSingleUserRecipientFilter user(pPlayer);
	user.MakeReliable();
	UserMessageBegin(user,"HapMeleeContact");
	MessageEnd();
#endif // !CLIENT_DLL
}

void HapticProcessSound(const char* soundname, int entIndex)
{
}

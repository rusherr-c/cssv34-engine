#ifndef HAPTIC_UTILS_H
#define HAPTIC_UTILS_H

class CBasePlayer;
class CBaseCombatWeapon;
class CTakeDamageInfo;

void RegisterHapticMessages(void);

void HapticSendWeaponAnim(class CBaseCombatWeapon* weapon, int iActivity);
void HapticSetDrag(class CBasePlayer* pPlayer, float drag);
void HapticSetConstantForce(class CBasePlayer* pPlayer,Vector force);
void HapticsDamage(CBasePlayer* pPlayer, const CTakeDamageInfo &info);
void HapticPunch(CBasePlayer* pPlayer, float amount, float x, float y);
void HapticMeleeContact(CBasePlayer* pPlayer);
void HapticProcessSound(const char* soundname, int entIndex);

#endif // HAPTIC_UTILS_H

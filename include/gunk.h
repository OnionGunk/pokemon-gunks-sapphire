#ifndef GUARD_GUNK_H
#define GUARD_GUNK_H

// Gunk's Ruby: shared switches and helpers for the quality-of-life changes.

// VAR_GUNK_SETTINGS bits
#define GUNK_ENDLESS_REPEL (1 << 0)
#define GUNK_EXP_ALL       (1 << 1)

bool8 GunkSettingOn(u16 bit);
void GunkToggleSetting(u16 bit);
u16 GunkCheckLastRepel(void);
void GunkUseLastRepel(void);
bool8 GunkCanUseFieldMove(struct Pokemon *mon, u16 move);
const u8 *GunkTradeItemEffect(u16 item);
bool8 GunkCanUseFieldMoveUntaught(struct Pokemon *mon, u16 move);

// DexNav (src/gunk_dexnav.c)
#define GUNK_DEXNAV_MAX_CHOICES 8
void GunkDexNavPrepare(void);
u8 GunkDexNavMenuEntries(const u8 **texts);
void GunkDexNavStart(void);
void GunkDexNavStartLast(void);
bool8 GunkDexNavSearchAgain(void);
void GunkDexNavUpdate(void);
bool8 GunkDexNavTryEncounter(void);
void GunkDexNavBattleEnded(u8 outcome);
bool8 ScriptMenu_GunkDexNav(void);

// Following Pokémon (src/gunk_follower.c)
#define GUNK_FOLLOWER_PAL_TAG 0x1120
extern u16 gGunkFollowerPalette[16];
const struct ObjectEventGraphicsInfo *GunkFollowerGraphicsInfo(void);
bool8 GunkIsFollower(struct ObjectEvent *objectEvent);
bool8 GunkFollowerShouldShow(void);
void GunkFollowerUpdate(void);
void GunkFollowerOnMapLoad(void);
void GunkFollowerStripFromSave(struct ObjectEvent *objectEvents);
void GunkFollowerCry(void);

#endif // GUARD_GUNK_H

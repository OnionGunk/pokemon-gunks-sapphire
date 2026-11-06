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
#define GUNK_DEXNAV_LAND_MAX  12
#define GUNK_DEXNAV_WATER_MAX 5
#define GUNK_DEXNAV_WATER     16   // screen result: water entry n is GUNK_DEXNAV_WATER + n
#define GUNK_DEXNAV_CLOSED    0x7F // screen result: closed with B
struct GunkDexNavArea
{
    u16 land[GUNK_DEXNAV_LAND_MAX];
    u16 water[GUNK_DEXNAV_WATER_MAX];
    u8 landMin[GUNK_DEXNAV_LAND_MAX], landMax[GUNK_DEXNAV_LAND_MAX];
    u8 waterMin[GUNK_DEXNAV_WATER_MAX], waterMax[GUNK_DEXNAV_WATER_MAX];
    u8 landCount, waterCount;
};
void GunkDexNavPrepare(void);
const struct GunkDexNavArea *GunkDexNavGetArea(void);
u8 GunkDexNavSearchLevel(u16 species);
u8 GunkDexNavChain(u16 species);
u16 GunkDexNavTarget(bool8 *water);
s8 GunkDexNavOnlyBy(bool8 rockSmash, u16 *out, u8 max);
void GunkDexNavStart(void);
void GunkDexNavStartLast(void);
bool8 GunkDexNavSearchAgain(void);
void GunkDexNavUpdate(void);
bool8 GunkDexNavTryEncounter(void);
void GunkDexNavBattleEnded(u8 outcome);
u8 GunkDexNavHeldItemBonus(void);
void GunkDexNavShowScreen(void);

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
void GunkFollowerShowBall(struct ObjectEvent *follower, s16 x, s16 y);
void GunkFollowerHideForScene(void);

// Emote bubbles over an object (src/trainer_see.c)
#define GUNK_EMOTE_NONE        0
#define GUNK_EMOTE_EXCLAMATION 1
#define GUNK_EMOTE_QUESTION    2
#define GUNK_EMOTE_HEART       3
#define GUNK_EMOTE_NOTE        4
#define GUNK_EMOTE_DOTS        5
void GunkShowEmote(struct ObjectEvent *objectEvent, u8 emote);

// Area info for the DexNav screen (src/gunk.c, src/wild_encounter.c)
bool8 GunkInfo_GetOutbreak(u16 *species, u8 *mapGroup, u8 *mapNum, u8 *level, u16 *daysLeft);
u8 GunkInfo_CountRipeBerryTrees(void);
u8 GunkInfo_GetFeebasSpots(s16 *xs, s16 *ys);

// Quick Ball and Quick Run (src/gunk_quickball.c, battle_controller_player.c)
#define QUICK_BALL_NONE     0
#define QUICK_BALL_CONSUMED 1
#define QUICK_BALL_THROW    2
void GunkQuickBall_Show(void);
void GunkQuickBall_Hide(void);
u8 GunkQuickBall_HandleInput(void);
u16 GunkQuickBall_TakePendingThrow(void);
void GunkQuickBall_RememberBall(u16 item);

#endif // GUARD_GUNK_H

// Gunk's Ruby: the first Pokémon in the party follows the player, HeartGold style.
//
// Sprites and the follow logic are adapted from pokeemerald-expansion's overworld followers. Ruby's object event
// graphics ids are a u8 with few spare, so the follower uses one id (OBJ_EVENT_GFX_GUNK_FOLLOWER) whose graphics
// info lives in RAM and is filled for the current species. Its palette goes to sprite palette slot 10 (the slot
// for special objects); on maps that show such an object the follower stays in its Poké Ball.
//
// The follower hides while surfing, diving or biking, when the lead is an egg or fainted, and on maps where it
// would clash; it never blocks anyone (DoesObjectCollideWithObjectAt skips it) and is never saved with the map's
// objects. The movement type itself is MovementType_GunkFollow in event_object_movement.c.
#include "global.h"
#include "gunk.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "metatile_behavior.h"
#include "pokemon.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "text.h"
#include "constants/event_object_movement.h"
#include "constants/event_objects.h"
#include "constants/species.h"
#include "data2.h"

struct GunkFollowerGfx
{
    const struct SpriteFrameImage *pics;
    const u16 *palette;
    const u16 *shinyPalette;
    bool8 big;  // 64x64 frames (else 32x32)
    bool8 asym; // separate east frames (else west flipped)
};

#include "data/gunk_followers.h"

extern const struct OamData gFieldOamData_32x32;
extern const struct OamData gFieldOamData_64x64;
extern const struct SubspriteTable gObjectEventSubspriteTables_32x32[];
extern const struct SubspriteTable gObjectEventSubspriteTables_64x64[];

#define FACE(frame) {ANIMCMD_FRAME(frame, 16), ANIMCMD_JUMP(0)}
#define FACE_FLIP(frame) {ANIMCMD_FRAME(frame, 16, .hFlip = TRUE), ANIMCMD_JUMP(0)}
#define GO(a, b, t) {ANIMCMD_FRAME(a, t), ANIMCMD_FRAME(b, t), ANIMCMD_FRAME(b, t), ANIMCMD_FRAME(a, t), ANIMCMD_JUMP(0)}
#define GO_FLIP(a, b, t) {ANIMCMD_FRAME(a, t, .hFlip = TRUE), ANIMCMD_FRAME(b, t, .hFlip = TRUE), ANIMCMD_FRAME(b, t, .hFlip = TRUE), ANIMCMD_FRAME(a, t, .hFlip = TRUE), ANIMCMD_JUMP(0)}

static const union AnimCmd sAnim_FaceS[] = FACE(0);
static const union AnimCmd sAnim_FaceN[] = FACE(2);
static const union AnimCmd sAnim_FaceW[] = FACE(4);
static const union AnimCmd sAnim_FaceE[] = FACE_FLIP(4);
static const union AnimCmd sAnim_FaceEAsym[] = FACE(6);
static const union AnimCmd sAnim_GoS[] = GO(0, 1, 6);
static const union AnimCmd sAnim_GoN[] = GO(2, 3, 6);
static const union AnimCmd sAnim_GoW[] = GO(4, 5, 6);
static const union AnimCmd sAnim_GoE[] = GO_FLIP(4, 5, 6);
static const union AnimCmd sAnim_GoEAsym[] = GO(6, 7, 6);
static const union AnimCmd sAnim_GoFastS[] = GO(0, 1, 3);
static const union AnimCmd sAnim_GoFastN[] = GO(2, 3, 3);
static const union AnimCmd sAnim_GoFastW[] = GO(4, 5, 3);
static const union AnimCmd sAnim_GoFastE[] = GO_FLIP(4, 5, 3);
static const union AnimCmd sAnim_GoFastEAsym[] = GO(6, 7, 3);
static const union AnimCmd sAnim_GoFasterS[] = GO(0, 1, 2);
static const union AnimCmd sAnim_GoFasterN[] = GO(2, 3, 2);
static const union AnimCmd sAnim_GoFasterW[] = GO(4, 5, 2);
static const union AnimCmd sAnim_GoFasterE[] = GO_FLIP(4, 5, 2);
static const union AnimCmd sAnim_GoFasterEAsym[] = GO(6, 7, 2);
static const union AnimCmd sAnim_GoFastestS[] = GO(0, 1, 1);
static const union AnimCmd sAnim_GoFastestN[] = GO(2, 3, 1);
static const union AnimCmd sAnim_GoFastestW[] = GO(4, 5, 1);
static const union AnimCmd sAnim_GoFastestE[] = GO_FLIP(4, 5, 1);
static const union AnimCmd sAnim_GoFastestEAsym[] = GO(6, 7, 1);

// Same order as gObjectEventImageAnimTable_Standard: face, go, go fast, go faster, go fastest (S, N, W, E)
static const union AnimCmd *const sAnimTable_Follower[] = {
    sAnim_FaceS, sAnim_FaceN, sAnim_FaceW, sAnim_FaceE,
    sAnim_GoS, sAnim_GoN, sAnim_GoW, sAnim_GoE,
    sAnim_GoFastS, sAnim_GoFastN, sAnim_GoFastW, sAnim_GoFastE,
    sAnim_GoFasterS, sAnim_GoFasterN, sAnim_GoFasterW, sAnim_GoFasterE,
    sAnim_GoFastestS, sAnim_GoFastestN, sAnim_GoFastestW, sAnim_GoFastestE,
};

static const union AnimCmd *const sAnimTable_FollowerAsym[] = {
    sAnim_FaceS, sAnim_FaceN, sAnim_FaceW, sAnim_FaceEAsym,
    sAnim_GoS, sAnim_GoN, sAnim_GoW, sAnim_GoEAsym,
    sAnim_GoFastS, sAnim_GoFastN, sAnim_GoFastW, sAnim_GoFastEAsym,
    sAnim_GoFasterS, sAnim_GoFasterN, sAnim_GoFasterW, sAnim_GoFasterEAsym,
    sAnim_GoFastestS, sAnim_GoFastestN, sAnim_GoFastestW, sAnim_GoFastestEAsym,
};

EWRAM_DATA static struct ObjectEventGraphicsInfo sFollowerInfo = {0};
EWRAM_DATA u16 gGunkFollowerPalette[16] = {0};
EWRAM_DATA static u16 sFollowerSpecies = 0;
EWRAM_DATA static bool8 sFollowerShiny = FALSE;

const struct ObjectEventGraphicsInfo *GunkFollowerGraphicsInfo(void)
{
    return &sFollowerInfo;
}

// The party's first Pokémon if it should follow, else SPECIES_NONE
static u16 GetLead(bool8 *shiny)
{
    struct Pokemon *mon = &gPlayerParty[0];
    u16 species = GetMonData(mon, MON_DATA_SPECIES);

    if (species == SPECIES_NONE || species >= ARRAY_COUNT(sGunkFollowerGfx) || sGunkFollowerGfx[species].pics == NULL
     || GetMonData(mon, MON_DATA_IS_EGG) || GetMonData(mon, MON_DATA_HP) == 0)
        return SPECIES_NONE;
    *shiny = IsShiny(mon);
    return species;
}

static void FillGraphicsInfo(u16 species, bool8 shiny)
{
    const struct GunkFollowerGfx *gfx = &sGunkFollowerGfx[species];

    sFollowerInfo.tileTag = 0xFFFF;
    sFollowerInfo.paletteTag = GUNK_FOLLOWER_PAL_TAG;
    sFollowerInfo.bridgeReflectionPaletteTag = GUNK_FOLLOWER_PAL_TAG;
    sFollowerInfo.size = gfx->big ? 2048 : 512;
    sFollowerInfo.width = gfx->big ? 64 : 32;
    sFollowerInfo.height = gfx->big ? 64 : 32;
    sFollowerInfo.paletteSlot = 10;
    sFollowerInfo.shadowSize = SHADOW_SIZE_M;
    sFollowerInfo.inanimate = FALSE;
    sFollowerInfo.disableReflectionPaletteLoad = TRUE;
    sFollowerInfo.tracks = TRACKS_NONE;
    sFollowerInfo.oam = gfx->big ? &gFieldOamData_64x64 : &gFieldOamData_32x32;
    sFollowerInfo.subspriteTables = gfx->big ? gObjectEventSubspriteTables_64x64 : gObjectEventSubspriteTables_32x32;
    sFollowerInfo.anims = gfx->asym ? sAnimTable_FollowerAsym : sAnimTable_Follower;
    sFollowerInfo.images = gfx->pics;
    sFollowerInfo.affineAnims = gDummySpriteAffineAnimTable;
    CpuCopy16(shiny ? gfx->shinyPalette : gfx->palette, gGunkFollowerPalette, sizeof(gGunkFollowerPalette));
    sFollowerSpecies = species;
    sFollowerShiny = shiny;
}

static struct ObjectEvent *GetFollower(void)
{
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active && gObjectEvents[i].localId == GUNK_FOLLOWER_LOCAL_ID)
            return &gObjectEvents[i];
    }
    return NULL;
}

bool8 GunkIsFollower(struct ObjectEvent *objectEvent)
{
    return objectEvent->active && objectEvent->localId == GUNK_FOLLOWER_LOCAL_ID;
}

// Another object using palette slot 10 is on the map: the follower would recolour it
static bool8 SlotTenTaken(void)
{
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        struct ObjectEvent *obj = &gObjectEvents[i];
        if (obj->active && obj->localId != GUNK_FOLLOWER_LOCAL_ID
         && GetObjectEventGraphicsInfo(obj->graphicsId)->paletteSlot >= 10)
            return TRUE;
    }
    return FALSE;
}

// Whether the follower should be out right now (it stays in its ball, invisible, otherwise)
bool8 GunkFollowerShouldShow(void)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];

    if (TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING | PLAYER_AVATAR_FLAG_UNDERWATER
                            | PLAYER_AVATAR_FLAG_MACH_BIKE | PLAYER_AVATAR_FLAG_ACRO_BIKE))
        return FALSE;
    if (MetatileBehavior_IsSurfableWaterOrUnderwater(player->currentMetatileBehavior)
     || MetatileBehavior_IsForcedMovementTile(player->currentMetatileBehavior))
        return FALSE;
    return TRUE;
}

static void RemoveFollower(void)
{
    struct ObjectEvent *follower = GetFollower();

    if (follower != NULL)
        GunkRemoveObjectEvent(follower);
}

static void SpawnFollower(u16 species, bool8 shiny)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    struct ObjectEventTemplate template = {0};
    u8 id;

    FillGraphicsInfo(species, shiny);
    template.localId = GUNK_FOLLOWER_LOCAL_ID;
    template.graphicsId = OBJ_EVENT_GFX_GUNK_FOLLOWER;
    template.kind = OBJ_KIND_NORMAL;
    template.x = player->currentCoords.x - 7;
    template.y = player->currentCoords.y - 7;
    template.elevation = player->currentElevation;
    template.movementType = MOVEMENT_TYPE_GUNK_FOLLOW;
    id = SpawnSpecialObjectEvent(&template);
    if (id < OBJECT_EVENTS_COUNT)
    {
        gObjectEvents[id].invisible = TRUE; // comes out of its ball on the player's next step
        gObjectEvents[id].facingDirection = player->facingDirection;
    }
}

// Every overworld frame, and after the map's objects were spawned: keep the follower matching the party lead
void GunkFollowerUpdate(void)
{
    struct ObjectEvent *follower = GetFollower();
    bool8 shiny = FALSE;
    u16 species = GetLead(&shiny);

    if (species == SPECIES_NONE || SlotTenTaken())
    {
        RemoveFollower();
        return;
    }
    if (follower != NULL && (species != sFollowerSpecies || shiny != sFollowerShiny))
    {
        RemoveFollower();
        follower = NULL;
    }
    if (follower == NULL)
        SpawnFollower(species, shiny);
}

// A new map's objects were just spawned (warps, Continue): start without a follower and spawn a fresh one
void GunkFollowerOnMapLoad(void)
{
    RemoveFollower();
    GunkFollowerUpdate();
}

// SaveBlock1's copy of the objects must not keep the follower (it's respawned on Continue)
void GunkFollowerStripFromSave(struct ObjectEvent *objectEvents)
{
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (objectEvents[i].active && objectEvents[i].localId == GUNK_FOLLOWER_LOCAL_ID)
            objectEvents[i].active = FALSE;
    }
}

// Special for talking to the follower: its cry
void GunkFollowerCry(void)
{
    if (sFollowerSpecies != SPECIES_NONE)
        PlayCry1(sFollowerSpecies, 0);
    StringCopy(gStringVar1, gSpeciesNames[sFollowerSpecies]);
}

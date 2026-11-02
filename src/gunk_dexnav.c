// Gunk's Ruby: DexNav.
//
// The DEXNAV key item lists the grass Pokémon of the current area (seen ones by name, unseen as ?????, caught
// but not caught yet marked NEW). Picking one hides it in a rustling patch of tall grass 2-4 tiles away; stepping into the patch
// starts the battle. R searches again for the last pick. Each DexNav battle raises that species' search level
// (saved), and battling the same species again in a row builds a chain. Both give extra IV rolls and shiny rolls;
// every roll is a whole normal wild Pokémon (CreateMonWithNature), so whatever is kept is a legal wild Pokémon.
// Leaving the map, walking 8 tiles away or running from the battle ends the search; running also ends the chain.
#include "global.h"
#include "gunk.h"
#include "battle_setup.h"
#include "data2.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_effect.h"
#include "field_player_avatar.h"
#include "fieldmap.h"
#include "item.h"
#include "metatile_behavior.h"
#include "pokedex.h"
#include "pokemon.h"
#include "random.h"
#include "script.h"
#include "string_util.h"
#include "text.h"
#include "wild_encounter.h"
#include "constants/battle.h"
#include "constants/field_effects.h"
#include "constants/items.h"
#include "constants/species.h"

#define LAND_SLOTS 12
#define RUSTLE_FRAMES 40
#define MAX_DISTANCE 8

struct GunkDexNav
{
    u16 species;
    u16 lastSpecies;    // R searches again for this one
    u16 chainSpecies;
    s16 x, y;           // patch, in map grid coordinates
    s16 startX, startY; // player when the search began
    u8 mapGroup;
    u8 mapNum;
    u8 level;
    u8 chain;
    u8 active:1;
    u8 inBattle:1;
    u8 timer;
};

EWRAM_DATA static struct GunkDexNav sDexNav = {0};
EWRAM_DATA static u16 sMenuSpecies[GUNK_DEXNAV_MAX_CHOICES] = {0};
EWRAM_DATA static u8 sMenuText[GUNK_DEXNAV_MAX_CHOICES][16] = {0};

extern const u8 EventScript_GunkDexNavAgain[];

static const u8 sText_Unseen[] = _("????? "); // trailing spaces: the menu width measure clips the last letter
static const u8 sText_NotCaught[] = _(" NEW ");
static const u8 sText_Space[] = _(" ");
static const u8 sText_SearchLv[] = _("Search Lv. ");
static const u8 sText_Chain[] = _("  Chain ");

static const struct WildPokemonInfo *GetLandMons(void)
{
    u16 i;

    for (i = 0; gWildMonHeaders[i].mapGroup != 0xFF; i++)
    {
        if (gWildMonHeaders[i].mapGroup == gSaveBlock1.location.mapGroup
         && gWildMonHeaders[i].mapNum == gSaveBlock1.location.mapNum)
            return gWildMonHeaders[i].landMonsInfo;
    }
    return NULL;
}

static u8 *SearchLevel(u16 species)
{
    return &gSaveBlock2.gunkDexNavLevels[SpeciesToNationalPokedexNum(species)];
}

// Special: build the menu of this area's grass Pokémon; VAR_RESULT = how many (0: none here)
void GunkDexNavPrepare(void)
{
    const struct WildPokemonInfo *land = GetLandMons();
    u8 count = 0;
    u8 i, j;

    if (land != NULL)
    {
        for (i = 0; i < LAND_SLOTS && count < GUNK_DEXNAV_MAX_CHOICES; i++)
        {
            u16 species = land->wildPokemon[i].species;
            u16 dexNum = SpeciesToNationalPokedexNum(species);

            for (j = 0; j < count; j++)
            {
                if (sMenuSpecies[j] == species)
                    break;
            }
            if (j < count)
                continue;
            sMenuSpecies[count] = species;
            if (GetSetPokedexFlag(dexNum, FLAG_GET_SEEN))
            {
                u8 *end = StringCopy(sMenuText[count], gSpeciesNames[species]);
                StringCopy(end, GetSetPokedexFlag(dexNum, FLAG_GET_CAUGHT) ? sText_Space : sText_NotCaught);
            }
            else
            {
                StringCopy(sMenuText[count], sText_Unseen);
            }
            count++;
        }
    }
    gSpecialVar_Result = count;
}

u8 GunkDexNavMenuEntries(const u8 **texts)
{
    u8 i;

    for (i = 0; i < GUNK_DEXNAV_MAX_CHOICES && sMenuSpecies[i] != SPECIES_NONE; i++)
        texts[i] = sMenuText[i];
    return i;
}

static u8 PickLevel(u16 species)
{
    const struct WildPokemonInfo *land = GetLandMons();
    u8 min = 100, max = 1;
    u8 i;

    for (i = 0; i < LAND_SLOTS; i++)
    {
        const struct WildPokemon *mon = &land->wildPokemon[i];
        if (mon->species != species)
            continue;
        if (mon->minLevel < min)
            min = mon->minLevel;
        if (mon->maxLevel > max)
            max = mon->maxLevel;
    }
    if (max < min)
        max = min;
    return min + Random() % (max - min + 1);
}

static bool8 IsPatchTile(s16 x, s16 y)
{
    return MetatileBehavior_IsTallGrass(MapGridGetMetatileBehaviorAt(x, y))
        && MapGridGetCollisionAt(x, y) == 0
        && GetObjectEventIdByXY(x, y) == OBJECT_EVENTS_COUNT;
}

static bool8 StartSearch(u16 species)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    s16 px = player->currentCoords.x;
    s16 py = player->currentCoords.y;
    u8 tries;

    if (GetLandMons() == NULL)
        return FALSE;
    for (tries = 0; tries < 64; tries++)
    {
        s16 dx = (Random() % 9) - 4;
        s16 dy = (Random() % 9) - 4;
        s16 dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);

        if (dist < 2 || dist > 4 || !IsPatchTile(px + dx, py + dy))
            continue;
        if (species != sDexNav.chainSpecies)
        {
            sDexNav.chainSpecies = species;
            sDexNav.chain = 0;
        }
        sDexNav.species = species;
        sDexNav.lastSpecies = species;
        sDexNav.x = px + dx;
        sDexNav.y = py + dy;
        sDexNav.startX = px;
        sDexNav.startY = py;
        sDexNav.mapGroup = gSaveBlock1.location.mapGroup;
        sDexNav.mapNum = gSaveBlock1.location.mapNum;
        sDexNav.level = PickLevel(species);
        sDexNav.timer = 0;
        sDexNav.active = TRUE;
        sDexNav.inBattle = FALSE;
        return TRUE;
    }
    return FALSE;
}

// Strings for the search message: STR_VAR_1 name, STR_VAR_2 level, STR_VAR_3 search level and chain
static void BufferSearchStrings(void)
{
    u8 *s;

    StringCopy(gStringVar1, gSpeciesNames[sDexNav.species]);
    ConvertIntToDecimalStringN(gStringVar2, sDexNav.level, STR_CONV_MODE_LEFT_ALIGN, 3);
    s = StringCopy(gStringVar3, sText_SearchLv);
    s = ConvertIntToDecimalStringN(s, *SearchLevel(sDexNav.species), STR_CONV_MODE_LEFT_ALIGN, 3);
    s = StringCopy(s, sText_Chain);
    ConvertIntToDecimalStringN(s, sDexNav.chain, STR_CONV_MODE_LEFT_ALIGN, 3);
}

// Special: start a search for menu entry VAR_RESULT; VAR_RESULT = TRUE when a patch was found
void GunkDexNavStart(void)
{
    u16 index = gSpecialVar_Result;

    gSpecialVar_Result = FALSE;
    if (index >= GUNK_DEXNAV_MAX_CHOICES || sMenuSpecies[index] == SPECIES_NONE)
        return;
    if (StartSearch(sMenuSpecies[index]))
    {
        BufferSearchStrings();
        gSpecialVar_Result = TRUE;
    }
}

// Special: search again for the last pick (R); VAR_RESULT = TRUE when a patch was found
void GunkDexNavStartLast(void)
{
    gSpecialVar_Result = FALSE;
    if (StartSearch(sDexNav.lastSpecies))
    {
        BufferSearchStrings();
        gSpecialVar_Result = TRUE;
    }
}

// Field input: R searches again for the last pick in an area that has it
bool8 GunkDexNavSearchAgain(void)
{
    const struct WildPokemonInfo *land = GetLandMons();
    u8 i;

    if (sDexNav.lastSpecies == SPECIES_NONE || land == NULL || !CheckBagHasItem(ITEM_DEXNAV, 1))
        return FALSE;
    for (i = 0; i < LAND_SLOTS; i++)
    {
        if (land->wildPokemon[i].species == sDexNav.lastSpecies)
        {
            LockPlayerFieldControls();
            ScriptContext_SetupScript(EventScript_GunkDexNavAgain);
            return TRUE;
        }
    }
    return FALSE;
}

// Every overworld frame: rustle the patch, and end the search when the player leaves
void GunkDexNavUpdate(void)
{
    struct ObjectEvent *player;
    s16 dx, dy;

    if (!sDexNav.active || sDexNav.inBattle)
        return;
    if (gSaveBlock1.location.mapGroup != sDexNav.mapGroup || gSaveBlock1.location.mapNum != sDexNav.mapNum)
    {
        sDexNav.active = FALSE;
        return;
    }
    player = &gObjectEvents[gPlayerAvatar.objectEventId];
    dx = player->currentCoords.x - sDexNav.startX;
    dy = player->currentCoords.y - sDexNav.startY;
    if ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) > MAX_DISTANCE)
    {
        sDexNav.active = FALSE;
        return;
    }
    if (++sDexNav.timer < RUSTLE_FRAMES)
        return;
    sDexNav.timer = 0;
    // the tall grass "walked through" animation, tied to the player object so it ends by itself
    gFieldEffectArguments[0] = sDexNav.x;
    gFieldEffectArguments[1] = sDexNav.y;
    gFieldEffectArguments[2] = player->previousElevation;
    gFieldEffectArguments[3] = 2;
    gFieldEffectArguments[4] = player->localId << 8 | player->mapNum;
    gFieldEffectArguments[5] = player->mapGroup;
    gFieldEffectArguments[6] = (u8)gSaveBlock1.location.mapNum << 8 | (u8)gSaveBlock1.location.mapGroup;
    gFieldEffectArguments[7] = 0;
    FieldEffectStart(FLDEFF_TALL_GRASS);
}

static u16 IvTotal(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_HP_IV) + GetMonData(mon, MON_DATA_ATK_IV) + GetMonData(mon, MON_DATA_DEF_IV)
         + GetMonData(mon, MON_DATA_SPEED_IV) + GetMonData(mon, MON_DATA_SPATK_IV) + GetMonData(mon, MON_DATA_SPDEF_IV);
}

// Each step: stepping into the patch starts the battle
bool8 GunkDexNavTryEncounter(void)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    u8 *searchLevel;
    u8 ivRolls, shinyRolls, rolls, i;
    u16 best = 0;

    if (!sDexNav.active || sDexNav.inBattle
     || gSaveBlock1.location.mapGroup != sDexNav.mapGroup || gSaveBlock1.location.mapNum != sDexNav.mapNum
     || player->currentCoords.x != sDexNav.x || player->currentCoords.y != sDexNav.y)
        return FALSE;

    searchLevel = SearchLevel(sDexNav.species);
    ivRolls = 1 + *searchLevel / 2 + 2 * sDexNav.chain;
    shinyRolls = 1 + *searchLevel / 10 + sDexNav.chain / 5;
    if (ivRolls > 20)
        ivRolls = 20;
    if (shinyRolls > 10)
        shinyRolls = 10;
    rolls = ivRolls > shinyRolls ? ivRolls : shinyRolls;

    ZeroEnemyPartyMons();
    for (i = 0; i < rolls; i++)
    {
        // gEnemyParty[1] is free in a wild battle: roll there, keep the best in slot 0
        CreateMonWithNature(&gEnemyParty[1], sDexNav.species, sDexNav.level, 32, Random() % 25);
        if (i < shinyRolls && IsShiny(&gEnemyParty[1]))
        {
            gEnemyParty[0] = gEnemyParty[1];
            break;
        }
        if (i < ivRolls && (i == 0 || IvTotal(&gEnemyParty[1]) > best))
        {
            gEnemyParty[0] = gEnemyParty[1];
            best = IvTotal(&gEnemyParty[0]);
        }
    }
    ZeroMonData(&gEnemyParty[1]);

    if (*searchLevel < 255)
        (*searchLevel)++;
    sDexNav.inBattle = TRUE;
    BattleSetup_StartWildBattle();
    return TRUE;
}

// End of a wild battle: count the chain, or end it when the player ran or the Pokémon fled
void GunkDexNavBattleEnded(u8 outcome)
{
    if (!sDexNav.inBattle)
        return;
    sDexNav.inBattle = FALSE;
    sDexNav.active = FALSE;
    if (outcome == B_OUTCOME_WON || outcome == B_OUTCOME_CAUGHT)
    {
        if (sDexNav.chain < 255)
            sDexNav.chain++;
    }
    else
    {
        sDexNav.chain = 0;
    }
}

// Gunk's Ruby: DexNav.
//
// The DEXNAV key item opens the DexNav screen (src/gunk_dexnav_screen.c): the area's grass and surfing Pokémon.
// Picking one hides it in a rustling patch of tall grass, or rippling water when surfing, 2-4 tiles away; stepping
// into the patch starts the battle. R searches again for the last pick. Each DexNav battle raises that species' search level
// (saved), and battling the same species again in a row builds a chain. Both give extra IV rolls and shiny rolls;
// every roll makes a whole normal wild Pokémon's RNG calls (CreateMonWithNature), so whatever is kept is a legal wild
// Pokémon.
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
#include "map_name_popup.h"
#include "menu.h"
#include "pokemon_summary_screen.h"
#include "task.h"
#include "metatile_behavior.h"
#include "pokedex.h"
#include "pokemon.h"
#include "random.h"
#include "safari_zone.h"
#include "script.h"
#include "string_util.h"
#include "text.h"
#include "wild_encounter.h"
#include "constants/battle.h"
#include "constants/field_effects.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/species.h"

#define LAND_SLOTS 12
#define WATER_SLOTS 5
#define ROCK_SMASH_SLOTS 5
#define FISHING_SLOTS 10
#define RUSTLE_FRAMES 30  // the patch shakes twice a second
#define SEARCH_FRAMES 600 // a search lasts 10 seconds of field time (the player free to move)
#define MAX_DISTANCE 8

extern const u8 EventScript_GunkDexNavGotAway[];

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
    u8 water:1;         // surfing search (patch on water)
    u8 lastWater:1;
    u8 timer;
    u16 age;            // field frames since the search began
    u32 personality;    // the Pokémon in the patch, rolled when the search starts
    u16 ivs1, ivs2;
    u8 shiny:1;
    u8 barTaskId;       // the search bar on the field (0xFF: none)
};

EWRAM_DATA static struct GunkDexNav sDexNav = {0};
EWRAM_DATA static struct GunkDexNavArea sArea = {0};

extern const u8 EventScript_GunkDexNavAgain[];
extern const u8 EventScript_GunkDexNavNoTarget[];
extern const u8 EventScript_GunkDexNavNotHere[];

static const u8 sText_SearchLv[] = _("Search Lv. ");
static const u8 sText_Chain[] = _("  Chain ");
static const u8 sText_BarLv[] = _("  Lv. ");
static const u8 sText_BarShiny[] = _("  SHINY");
static const u8 sText_BarSearchLv[] = _("Search Lv. ");
static const u8 sText_BarChain[] = _("Chain ");
static const u8 sText_BarMaxIvs[] = _("  Max IVs ");
static const u8 sText_Space2[] = _(" ");

static void RollMon(void);
static void SearchBar_Start(void);
static void SearchBar_Keep(void);

static const struct WildPokemonHeader *GetAreaHeader(void)
{
    u16 i;

    for (i = 0; gWildMonHeaders[i].mapGroup != 0xFF; i++)
    {
        if (gWildMonHeaders[i].mapGroup == gSaveBlock1.location.mapGroup
         && gWildMonHeaders[i].mapNum == gSaveBlock1.location.mapNum)
            return &gWildMonHeaders[i];
    }
    return NULL;
}

static const struct WildPokemonInfo *GetAreaMons(bool8 water)
{
    const struct WildPokemonHeader *header = GetAreaHeader();

    if (header == NULL)
        return NULL;
    return water ? header->waterMonsInfo : header->landMonsInfo;
}

static bool8 PlayerSurfing(void)
{
    return TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING) != 0;
}

static u8 *SearchLevel(u16 species)
{
    return &gSaveBlock2.gunkDexNavLevels[SpeciesToNationalPokedexNum(species)];
}

// The area's species in slot order, each once, with the lowest and highest level of its slots
static u8 ListSpecies(const struct WildPokemonInfo *info, u8 slots, u16 *species, u8 *minLevel, u8 *maxLevel)
{
    u8 count = 0;
    u8 i, j;

    if (info == NULL)
        return 0;
    for (i = 0; i < slots; i++)
    {
        const struct WildPokemon *mon = &info->wildPokemon[i];
        u8 lo = mon->minLevel < mon->maxLevel ? mon->minLevel : mon->maxLevel;
        u8 hi = mon->minLevel < mon->maxLevel ? mon->maxLevel : mon->minLevel;

        for (j = 0; j < count; j++)
        {
            if (species[j] == mon->species)
                break;
        }
        if (j == count)
        {
            species[count] = mon->species;
            minLevel[count] = lo;
            maxLevel[count] = hi;
            count++;
            continue;
        }
        if (lo < minLevel[j])
            minLevel[j] = lo;
        if (hi > maxLevel[j])
            maxLevel[j] = hi;
    }
    return count;
}

// Special: list this area's grass and surfing Pokémon; VAR_RESULT = how many (0: none here)
void GunkDexNavPrepare(void)
{
    sArea.landCount = ListSpecies(GetAreaMons(FALSE), LAND_SLOTS, sArea.land, sArea.landMin, sArea.landMax);
    sArea.waterCount = ListSpecies(GetAreaMons(TRUE), WATER_SLOTS, sArea.water, sArea.waterMin, sArea.waterMax);
    gSpecialVar_Result = sArea.landCount + sArea.waterCount;
}

const struct GunkDexNavArea *GunkDexNavGetArea(void)
{
    return &sArea;
}

// The area's Pokémon found only with a rod (or only by Rock Smash): not in its grass or surfing lists, so the
// DexNav can't search for them. Fills out[] (each species once) and returns how many; -1 if there are no such
// encounters here at all. Needs GunkDexNavPrepare first.
s8 GunkDexNavOnlyBy(bool8 rockSmash, u16 *out, u8 max)
{
    const struct WildPokemonHeader *header = GetAreaHeader();
    const struct WildPokemonInfo *info;
    u8 slots = rockSmash ? ROCK_SMASH_SLOTS : FISHING_SLOTS;
    u8 count = 0;
    u8 i, j;

    if (header == NULL)
        return -1;
    info = rockSmash ? header->rockSmashMonsInfo : header->fishingMonsInfo;
    if (info == NULL)
        return -1;
    for (i = 0; i < slots && count < max; i++)
    {
        u16 species = info->wildPokemon[i].species;

        for (j = 0; j < sArea.landCount && sArea.land[j] != species; j++)
            ;
        if (j < sArea.landCount)
            continue;
        for (j = 0; j < sArea.waterCount && sArea.water[j] != species; j++)
            ;
        if (j < sArea.waterCount)
            continue;
        for (j = 0; j < count && out[j] != species; j++)
            ;
        if (j == count)
            out[count++] = species;
    }
    return count;
}

u8 GunkDexNavSearchLevel(u16 species)
{
    return *SearchLevel(species);
}

u8 GunkDexNavChain(u16 species)
{
    return species == sDexNav.chainSpecies ? sDexNav.chain : 0;
}

// The registered target (R searches for it), SPECIES_NONE before the first pick
u16 GunkDexNavTarget(bool8 *water)
{
    *water = sDexNav.lastWater;
    return sDexNav.lastSpecies;
}

// A level the species can have here: one of its slots, then a level in that slot's range (as a wild encounter
// rolls it). The lowest-to-highest range over all its slots would allow levels between slots (e.g. 25 for a
// species found at 24 and 26), which no wild encounter gives and legality checkers reject.
static u8 PickLevel(u16 species, bool8 water)
{
    const struct WildPokemonInfo *info = GetAreaMons(water);
    const struct WildPokemon *mon;
    u8 slots[LAND_SLOTS];
    u8 count = 0;
    u8 i, min, max;

    for (i = 0; i < (water ? WATER_SLOTS : LAND_SLOTS); i++)
    {
        if (info->wildPokemon[i].species == species)
            slots[count++] = i;
    }
    if (count == 0)
        return 5;
    mon = &info->wildPokemon[slots[Random() % count]];
    min = mon->minLevel;
    max = mon->maxLevel;
    if (max < min)
    {
        u8 temp = min;
        min = max;
        max = temp;
    }
    return min + Random() % (max - min + 1);
}

static bool8 IsPatchTile(s16 x, s16 y, bool8 water)
{
    u8 behavior = MapGridGetMetatileBehaviorAt(x, y);

    return (water ? MetatileBehavior_IsSurfableWaterOrUnderwater(behavior) : MetatileBehavior_IsTallGrass(behavior))
        && MapGridGetCollisionAt(x, y) == 0
        && GetObjectEventIdByXY(x, y) == OBJECT_EVENTS_COUNT;
}

// A grass search needs the player on land, a water search needs them surfing
static bool8 StartSearch(u16 species, bool8 water)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    s16 px = player->currentCoords.x;
    s16 py = player->currentCoords.y;
    u8 tries;

    if (GetAreaMons(water) == NULL || PlayerSurfing() != water)
        return FALSE;
    for (tries = 0; tries < 64; tries++)
    {
        s16 dx = (Random() % 9) - 4;
        s16 dy = (Random() % 9) - 4;
        s16 dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);

        if (dist < 2 || dist > 4 || !IsPatchTile(px + dx, py + dy, water))
            continue;
        if (species != sDexNav.chainSpecies)
        {
            sDexNav.chainSpecies = species;
            sDexNav.chain = 0;
        }
        sDexNav.species = species;
        sDexNav.lastSpecies = species;
        sDexNav.water = water;
        sDexNav.lastWater = water;
        sDexNav.x = px + dx;
        sDexNav.y = py + dy;
        sDexNav.startX = px;
        sDexNav.startY = py;
        sDexNav.mapGroup = gSaveBlock1.location.mapGroup;
        sDexNav.mapNum = gSaveBlock1.location.mapNum;
        sDexNav.level = PickLevel(species, water);
        sDexNav.timer = 0;
        sDexNav.age = 0;
        sDexNav.active = TRUE;
        sDexNav.inBattle = FALSE;
        RollMon();
        SearchBar_Start();
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

// VAR_RESULT after a search starts (event_scripts.s EventScript_GunkDexNavResult)
#define SEARCH_NO_GRASS    0 // on land, no tall grass close enough
#define SEARCH_GRASS       1
#define SEARCH_NEEDS_SURF  2 // a water Pokémon, the player on land
#define SEARCH_WATER       3
#define SEARCH_SAFARI      4
#define SEARCH_NO_WATER    5 // surfing, no open water close enough
#define SEARCH_NEEDS_LAND  6 // a grass Pokémon, the player surfing

static void TrySearch(u16 species, bool8 water)
{
    // No searches in the Safari Zone: its POKéMON must come from Safari encounters (Safari Ball, Safari rules)
    if (GetSafariZoneFlag())
        gSpecialVar_Result = SEARCH_SAFARI;
    else if (PlayerSurfing() != water)
        gSpecialVar_Result = water ? SEARCH_NEEDS_SURF : SEARCH_NEEDS_LAND;
    else if (StartSearch(species, water))
    {
        BufferSearchStrings();
        gSpecialVar_Result = water ? SEARCH_WATER : SEARCH_GRASS;
    }
    else
    {
        gSpecialVar_Result = water ? SEARCH_NO_WATER : SEARCH_NO_GRASS;
    }
}

// Special: start a search for the screen's pick in VAR_RESULT (GUNK_DEXNAV_WATER + n: water entry n)
void GunkDexNavStart(void)
{
    u16 index = gSpecialVar_Result;

    if (index >= GUNK_DEXNAV_WATER && index - GUNK_DEXNAV_WATER < sArea.waterCount)
        TrySearch(sArea.water[index - GUNK_DEXNAV_WATER], TRUE);
    else if (index < sArea.landCount)
        TrySearch(sArea.land[index], FALSE);
    else
        gSpecialVar_Result = SEARCH_NO_GRASS;
}

// Special: search again for the last pick (R)
void GunkDexNavStartLast(void)
{
    TrySearch(sDexNav.lastSpecies, sDexNav.lastWater);
}

// Field input: R searches again for the last pick in an area that has it (on its terrain: grass or water)
bool8 GunkDexNavSearchAgain(void)
{
    const struct WildPokemonInfo *info = GetAreaMons(sDexNav.lastWater);
    u8 i;

    if (!CheckBagHasItem(ITEM_DEXNAV, 1))
        return FALSE;
    LockPlayerFieldControls();
    if (sDexNav.lastSpecies == SPECIES_NONE)
    {
        ScriptContext_SetupScript(EventScript_GunkDexNavNoTarget);
        return TRUE;
    }
    for (i = 0; info != NULL && i < (sDexNav.lastWater ? WATER_SLOTS : LAND_SLOTS); i++)
    {
        if (info->wildPokemon[i].species == sDexNav.lastSpecies)
        {
            ScriptContext_SetupScript(EventScript_GunkDexNavAgain);
            return TRUE;
        }
    }
    StringCopy(gStringVar1, gSpeciesNames[sDexNav.lastSpecies]);
    ScriptContext_SetupScript(EventScript_GunkDexNavNotHere);
    return TRUE;
}

// Every overworld frame: rustle the patch, and end the search when the player leaves
void GunkDexNavUpdate(void)
{
    struct ObjectEvent *player;
    s16 dx, dy;

    if (!sDexNav.active || sDexNav.inBattle)
        return;
    SearchBar_Keep();
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
    // 10 seconds: the Pokémon got away and the chain ends (the message waits until the player stands still)
    if (!ArePlayerFieldControlsLocked() && sDexNav.age < SEARCH_FRAMES)
        sDexNav.age++;
    if (sDexNav.age >= SEARCH_FRAMES)
    {
        if (ArePlayerFieldControlsLocked() || gPlayerAvatar.tileTransitionState != T_NOT_MOVING)
            return;
        sDexNav.active = FALSE;
        sDexNav.chain = 0;
        StringCopy(gStringVar1, gSpeciesNames[sDexNav.species]);
        LockPlayerFieldControls();
        ScriptContext_SetupScript(EventScript_GunkDexNavGotAway);
        return;
    }
    if (++sDexNav.timer < RUSTLE_FRAMES)
        return;
    sDexNav.timer = 0;
    if (sDexNav.shiny)
    {
        // a shiny one sparkles as well
        gFieldEffectArguments[0] = sDexNav.x - 7;
        gFieldEffectArguments[1] = sDexNav.y - 7;
        gFieldEffectArguments[2] = 1;
        FieldEffectStart(FLDEFF_SPARKLE);
    }
    if (sDexNav.water)
    {
        s16 x = sDexNav.x, y = sDexNav.y;

        // a ripple on the water, as a Pokémon under the surface makes
        sub_8060470(&x, &y, 8, 8);
        gFieldEffectArguments[0] = x;
        gFieldEffectArguments[1] = y + 4;
        gFieldEffectArguments[2] = 151;
        gFieldEffectArguments[3] = 3;
        FieldEffectStart(FLDEFF_RIPPLE);
        return;
    }
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

bool8 IsShinyOtIdPersonality(u32 otId, u32 personality);

static u16 IvTotal(u16 ivs1, u16 ivs2)
{
    return (ivs1 & 0x1F) + ((ivs1 >> 5) & 0x1F) + ((ivs1 >> 10) & 0x1F)
         + (ivs2 & 0x1F) + ((ivs2 >> 5) & 0x1F) + ((ivs2 >> 10) & 0x1F);
}

// The IVs CreateMon draws from two IV words, then the stats for them (full HP: it was full at all IVs 0)
static void SetDexNavIvs(struct Pokemon *mon, u16 ivs1, u16 ivs2)
{
    u32 iv;

    iv = ivs1 & 0x1F;
    SetMonData(mon, MON_DATA_HP_IV, &iv);
    iv = (ivs1 >> 5) & 0x1F;
    SetMonData(mon, MON_DATA_ATK_IV, &iv);
    iv = (ivs1 >> 10) & 0x1F;
    SetMonData(mon, MON_DATA_DEF_IV, &iv);
    iv = ivs2 & 0x1F;
    SetMonData(mon, MON_DATA_SPEED_IV, &iv);
    iv = (ivs2 >> 5) & 0x1F;
    SetMonData(mon, MON_DATA_SPATK_IV, &iv);
    iv = (ivs2 >> 10) & 0x1F;
    SetMonData(mon, MON_DATA_SPDEF_IV, &iv);
    CalculateMonStats(mon);
}

// When the search starts: roll the Pokémon in the patch. Building a whole Pokémon per roll took up to 45 frames at
// 20 rolls; instead each roll makes exactly the RNG calls CreateMonWithNature(..., 32, Random() % 25) makes (nature,
// personality until it has that nature, two IV words) and only the winner is kept, to be built on stepping in
// (CreateMon with fixed IVs draws nothing). Rolling here means a shiny shown by the patch's sparkle is the one met.
static void RollMon(void)
{
    u8 searchLevel = *SearchLevel(sDexNav.species);
    u8 ivRolls, shinyRolls, rolls, i, nature;
    u16 best = 0, ivs1 = 0, ivs2 = 0;
    u32 otId, personality = 0;

    ivRolls = 1 + searchLevel / 2 + 2 * sDexNav.chain;
    shinyRolls = 1 + searchLevel / 10 + sDexNav.chain / 5;
    if (ivRolls > 20)
        ivRolls = 20;
    if (shinyRolls > 10)
        shinyRolls = 10;
    rolls = ivRolls > shinyRolls ? ivRolls : shinyRolls;
    otId = gSaveBlock2.playerTrainerId[0] | (gSaveBlock2.playerTrainerId[1] << 8)
         | (gSaveBlock2.playerTrainerId[2] << 16) | (gSaveBlock2.playerTrainerId[3] << 24);
    sDexNav.shiny = FALSE;
    for (i = 0; i < rolls; i++)
    {
        u32 rollPersonality;
        u16 rollIvs1, rollIvs2;

        nature = Random() % 25;
        do
        {
            rollPersonality = Random32();
        }
        while (nature != GetNatureFromPersonality(rollPersonality));
        rollIvs1 = Random();
        rollIvs2 = Random();
        if (i < shinyRolls && IsShinyOtIdPersonality(otId, rollPersonality))
        {
            personality = rollPersonality, ivs1 = rollIvs1, ivs2 = rollIvs2;
            sDexNav.shiny = TRUE;
            break;
        }
        if (i < ivRolls && (i == 0 || IvTotal(rollIvs1, rollIvs2) > best))
        {
            personality = rollPersonality, ivs1 = rollIvs1, ivs2 = rollIvs2;
            best = IvTotal(ivs1, ivs2);
        }
    }
    sDexNav.personality = personality;
    sDexNav.ivs1 = ivs1;
    sDexNav.ivs2 = ivs2;
}

// Each step: stepping into the patch starts the battle with the Pokémon rolled for it
bool8 GunkDexNavTryEncounter(void)
{
    struct ObjectEvent *player = &gObjectEvents[gPlayerAvatar.objectEventId];
    u8 *searchLevel;

    if (!sDexNav.active || sDexNav.inBattle || GetSafariZoneFlag()
     || gSaveBlock1.location.mapGroup != sDexNav.mapGroup || gSaveBlock1.location.mapNum != sDexNav.mapNum
     || player->currentCoords.x != sDexNav.x || player->currentCoords.y != sDexNav.y)
        return FALSE;

    ZeroEnemyPartyMons();
    CreateMon(&gEnemyParty[0], sDexNav.species, sDexNav.level, 0, TRUE, sDexNav.personality, 0, 0);
    SetDexNavIvs(&gEnemyParty[0], sDexNav.ivs1, sDexNav.ivs2);

    searchLevel = SearchLevel(sDexNav.species);
    if (*searchLevel < 255)
        (*searchLevel)++;
    sDexNav.inBattle = TRUE;
    BattleSetup_StartWildBattle();
    return TRUE;
}

// SetWildMonHeldItem: extra percentage points for a held item in a DexNav battle (search level and chain; a lead
// with Compound Eyes adds more). 0 outside DexNav battles, so other wild battles are vanilla.
u8 GunkDexNavHeldItemBonus(void)
{
    u8 bonus;

    if (!sDexNav.inBattle)
        return 0;
    bonus = *SearchLevel(sDexNav.species) / 10 + sDexNav.chain;
    if (!GetMonData(&gPlayerParty[0], MON_DATA_IS_EGG) && GetMonAbility(&gPlayerParty[0]) == ABILITY_COMPOUND_EYES)
        bonus += 10;
    return bonus > 30 ? 30 : bonus;
}

// End of a wild battle. A DexNav battle won or caught adds to the chain; running from it or losing ends the chain.
// Any other wild battle keeps the chain when it was the chained species and ends it otherwise.
void GunkDexNavBattleEnded(u8 outcome)
{
    if (!sDexNav.inBattle)
    {
        if (GetMonData(&gEnemyParty[0], MON_DATA_SPECIES) != sDexNav.chainSpecies)
            sDexNav.chain = 0;
        return;
    }
    sDexNav.inBattle = FALSE;
    sDexNav.active = FALSE;
    // Beating or catching it grows the chain; running (or anything else) ends it
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

// The search bar: while a search runs and the field is free, the top of the screen shows the Pokémon in the patch
// (name, level, SHINY), its nature and ability, the search level, the chain and how many of its IVs are 31.
// It goes away while a script, message or menu has the field, and when the search ends.
#define tDrawn data[0]

static void SearchBar_Draw(void)
{
    u8 ivs[6];
    u8 i, maxIvs = 0;
    u8 *s;

    HideMapNamePopup();
    Menu_LoadStdFrameGraphicsOverrideStyle(0);
    Menu_DrawStdWindowFrame(0, 0, 29, 5);
    s = StringCopy(gStringVar4, gSpeciesNames[sDexNav.species]);
    s = StringCopy(s, sText_BarLv);
    s = ConvertIntToDecimalStringN(s, sDexNav.level, STR_CONV_MODE_LEFT_ALIGN, 3);
    if (sDexNav.shiny)
        s = StringCopy(s, sText_BarShiny);
    Menu_PrintText(gStringVar4, 1, 1);
    s = StringCopy(gStringVar4, sText_BarSearchLv);
    ConvertIntToDecimalStringN(s, *SearchLevel(sDexNav.species), STR_CONV_MODE_LEFT_ALIGN, 3);
    Menu_PrintText(gStringVar4, 16, 1);
    s = StringCopy(gStringVar4, gNatureNames[GetNatureFromPersonality(sDexNav.personality)]);
    s = StringCopy(s, sText_Space2);
    StringCopy(s, gAbilityNames[GetAbilityBySpecies(sDexNav.species, sDexNav.personality & 1)]);
    Menu_PrintText(gStringVar4, 1, 3);
    ivs[0] = sDexNav.ivs1 & 0x1F, ivs[1] = (sDexNav.ivs1 >> 5) & 0x1F, ivs[2] = (sDexNav.ivs1 >> 10) & 0x1F;
    ivs[3] = sDexNav.ivs2 & 0x1F, ivs[4] = (sDexNav.ivs2 >> 5) & 0x1F, ivs[5] = (sDexNav.ivs2 >> 10) & 0x1F;
    for (i = 0; i < 6; i++)
        maxIvs += ivs[i] == 31;
    s = StringCopy(gStringVar4, sText_BarChain);
    s = ConvertIntToDecimalStringN(s, sDexNav.chain, STR_CONV_MODE_LEFT_ALIGN, 3);
    s = StringCopy(s, sText_BarMaxIvs);
    ConvertIntToDecimalStringN(s, maxIvs, STR_CONV_MODE_LEFT_ALIGN, 1);
    Menu_PrintText(gStringVar4, 16, 3);
}

static void Task_SearchBar(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    bool8 show = sDexNav.active && !sDexNav.inBattle && !ArePlayerFieldControlsLocked();

    if (show && !task->tDrawn)
    {
        SearchBar_Draw();
        task->tDrawn = TRUE;
    }
    else if (!show && task->tDrawn)
    {
        Menu_EraseWindowRect(0, 0, 29, 5);
        task->tDrawn = FALSE;
    }
    if (!sDexNav.active)
    {
        if (task->tDrawn)
            Menu_EraseWindowRect(0, 0, 29, 5);
        sDexNav.barTaskId = 0xFF;
        DestroyTask(taskId);
    }
}

static void SearchBar_Start(void)
{
    if (sDexNav.barTaskId != 0xFF && gTasks[sDexNav.barTaskId].isActive
     && gTasks[sDexNav.barTaskId].func == Task_SearchBar)
    {
        gTasks[sDexNav.barTaskId].tDrawn = FALSE; // redraw for the new search
        return;
    }
    sDexNav.barTaskId = CreateTask(Task_SearchBar, 90);
    gTasks[sDexNav.barTaskId].tDrawn = FALSE;
}

// Every overworld frame (from GunkDexNavUpdate): a map load resets the tasks, so the bar comes back with the field
static void SearchBar_Keep(void)
{
    if (sDexNav.barTaskId == 0xFF || !gTasks[sDexNav.barTaskId].isActive
     || gTasks[sDexNav.barTaskId].func != Task_SearchBar)
    {
        sDexNav.barTaskId = CreateTask(Task_SearchBar, 90);
        gTasks[sDexNav.barTaskId].tDrawn = FALSE;
    }
}

#undef tDrawn

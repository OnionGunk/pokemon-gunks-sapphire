#include "global.h"
#include "gunk.h"
#include "event_data.h"
#include "item.h"
#include "pokemon.h"
#include "party_menu.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "string_util.h"
#include "constants/items.h"
#include "constants/vars.h"

bool8 GunkSettingOn(u16 bit)
{
    return (VarGet(VAR_GUNK_SETTINGS) & bit) != 0;
}

void GunkToggleSetting(u16 bit)
{
    VarSet(VAR_GUNK_SETTINGS, VarGet(VAR_GUNK_SETTINGS) ^ bit);
}

// Repel prompt (special): TRUE when another Repel of the kind last used is in the bag; its name goes to gStringVar2
u16 GunkCheckLastRepel(void)
{
    u16 item = VarGet(VAR_GUNK_LAST_REPEL);

    if (item != ITEM_REPEL && item != ITEM_SUPER_REPEL && item != ITEM_MAX_REPEL)
        return FALSE;
    if (!CheckBagHasItem(item, 1))
        return FALSE;
    CopyItemName(item, gStringVar2);
    return TRUE;
}

// Repel prompt (special): use one more of the Repel kind last used
void GunkUseLastRepel(void)
{
    u16 item = VarGet(VAR_GUNK_LAST_REPEL);

    RemoveBagItem(item, 1);
    VarSet(VAR_REPEL_STEP_COUNT, ItemId_GetHoldEffectParam(item));
}

// HMs without teaching: a Pokémon that could learn an HM the player owns can use its field move (badges are
// still checked where the game checks them)
static const u16 sGunkHMMoves[][2] =
{
    {MOVE_CUT,        ITEM_HM01_CUT},
    {MOVE_FLY,        ITEM_HM02_FLY},
    {MOVE_SURF,       ITEM_HM03_SURF},
    {MOVE_STRENGTH,   ITEM_HM04_STRENGTH},
    {MOVE_FLASH,      ITEM_HM05_FLASH},
    {MOVE_ROCK_SMASH, ITEM_HM06_ROCK_SMASH},
    {MOVE_WATERFALL,  ITEM_HM07_WATERFALL},
    {MOVE_DIVE,       ITEM_HM08_DIVE},
};

bool8 GunkCanUseFieldMoveUntaught(struct Pokemon *mon, u16 move)
{
    u8 i;

    if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
        return FALSE;
    for (i = 0; i < ARRAY_COUNT(sGunkHMMoves); i++)
    {
        if (sGunkHMMoves[i][0] == move)
            return CheckBagHasItem(sGunkHMMoves[i][1], 1) && CanMonLearnTMHM(mon, sGunkHMMoves[i][1] - ITEM_TM01_FOCUS_PUNCH);
    }
    return FALSE;
}

bool8 GunkCanUseFieldMove(struct Pokemon *mon, u16 move)
{
    return pokemon_has_move(mon, move) || GunkCanUseFieldMoveUntaught(mon, move);
}

#include "global.h"
#include "gunk.h"
#include "event_data.h"
#include "item.h"
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

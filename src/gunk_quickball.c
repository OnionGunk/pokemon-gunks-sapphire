#include "global.h"
#include "battle.h"
#include "event_data.h"
#include "gunk.h"
#include "item.h"
#include "main.h"
#include "pokeball.h"
#include "pokemon.h"
#include "sound.h"
#include "sprite.h"
#include "constants/items.h"
#include "constants/songs.h"

// Gunk: Quick Ball. In a wild battle where balls can be thrown, a small widget at the bottom left of the action
// menu shows a ball, how many are in the bag (up to 99) and an "L" hint. Tapping L throws that ball; holding L,
// Left / Right picks another kind. It starts on the kind thrown last (VAR_GUNK_LAST_BALL).

#define TAG_QUICK_BALL_TEXT 0x5C00

#define WIDGET_BALL_X 12
#define WIDGET_TEXT_X 40
#define WIDGET_Y      104

// L icon, multiplication sign, digits 0-9: 8x8 tiles, 4bpp
static const u8 sQuickBallFontTiles[] = INCBIN_U8("graphics/gunk/quick_ball.4bpp");
static const u16 sQuickBallFontPalette[] = INCBIN_U16("graphics/gunk/quick_ball.gbapal");

#define FONT_TILE_L     0
#define FONT_TILE_TIMES 1
#define FONT_TILE_DIGIT 2

// Ball sprite index (pokeball.c) by item: Master, Ultra, Great, Poke, Safari, Net, Dive, Nest, Repeat, Timer,
// Luxury, Premier
static const u8 sBallIndexByItem[LAST_BALL + 1] = {0, 4, 3, 1, 0, 2, 5, 6, 7, 8, 9, 10, 11};

static const struct OamData sQuickBallTextOam =
{
    .shape = 1, // 32x8
    .size = 1,
    .priority = 0,
};

static const struct SpriteTemplate sQuickBallTextTemplate =
{
    .tileTag = TAG_QUICK_BALL_TEXT,
    .paletteTag = TAG_QUICK_BALL_TEXT,
    .oam = &sQuickBallTextOam,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

EWRAM_DATA static u8 sQuickBallTiles[4 * TILE_SIZE_4BPP] = {0};
EWRAM_DATA static u8 sTextSpriteId = 0;
EWRAM_DATA static u8 sBallSpriteId = 0;
EWRAM_DATA static bool8 sShown = FALSE;
EWRAM_DATA static u16 sBall = ITEM_NONE;        // the kind shown
EWRAM_DATA static u16 sPendingThrow = ITEM_NONE; // thrown with L: given to the battle instead of opening the bag
EWRAM_DATA static bool8 sLHeld = FALSE;
EWRAM_DATA static bool8 sCycled = FALSE;

static u16 BallCount(u16 item)
{
    u8 i;

    for (i = 0; i < BAG_POKEBALLS_COUNT; i++)
    {
        if (gSaveBlock1.bagPocket_PokeBalls[i].itemId == item)
            return gSaveBlock1.bagPocket_PokeBalls[i].quantity;
    }
    return 0;
}

static bool8 IsBall(u16 item)
{
    return item >= FIRST_BALL && item <= LAST_BALL;
}

// The next (dir 1) or previous (dir -1) kind of ball in the bag's Poke Balls pocket after `from`; with `from`
// not in the bag, the first one. ITEM_NONE when the bag has no balls.
static u16 NextBall(u16 from, s8 dir)
{
    s8 i, start = -1, n = BAG_POKEBALLS_COUNT;

    for (i = 0; i < n; i++)
    {
        if (gSaveBlock1.bagPocket_PokeBalls[i].itemId == from && gSaveBlock1.bagPocket_PokeBalls[i].quantity)
            start = i;
    }
    for (i = 1; i <= n; i++)
    {
        s8 j = (start < 0) ? i - 1 : (start + dir * i + n * 2) % n;
        struct ItemSlot *slot = &gSaveBlock1.bagPocket_PokeBalls[j];

        if (IsBall(slot->itemId) && slot->quantity)
            return slot->itemId;
    }
    return ITEM_NONE;
}

static void CopyFontTile(u8 dest, u8 src)
{
    CpuCopy16(&sQuickBallFontTiles[src * TILE_SIZE_4BPP], &sQuickBallTiles[dest * TILE_SIZE_4BPP], TILE_SIZE_4BPP);
}

// L x N N (count capped at 99)
static void DrawText(void)
{
    u16 count = BallCount(sBall);

    if (count > 99)
        count = 99;
    CopyFontTile(0, FONT_TILE_L);
    CopyFontTile(1, FONT_TILE_TIMES);
    if (count >= 10)
    {
        CopyFontTile(2, FONT_TILE_DIGIT + count / 10);
        CopyFontTile(3, FONT_TILE_DIGIT + count % 10);
    }
    else
    {
        CopyFontTile(2, FONT_TILE_DIGIT + count);
        CpuFill16(0, &sQuickBallTiles[3 * TILE_SIZE_4BPP], TILE_SIZE_4BPP);
    }
    CpuCopy16(sQuickBallTiles, (u8 *)OBJ_VRAM0 + GetSpriteTileStartByTag(TAG_QUICK_BALL_TEXT) * TILE_SIZE_4BPP,
              sizeof(sQuickBallTiles));
}

static void CreateBallSprite(void)
{
    struct SpriteTemplate template;
    u8 index = sBallIndexByItem[sBall];

    LoadBallGraphics(index);
    template = gBallSpriteTemplates[index];
    template.callback = SpriteCallbackDummy;
    sBallSpriteId = CreateSprite(&template, WIDGET_BALL_X, WIDGET_Y, 0);
    gSprites[sBallSpriteId].oam.priority = 0;
}

static void DestroyBallSprite(void)
{
    DestroySprite(&gSprites[sBallSpriteId]);
    FreeBallGraphics(sBallIndexByItem[sBall]);
}

static bool8 QuickBallAllowed(void)
{
    return !(gBattleTypeFlags & (BATTLE_TYPE_DOUBLE | BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER | BATTLE_TYPE_FIRST_BATTLE
                                 | BATTLE_TYPE_SAFARI | BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_WALLY_TUTORIAL
                                 | BATTLE_TYPE_EREADER_TRAINER))
        && gSaveBlock2.optionsButtonMode != OPTIONS_BUTTON_MODE_L_EQUALS_A;
}

// The action menu is up: show the widget
void GunkQuickBall_Show(void)
{
    struct SpriteSheet sheet = {sQuickBallTiles, sizeof(sQuickBallTiles), TAG_QUICK_BALL_TEXT};
    struct SpritePalette palette = {sQuickBallFontPalette, TAG_QUICK_BALL_TEXT};

    sLHeld = FALSE;
    sCycled = FALSE;
    if (sShown || !QuickBallAllowed())
        return;
    sBall = VarGet(VAR_GUNK_LAST_BALL);
    if (!IsBall(sBall) || BallCount(sBall) == 0)
        sBall = NextBall(ITEM_NONE, 1);
    if (sBall == ITEM_NONE)
        return;
    LoadSpriteSheet(&sheet);
    LoadSpritePalette(&palette);
    sTextSpriteId = CreateSprite(&sQuickBallTextTemplate, WIDGET_TEXT_X, WIDGET_Y, 0);
    CreateBallSprite();
    DrawText();
    sShown = TRUE;
}

void GunkQuickBall_Hide(void)
{
    if (!sShown)
        return;
    DestroySprite(&gSprites[sTextSpriteId]);
    FreeSpriteTilesByTag(TAG_QUICK_BALL_TEXT);
    FreeSpritePaletteByTag(TAG_QUICK_BALL_TEXT);
    DestroyBallSprite();
    sShown = FALSE;
}

// Input on the action menu. QUICK_BALL_NONE: not ours; QUICK_BALL_CONSUMED: used (L held, cycling);
// QUICK_BALL_THROW: throw sBall (the caller sends B_ACTION_USE_ITEM; the widget is hidden).
u8 GunkQuickBall_HandleInput(void)
{
    if (!sShown)
        return QUICK_BALL_NONE;
    if (JOY_HELD(L_BUTTON))
    {
        sLHeld = TRUE;
        if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
        {
            u16 next = NextBall(sBall, JOY_NEW(DPAD_RIGHT) ? 1 : -1);

            sCycled = TRUE;
            if (next != ITEM_NONE && next != sBall)
            {
                PlaySE(SE_SELECT);
                DestroyBallSprite();
                sBall = next;
                CreateBallSprite();
                DrawText();
            }
        }
        return QUICK_BALL_CONSUMED;
    }
    if (sLHeld)
    {
        sLHeld = FALSE;
        if (sCycled)
        {
            sCycled = FALSE;
            return QUICK_BALL_CONSUMED;
        }
        if (PlayerPartyAndPokemonStorageFull() || BallCount(sBall) == 0)
        {
            PlaySE(SE_FAILURE);
            return QUICK_BALL_CONSUMED;
        }
        sPendingThrow = sBall;
        VarSet(VAR_GUNK_LAST_BALL, sBall);
        GunkQuickBall_Hide();
        return QUICK_BALL_THROW;
    }
    return QUICK_BALL_NONE;
}

// The battle asks for the item to use: a ball thrown with L, taken from the bag (ITEM_NONE: open the bag)
u16 GunkQuickBall_TakePendingThrow(void)
{
    u16 item = sPendingThrow;

    sPendingThrow = ITEM_NONE;
    if (item != ITEM_NONE)
        RemoveBagItem(item, 1);
    return item;
}

// A ball chosen from the bag becomes the widget's default
void GunkQuickBall_RememberBall(u16 item)
{
    if (IsBall(item))
        VarSet(VAR_GUNK_LAST_BALL, item);
}

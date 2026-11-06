// Gunk's Ruby/Sapphire: the DexNav screen.
//
// Opened by the DEXNAV key item (src/gunk_dexnav.c lists the area first). The layout follows Gunk's Platinum DexNav,
// fitted to one 240x160 screen:
//   title bar      DEXNAV, Caught x/y for the area
//   LAND grid      2 rows of 5 cells: a caught species' menu icon in colour (with a ball mark), a seen one's as a
//                  silhouette, "?" for an unseen one; only seen ones can be chosen
//   WATER grid     1 row of 5 cells, the same for surfing encounters
//   info panel     the highlighted species: name, Not seen yet / Seen / Caught, where (grass or surfing) and
//                  the area's level range, its search level
//   search panel   the registered target (R searches for it) with its chain, and a hint
//   footer         A: Select  B: Close
// A on a cell returns its entry to the script in VAR_RESULT (GUNK_DEXNAV_WATER + n for water), B returns
// GUNK_DEXNAV_CLOSED. Each game's look (tiles and palettes) is in graphics/gunk_dexnav/<game>/, drawn by
// tools/gunk/dexnav_skin.py; the layout is the same for both.
#include "global.h"
#include "gunk.h"
#include "data2.h"
#include "event_data.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "palette.h"
#include "pokedex.h"
#include "pokemon_icon.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "constants/songs.h"
#include "constants/species.h"

#ifdef SAPPHIRE
static const u8 sTiles[] = INCBIN_U8("graphics/gunk_dexnav/sapphire/tiles.4bpp");
static const u16 sBgPalette[] = INCBIN_U16("graphics/gunk_dexnav/sapphire/bg.gbapal");
static const u16 sTextPalette[] = INCBIN_U16("graphics/gunk_dexnav/sapphire/text.gbapal");
#else
static const u8 sTiles[] = INCBIN_U8("graphics/gunk_dexnav/ruby/tiles.4bpp");
static const u16 sBgPalette[] = INCBIN_U16("graphics/gunk_dexnav/ruby/bg.gbapal");
static const u16 sTextPalette[] = INCBIN_U16("graphics/gunk_dexnav/ruby/text.gbapal");
#endif

// Background tiles, in tools/gunk/dexnav_skin.py's order
#define DNT_BACK      0
#define DNT_WAVE      1
#define DNT_TITLE     2
#define DNT_TITLE_LOW 3
#define DNT_PANEL     4  // 3x3: corners, edges, fill
#define DNT_CELL      13 // 3x3
#define DNT_UNSEEN    22 // cell centre with "?"
#define DNT_CAUGHT    23 // cell top-left with a ball

#define BG_PAL_CURSOR 1
#define TEXT_PAL      15

// Layout, in tiles
#define GRID_COLS     5
#define CELL_SIZE     3
#define LAND_TOP      3
#define WATER_TOP     10
#define INFO_LEFT     15
#define INFO_TOP      2
#define INFO_WIDTH    15
#define INFO_HEIGHT   11
#define SEARCH_TOP    13
#define SEARCH_HEIGHT 6
#define FOOTER_ROW    19

#define TILEMAP_VRAM  (VRAM + 0x3000) // BG1 screen block 6, its tiles at character block 0

#define NO_CELL       0xFF

#define SILHOUETTE_PAL_TAG 0x1310

struct DexNavScreen
{
    u16 tilemap[32 * 32];
    u8 iconSpriteIds[GUNK_DEXNAV_LAND_MAX + GUNK_DEXNAV_WATER_MAX];
    u8 cursor;          // cell: 0-9 land, 10-14 water
    bool8 tilemapDirty;
};

EWRAM_DATA static struct DexNavScreen *sScreen = NULL;
EWRAM_DATA static struct DexNavScreen sScreenData = {0};

// The menu text window, with a clear background so the panels show behind the letters
static const struct WindowTemplate sTextWindowTemplate =
{
    0, // BG number
    2, // BG character base block
    31, // BG screen base block
    0, // BG priority
    TEXT_PAL, // palette number
    1, // foreground color
    0, // background color
    3, // shadow color
    3, // font
    2, // text mode
    0, // spacing
    0, // tilemap left coordinate
    0, // tilemap top coordinate
    30, // width
    20, // height
    BG_CHAR_ADDR(2), // tile data
    BG_SCREEN_ADDR(31), // tilemap
};

// The text window's palette (text.pal): 1 ink, 2 white, 3 ink shadow, 4 title shadow. The colour names below are
// the charmap's for those indices.
static const u8 sText_TitleColors[] = _("{COLOR RED}{SHADOW BLUE}");
static const u8 sText_InkColors[] = _("{COLOR DARK_GREY}{SHADOW GREEN}");
static const u8 sText_Small[] = _("{SIZE 4}");

static const u8 sText_DexNav[] = _("DEXNAV");
static const u8 sText_Caught[] = _("Caught ");
static const u8 sText_Land[] = _("LAND");
static const u8 sText_Water[] = _("WATER");
static const u8 sText_Search[] = _("SEARCH");
static const u8 sText_Footer[] = _("A: Select  B: Close");
static const u8 sText_Unseen[] = _("?????");
static const u8 sText_NotSeenYet[] = _("Not seen yet");
static const u8 sText_Seen[] = _("Seen");
static const u8 sText_CaughtStatus[] = _("Caught");
static const u8 sText_Grass[] = _("Grass");
static const u8 sText_Surfing[] = _("Surfing");
static const u8 sText_Lv[] = _("  Lv. ");
static const u8 sText_SearchLv[] = _("Search Lv. ");
static const u8 sText_Chain[] = _("   Chain ");
static const u8 sText_None[] = _("None yet");
static const u8 sText_HintChoose[] = _("Press A on a seen POKéMON to choose it.");
static const u8 sText_HintGrass[] = _("Press R in tall grass to search.");
static const u8 sText_HintWater[] = _("Press R while surfing to search.");

static void VBlankCB(void);
static void MainCB2(void);
static void Task_FadeIn(u8 taskId);
static void Task_HandleInput(u8 taskId);
static void Task_FadeOut(u8 taskId);

static const struct GunkDexNavArea *Area(void)
{
    return GunkDexNavGetArea();
}

// Cell -> species (SPECIES_NONE for an empty cell)
static u16 CellSpecies(u8 cell)
{
    const struct GunkDexNavArea *area = Area();

    if (cell < GUNK_DEXNAV_LAND_MAX)
        return cell < area->landCount ? area->land[cell] : SPECIES_NONE;
    cell -= GUNK_DEXNAV_LAND_MAX;
    return cell < area->waterCount ? area->water[cell] : SPECIES_NONE;
}

static bool8 CellIsWater(u8 cell)
{
    return cell >= GUNK_DEXNAV_LAND_MAX;
}

// Cells on screen: land 0-9 in two rows, water 10-14 in one; GUNK_DEXNAV_LAND_MAX is 12 but at most 8 land
// species share a map in Ruby/Sapphire, so 10 cells are drawn
#define LAND_CELLS (GRID_COLS * 2)
static void CellToScreen(u8 cell, u8 *x, u8 *y)
{
    if (CellIsWater(cell))
    {
        *x = (cell - GUNK_DEXNAV_LAND_MAX) * CELL_SIZE;
        *y = WATER_TOP;
    }
    else
    {
        *x = (cell % GRID_COLS) * CELL_SIZE;
        *y = LAND_TOP + (cell / GRID_COLS) * CELL_SIZE;
    }
}

static u8 CellAt(u8 row, u8 col)
{
    return row < 2 ? row * GRID_COLS + col : GUNK_DEXNAV_LAND_MAX + col;
}

static bool8 SpeciesFlag(u16 species, u8 flag)
{
    return GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), flag);
}

// --- background ---

static void SetTile(u8 x, u8 y, u16 tile, u8 palette)
{
    sScreen->tilemap[y * 32 + x] = tile | (palette << 12);
    sScreen->tilemapDirty = TRUE;
}

static void DrawBox(u8 left, u8 top, u8 width, u8 height, u16 base, u8 palette)
{
    u8 x, y;

    for (y = 0; y < height; y++)
    {
        u8 row = y == 0 ? 0 : (y == height - 1 ? 2 : 1);

        for (x = 0; x < width; x++)
        {
            u8 col = x == 0 ? 0 : (x == width - 1 ? 2 : 1);

            SetTile(left + x, top + y, base + row * 3 + col, palette);
        }
    }
}

static void DrawCell(u8 cell)
{
    u16 species = CellSpecies(cell);
    u8 palette = cell == sScreen->cursor ? BG_PAL_CURSOR : 0;
    u8 x, y;

    CellToScreen(cell, &x, &y);
    DrawBox(x, y, CELL_SIZE, CELL_SIZE, DNT_CELL, palette);
    if (species == SPECIES_NONE)
        return;
    if (!SpeciesFlag(species, FLAG_GET_SEEN))
        SetTile(x + 1, y + 1, DNT_UNSEEN, palette);
    else if (SpeciesFlag(species, FLAG_GET_CAUGHT))
        SetTile(x, y, DNT_CAUGHT, palette);
}

static void DrawBackground(void)
{
    u8 x, y, cell;

    for (y = 0; y < 20; y++)
    {
        for (x = 0; x < 30; x++)
            SetTile(x, y, (x * 3 + y * 5) % 11 == 0 ? DNT_WAVE : DNT_BACK, 0);
    }
    for (x = 0; x < 30; x++)
    {
        SetTile(x, 0, DNT_TITLE, 0);
        SetTile(x, 1, DNT_TITLE_LOW, 0);
        SetTile(x, FOOTER_ROW, DNT_TITLE, 0);
    }
    DrawBox(INFO_LEFT, INFO_TOP, INFO_WIDTH, INFO_HEIGHT, DNT_PANEL, 0);
    DrawBox(0, SEARCH_TOP, 30, SEARCH_HEIGHT, DNT_PANEL, 0);
    for (cell = 0; cell < LAND_CELLS; cell++)
        DrawCell(cell);
    for (cell = GUNK_DEXNAV_LAND_MAX; cell < GUNK_DEXNAV_LAND_MAX + GUNK_DEXNAV_WATER_MAX; cell++)
        DrawCell(cell);
}

// --- text ---

static void PrintColored(const u8 *colors, const u8 *text, u8 x, u8 y, bool8 small)
{
    u8 *s = StringCopy(gStringVar4, colors);

    if (small)
        s = StringCopy(s, sText_Small);
    StringCopy(s, text);
    Menu_PrintText(gStringVar4, x, y);
}

static void PrintStaticText(void)
{
    const struct GunkDexNavArea *area = Area();
    u8 caught = 0, total = 0;
    u8 i, j;
    u8 *s;

    // Caught x/y: each of the area's species once (one can be in the grass and in the water)
    for (i = 0; i < area->landCount + area->waterCount; i++)
    {
        u16 species = i < area->landCount ? area->land[i] : area->water[i - area->landCount];

        for (j = 0; j < area->landCount && j < i; j++)
        {
            if (area->land[j] == species)
                break;
        }
        if (j < area->landCount && j < i)
            continue;
        total++;
        if (SpeciesFlag(species, FLAG_GET_CAUGHT))
            caught++;
    }
    PrintColored(sText_TitleColors, sText_DexNav, 1, 0, FALSE);
    s = StringCopy(gStringVar1, sText_Caught);
    s = ConvertIntToDecimalStringN(s, caught, STR_CONV_MODE_LEFT_ALIGN, 2);
    *s++ = CHAR_SLASH;
    ConvertIntToDecimalStringN(s, total, STR_CONV_MODE_LEFT_ALIGN, 2);
    PrintColored(sText_TitleColors, gStringVar1, 21, 0, FALSE);
    PrintColored(sText_TitleColors, sText_Land, 1, LAND_TOP - 1, TRUE);
    PrintColored(sText_TitleColors, sText_Water, 1, WATER_TOP - 1, TRUE);
    PrintColored(sText_TitleColors, sText_Search, 2, SEARCH_TOP, TRUE);
    PrintColored(sText_TitleColors, sText_Footer, 11, FOOTER_ROW, TRUE);
}

static void PrintInfo(void)
{
    const struct GunkDexNavArea *area = Area();
    u8 cell = sScreen->cursor;
    u16 species = CellSpecies(cell);
    bool8 water = CellIsWater(cell);
    u8 n = water ? cell - GUNK_DEXNAV_LAND_MAX : cell;
    u8 *s;

    Menu_EraseWindowRect(INFO_LEFT + 1, INFO_TOP + 1, INFO_LEFT + INFO_WIDTH - 2, INFO_TOP + INFO_HEIGHT - 2);
    if (species == SPECIES_NONE)
        return;
    if (SpeciesFlag(species, FLAG_GET_SEEN))
    {
        PrintColored(sText_InkColors, gSpeciesNames[species], INFO_LEFT + 1, INFO_TOP + 1, FALSE);
        PrintColored(sText_InkColors, SpeciesFlag(species, FLAG_GET_CAUGHT) ? sText_CaughtStatus : sText_Seen,
                     INFO_LEFT + 1, INFO_TOP + 3, FALSE);
    }
    else
    {
        PrintColored(sText_InkColors, sText_Unseen, INFO_LEFT + 1, INFO_TOP + 1, FALSE);
        PrintColored(sText_InkColors, sText_NotSeenYet, INFO_LEFT + 1, INFO_TOP + 3, FALSE);
    }
    s = StringCopy(gStringVar1, water ? sText_Surfing : sText_Grass);
    s = StringCopy(s, sText_Lv);
    s = ConvertIntToDecimalStringN(s, water ? area->waterMin[n] : area->landMin[n], STR_CONV_MODE_LEFT_ALIGN, 3);
    if ((water ? area->waterMax[n] : area->landMax[n]) != (water ? area->waterMin[n] : area->landMin[n]))
    {
        *s++ = CHAR_HYPHEN;
        ConvertIntToDecimalStringN(s, water ? area->waterMax[n] : area->landMax[n], STR_CONV_MODE_LEFT_ALIGN, 3);
    }
    PrintColored(sText_InkColors, gStringVar1, INFO_LEFT + 1, INFO_TOP + 5, FALSE);
    s = StringCopy(gStringVar1, sText_SearchLv);
    ConvertIntToDecimalStringN(s, GunkDexNavSearchLevel(species), STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintColored(sText_InkColors, gStringVar1, INFO_LEFT + 1, INFO_TOP + 7, FALSE);
}

static void PrintSearch(void)
{
    bool8 water;
    u16 target = GunkDexNavTarget(&water);
    u8 *s;

    Menu_EraseWindowRect(1, SEARCH_TOP + 1, 28, SEARCH_TOP + SEARCH_HEIGHT - 2);
    if (target == SPECIES_NONE)
    {
        PrintColored(sText_InkColors, sText_None, 1, SEARCH_TOP + 1, FALSE);
        PrintColored(sText_InkColors, sText_HintChoose, 1, SEARCH_TOP + 3, FALSE);
        return;
    }
    s = StringCopy(gStringVar1, gSpeciesNames[target]);
    s = StringCopy(s, sText_Chain);
    ConvertIntToDecimalStringN(s, GunkDexNavChain(target), STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintColored(sText_InkColors, gStringVar1, 1, SEARCH_TOP + 1, FALSE);
    PrintColored(sText_InkColors, water ? sText_HintWater : sText_HintGrass, 1, SEARCH_TOP + 3, FALSE);
}

// --- icons ---

// Seen but not caught: the icon in one flat colour
static const u16 sSilhouettePalette[16] = {
    RGB(0, 0, 0), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14),
    RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14), RGB(6, 9, 14),
    RGB(6, 9, 14), RGB(6, 9, 14),
};
static const struct SpritePalette sSilhouetteSpritePalette = {sSilhouettePalette, SILHOUETTE_PAL_TAG};

static void SpriteCB_Icon(struct Sprite *sprite)
{
    // only the highlighted species moves
    if (sprite->data[7])
        UpdateMonIconFrame(sprite);
}

static void CreateIcons(void)
{
    u8 cell;

    sub_809D51C(); // every menu icon palette
    LoadSpritePalette(&sSilhouetteSpritePalette);
    for (cell = 0; cell < GUNK_DEXNAV_LAND_MAX + GUNK_DEXNAV_WATER_MAX; cell++)
    {
        u16 species = CellSpecies(cell);
        u8 x, y, spriteId;

        sScreen->iconSpriteIds[cell] = MAX_SPRITES;
        if (species == SPECIES_NONE || !SpeciesFlag(species, FLAG_GET_SEEN))
            continue;
        if (!CellIsWater(cell) && cell >= LAND_CELLS)
            continue;
        CellToScreen(cell, &x, &y);
        spriteId = CreateMonIcon(species, SpriteCB_Icon, x * 8 + 12, y * 8 + 10, 1, 0);
        if (spriteId == MAX_SPRITES)
            continue;
        gSprites[spriteId].oam.priority = 0;
        if (!SpeciesFlag(species, FLAG_GET_CAUGHT))
            gSprites[spriteId].oam.paletteNum = IndexOfSpritePaletteTag(SILHOUETTE_PAL_TAG);
        sScreen->iconSpriteIds[cell] = spriteId;
    }
}

static void SetIconActive(u8 cell, bool8 active)
{
    u8 spriteId = sScreen->iconSpriteIds[cell];

    if (spriteId != MAX_SPRITES)
        gSprites[spriteId].data[7] = active;
}

// --- cursor ---

static u8 FirstCell(void)
{
    const struct GunkDexNavArea *area = Area();
    bool8 water;
    u16 target = GunkDexNavTarget(&water);
    u8 i;

    // the registered target, else the first seen species, else the first cell
    for (i = 0; i < area->landCount && i < LAND_CELLS; i++)
    {
        if (!water && area->land[i] == target)
            return i;
    }
    for (i = 0; i < area->waterCount; i++)
    {
        if (water && area->water[i] == target)
            return GUNK_DEXNAV_LAND_MAX + i;
    }
    for (i = 0; i < area->landCount && i < LAND_CELLS; i++)
    {
        if (SpeciesFlag(area->land[i], FLAG_GET_SEEN))
            return i;
    }
    for (i = 0; i < area->waterCount; i++)
    {
        if (SpeciesFlag(area->water[i], FLAG_GET_SEEN))
            return GUNK_DEXNAV_LAND_MAX + i;
    }
    return area->landCount != 0 ? 0 : GUNK_DEXNAV_LAND_MAX;
}

static void MoveCursorTo(u8 cell)
{
    u8 old = sScreen->cursor;

    if (cell == NO_CELL || cell == old)
        return;
    PlaySE(SE_SELECT);
    SetIconActive(old, FALSE);
    sScreen->cursor = cell;
    DrawCell(old);
    DrawCell(cell);
    SetIconActive(cell, TRUE);
    PrintInfo();
}

static void CellRowCol(u8 cell, u8 *row, u8 *col)
{
    if (CellIsWater(cell))
    {
        *row = 2;
        *col = cell - GUNK_DEXNAV_LAND_MAX;
    }
    else
    {
        *row = cell / GRID_COLS;
        *col = cell % GRID_COLS;
    }
}

// Left/right: the next cell with a species in the row
static u8 StepInRow(s8 dir)
{
    u8 row, col;

    CellRowCol(sScreen->cursor, &row, &col);
    for (col += dir; col < GRID_COLS; col += dir)
    {
        if (CellSpecies(CellAt(row, col)) != SPECIES_NONE)
            return CellAt(row, col);
    }
    return NO_CELL;
}

// Up/down: the nearest cell with a species in the next row that has one
static u8 StepRows(s8 dir)
{
    u8 row, col, best = NO_CELL, bestDist = 0xFF, c;

    CellRowCol(sScreen->cursor, &row, &col);
    for (row += dir; row < 3; row += dir)
    {
        for (c = 0; c < GRID_COLS; c++)
        {
            u8 dist = c > col ? c - col : col - c;

            if (CellSpecies(CellAt(row, c)) != SPECIES_NONE && dist < bestDist)
            {
                best = CellAt(row, c);
                bestDist = dist;
            }
        }
        if (best != NO_CELL)
            return best;
    }
    return NO_CELL;
}

// --- screen ---

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    if (sScreen->tilemapDirty)
    {
        DmaCopy16(3, sScreen->tilemap, (void *)TILEMAP_VRAM, sizeof(sScreen->tilemap));
        sScreen->tilemapDirty = FALSE;
    }
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void CB2_DexNavScreen(void)
{
    u32 savedIme;

    SetVBlankCallback(NULL);
    REG_DISPCNT = 0;
    REG_BG3CNT = 0;
    REG_BG2CNT = 0;
    REG_BG1CNT = 0;
    REG_BG0CNT = 0;
    REG_BG3HOFS = 0;
    REG_BG3VOFS = 0;
    REG_BG2HOFS = 0;
    REG_BG2VOFS = 0;
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    ResetPaletteFade();
    FreeAllSpritePalettes();

    sScreen = &sScreenData;
    sScreen->cursor = FirstCell();
    DmaCopy16(3, sTiles, (void *)VRAM, sizeof(sTiles));
    LoadPalette(sBgPalette, 0, sizeof(sBgPalette));
    DrawBackground();
    DmaCopy16(3, sScreen->tilemap, (void *)TILEMAP_VRAM, sizeof(sScreen->tilemap));
    sScreen->tilemapDirty = FALSE;

    Text_LoadWindowTemplate(&sTextWindowTemplate);
    InitMenuWindow(&sTextWindowTemplate);
    LoadPalette(sTextPalette, TEXT_PAL * 16, sizeof(sTextPalette));
    PrintStaticText();
    PrintInfo();
    PrintSearch();
    CreateIcons();
    SetIconActive(sScreen->cursor, TRUE);

    BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB(0, 0, 0));
    savedIme = REG_IME;
    REG_IME = 0;
    REG_IE |= INTR_FLAG_VBLANK;
    REG_IME = savedIme;
    REG_DISPSTAT |= DISPSTAT_VBLANK_INTR;
    SetVBlankCallback(VBlankCB);
    SetMainCallback2(MainCB2);
    REG_BLDCNT = 0;
    REG_BLDALPHA = 0;
    REG_BLDY = 0;
    REG_BG1CNT = BGCNT_PRIORITY(1) | BGCNT_CHARBASE(0) | BGCNT_SCREENBASE(6) | BGCNT_16COLOR | BGCNT_TXT256x256;
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_OBJ_ON;
    CreateTask(Task_FadeIn, 0);
}

static void Task_FadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_HandleInput;
}

static void Task_HandleInput(u8 taskId)
{
    if (JOY_NEW(A_BUTTON))
    {
        u8 cell = sScreen->cursor;

        if (CellSpecies(cell) == SPECIES_NONE)
            return;
        if (!SpeciesFlag(CellSpecies(cell), FLAG_GET_SEEN))
        {
            PlaySE(SE_FAILURE); // only a POKéMON seen before can be searched for
            return;
        }
        PlaySE(SE_SELECT);
        gSpecialVar_Result = CellIsWater(cell) ? GUNK_DEXNAV_WATER + cell - GUNK_DEXNAV_LAND_MAX : cell;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        gSpecialVar_Result = GUNK_DEXNAV_CLOSED;
    }
    else
    {
        if (JOY_NEW(DPAD_LEFT))
            MoveCursorTo(StepInRow(-1));
        else if (JOY_NEW(DPAD_RIGHT))
            MoveCursorTo(StepInRow(1));
        else if (JOY_NEW(DPAD_UP))
            MoveCursorTo(StepRows(-1));
        else if (JOY_NEW(DPAD_DOWN))
            MoveCursorTo(StepRows(1));
        return;
    }
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB(0, 0, 0));
    gTasks[taskId].func = Task_FadeOut;
}

static void Task_FadeOut(u8 taskId)
{
    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);
    sScreen = NULL;
    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic); // the script goes on from its waitstate
}

// Special (after GunkDexNavPrepare and fadescreen): open the screen; waitstate, the pick in VAR_RESULT
void GunkDexNavShowScreen(void)
{
    SetMainCallback2(CB2_DexNavScreen);
}

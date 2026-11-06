"""Draws the DexNav screen's background tiles and palettes for one skin (Gunk's Ruby/Sapphire).

    python tools/gunk/dexnav_skin.py sapphire|ruby

Writes graphics/gunk_dexnav/<skin>/tiles.png (indexed, 16 colours) and bg.pal: palette 0 for the screen and
palette 1, the same with the cell colours swapped for the cursor; text.pal for the text window. The tile order is
the one src/gunk_dexnav_screen.c uses (DNT_*).
"""
import os
import sys
from PIL import Image

SKINS = {
    # backdrop, backdrop wave, wave light, panel border, panel fill, panel shade, title, title light,
    # cell fill, cell border, cursor fill, cursor border, ball red, ball white, ink
    "sapphire": [(16, 40, 72), (24, 56, 96), (40, 88, 136), (32, 96, 168), (224, 240, 248),
                 (184, 216, 240), (48, 112, 192), (88, 160, 224), (248, 252, 255), (96, 152, 208),
                 (160, 240, 240), (24, 168, 184), (224, 56, 56), (255, 255, 255), (32, 32, 40)],
    "ruby":     [(56, 16, 16), (80, 24, 24), (120, 40, 32), (160, 40, 40), (252, 236, 224),
                 (240, 200, 184), (184, 48, 40), (232, 112, 72), (255, 250, 244), (200, 120, 104),
                 (255, 224, 128), (224, 136, 32), (224, 56, 56), (255, 255, 255), (40, 24, 24)],
}
TEXT = {
    # 0 clear, 1 ink, 2 white, 3 ink shadow, 4 title shadow
    "sapphire": [(0, 0, 0), (24, 48, 88), (255, 255, 255), (168, 200, 232), (28, 76, 144)],
    "ruby":     [(0, 0, 0), (88, 24, 24), (255, 255, 255), (232, 184, 168), (128, 32, 24)],
}
BACK, WAVE, WAVE_LIGHT, BORDER, FILL, SHADE, TITLE, TITLE_LIGHT, CELL, CELL_BORDER, CUR, CUR_BORDER, \
    BALL_RED, BALL_WHITE, INK = range(1, 16)

QUESTION = ["..XXX...", ".X...X..", ".....X..", "....X...", "...X....", "........", "...X....", "........"]
BALL = [".KKK.", "KRRRK", "KKWKK", "KWWWK", ".KKK."]


def tile(fill):
    return [[fill] * 8 for _ in range(8)]


def frame(fill, border, shade=None):
    """3x3 tiles of a rounded box: TL T TR L C R BL B BR."""
    big = [[fill] * 24 for _ in range(24)]
    for i in range(24):
        big[0][i] = big[23][i] = big[i][0] = big[i][23] = border
    for (x, y) in ((0, 0), (23, 0), (0, 23), (23, 23)):
        big[y][x] = 0
    for (x, y) in ((1, 1), (22, 1), (1, 22), (22, 22)):
        big[y][x] = border
    if shade:
        for i in range(2, 22):
            big[1][i] = shade
    return [[row[tx * 8:tx * 8 + 8] for row in big[ty * 8:ty * 8 + 8]] for ty in range(3) for tx in range(3)]


def tiles():
    out = []
    out.append(tile(BACK))                                   # 0 backdrop
    t = tile(BACK)                                           # 1 backdrop with a wave
    for x, y in ((1, 5), (2, 4), (3, 4), (4, 5), (5, 6), (6, 6), (7, 5)):
        t[y][x] = WAVE
    out.append(t)
    out.append(tile(TITLE))                                  # 2 title bar
    t = tile(TITLE)                                          # 3 title bar, bottom edge
    t[6] = [TITLE_LIGHT] * 8
    t[7] = [WAVE_LIGHT] * 8
    out.append(t)
    out += frame(FILL, BORDER, SHADE)                        # 4-12 panel
    out += frame(CELL, CELL_BORDER)                          # 13-21 cell
    t = tile(CELL)                                           # 22 cell centre: unseen
    for y, row in enumerate(QUESTION):
        for x, c in enumerate(row):
            if c == "X":
                t[y][x] = INK
    out.append(t)
    t = [row[:] for row in frame(CELL, CELL_BORDER)[0]]      # 23 cell top-left: caught mark
    for y, row in enumerate(BALL):
        for x, c in enumerate(row):
            if c != ".":
                t[y + 2][x + 2] = {"K": INK, "R": BALL_RED, "W": BALL_WHITE}[c]
    out.append(t)
    return out


def main():
    skin = sys.argv[1]
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "graphics", "gunk_dexnav", skin)
    os.makedirs(here, exist_ok=True)
    ts = tiles()
    img = Image.new("P", (128, 8 * ((len(ts) + 15) // 16)))
    for n, t in enumerate(ts):
        for y in range(8):
            for x in range(8):
                img.putpixel(((n % 16) * 8 + x, (n // 16) * 8 + y), t[y][x])
    pal = [(0, 0, 0)] + SKINS[skin]
    assert len(pal) == 16, len(pal)
    img.putpalette([c for rgb in pal for c in rgb])
    img.save(os.path.join(here, "tiles.png"))
    cursor = pal[:]
    cursor[CELL], cursor[CELL_BORDER] = pal[CUR], pal[CUR_BORDER]
    write_pal(os.path.join(here, "bg.pal"), pal + cursor)
    write_pal(os.path.join(here, "text.pal"), TEXT[skin] + [(0, 0, 0)] * 11)


def write_pal(path, colours):
    with open(path, "w", newline="\r\n") as f:
        f.write("JASC-PAL\n0100\n%d\n" % len(colours))
        for r, g, b in colours:
            f.write("%d %d %d\n" % (r, g, b))


if __name__ == "__main__":
    main()

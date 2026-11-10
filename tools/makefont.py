#!/usr/bin/env python3
# Builds assets/font.ttf, a 5x7 pixel font with narrow I and punctuation (crisp at 8, 16, 24 px). Needs: pip install fonttools
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

G = {
' ': "...../...../...../...../...../...../.....",
'A': ".###./#...#/#...#/#####/#...#/#...#/#...#",
'B': "####./#...#/#...#/####./#...#/#...#/####.",
'C': ".###./#...#/#..../#..../#..../#...#/.###.",
'D': "####./#...#/#...#/#...#/#...#/#...#/####.",
'E': "#####/#..../#..../####./#..../#..../#####",
'F': "#####/#..../#..../####./#..../#..../#....",
'G': ".###./#...#/#..../#.###/#...#/#...#/.###.",
'H': "#...#/#...#/#...#/#####/#...#/#...#/#...#",
'I': "###../.#.../.#.../.#.../.#.../.#.../###..",
'J': "..###/...#./...#./...#./...#./#..#./.##..",
'K': "#...#/#..#./#.#../##.../#.#../#..#./#...#",
'L': "#..../#..../#..../#..../#..../#..../#####",
'M': "#...#/##.##/#.#.#/#.#.#/#...#/#...#/#...#",
'N': "#...#/##..#/#.#.#/#..##/#...#/#...#/#...#",
'O': ".###./#...#/#...#/#...#/#...#/#...#/.###.",
'P': "####./#...#/#...#/####./#..../#..../#....",
'Q': ".###./#...#/#...#/#...#/#.#.#/#..#./.##.#",
'R': "####./#...#/#...#/####./#.#../#..#./#...#",
'S': ".####/#..../#..../.###./....#/....#/####.",
'T': "#####/..#../..#../..#../..#../..#../..#..",
'U': "#...#/#...#/#...#/#...#/#...#/#...#/.###.",
'V': "#...#/#...#/#...#/#...#/#...#/.#.#./..#..",
'W': "#...#/#...#/#...#/#.#.#/#.#.#/##.##/#...#",
'X': "#...#/#...#/.#.#./..#../.#.#./#...#/#...#",
'Y': "#...#/#...#/.#.#./..#../..#../..#../..#..",
'Z': "#####/....#/...#./..#../.#.../#..../#####",
'0': ".###./#...#/#..##/#.#.#/##..#/#...#/.###.",
'1': "..#../.##../..#../..#../..#../..#../.###.",
'2': ".###./#...#/....#/...#./..#../.#.../#####",
'3': "####./....#/....#/.###./....#/....#/####.",
'4': "...#./..##./.#.#./#..#./#####/...#./...#.",
'5': "#####/#..../####./....#/....#/#...#/.###.",
'6': ".###./#..../#..../####./#...#/#...#/.###.",
'7': "#####/....#/...#./..#../.#.../.#.../.#...",
'8': ".###./#...#/#...#/.###./#...#/#...#/.###.",
'9': ".###./#...#/#...#/.####/....#/....#/.###.",
'!': "#..../#..../#..../#..../#..../...../#....",
'"': ".#.#./.#.#./...../...../...../...../.....",
'#': ".#.#./.#.#./#####/.#.#./#####/.#.#./.#.#.",
'%': "##..#/##..#/...#./..#../.#.../#..##/#..##",
'&': ".##../#..#./#.#../.#.../#.#.#/#..#./.##.#",
"'": "#..../#..../...../...../...../...../.....",
'(': "...#./..#../.#.../.#.../.#.../..#../...#.",
')': ".#.../..#../...#./...#./...#./..#../.#...",
'*': "...../..#../#.#.#/.###./#.#.#/..#../.....",
'+': "...../..#../..#../#####/..#../..#../.....",
',': "...../...../...../...../##.../.#.../#....",
'-': "...../...../...../#####/...../...../.....",
'.': "...../...../...../...../...../##.../##...",
'/': "....#/....#/...#./..#../.#.../#..../#....",
':': "...../##.../##.../...../##.../##.../.....",
'<': "...#./..#../.#.../#..../.#.../..#../...#.",
'=': "...../...../#####/...../#####/...../.....",
'>': ".#.../..#../...#./....#/...#./..#../.#...",
'?': ".###./#...#/....#/...#./..#../...../..#..",
'_': "...../...../...../...../...../...../#####",
}
U = 100
def glyph(rows):
    # Every horizontal run and every vertical run of pixels becomes one rectangle. The rectangles
    # overlap, so stems and bars are solid pieces and no renderer can open a gap between them.
    grid = [list(r) for r in rows.split('/')]
    pen = TTGlyphPen(None)
    def rect(c0, c1, r0, r1):
        x0, x1, y1, y0 = c0*U, c1*U, 700 - r0*U, 700 - r1*U
        pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
    for r in range(7):
        c = 0
        while c < 5:
            if grid[r][c] == '#':
                c1 = c
                while c1 < 5 and grid[r][c1] == '#': c1 += 1
                rect(c, c1, r, r + 1); c = c1
            else: c += 1
    for c in range(5):
        r = 0
        while r < 7:
            if grid[r][c] == '#':
                r1 = r
                while r1 < 7 and grid[r1][c] == '#': r1 += 1
                if r1 - r > 1: rect(c, c + 1, r, r1)
                r = r1
            else: r += 1
    return pen.glyph()

names = ['.notdef']; glyphs = {'.notdef': glyph("#####/#...#/#...#/#...#/#...#/#...#/#####")}; cmap = {}
for ch, rows in G.items():
    n = 'u%04X' % ord(ch); names.append(n); glyphs[n] = glyph(rows); cmap[ord(ch)] = n
    if ch.isalpha(): cmap[ord(ch.lower())] = n
fb = FontBuilder(800, isTTF=True)
fb.setupGlyphOrder(names); fb.setupCharacterMap(cmap); fb.setupGlyf(glyphs)
# the left side bearing must match each outline's real left edge, or renderers shift the glyph
glyf = fb.font['glyf']
# letters are 5 wide plus 1 space; narrow glyphs (I . , : ! ') only take the width they need
def adv(n):
    g = glyf[n]
    return 600 if g.numberOfContours <= 0 or g.xMax > 300 else g.xMax + 100
fb.setupHorizontalMetrics({n: (adv(n), glyf[n].xMin if glyf[n].numberOfContours > 0 else 0) for n in names})
fb.setupHorizontalHeader(ascent=700, descent=-100)
fb.setupNameTable({"familyName": "Mayhem Pixel", "styleName": "Regular"})
fb.setupOS2(sTypoAscender=700, sTypoDescender=-100, usWinAscent=700, usWinDescent=100)
fb.setupPost()
# ask renderers not to grid-fit (hint) the pixel outlines at any size
from fontTools.ttLib import newTable
gasp = newTable('gasp'); gasp.version = 1; gasp.gaspRange = {0xFFFF: 0x000A}; fb.font['gasp'] = gasp
fb.save(sys.argv[1] if len(sys.argv) > 1 else "font.ttf")

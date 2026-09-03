#include "palette.h"

#include <stdio.h>

#include <kernel.h>
#include <swis.h>

static _kernel_oserror *err;
static _kernel_swi_regs rin, rout;

#ifdef PAL_256
unsigned int g_fogTable[64 * 2 * 256];
#else
unsigned int g_fogTable[4 * 16 * 32];
#endif
extern unsigned int FogTable;

#define HEX_VAL(x) (((x) >> 16) & 0xFF), (((x) >> 8) & 0xFF), ((x) & 0xFF)

static unsigned int inputPalette[16] = {
#ifdef PAL_256
    (0x004488), // low blue
    (0x3377bb), // mid blue
    (0x77bbff), // high blue
    (0x115511), // low green
    (0x448800), // mid green
    (0x88cc00), // high green
    (0x66aaee), // sky color
    (0x155A78),
    (0x2A7190),
    (0x4088A8),
    (0x4698BD),
    (0x4DA9D2),
    (0x54BAE8),
    (0x88CCE8),
    (0xA8ECE8),
    (0xE8ECE8),
#else  // Original ST/Amiga Midwinter Palette
    (0x000000),
    (0xEEEEEE),
    (0x22AAEE),
    (0x004466),
    (0x226688),
    (0x4488AA),
    (0x88CCEE),
    (0x442200),
    (0x662200),
    (0x884422),
    (0xCC8866),
    (0x004400),
    (0x006600),
    (0xAA6600),
    (0xAA2200),
    (0xCCCC00),
#endif // PAL_256
};

#ifndef PAL_256
#define FOG_LOOKUP9_BLEND_TABLE_WORDS (24 * 2 * 16)

/* Map-screen palette expanded from the Amiga's twelve-bit colour registers. */
static const unsigned int mapPalette[16] = {
    0xcc8800,
    0x000066, /* sea */
    0x00aa00,
    0x222266,
    0x66cc00,
    0x88ee00,
    0x444488,
    0xeecc00,
    0xcccc00,
    0xccccee, /* UI highlight; duplicates colour 14 */
    0x6666aa,
    0x8888cc,
    0xaaaaee,
    0x880000,
    0xccccee, /* brightest relief shade */
    0x000022
};
#endif

/** Publish the fog lookup table address to the assembly rasterizer. */
void SetupPaletteLookup(void)
{
    FogTable = (unsigned int)g_fogTable;
}

/** Program sixteen RGB values into the active mode's logical colours. */
static void ProgramPalette(const unsigned int *colors)
{
    typedef struct PalEntry
    {
        unsigned char VDU, INDEX, MODE, R, G, B;
    } PalEntry;

    PalEntry pal;
    int i = 0;

    pal.VDU = 19;
    pal.MODE = 16;

    rin.r[0] = (unsigned int)&pal;
    rin.r[1] = 6;

    for (i = 0; i < 16; ++i)
    {
        pal.R = (colors[i] >> 16) & 0xFF;
        pal.G = (colors[i] >> 8) & 0xFF;
        pal.B = colors[i] & 0xFF;
        pal.INDEX = i;

        err = _kernel_swi(OS_WriteN, &rin, &rout);
    }
}

/** Program the sixteen configurable logical colours used by the renderer.
 *
 * The Archimedes 256-colour palette exposes sixteen mostly configurable
 * colours; VIDC derives the remaining entries as a house mixture.
 */
void SetPalette(void)
{
    ProgramPalette(inputPalette);
}

#ifndef PAL_256
/** Install the Amiga map-screen palette used by the relief view. */
void SetMapPalette(void)
{
    ProgramPalette(mapPalette);
}

/** Toggle the assembly rasterizer between the two concatenated lookup tables. */
void ToggleFogLookupBlend(void)
{
    unsigned int straightTable = (unsigned int)g_fogTable;
    unsigned int interpolatedTable =
        (unsigned int)(g_fogTable + FOG_LOOKUP9_BLEND_TABLE_WORDS);

    FogTable = FogTable == straightTable ? interpolatedTable : straightTable;
}
#endif

/** Dump the current hardware palette and logical-colour match counts. */
void Save256(void)
{
    unsigned int i, j, h;
    char hex[200];
    FILE *file;
    char buf[256];
    char *ptr;
    const char *filename = "colors_txt";

    sprintf(&buf[0], "%s.%s", gBaseDirectoryPath, filename);
    ptr = &buf[0];

    unsigned char originalFound[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0};

    file = fopen(ptr, "w");
    if (!file)
        return;

    for (i = 0; i < 256; i++)
    {
        rin.r[0] = i;
        rin.r[1] = 16;
        err = _kernel_swi(OS_ReadPalette, &rin, &rout);
        h = ((rout.r[2] >> 8) & 0xFF) << 16 | ((rout.r[2] >> 16) & 0xFF) << 8 |
            ((rout.r[2] >> 24) & 0xFF);

        for (j = 0; j < 16; ++j)
        {
            if (h == inputPalette[j])
            {
                ++originalFound[j];
                break;
            }
        }

        sprintf(hex, "%06X\n", h);
        fputs(hex, file);
    }

    for (i = 0; i < 16; ++i)
    {
        if (originalFound[i] > 0)
            sprintf(hex, "%d: %d times\n", i, originalFound[i]);
        else
            sprintf(hex, "%d: not found\n", i);

        fputs(hex, file);
    }

    fclose(file);
}

/** Load the precomputed Bayer-dithered distance-fog table.
 * @return Zero on success; non-zero if the lookup asset cannot be opened.
 */
int LoadFogLookup(void)
{
    FILE *file;
    char buf[256];
    char *ptr;
    const char *filename =
#ifdef PAL_256
        "assets.lookup";
#else
        "assets.lookup9";
#endif // PAL_256

    sprintf(&buf[0], "%s.%s", gBaseDirectoryPath, filename);
    ptr = &buf[0];

    file = fopen(ptr, "r");
    if (file == NULL)
    {
        printf("Failed to open file: %s\n", ptr);
        return 1;
    }

    fread(g_fogTable, sizeof *g_fogTable,
          sizeof g_fogTable / sizeof *g_fogTable, file);

    fclose(file);

    return 0;
}

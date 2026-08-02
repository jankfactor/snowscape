#ifndef PALETTE_H
#define PALETTE_H

extern char *gBaseDirectoryPath;

extern unsigned int *g_fogTable;

/** Allocate or release the distance-fog lookup table.
 * @param allocating Non-zero to allocate; zero to release.
 */
void SetupPaletteLookup(int allocating);
/** Program the application's logical colours into the RISC OS palette. */
void SetPalette(void);
#ifndef PAL_256
/** Install the temporary blue relief-map palette. */
void SetMapPalette(void);
#endif
/** Write the hardware palette and logical-colour match counts to a file. */
void Save256(void);
/** Load the precomputed Bayer-dithered fog lookup table.
 * @return Zero on success; non-zero if the lookup asset cannot be opened.
 */
int LoadFogLookup(void);

#endif // PALETTE_H

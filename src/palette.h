#ifndef PALETTE_H
#define PALETTE_H

extern char *gBaseDirectoryPath;

extern unsigned int *g_fogTable;

void SetupPaletteLookup(int allocating);
void SetPalette(void);
void Save256(void);
int LoadFogLookup(void);

#endif // PALETTE_H

#ifndef GUARD_GUNK_H
#define GUARD_GUNK_H

// Gunk's Ruby: shared switches and helpers for the quality-of-life changes.

// VAR_GUNK_SETTINGS bits
#define GUNK_ENDLESS_REPEL (1 << 0)
#define GUNK_EXP_ALL       (1 << 1)

bool8 GunkSettingOn(u16 bit);
void GunkToggleSetting(u16 bit);
u16 GunkCheckLastRepel(void);
void GunkUseLastRepel(void);

#endif // GUARD_GUNK_H

#ifndef GAME_RTL_H
#define GAME_RTL_H

#include "common_cpu_infra.h"

/* One frame of Super Mario World 2: Yoshi's Island */
void GameRunOneFrame(void);
void GameDrawPpuFrame(void);
void GameSessionReset(void);
uint32_t GameGetResumePc(void);

extern const RtlGameInfo kGameInfo;

/* Host presentation hook, called before each visible scanline is drawn.
 * NULL (the default) changes nothing. */
extern void (*g_yi_ppu_line_hook)(int line);

#endif /* GAME_RTL_H */


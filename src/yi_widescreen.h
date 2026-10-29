#ifndef YI_WIDESCREEN_H
#define YI_WIDESCREEN_H

/* Yoshi's Island widescreen presentation policy (EXPERIMENTAL, host-only).
 * Reads guest state, never writes it: with g_ws_extra == 0 nothing here
 * touches the PPU or ws_shadow and output stays bit-identical. */

#include <stdbool.h>

/* Once per frame, before RtlRunFrame: applies the margin and the per-layer
 * policy for the frame about to be rendered. */
void YiWidescreenBeginFrame(void);

/* Draw sprites the game culls at the native screen edge into the side
 * margins (default on; only has an effect while widescreen is active).
 * Call before the first YiWidescreenBeginFrame. */
void YiWidescreenSetSpritesEnabled(bool enabled);

#endif  // YI_WIDESCREEN_H

/* Yoshi's Island widescreen presentation policy (EXPERIMENTAL, host-only).
 *
 * BG1 (the level layer) is a 64x32 tilemap of 8x8 entries at VRAM $6800 that
 * the game streams one 16px column past the native edge (check_new_row_column,
 * $10:9058) and one native-width row (init_new_row). Everything further out
 * in the side margins is stale tilemap. The whole level's MAP16 layout stays
 * resident in WRAM ($7F:8000, indexed through the screen ID table at GSU RAM
 * $0CAA), and MAP16 -> tilemap is a pure ROM table lookup (GSU routine
 * $09:FA28, tables at $4C:32A4 / $4C:33F2). The margins are therefore filled
 * with exactly the entries the game would have streamed (WIDESCREEN_PATTERNS
 * P2), while the native 256 columns keep reading real VRAM.
 *
 * World position comes from the scroll the PPU renders with, with only the
 * high bits taken from the WRAM camera ($39/$3B) (P1, P4). The game rewrites
 * the scroll around line 216, so the position is refreshed per scanline.
 *
 * BG3 holds a static, non-streamed foreground layer during gameplay, so its
 * own tilemap wrap is the authentic wider view; it is widened only there.
 *
 * Sprites: the game builds a 16-bit OAM buffer (GSU RAM $0200, 256 entries
 * of X, Y, OAM bytes 3/4, size/order word) and copy_oam_buffer (GSU
 * $08:B1EF) converts it to OAM, dropping every entry outside X -15..255.
 * A presentation-replay prepare callback snapshots that buffer read-only
 * when the task starts (declining the replay), and the PPU line enhancer
 * draws the dropped entries into the margin columns only. Before a frame
 * uses a snapshot it is checked against the OAM about to be rendered by
 * re-running copy_oam_buffer's conversion; on any mismatch no margin
 * sprites are drawn that frame. */
#include "yi_widescreen.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "common_rtl.h"
#include "game_rtl.h"
#include "widescreen.h"
#include "snes/snes.h"
#include "snes/cart.h"
#include "snes/ppu.h"
#include "snes/superfx.h"
#include "snes/ws_shadow.h"

enum {
  kYiGameModeLevel = 0x0F,
  kYiBg1MapBase = 0x6800,        /* BG1SC $69: $6800, 64x32 */
  kYiLevelMapWram = 0x18000,     /* $7F:8000 within g_ram */
  kYiScreenTableGsuRam = 0x0CAA, /* screen# -> screen ID, $80 = no screen */
  kYiMap16PageTable = 0x32A4,    /* GSU ROM bank $4C */
  kYiMap16Base = 0x33F2,
  kYiScreensX = 16, kYiScreensY = 8,
  kYiOamBufferGsuRam = 0x0200,   /* 256 entries x 8 bytes */
  kYiOamBufferEntries = 256,
  kYiCopyOamBank = 0x08, kYiCopyOamAddress = 0xB1EF,
  kYiFirstBufferOamSlot = 16,    /* slots 0-15 are written directly */
};

static bool s_bg1_active;

static uint16_t WramWord(uint32_t offset) {
  return g_ram[offset] | g_ram[offset + 1] << 8;
}

static uint8_t GsuRom(const SuperFx *fx, uint16_t address) {
  return fx->rom[(0x4C0000u | address) & fx->rom_mask];
}

/* One 8x8 tilemap entry of a MAP16 tile: qx/qy select the quadrant. */
static uint16_t Map16Entry(const SuperFx *fx, uint16_t map16, int qx, int qy) {
  uint16_t page = (uint16_t)(kYiMap16PageTable + (map16 >> 8) * 2);
  uint16_t page_offset = GsuRom(fx, page) | GsuRom(fx, (uint16_t)(page + 1)) << 8;
  uint16_t a = (uint16_t)(kYiMap16Base + page_offset + (map16 & 0xFF) * 8 +
                          qy * 4 + qx * 2);
  return GsuRom(fx, a) | GsuRom(fx, (uint16_t)(a + 1)) << 8;
}

/* Tilemap entry the game would stream for the 8x8 world tile (tx, ty).
 * Cells outside the level resolve to MAP16 $0000, the engine's empty tile. */
static uint16_t LevelTileEntry(const SuperFx *fx, int32_t tx, int32_t ty) {
  uint16_t map16 = 0;
  int32_t tx16 = tx >> 1, ty16 = ty >> 1;
  if (tx >= 0 && ty >= 0 && tx16 < kYiScreensX * 16 && ty16 < kYiScreensY * 16) {
    uint8_t screen_id = fx->ram[kYiScreenTableGsuRam + ((ty16 >> 4) << 4 | (tx16 >> 4))];
    if (!(screen_id & 0x80))
      map16 = WramWord(kYiLevelMapWram + ((screen_id & 0x3F) << 9 |
                                          (ty16 & 15) << 5 | (tx16 & 15) << 1));
  }
  return Map16Entry(fx, map16, tx & 1, ty & 1);
}

/* P1: phase from the PPU scroll, high bits from the WRAM camera. */
static int32_t UnwrapScroll(uint16_t camera, uint16_t scroll) {
  int32_t d = (scroll - camera) & 0x3FF;
  return camera + (d >= 0x200 ? d - 0x400 : d);
}

static int32_t Bg1WorldX(void) { return UnwrapScroll(WramWord(0x39), g_ppu->hScroll[0]); }
static int32_t Bg1WorldY(void) { return UnwrapScroll(WramWord(0x3B), g_ppu->vScroll[0]); }

static bool LevelLayoutMatches(void) {
  return g_ram[0x0118] == kYiGameModeLevel && PPU_mode(g_ppu) == 1 &&
         !PPU_bigTiles(g_ppu, 0) && PPU_bgTilemapWider(g_ppu, 0) &&
         !PPU_bgTilemapHigher(g_ppu, 0) &&
         PPU_bgTilemapAdr(g_ppu, 0) == kYiBg1MapBase &&
         g_snes->cart && g_snes->cart->superfx;
}

static void UpdateLineEnhancer(int line);

static void OnPpuLine(int line) {
  if (s_bg1_active)
    WsShadowSetWorld(0, (uint32_t)Bg1WorldX(), (uint32_t)Bg1WorldY());
  UpdateLineEnhancer(line);
}

static uint16_t Bg1VramEntry(int32_t tx, int32_t ty) {
  int col = tx & 63, row = ty & 31;
  return g_ppu->vram[kYiBg1MapBase + (col & 32 ? 0x400 : 0) + row * 32 + (col & 31)];
}

static void FillBg1Margins(const SuperFx *fx, int32_t wx, int32_t wy) {
  /* WsShadowFrame captures 32 native columns from VRAM. The partially
   * visible overhang column (fine scroll != 0) is beyond its capture limit;
   * it is part of the authentic picture, so it also comes from VRAM. Every
   * column further out (plus slack for the per-line scroll change) comes
   * from the level map. */
  const int32_t tx0 = wx >> 3, ty0 = wy >> 3;
  const int32_t reach = (g_ws_extra + 16 + 7) >> 3;
  for (int32_t ty = ty0 - 2; ty <= ty0 + 31; ty++) {
    if (ty < 0)
      continue;
    for (int32_t tx = tx0 - reach; tx <= tx0 + 32 + reach; tx++) {
      if (tx >= tx0 && tx < tx0 + 32)
        tx = tx0 + 32;
      if (tx < 0)
        continue;
      bool overhang = tx == tx0 + 32 && (wx & 7) && ty >= ty0 && ty <= ty0 + 28;
      uint16_t entry = overhang ? Bg1VramEntry(tx, ty) : LevelTileEntry(fx, tx, ty);
      WsShadowForceTile(0, (uint32_t)tx, (uint32_t)ty, entry);
    }
  }
}

typedef struct YiMarginSprite {
  int16_t x;
  uint8_t oam_y;  /* OAM byte 1 as copy_oam_buffer stores it (Y - 1) */
  uint8_t large;
  uint16_t attr;  /* OAM bytes 2/3: tile, vhoopppN */
} YiMarginSprite;

static bool s_sprites_enabled = true;
static bool s_sprites_installed;
static uint8_t s_oam_buffer[kYiOamBufferEntries * 8];
static bool s_oam_buffer_valid;
/* Entries copy_oam_buffer dropped for the frame being rendered, in drawing
 * order: lowest OAM priority first. */
static YiMarginSprite s_margin_sprites[kYiOamBufferEntries];
static int s_margin_sprite_count;
static uint16_t s_line_obj[kPpuBufWidth];
static uint16_t s_bg1_blank_entry;

void YiWidescreenSetSpritesEnabled(bool enabled) {
  s_sprites_enabled = enabled;
}

static bool CaptureOamBuffer(void *context, const SuperFx *source, uint8_t *private_ram) {
  (void)context;
  (void)private_ram;
  memcpy(s_oam_buffer, source->ram + kYiOamBufferGsuRam, sizeof(s_oam_buffer));
  s_oam_buffer_valid = true;
  return false;  /* snapshot only; no replay */
}

static int16_t OamBufferWord(int entry, int word) {
  const uint8_t *p = s_oam_buffer + entry * 8 + word * 2;
  return (int16_t)(p[0] | p[1] << 8);
}

/* Re-run copy_oam_buffer's conversion on the snapshot. Returns true iff it
 * yields exactly OAM slots 16-127 as the PPU is about to render them, and
 * collects the entries it drops for their X position. */
static bool BuildMarginSprites(void) {
  YiMarginSprite forward[kYiOamBufferEntries], reverse[kYiOamBufferEntries];
  int num_forward = 0, num_reverse = 0;
  bool used[128] = {false};
  int next_forward = kYiFirstBufferOamSlot, next_reverse = 127;
  for (int i = 0; i < kYiOamBufferEntries && next_forward <= next_reverse; i++) {
    int16_t x = OamBufferWord(i, 0), y = OamBufferWord(i, 1);
    uint16_t attr = (uint16_t)OamBufferWord(i, 2), order = (uint16_t)OamBufferWord(i, 3);
    if (y < -15 || y >= 240)
      continue;
    YiMarginSprite s = {x, (uint8_t)(y - 1), (uint8_t)((order >> 1) & 1), attr};
    bool at_end = (order & 0x4000) != 0;
    if (x < -15 || x >= 256) {
      if (x == INT16_MIN)  /* $8000: unused buffer entry */
        continue;
      if (at_end)
        reverse[num_reverse++] = s;
      else
        forward[num_forward++] = s;
      continue;
    }
    int slot = at_end ? next_reverse-- : next_forward++;
    uint16_t oam0 = g_ppu->oam[slot * 2], oam1 = g_ppu->oam[slot * 2 + 1];
    int high = (g_ppu->highOam[slot >> 2] >> ((slot & 3) * 2)) & 3;
    if (oam0 != ((uint8_t)x | s.oam_y << 8) || oam1 != attr ||
        high != (((order & 0xff) | (x < 0)) & 3))
      return false;
    used[slot] = true;
  }
  for (int slot = kYiFirstBufferOamSlot; slot < 128; slot++) {
    if (!used[slot] && (g_ppu->oam[slot * 2] >> 8) != 0xF0)
      return false;
  }
  /* OAM priority: forward slots beat reverse slots, a lower slot wins, and
   * reverse slots are handed out from 127 downwards. */
  int n = 0;
  for (int i = 0; i < num_reverse; i++)
    s_margin_sprites[n++] = reverse[i];
  for (int i = num_forward - 1; i >= 0; i--)
    s_margin_sprites[n++] = forward[i];
  s_margin_sprite_count = n;
  return true;
}

/* Rasterize the dropped entries for one scanline, margin columns only,
 * exactly as ppu_evaluateSprites would. Returns whether any pixel landed. */
static bool DrawMarginSpritesLine(const Ppu *ppu, unsigned y) {
  bool any = false;
  static const uint8_t kObjSizes[8][2] = {
    {8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}, {16, 32}, {16, 32}
  };
  const int left = -(int)ppu->extraLeftCur, right = 256 + ppu->extraRightCur;
  memset(&s_line_obj[kPpuExtraLeftRight + left], 0, -left * sizeof(uint16_t));
  memset(&s_line_obj[kPpuExtraLeftRight + 256], 0, (right - 256) * sizeof(uint16_t));
  const uint8_t line = (uint8_t)(y - 1);
  for (int i = 0; i < s_margin_sprite_count; i++) {
    const YiMarginSprite *s = &s_margin_sprites[i];
    int size = kObjSizes[PPU_objSize(ppu)][s->large];
    int height = PPU_objInterlace(ppu) ? size / 2 : size;
    uint8_t row = (uint8_t)(line - s->oam_y);
    if (row >= height || s->x + size <= left || s->x >= right)
      continue;
    if (PPU_objInterlace(ppu))
      row = row * 2 + (ppu->evenFrame ? 0 : 1);
    if (s->attr & 0x8000)
      row = size - 1 - row;
    int obj_adr = (s->attr & 0x100) ? PPU_objTileAdr2(ppu) : PPU_objTileAdr1(ppu);
    int palette_base = 0x80 + 16 * ((s->attr & 0xe00) >> 9);
    int prio = SPRITE_PRIO_TO_PRIO((s->attr & 0x3000) >> 12, (s->attr & 0x800) == 0);
    uint16_t z = (uint16_t)(palette_base + (prio << 8));
    for (int col = 0; col < size; col += 8) {
      int sx0 = s->x + col;
      if (sx0 + 8 <= left || sx0 >= right || (sx0 >= 0 && sx0 + 8 <= 256))
        continue;
      int used_col = (s->attr & 0x4000) ? size - 1 - col : col;
      int tile = ((((s->attr & 0xff) >> 4) + (row >> 3)) << 4) |
                 (((s->attr & 0xf) + (used_col >> 3)) & 0xf);
      const uint16_t *addr = &PpuRenderVram(ppu)[(obj_adr + tile * 16 + (row & 7)) & 0x7fff];
      uint32_t plane = addr[0] | addr[8] << 16;
      for (int px = 0; px < 8; px++) {
        int sx = sx0 + px;
        if ((sx >= 0 && sx < 256) || sx < left || sx >= right)
          continue;
        uint32_t bits = plane >> ((s->attr & 0x4000) ? px : 7 - px);
        int pixel = (bits & 1) | (bits >> 7 & 2) | (bits >> 14 & 4) | (bits >> 21 & 8);
        if (pixel) {
          s_line_obj[kPpuExtraLeftRight + sx] = (uint16_t)(z + pixel);
          any = true;
        }
      }
    }
  }
  return any;
}

/* Per-pixel form of PpuWindows_Calc for a margin column: true when `layer`
 * is masked at screen x on this screen. Same pinned-edge widening, window
 * inversion and WBGLOG/WOBJLOG combination as the native span builder. */
static bool LayerMaskedAt(const Ppu *ppu, unsigned layer, bool sub, int x) {
  if (!IS_SCREEN_WINDOWED(ppu, sub, layer))
    return false;
  const uint32_t flags = GET_WINDOW_FLAGS(ppu, layer);
  int w1l = ppu->window1left, w1r = ppu->window1right;
  int w2l = ppu->window2left, w2r = ppu->window2right;
  PpuWidescreenAdjustPinnedWindowEdges(-(int)ppu->extraLeftCur, 256 + ppu->extraRightCur,
                                       &w1l, &w1r, &w2l, &w2r);
  bool in1 = (flags & kWindow1Enabled) && w1l <= w1r && x >= w1l && x <= w1r;
  bool in2 = (flags & kWindow2Enabled) && w2l <= w2r && x >= w2l && x <= w2r;
  if ((flags & (kWindow1Enabled | kWindow1Inversed)) == (kWindow1Enabled | kWindow1Inversed))
    in1 = !in1;
  if ((flags & (kWindow2Enabled | kWindow2Inversed)) == (kWindow2Enabled | kWindow2Inversed))
    in2 = !in2;
  if ((flags & (kWindow1Enabled | kWindow2Enabled)) != (kWindow1Enabled | kWindow2Enabled))
    return in1 || in2;
  switch ((ppu->wbgobjlog >> (layer * 2)) & 3) {
    case 0: return in1 || in2;
    case 1: return in1 && in2;
    case 2: return in1 != in2;
    default: return in1 == in2;
  }
}

/* With a line enhancer registered the PPU keeps BG1 inside the native 256
 * columns and the host supplies BG1's margin pixels (PpuViewportAllows).
 * This draws them exactly as PpuDrawBackground_4bpp would from the same
 * ws_shadow entries: Mode 1 BG1, 8x8 tiles, no mosaic. */
static void DrawBg1Margins(Ppu *ppu, unsigned y, bool sub) {
  if (!IS_SCREEN_ENABLED(ppu, sub, 0))
    return;
  const int left = -(int)ppu->extraLeftCur, right = 256 + ppu->extraRightCur;
  const int32_t wx = (int32_t)WsShadowWorldX(0);
  const int32_t wy = (int32_t)WsShadowWorldY(0) + (int32_t)y;
  const unsigned fine_y = (y + ppu->vScroll[0]) & 7;
  const int tile_base = PPU_bgTileAdr(ppu, 0);
  PpuZbufType *dst = ppu->bgBuffers[sub].data;
  for (int sx = left; sx < right; sx++) {
    if (sx == 0)
      sx = 256;
    if (LayerMaskedAt(ppu, 0, sub, sx))
      continue;
    const int32_t mx = wx + sx;
    uint16_t tile = s_bg1_blank_entry;
    if (mx >= 0 && wy >= 0 &&
        !WsShadowLookupWorldTile(0, (uint32_t)mx >> 3, (uint32_t)wy >> 3, &tile))
      tile = s_bg1_blank_entry;
    const int ta = tile_base + ((tile & 0x8000) ? 7 - fine_y : fine_y);
    const uint16_t *addr = &PpuRenderVram(ppu)[(ta + (tile & 0x3ff) * 16) & 0x7fff];
    const uint32_t bits = addr[0] | addr[8] << 16;
    const int bit = (tile & 0x4000) ? (mx & 7) : 7 - (mx & 7);
    const int pixel = (bits >> bit & 1) | (bits >> (7 + bit) & 2) |
                      (bits >> (14 + bit) & 4) | (bits >> (21 + bit) & 8);
    const PpuZbufType z = (PpuZbufType)(((tile & 0x2000) ? 0xc000 : 0x8000) +
                                        ((tile & 0x1c00) >> 6));
    PpuZbufType *d = &dst[kPpuExtraLeftRight + sx];
    if (pixel && z > *d)
      *d = (PpuZbufType)(z + pixel);
  }
}

/* Merge like PpuDrawSprites: z comparison against the layers already drawn.
 * A native sprite pixel in the margin keeps precedence. */
static void MarginSpriteEnhancer(Ppu *ppu, uint y, bool sub, void *context) {
  (void)context;
  DrawBg1Margins(ppu, y, sub);
  if (!IS_SCREEN_ENABLED(ppu, sub, 4))
    return;
  const int left = -(int)ppu->extraLeftCur, right = 256 + ppu->extraRightCur;
  PpuZbufType *dst = ppu->bgBuffers[sub].data;
  for (int sx = left; sx < right; sx++) {
    if (sx == 0)
      sx = 256;
    int i = kPpuExtraLeftRight + sx;
    uint16_t v = s_line_obj[i];
    if (v && !(ppu->objBuffer.data[i] & 0xff) && v > dst[i] &&
        !LayerMaskedAt(ppu, 4, sub, sx))
      dst[i] = v;
  }
}

/* Lines on which the enhancer path reproduces BG1 exactly: everywhere else
 * (mosaic, other modes) the line keeps the plain Phase 2A rendering and
 * simply shows no margin sprites. */
static bool LineSupportsMarginSprites(const Ppu *ppu) {
  return PPU_mode(ppu) == 1 && !PPU_bigTiles(ppu, 0) &&
         !(PPU_mosaicEnabled(ppu, 0) && PPU_mosaicSize(ppu) > 1);
}

/* Runs before each scanline is drawn, after that line's HDMA: registers
 * the enhancer only on lines that actually carry a margin sprite pixel. */
static void UpdateLineEnhancer(int line) {
  if (!s_sprites_installed)
    return;
  bool needed = s_margin_sprite_count && line >= 1 && line <= 224 &&
                LineSupportsMarginSprites(g_ppu) &&
                DrawMarginSpritesLine(g_ppu, (unsigned)line);
  PpuSetWidescreenLineEnhancer(g_ppu, needed ? MarginSpriteEnhancer : NULL, NULL);
}

static void UpdateMarginSprites(void) {
  s_margin_sprite_count = 0;
  if (!s_sprites_enabled)
    return;
  if (!s_sprites_installed) {
    SuperFx *fx = g_snes->cart->superfx;
    superfx_set_enhancement_mode(fx, kSuperFxEnhancement_PresentationReplay);
    superfx_set_presentation_replay(fx, kYiCopyOamBank, kYiCopyOamAddress,
                                    CaptureOamBuffer, NULL, NULL);
    s_sprites_installed = true;
  }
  if (s_oam_buffer_valid && !BuildMarginSprites())
    s_margin_sprite_count = 0;
}

void YiWidescreenBeginFrame(void) {
  g_yi_ppu_line_hook = OnPpuLine;
  PpuSetExtraSpace(g_ppu, (uint16_t)g_ws_extra);
  if (!g_ws_active)
    return;

  if (!LevelLayoutMatches()) {
    if (s_bg1_active) {
      WsShadowReset();
      s_bg1_active = false;
    }
    PpuSetWidescreenBg3Widen(g_ppu, 0);
    s_margin_sprite_count = 0;
    return;
  }
  if (!s_bg1_active) {
    WsShadowReset();  /* new level or retry: world keys start over */
    s_bg1_active = true;
  }

  const SuperFx *fx = g_snes->cart->superfx;
  const int32_t wx = Bg1WorldX(), wy = Bg1WorldY();
  s_bg1_blank_entry = LevelTileEntry(fx, -1, -1);
  WsShadowSetBlankTile(0, s_bg1_blank_entry);
  WsShadowSetWorld(0, (uint32_t)wx, (uint32_t)wy);
  WsShadowFrame(g_ppu);
  FillBg1Margins(fx, wx, wy);

  PpuSetWidescreenBg3Widen(g_ppu, 1);
  UpdateMarginSprites();
}

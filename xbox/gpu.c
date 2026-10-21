// FOLD on the original Xbox's GPU (NV2A) through nxdk's pbkit: gfx.h's GfxGpu. Each frame: the background gradient,
// then the 3D scene (the level view's triangles and lines, already projected and clipped by gfx.c), then the 2D layer
// (gfx_px, see-through where nothing was drawn) over everything as one texture. Set up after the mh1-xbox port's
// gfx_nv2a.c (same owner), which runs on the real console; its hardware lessons are kept: texture pitches padded to
// 64 bytes, the vertex cache broken before each draw, texture memory never rewritten in place (new memory, the old
// freed a few frames later), waits that give the CPU away.
#include "../app/gfx.h"
#include <hal/video.h>
#include <pbkit/pbkit.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>
#include "gpu.h"

#define MASK(mask, val) (((val) << (ffs(mask) - 1)) & (mask))
#define GPU_MAXRAM 0x03FFAFFF
#define RING_BYTES (4u << 20)
#define ZMAX 16777215.0f
#define PITCH64(n) (((n) + 63u) & ~63u)

typedef struct { float pos[4]; uint8_t col[4]; float st[2]; } vtx;   // 28 bytes: clip position, colour, texel coords
typedef struct { const uint32_t *px; int w, h; uint32_t sum, frame; uint8_t *mem; uint32_t pitch; } GTex;

static const uint32_t vs_program[] = {
#include "vs.inl"
};

static uint8_t *ring; static uint32_t ringUsed;
static vtx *bStart; static int bN, bMode = -1; static GTex *bTex;   // the batch being gathered (one draw)
static int vpX = 1 << 30, vpY;                                    // the view offset the constants hold
static GTex cache[384]; static int ncache; static GTex white;
static uint8_t *layer[3]; static int layerK;                       // the 2D layer: three textures in turn
static uint32_t frameNo;

static void push1(uint32_t reg, uint32_t v) { uint32_t *p = pb_begin(); p = pb_push1(p, reg, v); pb_end(p); }
static void wait_idle(void) { while (pb_busy()) SwitchToThread(); }

// GPU memory given back a few frames late: NV2A caches texels by address and doesn't notice the CPU writing there again
#define DEFER 3
static struct { void *p; uint32_t f; } later[256]; static int nlater;
static void free_later(void *p) {
  if (!p) return;
  if (nlater == 256) { wait_idle(); MmFreeContiguousMemory(p); return; }
  later[nlater].p = p; later[nlater].f = frameNo; nlater++;
}
static void free_old(void) {
  int k = 0;
  for (int i = 0; i < nlater; i++) if (frameNo - later[i].f >= DEFER) MmFreeContiguousMemory(later[i].p); else later[k++] = later[i];
  nlater = k;
}
static void *gpu_alloc(size_t n) { return MmAllocateContiguousMemoryEx(n, 0, GPU_MAXRAM, 0, PAGE_READWRITE | PAGE_WRITECOMBINE); }

// the vertex program's constants: c0..3 the viewport (row vectors), c4 fog (k, b), c5 fog colour, c6 cgc's (1, 0)
static void constants(int ox, int oy, int fog) {
  float m[16] = { GW / 2.0f, 0, 0, 0,  0, -GH / 2.0f, 0, 0,  0, 0, ZMAX / 2, 0,  GW / 2.0f + ox, GH / 2.0f + oy, ZMAX / 2, 1 };
  float kb[4] = { 0, 0, 0, 0 }, fc[4] = { 0, 0, 0, 0 }, c6[4] = { 1, 0, 0, 0 };
  if (fog) {                                    // as gfx.c: towards the fog colour from start, 0.85 of the way at end
    uint32_t c; float s, e; gfx_fog_get(&c, &s, &e);
    if (e > s && s < 1e8f) { kb[0] = 0.85f / (e - s); kb[1] = -s * 0.85f / (e - s); }
    fc[0] = (c >> 16 & 255) / 255.0f; fc[1] = (c >> 8 & 255) / 255.0f; fc[2] = (c & 255) / 255.0f;
  }
  uint32_t *p = pb_begin();
  p = pb_push1(p, NV097_SET_TRANSFORM_CONSTANT_LOAD, 96);
  pb_push(p++, NV097_SET_TRANSFORM_CONSTANT, 16); memcpy(p, m, 64); p += 16;
  pb_push(p++, NV097_SET_TRANSFORM_CONSTANT, 12); memcpy(p, kb, 16); memcpy(p + 4, fc, 16); memcpy(p + 8, c6, 16); p += 12;
  pb_end(p);
}

static void bind(const GTex *t) {
  uint32_t fmt = MASK(NV097_SET_TEXTURE_FORMAT_COLOR, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8)
               | MASK(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA, 2) | NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE
               | MASK(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY, 2) | MASK(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS, 1);
  uint32_t *p = pb_begin();
  p = pb_push2(p, NV097_SET_TEXTURE_OFFSET, (uint32_t)(uintptr_t)t->mem & 0x03ffffff, fmt);
  p = pb_push1(p, NV097_SET_TEXTURE_CONTROL1, t->pitch << 16);
  p = pb_push1(p, NV097_SET_TEXTURE_IMAGE_RECT, ((uint32_t)t->w << 16) | (uint32_t)t->h);
  p = pb_push1(p, NV097_SET_TEXTURE_ADDRESS, 0x00030303);   // clamp (rect textures)
  p = pb_push1(p, NV097_SET_TEXTURE_CONTROL0, 0x4003ffc0);
  p = pb_push1(p, NV097_SET_TEXTURE_FILTER, 0x01012000);    // nearest: the pixel look
  pb_end(p);
}

// a texture for gfx.c's Tex: kept by its pixels' address, sent again (to new memory) when its pixels change
static void tex_fill(GTex *g) {
  g->pitch = PITCH64((uint32_t)g->w * 4);
  uint8_t *m = gpu_alloc((size_t)g->pitch * g->h);
  if (!m) return;
  for (int y = 0; y < g->h; y++) { uint32_t *row = (uint32_t *)(m + (size_t)y * g->pitch); for (int x = 0; x < g->w; x++) row[x] = g->px[y * g->w + x] | 0xFF000000u; }
  free_later(g->mem); g->mem = m;
}
static GTex *tex_get(const Tex *t) {
  if (!t) return &white;
  GTex *g = NULL;
  for (int i = 0; i < ncache; i++) if (cache[i].px == t->px && cache[i].w == t->w && cache[i].h == t->h) { g = &cache[i]; break; }
  if (!g) {
    if (ncache == (int)(sizeof cache / sizeof cache[0])) return &white;
    g = &cache[ncache++]; memset(g, 0, sizeof *g); g->px = t->px; g->w = t->w; g->h = t->h; g->frame = frameNo - 1;
  }
  if (g->frame != frameNo) {                    // once a frame: has it changed?
    uint32_t s = 0; for (int i = 0; i < t->w * t->h; i++) s = s * 31 + t->px[i];
    if (!g->mem || s != g->sum) { g->sum = s; tex_fill(g); }
    g->frame = frameNo;
  }
  return g->mem ? g : &white;
}

static void flush(void) {
  if (!bN) return;
  uint32_t base = (uint32_t)(uintptr_t)bStart & 0x03ffffff, *p = pb_begin();
  p = pb_push1(p, NV097_BREAK_VERTEX_BUFFER_CACHE, 0);
  pb_push(p++, NV097_SET_VERTEX_DATA_ARRAY_FORMAT, 16);
  for (int i = 0; i < 16; i++) *p++ = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
  p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT + 0 * 4, MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F)
               | MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE, 4) | MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE, sizeof(vtx)));
  p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_OFFSET + 0 * 4, base);
  p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT + 3 * 4, MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL)
               | MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE, 4) | MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE, sizeof(vtx)));
  p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_OFFSET + 3 * 4, base + 16);
  p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT + 9 * 4, MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F)
               | MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE, 2) | MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE, sizeof(vtx)));
  p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_OFFSET + 9 * 4, base + 20);
  pb_end(p);
  p = pb_begin();
  p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_TRIANGLES);
  for (int i = 0, k = 0; i < bN; k++) {         // 255 vertices a call; a new pb_begin every 32 (pbkit's 128-word limit)
    int c = bN - i > 255 ? 255 : bN - i;
    if (k && k % 32 == 0) { pb_end(p); p = pb_begin(); }
    p = pb_push1(p, NV097_DRAW_ARRAYS, MASK(NV097_DRAW_ARRAYS_COUNT, c - 1) | MASK(NV097_DRAW_ARRAYS_START_INDEX, i));
    i += c;
  }
  p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
  pb_end(p);
  bStart = (vtx *)(ring + ringUsed); bN = 0;
}

// modes: 0 solid (depth test + write), 1 see-through (blend, no depth write), 2 solid without depth write (lines),
// 3 the 2D layer (no depth, blended: see-through by its top byte), 4 the background (no depth, no blending)
static void state(int mode, GTex *t) {
  int ox, oy; gfx_view_offset_get(&ox, &oy);
  if (mode == bMode && t == bTex && (mode >= 3 || (ox == vpX && oy == vpY))) return;
  flush();
  if (mode != bMode) {
    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_DEPTH_TEST_ENABLE, mode < 3);
    p = pb_push1(p, NV097_SET_DEPTH_MASK, mode == 0);
    p = pb_push1(p, NV097_SET_BLEND_ENABLE, mode == 1 || mode == 3);
    p = pb_push1(p, NV097_SET_BLEND_FUNC_SFACTOR, mode == 3 ? NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_SRC_ALPHA : NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA);
    p = pb_push1(p, NV097_SET_BLEND_FUNC_DFACTOR, mode == 3 ? NV097_SET_BLEND_FUNC_DFACTOR_V_SRC_ALPHA : NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA);
    pb_end(p);
  }
  if (mode >= 3) { constants(0, 0, 0); vpX = 1 << 30; }
  else if (ox != vpX || oy != vpY || bMode >= 3 || bMode < 0) { constants(ox, oy, 1); vpX = ox; vpY = oy; }
  if (t != bTex) bind(t);
  bMode = mode; bTex = t;
}
static vtx *room_for(int n) {                   // n more vertices in the batch (a full ring: draw, wait, start over)
  if (ringUsed + (uint32_t)n * sizeof(vtx) > RING_BYTES) { flush(); wait_idle(); ringUsed = 0; bStart = (vtx *)ring; }
  vtx *v = (vtx *)(ring + ringUsed); ringUsed += (uint32_t)n * sizeof(vtx); bN += n;
  return v;
}
static void put(vtx *o, float x, float y, float z, float w, uint32_t c, float s, float t) {
  o->pos[0] = x; o->pos[1] = y; o->pos[2] = z; o->pos[3] = w;
  o->col[0] = (uint8_t)(c >> 16); o->col[1] = (uint8_t)(c >> 8); o->col[2] = (uint8_t)c; o->col[3] = (uint8_t)(c >> 24);
  o->st[0] = s; o->st[1] = t;
}
static uint32_t scale(uint32_t c, float k) {    // as gfx.c's shade_c: each channel times k, at most 255
  if (k < 0) k = 0;
  int r = (int)((c >> 16 & 255) * k), g = (int)((c >> 8 & 255) * k), b = (int)((c & 255) * k);
  return (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b));
}

static void g_tri(const GfxVert v[3], const Tex *t, uint32_t colour, float shade, int mode, float alpha) {
  GTex *g = mode == 0 ? tex_get(t) : &white;
  state(mode == 1 ? 1 : 0, g);
  vtx *o = room_for(3);
  for (int i = 0; i < 3; i++) {
    uint32_t c;
    if (mode == 3) c = gfx_shade_lit(colour, v[i].u) | 0xFF000000u;            // lit per corner (u = the light)
    else if (mode == 1) c = colour | (uint32_t)(alpha * 255) << 24;
    else c = (t ? scale(0xFFFFFF, shade) : scale(colour, shade)) | 0xFF000000u;   // the texture times the shade
    put(&o[i], v[i].x, v[i].y, v[i].z, v[i].w, c, mode == 0 && t ? v[i].u * g->w : 0, mode == 0 && t ? v[i].v * g->h : 0);
  }
}
static void g_line(const GfxVert *a, const GfxVert *b, uint32_t colour, int thick) {
  int ox, oy; gfx_view_offset_get(&ox, &oy);
  state(2, &white);
  // across the line on screen, half the thickness each way, back to clip space at each end's own w; nudged forward
  float x0 = a->x / a->w, y0 = a->y / a->w, x1 = b->x / b->w, y1 = b->y / b->w;
  float dx = (x1 - x0) * GW / 2, dy = (y1 - y0) * GH / 2, l = dx * dx + dy * dy;
  if (l < 1e-6f) return;
  l = 1.0f / sqrtf(l);
  float hw = (thick < 1 ? 1 : thick) * 0.5f, nx = -dy * l * hw * 2 / GW, ny = dx * l * hw * 2 / GH;
  float z0 = a->z / a->w - 0.0006f, z1 = b->z / b->w - 0.0006f;
  vtx *o = room_for(6); uint32_t c = colour | 0xFF000000u;
  float P[4][3] = { { x0 - nx, y0 - ny, z0 }, { x0 + nx, y0 + ny, z0 }, { x1 + nx, y1 + ny, z1 }, { x1 - nx, y1 - ny, z1 } };
  float W[4] = { a->w, a->w, b->w, b->w };
  static const int ix[6] = { 0, 1, 2, 0, 2, 3 };
  for (int i = 0; i < 6; i++) { int k = ix[i]; put(&o[i], P[k][0] * W[k], P[k][1] * W[k], P[k][2] * W[k], W[k], c, 0, 0); }
}
static void quad2d(GTex *t, int mode) {         // a texture over the whole screen (mode 3 the layer, 4 the background)
  state(mode, t);
  vtx *o = room_for(6); uint32_t c = 0xFFFFFFFFu;
  float P[4][4] = { { -1, 1, 0, 0 }, { 1, 1, GW, 0 }, { 1, -1, GW, GH }, { -1, -1, 0, GH } };
  static const int ix[6] = { 0, 1, 2, 0, 2, 3 };
  for (int i = 0; i < 6; i++) { int k = ix[i]; put(&o[i], P[k][0], P[k][1], 0, 1, c, P[k][2], P[k][3]); }
}
static GTex bgTex;
static void g_background(const uint32_t *px) {
  if (bgTex.px != px || !bgTex.mem) { bgTex.px = px; bgTex.w = GW; bgTex.h = GH; tex_fill(&bgTex); }
  quad2d(&bgTex, 4);
}

static const GfxGpu G = { g_tri, g_line, g_background };

int gpu_init(void) {
  static const uint32_t one = 0xFFFFFFFFu;
  if (pb_init() != 0) return 0;              // (the video mode is set first: main_xbox.c)
  pb_show_front_screen();
  ring = gpu_alloc(RING_BYTES);
  for (int k = 0; k < 3; k++) layer[k] = gpu_alloc((size_t)GW * 4 * GH);
  if (!ring || !layer[0] || !layer[1] || !layer[2]) return 0;
  uint32_t *p = pb_begin();                     // the vertex program, then the pixel program's combiners
  p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_START, 0);
  p = pb_push1(p, NV097_SET_TRANSFORM_EXECUTION_MODE, MASK(NV097_SET_TRANSFORM_EXECUTION_MODE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_MODE_PROGRAM)
               | MASK(NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE_PRIV));
  p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN, 0);
  p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_LOAD, 0);
  pb_end(p);
  for (unsigned i = 0; i < sizeof vs_program / 16; i++) { p = pb_begin(); pb_push(p++, NV097_SET_TRANSFORM_PROGRAM, 4); memcpy(p, &vs_program[i * 4], 16); p += 4; pb_end(p); }
  p = pb_begin();
#include "ps.inl"
  pb_end(p);
  p = pb_begin();
  p = pb_push1(p, NV097_SET_CULL_FACE_ENABLE, 0);
  p = pb_push1(p, NV097_SET_DEPTH_FUNC, 0x203);           // GL_LEQUAL
  p = pb_push1(p, NV097_SET_ALPHA_TEST_ENABLE, 0);
  p = pb_push1(p, NV097_SET_BLEND_EQUATION, NV097_SET_BLEND_EQUATION_V_FUNC_ADD);
  p = pb_push1(p, NV097_SET_FOG_ENABLE, 0);               // fog is in the programs (COLOR1)
  p = pb_push1(p, NV097_SET_SPECULAR_ENABLE, 1);
  p = pb_push1(p, NV097_SET_TEXTURE_CONTROL0 + 64 * 1, 0x0003ffc0);
  p = pb_push1(p, NV097_SET_TEXTURE_CONTROL0 + 64 * 2, 0x0003ffc0);
  p = pb_push1(p, NV097_SET_TEXTURE_CONTROL0 + 64 * 3, 0x0003ffc0);
  pb_end(p);
  white.px = &one; white.w = white.h = 1; tex_fill(&white);
  gfx_set_gpu(&G);
  return 1;
}
void gpu_begin(void) {
  pb_wait_for_vbl();
  pb_reset();
  pb_target_back_buffer();
  wait_idle();
  frameNo++; free_old();
  ringUsed = 0; bStart = (vtx *)ring; bN = 0; bMode = -1; bTex = NULL; vpX = 1 << 30;
  push1(NV097_SET_DEPTH_MASK, 1);
  pb_erase_depth_stencil_buffer(0, 0, GW, GH);
  pb_fill(0, 0, GW, GH, 0xFF000000u);
}
void gpu_end(const uint32_t *px) {
  uint8_t *m = layer[layerK]; layerK = (layerK + 1) % 3;   // the 2D layer: one of three textures (not the one in use)
  memcpy(m, px, (size_t)GW * 4 * GH);
  static GTex lt; lt.px = px; lt.w = GW; lt.h = GH; lt.pitch = GW * 4; lt.mem = m;
  bTex = NULL;
  quad2d(&lt, 3);
  flush();
  wait_idle();
  while (pb_finished()) SwitchToThread();
}

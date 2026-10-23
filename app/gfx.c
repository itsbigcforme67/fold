// FOLD app: software renderer. See gfx.h.
#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
void gfx_hpanel(int x, int y, int w, int h, const char *title, uint32_t accent);

uint32_t gfx_px[GW * GH];
static float zbuf[GW * GH];
static Mat4 VP;
static uint32_t fogC; static float fogS = 1e9f, fogE = 2e9f;
static int vofsX, vofsY, clx0, cly0, clx1 = GW - 1, cly1 = GH - 1;
static const GfxGpu *gpu; static int inScene;   // a GPU takes the scene's triangles and lines (gfx_set_gpu)
void gfx_set_gpu(const GfxGpu *g) { gpu = g; }
void gfx_scene_begin(void) { inScene = gpu != NULL; }
void gfx_scene_end(void) { inScene = 0; }
void gfx_view_offset_get(int *dx, int *dy) { *dx = vofsX; *dy = vofsY; }
static const uint8_t *metal; static int metalW, metalH;
static uint32_t mixw(uint32_t a, uint32_t b, uint32_t w) {   // a towards b by w / 256 (red and blue in one multiply)
  uint32_t rb = (((a & 0xFF00FF) * (256 - w) + (b & 0xFF00FF) * w) >> 8) & 0xFF00FF;
  uint32_t g = (((a & 0x00FF00) * (256 - w) + (b & 0x00FF00) * w) >> 8) & 0x00FF00;
  return rb | g;
}
static uint32_t mix(uint32_t a, uint32_t b, float t) {   // a towards b by t
  if (t <= 0) return a; if (t >= 1) return b;
  return mixw(a, b, (uint32_t)(t * 256));
}
static uint32_t fogged(uint32_t c, float z) { return z <= fogS ? c : mix(c, fogC, (z - fogS) / (fogE - fogS) * 0.85f); }
void gfx_fog_get(uint32_t *c, float *start, float *end) { *c = fogC; *start = fogS; *end = fogE; }

static const char *FONT[128];
static void font_init(void) {
  static int done; if (done) return; done = 1;
  static const struct { char c; const char *b; } F[] = {
    {'A',"010101111101101"},{'B',"110101110101110"},{'C',"011100100100011"},{'D',"110101101101110"},{'E',"111100110100111"},
    {'F',"111100110100100"},{'G',"011100101101011"},{'H',"101101111101101"},{'I',"111010010010111"},{'J',"001001001101010"},
    {'K',"101101110101101"},{'L',"100100100100111"},{'M',"101111111101101"},{'N',"110101101101101"},{'O',"010101101101010"},
    {'P',"110101110100100"},{'Q',"010101101110011"},{'R',"110101110101101"},{'S',"011100010001110"},{'T',"111010010010010"},
    {'U',"101101101101111"},{'V',"101101101101010"},{'W',"101101111111101"},{'X',"101101010101101"},{'Y',"101101010010010"},
    {'Z',"111001010100111"},{'0',"111101101101111"},{'1',"010110010010111"},{'2',"110001010100111"},{'3',"110001010001110"},
    {'4',"101101111001001"},{'5',"111100110001110"},{'6',"011100111101111"},{'7',"111001010010010"},{'8',"111101111101111"},
    {'9',"111101111001110"},{'!',"010010010000010"},{':',"000010000010000"},{'-',"000000111000000"},{'.',"000000000000010"},
    {'/',"001001010100100"},{'+',"000010111010000"},{'>',"100010001010100"},{'<',"001010100010001"},{'?',"110001010000010"},
    {'(',"010100100100010"},{')',"010001001001010"},{'=',"000111000111000"},{',',"000000000010100"},{'#',"101111101111101"},
    {' ',"000000000000000"} };
  for (unsigned i = 0; i < sizeof F / sizeof F[0]; i++) FONT[(int)F[i].c] = F[i].b;
}

const char *gfx_glyph(int ch) { font_init(); if (ch >= 'a' && ch <= 'z') ch -= 32; return ch > 0 && ch < 128 && FONT[ch] ? FONT[ch] : FONT[' ']; }
void gfx_clear(uint32_t c) { for (int i = 0; i < GW * GH; i++) gfx_px[i] = c; }
void gfx_gradient(uint32_t top, uint32_t bottom) {   // made once per pair of colours, then copied
  static uint32_t bg[GW * GH], cTop = 1, cBot = 1;
  if (top != cTop || bottom != cBot) {
    cTop = top; cBot = bottom;
    for (int y = 0; y < GH; y++) {
      uint32_t row = mix(top, bottom, (float)y / GH);
      for (int x = 0; x < GW; x++) {
        float dx = (x - GW / 2) / (float)(GW / 2), dy = (y - GH / 2) / (float)(GH / 2), v = (dx * dx + dy * dy) * 0.35f;
        bg[y * GW + x] = mix(row, 0x000000, v > 0.6f ? 0.6f : v);
      }
    }
  }
  if (gpu) { gpu->background(bg); for (int i = 0; i < GW * GH; i++) gfx_px[i] = 0xFF000000u; return; }   // under the 2D layer
  memcpy(gfx_px, bg, sizeof bg);
}
void gfx_dim(int x, int y, int w, int h, float k) {
  uint32_t m = (uint32_t)((1 - k) * 256);
  int x0 = x < 0 ? 0 : x, x1 = x + w > GW ? GW : x + w;
  for (int j = y < 0 ? 0 : y; j < y + h && j < GH; j++) {
    uint32_t *r = gfx_px + j * GW;
    for (int i = x0; i < x1; i++) {
      uint32_t c = r[i];
      if (c >> 24) { r[i] = ((c >> 24) * m >> 8) << 24; continue; }   // over the GPU's picture: see-through black, darker
      r[i] = ((((c & 0xFF00FF) * m) >> 8) & 0xFF00FF) | ((((c & 0x00FF00) * m) >> 8) & 0x00FF00);
    }
  }
}

// ---- a 5x7 font for the interface -------------------------------------------------------------------------------
static const char *F5[128];
static void f5_init(void) {
  static int done; if (done) return; done = 1;
  static const struct { char c; const char *r; } T[] = {   // 7 rows of 5, '#' = on
    {'A',".###.#...##...#######...##...##...#"},{'B',"####.#...##...#####.#...##...#####."},{'C',".###.#...##....#....#....#...#.###."},
    {'D',"###..#..#.#...##...##...##..#.###.."},{'E',"######....#....####.#....#....#####"},{'F',"######....#....####.#....#....#...."},
    {'G',".###.#...##....#.####...##...#.####"},{'H',"#...##...##...#######...##...##...#"},{'I',".###...#....#....#....#....#...###."},
    {'J',"..###...#....#....#....#.#..#..##.."},{'K',"#...##..#.#.#..##...#.#..#..#.#...#"},{'L',"#....#....#....#....#....#....#####"},
    {'M',"#...###.###.#.##.#.##...##...##...#"},{'N',"#...##...###..##.#.##..###...##...#"},{'O',".###.#...##...##...##...##...#.###."},
    {'P',"####.#...##...#####.#....#....#...."},{'Q',".###.#...##...##...##.#.##..#..##.#"},{'R',"####.#...##...#####.#.#..#..#.#...#"},
    {'S',".####.....#.....###.....#....#####."},{'T',"#####..#....#....#....#....#....#.."},{'U',"#...##...##...##...##...##...#.###."},
    {'V',"#...##...##...##...##...#.#.#...#.."},{'W',"#...##...##...##.#.##.#.##.#.#.#.#."},{'X',"#...##...#.#.#...#...#.#.#...##...#"},
    {'Y',"#...##...#.#.#...#....#....#....#.."},{'Z',"#####....#...#...#...#...#....#####"},
    {'0',".###.#...##..###.#.###..##...#.###."},{'1',"..#...##....#....#....#....#...###."},{'2',".###.#...#....#...#...#...#...#####"},
    {'3',"#####...#...#.....#.....##...#.###."},{'4',"...#...##..#.#.#..#.#####...#....#."},{'5',"######....####.....#....##...#.###."},
    {'6',"..##..#...#....####.#...##...#.###."},{'7',"#####....#...#...#...#....#....#..."},{'8',".###.#...##...#.###.#...##...#.###."},
    {'9',".###.#...##...#.####....#...#..##.."},{'.',"..........................##...##.."},{',',".....................##...##...#..."},
    {':',".....##...##.........##...##......."},{'!',"..#....#....#....#....#.........#.."},{'?',".###.#...#....#...#...#.........#.."},
    {'-',"...............#####..............."},{'+',"..........#....#..#####..#....#...."},{'/',"....#....#...#...#...#...#....#...."},
    {'(',"...#...#...#....#....#.....#.....#."},{')',".#.....#.....#....#....#...#...#..."},{'=',"..........#####.....#####.........."},
    {'\'',"..#....#...#......................."},{'>',".#.....#.....#.....#...#...#...#..."},{'<',"...#...#...#...#.....#.....#.....#."},
    {'#',".#.#..#.#.#####.#.#.#####.#.#..#.#."},{'%',"##...##..#...#...#...#...#..##...##"},{'*',"......#..#.#.#.###.#.#.#..#........"},
    {'{',"...##..#....#...#.....#....#.....##"},{'}',"##.....#....#.....#...#....#..##..."},
    {' ',"..................................."} };
  for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) F5[(int)T[i].c] = T[i].r;
}
int gfx_text5_w(int scale, const char *s) { return (int)strlen(s) * 6 * scale - scale; }
void gfx_text5(int x, int y, int scale, uint32_t c, const char *s) {
  f5_init();
  for (; *s; s++, x += 6 * scale) {
    int ch = *s >= 'a' && *s <= 'z' ? *s - 32 : *s;
    const char *g = ch > 0 && ch < 128 && F5[ch] ? F5[ch] : F5[' '];
    int n = (int)strlen(g);
    for (int i = 0; i < 35 && i < n; i++) if (g[i] == '#') gfx_rect(x + (i % 5) * scale, y + (i / 5) * scale, scale, scale, c);
  }
}
static void round_rect(int x, int y, int w, int h, int r, uint32_t c) {
  for (int j = 0; j < h; j++) {
    int in = 0;
    if (j < r) in = r - (int)sqrtf((float)(r * r - (r - j) * (r - j)));
    else if (j >= h - r) in = r - (int)sqrtf((float)(r * r - (j - (h - r - 1)) * (j - (h - r - 1))));
    gfx_rect(x + in, y + j, w - 2 * in, 1, c);
  }
}
void gfx_panel(int x, int y, int w, int h, const char *title, uint32_t accent) {
  gfx_dim(x, y, w, h, 0.55f);                       // deeper glass for dialogs, so their text reads over the scene
  gfx_hpanel(x, y, w, h, title, accent == 0x58d838 ? GFX_HG : accent);   // the hologram look now (FOLD's green -> hologram green)
  return;
  gfx_dim(x + 6, y + 8, w, h, 0.45f);                              // (the old look) its shadow
  round_rect(x - 1, y - 1, w + 2, h + 2, 9, mix(accent, 0x000000, 0.45f));
  round_rect(x, y, w, h, 8, 0x0e130d);
  if (title) {
    round_rect(x, y, w, 30, 8, mix(accent, 0x0e130d, 0.72f)); gfx_rect(x, y + 22, w, 8, mix(accent, 0x0e130d, 0.72f));
    gfx_rect(x, y + 30, w, 1, mix(accent, 0x000000, 0.2f));
    gfx_text5(x + 14, y + 8, 2, accent, title);
  }
}
int gfx_badge(int x, int y, const char *label) {   // Duke-style colours: A green, B red, X blue, Y yellow
  uint32_t c = 0x5a6458, t = 0xffffff;
  if (!strcmp(label, "A")) c = 0x2fa84a; else if (!strcmp(label, "B")) c = 0xd0383a; else if (!strcmp(label, "X")) c = 0x3a6fd0;
  else if (!strcmp(label, "Y")) { c = 0xe8c830; t = 0x201c08; }
  int w = strlen(label) == 1 ? 18 : gfx_text5_w(1, label) + 12;
  round_rect(x, y, w, 18, 9, c);
  gfx_text5(x + w / 2 - gfx_text5_w(1, label) / 2, y + 6, 1, t, label);
  return w;
}
int gfx_hint(int x, int y, const char *label, const char *what) {
  int w = gfx_badge(x, y, label) + 6;
  gfx_text5(x + w, y + 5, 1, 0xc8d4c2, what);
  return w + gfx_text5_w(1, what) + 16;
}
void gfx_rect(int x, int y, int w, int h, uint32_t c) {
  for (int j = y < 0 ? 0 : y; j < y + h && j < GH; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < GW; i++) gfx_px[j * GW + i] = c;
}
void gfx_frame(int x, int y, int w, int h, uint32_t c) { gfx_rect(x, y, w, 1, c); gfx_rect(x, y + h - 1, w, 1, c); gfx_rect(x, y, 1, h, c); gfx_rect(x + w - 1, y, 1, h, c); }
void gfx_line(int x0, int y0, int x1, int y1, uint32_t c) {
  int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (int guard = 0; guard < 4096; guard++) {
    if (x0 >= 0 && y0 >= 0 && x0 < GW && y0 < GH) gfx_px[y0 * GW + x0] = c;
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
int gfx_text_w(int scale, const char *s) { return (int)strlen(s) * 4 * scale - scale; }
void gfx_text(int x, int y, int scale, uint32_t c, const char *s) {
  font_init();
  for (; *s; s++, x += 4 * scale) {
    int ch = *s >= 'a' && *s <= 'z' ? *s - 32 : *s;
    const char *b = ch > 0 && ch < 128 && FONT[ch] ? FONT[ch] : FONT[' '];
    for (int i = 0; i < 15; i++) if (b[i] == '1') gfx_rect(x + (i % 3) * scale, y + (i / 3) * scale, scale, scale, c);
  }
}
void gfx_blit(int x, int y, int scale, const Tex *t) {
  for (int j = 0; j < t->h; j++) for (int i = 0; i < t->w; i++) gfx_rect(x + i * scale, y + j * scale, scale, scale, t->px[j * t->w + i]);
}

// ---- 3D ----------------------------------------------------------------------------------------------------------
Mat4 mat_mul(Mat4 a, Mat4 b) {
  Mat4 r;
  for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) {
    float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
    r.m[c * 4 + rr] = s;
  }
  return r;
}
Mat4 mat_perspective(float fovy, float aspect, float zn, float zf) {
  Mat4 r; memset(&r, 0, sizeof r);
  float f = 1.0f / tanf(fovy / 2);
  r.m[0] = f / aspect; r.m[5] = f; r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1; r.m[14] = 2 * zf * zn / (zn - zf);
  return r;
}
static Vec3 vsub(Vec3 a, Vec3 b) { return v3f(a.x - b.x, a.y - b.y, a.z - b.z); }
static Vec3 vcross(Vec3 a, Vec3 b) { return v3f(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static float vdot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 vnorm(Vec3 a) { float l = sqrtf(vdot(a, a)); return l > 0 ? v3f(a.x / l, a.y / l, a.z / l) : a; }
Mat4 mat_lookat(Vec3 eye, Vec3 at, Vec3 up) {
  Vec3 f = vnorm(vsub(at, eye)), s = vnorm(vcross(f, up)), u = vcross(s, f);
  Mat4 r; memset(&r, 0, sizeof r);
  r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
  r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
  r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
  r.m[12] = -vdot(s, eye); r.m[13] = -vdot(u, eye); r.m[14] = vdot(f, eye); r.m[15] = 1;
  return r;
}
void gfx_camera(Mat4 vp) { VP = vp; }
void gfx_zclear(void) { if (inScene) return; for (int i = 0; i < GW * GH; i++) zbuf[i] = 1e30f; }   // (the GPU clears its own)

typedef struct { float x, y, z, w; } V4;
static V4 xform(Vec3 p) {
  const float *m = VP.m;
  V4 r = { m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
           m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14], m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15] };
  return r;
}
int gfx_project(Vec3 p, float *sx, float *sy) {
  V4 c = xform(p);
  if (c.w < 0.01f) return 0;
  *sx = (c.x / c.w * 0.5f + 0.5f) * GW + vofsX; *sy = (0.5f - c.y / c.w * 0.5f) * GH + vofsY;
  return 1;
}

// clip-space vertex with texture coords; clipped against the near plane, then rasterised as a fan
typedef struct { V4 c; float u, v; } CV;
static uint32_t shade_c(uint32_t c, float k) {
  int r = (int)(((c >> 16) & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)((c & 255) * k);
  return (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b));
}
static int triMode; static float triAlpha;      // 0 normal, 1 see-through (no depth write), 3 lit (u = shade)
uint32_t gfx_shade_lit(uint32_t c, float k);
static uint32_t shade_lit(uint32_t c, float k) { // up to 1 darkens; above it, a shine towards white
  if (k <= 1) return shade_c(c, k < 0 ? 0 : k);
  float w = k - 1 > 1 ? 1 : k - 1;
  int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
  return (uint32_t)((int)(r + (255 - r) * w) << 16 | (int)(g + (255 - g) * w) << 8 | (int)(b + (255 - b) * w));
}
static void tri(const CV *a, const CV *b, const CV *c, const Tex *t, uint32_t colour, float shade) {
  if (inScene) {                                // to the GPU, as it is
    const CV *q[3] = { a, b, c }; GfxVert v[3];
    for (int i = 0; i < 3; i++) { v[i].x = q[i]->c.x; v[i].y = q[i]->c.y; v[i].z = q[i]->c.z; v[i].w = q[i]->c.w; v[i].u = q[i]->u; v[i].v = q[i]->v; }
    gpu->tri(v, triMode == 3 ? NULL : t, colour, shade, triMode, triAlpha);
    return;
  }
  float X[3], Y[3], IW[3], UW[3], VW[3];
  const CV *v[3] = { a, b, c };
  for (int i = 0; i < 3; i++) {
    float iw = 1.0f / v[i]->c.w;
    X[i] = (v[i]->c.x * iw * 0.5f + 0.5f) * GW + vofsX; Y[i] = (0.5f - v[i]->c.y * iw * 0.5f) * GH + vofsY;
    IW[i] = iw; UW[i] = v[i]->u * iw; VW[i] = v[i]->v * iw;
  }
  float area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
  if (fabsf(area) < 1e-6f) return;
  int x0 = (int)floorf(fminf(X[0], fminf(X[1], X[2]))), x1 = (int)ceilf(fmaxf(X[0], fmaxf(X[1], X[2])));
  int y0 = (int)floorf(fminf(Y[0], fminf(Y[1], Y[2]))), y1 = (int)ceilf(fmaxf(Y[0], fmaxf(Y[1], Y[2])));
  if (x0 < clx0) x0 = clx0; if (y0 < cly0) y0 = cly0; if (x1 > clx1) x1 = clx1; if (y1 > cly1) y1 = cly1;
  if (x0 > x1 || y0 > y1) return;
  // the barycentric weights w0, w1 (w2 = 1 - w0 - w1) and 1/w, u/w, v/w are linear across the screen: step them
  float ia = 1.0f / area;
  float w0x = (Y[1] - Y[2]) * ia, w0y = (X[2] - X[1]) * ia, w1x = (Y[2] - Y[0]) * ia, w1y = (X[0] - X[2]) * ia;
  float px0 = x0 + 0.5f, py0 = y0 + 0.5f;
  float w0r = ((X[1] - px0) * (Y[2] - py0) - (X[2] - px0) * (Y[1] - py0)) * ia;
  float w1r = ((X[2] - px0) * (Y[0] - py0) - (X[0] - px0) * (Y[2] - py0)) * ia;
  float dI0 = IW[0] - IW[2], dI1 = IW[1] - IW[2], dU0 = UW[0] - UW[2], dU1 = UW[1] - UW[2], dV0 = VW[0] - VW[2], dV1 = VW[1] - VW[2];
  uint32_t flat = shade_c(colour, shade);
  for (int y = y0; y <= y1; y++, w0r += w0y, w1r += w1y) {
    // this row's span: where w0 >= 0, w1 >= 0 and w2 = 1 - w0 - w1 >= 0 (each linear in x)
    float lo = 0, hi = (float)(x1 - x0), w2r = 1 - w0r - w1r, w2x = -w0x - w1x;
    const float A[3] = { w0r, w1r, w2r }, D[3] = { w0x, w1x, w2x };
    int empty = 0;
    for (int k = 0; k < 3; k++) {
      if (D[k] > 1e-12f) { float t = -A[k] / D[k]; if (t > lo) lo = t; }
      else if (D[k] < -1e-12f) { float t = -A[k] / D[k]; if (t < hi) hi = t; }
      else if (A[k] < 0) empty = 1;
    }
    if (empty || lo > hi) continue;
    int xs = x0 + (int)ceilf(lo - 1e-4f), xe = x0 + (int)floorf(hi + 1e-4f);
    if (xs < x0) xs = x0; if (xe > x1) xe = x1;
    float w0 = w0r + w0x * (xs - x0), w1 = w1r + w1x * (xs - x0);
    float iw = IW[2] + w0 * dI0 + w1 * dI1, iwx = w0x * dI0 + w1x * dI1;
    float uw = UW[2] + w0 * dU0 + w1 * dU1, uwx = w0x * dU0 + w1x * dU1;
    float vw = VW[2] + w0 * dV0 + w1 * dV1, vwx = w0x * dV0 + w1x * dV1;
    int k = y * GW + xs;
    for (int x = xs; x <= xe; x++, k++, iw += iwx, uw += uwx, vw += vwx) {
      float z = 1.0f / iw;
      if (z >= zbuf[k]) continue;
      if (triMode == 1) { gfx_px[k] = mix(gfx_px[k], fogged(flat, z), triAlpha); continue; }
      uint32_t col = flat;
      if (triMode == 3) col = shade_lit(colour, uw * z);
      else if (t) {
        int tx = (int)(uw * z * t->w), ty = (int)(vw * z * t->h);
        if (tx < 0) tx = 0; if (ty < 0) ty = 0; if (tx >= t->w) tx = t->w - 1; if (ty >= t->h) ty = t->h - 1;
        col = shade_c(t->px[ty * t->w + tx], shade);
      }
      zbuf[k] = z; gfx_px[k] = fogged(col, z);
    }
  }
}
void gfx_quad(const Vec3 p[4], const Tex *t, uint32_t colour, float shade) {
  static const float U[4] = { 0, 1, 1, 0 }, V[4] = { 0, 0, 1, 1 };
  CV in[4], out[8]; int n = 0;
  for (int i = 0; i < 4; i++) { in[i].c = xform(p[i]); in[i].u = U[i]; in[i].v = V[i]; }
  const float NEAR = 0.05f;
  for (int i = 0; i < 4; i++) {                 // clip against w = NEAR
    const CV *a = &in[i], *b = &in[(i + 1) & 3];
    int ia = a->c.w >= NEAR, ib = b->c.w >= NEAR;
    if (ia) out[n++] = *a;
    if (ia != ib) {
      float k = (NEAR - a->c.w) / (b->c.w - a->c.w); CV m;
      m.c.x = a->c.x + (b->c.x - a->c.x) * k; m.c.y = a->c.y + (b->c.y - a->c.y) * k;
      m.c.z = a->c.z + (b->c.z - a->c.z) * k; m.c.w = NEAR;
      m.u = a->u + (b->u - a->u) * k; m.v = a->v + (b->v - a->v) * k;
      out[n++] = m;
    }
  }
  for (int i = 1; i + 1 < n; i++) tri(&out[0], &out[i], &out[i + 1], t, colour, shade);
}
uint32_t gfx_shade_lit(uint32_t c, float k) { return shade_lit(c, k); }
void gfx_tri_lit(const Vec3 p[3], const float sh[3], uint32_t colour) {
  CV in[3], out[6]; int n = 0;
  for (int i = 0; i < 3; i++) { in[i].c = xform(p[i]); in[i].u = sh[i]; in[i].v = 0; }
  const float NEAR = 0.05f;
  for (int i = 0; i < 3; i++) {
    const CV *a = &in[i], *b = &in[(i + 1) % 3];
    int ia = a->c.w >= NEAR, ib = b->c.w >= NEAR;
    if (ia) out[n++] = *a;
    if (ia != ib) {
      float k = (NEAR - a->c.w) / (b->c.w - a->c.w); CV m;
      m.c.x = a->c.x + (b->c.x - a->c.x) * k; m.c.y = a->c.y + (b->c.y - a->c.y) * k;
      m.c.z = a->c.z + (b->c.z - a->c.z) * k; m.c.w = NEAR; m.u = a->u + (b->u - a->u) * k; m.v = 0;
      out[n++] = m;
    }
  }
  triMode = 3;
  for (int i = 1; i + 1 < n; i++) tri(&out[0], &out[i], &out[i + 1], NULL, colour, 1);
  triMode = 0;
}
void gfx_quad_alpha(const Vec3 p[4], uint32_t colour, float alpha) { triMode = 1; triAlpha = alpha; gfx_quad(p, NULL, colour, 1); triMode = 0; }
void gfx_fog(uint32_t c, float start, float end) { fogC = c; fogS = start; fogE = end; }
void gfx_line3(Vec3 a, Vec3 b, uint32_t c, int thick) {
  // a line with depth: drawn where it is in front of (or within a small bias of) what is already there
  V4 A = xform(a), B = xform(b);
  const float NEAR = 0.05f;
  if (A.w < NEAR && B.w < NEAR) return;
  if (A.w < NEAR) { float k = (NEAR - A.w) / (B.w - A.w); A.x += (B.x - A.x) * k; A.y += (B.y - A.y) * k; A.z += (B.z - A.z) * k; A.w = NEAR; }
  if (B.w < NEAR) { float k = (NEAR - B.w) / (A.w - B.w); B.x += (A.x - B.x) * k; B.y += (A.y - B.y) * k; B.z += (A.z - B.z) * k; B.w = NEAR; }
  if (inScene) { GfxVert ga = { A.x, A.y, A.z, A.w, 0, 0 }, gb = { B.x, B.y, B.z, B.w, 0, 0 }; gpu->line(&ga, &gb, c, thick); return; }
  float x0 = (A.x / A.w * 0.5f + 0.5f) * GW + vofsX, y0 = (0.5f - A.y / A.w * 0.5f) * GH + vofsY;
  float x1 = (B.x / B.w * 0.5f + 0.5f) * GW + vofsX, y1 = (0.5f - B.y / B.w * 0.5f) * GH + vofsY;
  int steps = (int)fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) + 1;
  if (steps > 4000) steps = 4000;
  for (int i = 0; i <= steps; i++) {
    float k = (float)i / steps, x = x0 + (x1 - x0) * k, y = y0 + (y1 - y0) * k;
    float iw = (1 - k) / A.w + k / B.w, z = 1.0f / iw;          // perspective-correct depth
    for (int dy = 0; dy < thick; dy++) for (int dx = 0; dx < thick; dx++) {
      int xi = (int)x + dx - thick / 2, yi = (int)y + dy - thick / 2;
      if (xi < clx0 || yi < cly0 || xi > clx1 || yi > cly1) continue;
      int q = yi * GW + xi;
      if (z <= zbuf[q] * 1.002f + 0.002f) gfx_px[q] = fogged(c, z);
    }
  }
}

// ---- shapes for icons ---------------------------------------------------------------------------------------------
void gfx_view_offset(int dx, int dy) { vofsX = dx; vofsY = dy; }
void gfx_disc(int cx, int cy, int r, uint32_t c) {
  for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r + r) { int X = cx + x, Y = cy + y; if (X >= 0 && Y >= 0 && X < GW && Y < GH) gfx_px[Y * GW + X] = c; }
}
void gfx_ring(int cx, int cy, int r, int w, uint32_t c) {
  int ri = r - w;
  for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) { int d = x * x + y * y; if (d <= r * r + r && d > ri * ri + ri) { int X = cx + x, Y = cy + y; if (X >= 0 && Y >= 0 && X < GW && Y < GH) gfx_px[Y * GW + X] = c; } }
}
void gfx_ellipse(int cx, int cy, int rx, int ry, float a, uint32_t c) {
  int r = rx > ry ? rx : ry; float ca = cosf(a), sa = sinf(a);
  for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) {
    float u = (x * ca + y * sa) / rx, v = (-x * sa + y * ca) / ry;
    if (u * u + v * v <= 1) { int X = cx + x, Y = cy + y; if (X >= 0 && Y >= 0 && X < GW && Y < GH) gfx_px[Y * GW + X] = c; }
  }
}
void gfx_tri2(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
  int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2), maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
  int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2), maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  for (int y = miny; y <= maxy; y++) for (int x = minx; x <= maxx; x++) {
    int a = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0), b = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1), d = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
    if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) if (x >= 0 && y >= 0 && x < GW && y < GH) gfx_px[y * GW + x] = c;
  }
}
void gfx_thick(int x0, int y0, int x1, int y1, int w, uint32_t c) {
  float dx = (float)(x1 - x0), dy = (float)(y1 - y0), l = sqrtf(dx * dx + dy * dy); int n = (int)l + 1;
  for (int i = 0; i <= n; i++) { int x = x0 + (int)(dx * i / n), y = y0 + (int)(dy * i / n); gfx_disc(x, y, w / 2, c); }
}
static void metal_draw(uint32_t *out, int w, int h, int r, int mask);
static void metal_forget(void);
void gfx_metal_texture(const uint8_t *rgb, int w, int h) { metal_forget(); metal = rgb; metalW = w; metalH = h; }
static struct { int w, h, r, mask; uint32_t *px; } C[8]; static int nC;   // metal panels made so far
void gfx_metal_panel(int x, int y, int w, int h, int r, int mask) {   // made once per size, then copied
  gfx_dim(x + 5, y + 6, w, h, 0.5f);                                          // its shadow
  int k = 0; while (k < nC && !(C[k].w == w && C[k].h == h && C[k].r == r && C[k].mask == mask)) k++;
  if (k == nC) {
    if (nC == 8 || !(C[k].px = malloc(sizeof(uint32_t) * (size_t)(w * h)))) { k = -1; }
    else { C[k].w = w; C[k].h = h; C[k].r = r; C[k].mask = mask; metal_draw(C[k].px, w, h, r, mask); nC++; }
  }
  static uint32_t one[GW * 64]; uint32_t *src = k >= 0 ? C[k].px : NULL;
  if (!src) { if (w * h > GW * 64) return; metal_draw(one, w, h, r, mask); src = one; }
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
    int X = x + i, Y = y + j; uint32_t c = src[j * w + i];
    if (c != 0xff000000u && X >= 0 && Y >= 0 && X < GW && Y < GH) gfx_px[Y * GW + X] = c;
  }
}
static void metal_forget(void) { for (int k = 0; k < nC; k++) free(C[k].px); nC = 0; }
static void metal_draw(uint32_t *out, int w, int h, int r, int mask) {   // 0xff000000 = outside its rounded corners
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
    out[j * w + i] = 0xff000000u;
    int cx = -1, cy = -1;                                                      // inside a rounded corner?
    if (i < r && j < r && (mask & 1)) { cx = r; cy = r; } else if (i >= w - r && j < r && (mask & 2)) { cx = w - r - 1; cy = r; }
    else if (i >= w - r && j >= h - r && (mask & 4)) { cx = w - r - 1; cy = h - r - 1; } else if (i < r && j >= h - r && (mask & 8)) { cx = r; cy = h - r - 1; }
    if (cx >= 0 && (i - cx) * (i - cx) + (j - cy) * (j - cy) > r * r) continue;
    uint32_t col = 0xc8ccd0;
    if (metal) { const uint8_t *q = metal + 3 * (((j % metalH) * metalW) + (i % metalW)); col = (uint32_t)q[0] << 16 | q[1] << 8 | q[2]; }
    float k = 1.0f;                                                            // a little bevel: light top, dark bottom
    if (j < 2) k = 1.12f; else if (j >= h - 2) k = 0.7f; else if (i < 2 || i >= w - 2) k = 0.85f;
    int rr = (int)(((col >> 16) & 255) * k), gg = (int)(((col >> 8) & 255) * k), bb = (int)((col & 255) * k);
    out[j * w + i] = (uint32_t)((rr > 255 ? 255 : rr) << 16 | (gg > 255 ? 255 : gg) << 8 | (bb > 255 ? 255 : bb));
  }
}
void gfx_clip(int x, int y, int w, int h) {
  if (w <= 0) { clx0 = cly0 = 0; clx1 = GW - 1; cly1 = GH - 1; return; }
  clx0 = x < 0 ? 0 : x; cly0 = y < 0 ? 0 : y; clx1 = x + w - 1 > GW - 1 ? GW - 1 : x + w - 1; cly1 = y + h - 1 > GH - 1 ? GH - 1 : y + h - 1;
}
void gfx_zclear_rect(int x, int y, int w, int h) {
  for (int j = y < 0 ? 0 : y; j < y + h && j < GH; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < GW; i++) zbuf[j * GW + i] = 1e30f;
}

// ---- the hologram look (VMX World's house style, owner 2026-10-23; FOLD BUILDER follows it): glass panels with
// glowing edges and corner brackets, glowing text, lit rows, a filter that turns a picture into green light, and one
// pass of scanlines and a rolling band. With a GPU (the Xbox) gfx_px is a layer over the 3D scene: see-through pixels
// (top byte 0xFF) are treated as black where something is drawn over them, and left alone by the post pass.
uint32_t gfx_mix(uint32_t a, uint32_t b, int k) {
  a &= 0xffffff;
  int r = (int)((a >> 16) & 255) + ((((int)((b >> 16) & 255)) - (int)((a >> 16) & 255)) * k >> 8);
  int g = (int)((a >> 8) & 255) + ((((int)((b >> 8) & 255)) - (int)((a >> 8) & 255)) * k >> 8);
  int bl = (int)(a & 255) + ((((int)(b & 255)) - (int)(a & 255)) * k >> 8);
  return (uint32_t)(r << 16 | g << 8 | bl);
}
static uint32_t solid(uint32_t p) { return p >> 24 ? 0 : p; }
void gfx_add_rect(int x, int y, int w, int h, uint32_t c) {
  for (int j = y < 0 ? 0 : y; j < y + h && j < GH; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < GW; i++) {
    uint32_t p = solid(gfx_px[j * GW + i]);
    int r = (int)((p >> 16) & 255) + (int)((c >> 16) & 255), g = (int)((p >> 8) & 255) + (int)((c >> 8) & 255), bl = (int)(p & 255) + (int)(c & 255);
    gfx_px[j * GW + i] = (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (bl > 255 ? 255 : bl));
  }
}
void gfx_glass(int x, int y, int w, int h) {
  for (int j = y < 0 ? 0 : y; j < y + h && j < GH; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < GW; i++) {
    uint32_t p = solid(gfx_px[j * GW + i]);
    gfx_px[j * GW + i] = (((p >> 16) & 255) * 5 / 16 + 2) << 16 | (((p >> 8) & 255) * 5 / 16 + 12) << 8 | ((p & 255) * 5 / 16 + 6);
  }
}
void gfx_brackets(int x, int y, int w, int h, uint32_t c, int len) {
  gfx_rect(x, y, len, 2, c); gfx_rect(x, y, 2, len, c); gfx_rect(x + w - len, y, len, 2, c); gfx_rect(x + w - 2, y, 2, len, c);
  gfx_rect(x, y + h - 2, len, 2, c); gfx_rect(x, y + h - len, 2, len, c); gfx_rect(x + w - len, y + h - 2, len, 2, c); gfx_rect(x + w - 2, y + h - len, 2, len, c);
}
void gfx_gtext(int x, int y, int scale, uint32_t c, const char *s) {
  uint32_t g = gfx_mix(0x000000, c, 70);
  gfx_text5(x - 1, y, scale, g, s); gfx_text5(x + 1, y, scale, g, s); gfx_text5(x, y - 1, scale, g, s); gfx_text5(x, y + 1, scale, g, s);
  gfx_text5(x, y, scale, c, s);
}
void gfx_hpanel(int x, int y, int w, int h, const char *title, uint32_t accent) {
  gfx_glass(x, y, w, h);
  gfx_frame(x, y, w, h, gfx_mix(0x000000, accent, 110));
  gfx_brackets(x - 1, y - 1, w + 2, h + 2, accent, 14);
  if (title) {
    gfx_gtext(x + 14, y + 10, 2, accent, title);
    for (int i = 0; i < w - 28; i++) gfx_rect(x + 14 + i, y + 30, 1, 1, gfx_mix(0x000000, accent, 255 - i * 255 / (w - 28)));
  }
}
void gfx_hsel(int x, int y, int w, int h, int focused) {
  for (int i = 0; i < w; i++) { int k = focused ? 110 - i * 90 / w : 40 - i * 30 / w; gfx_add_rect(x + i, y, 1, h, (uint32_t)((k / 6) << 16 | k << 8 | (k / 3))); }
  gfx_rect(x, y, 3, h, focused ? GFX_HG : GFX_HG_EDGE);
  if (focused) { gfx_rect(x, y, w, 1, GFX_HG_EDGE); gfx_rect(x, y + h - 1, w, 1, GFX_HG_EDGE); }
}
void gfx_holo_filter(int x, int y, int w, int h) {      // a picture as green light: brightness -> green, edges brighter
  static uint8_t L[GW * 2]; int pw = w < GW ? w : GW;
  for (int j = 0; j < h; j++) {
    int Y = y + j; if (Y < 0 || Y >= GH) continue;
    uint8_t *cur = L + (j & 1) * GW, *up = L + ((j + 1) & 1) * GW;
    for (int i = 0; i < pw; i++) {
      int X = x + i; if (X < 0 || X >= GW) { cur[i] = 0; continue; }
      uint32_t p = solid(gfx_px[Y * GW + X]);
      cur[i] = (uint8_t)((((p >> 16) & 255) * 77 + ((p >> 8) & 255) * 150 + (p & 255) * 29) >> 8);
    }
    for (int i = 0; i < pw; i++) {
      int X = x + i; if (X < 0 || X >= GW) continue;
      int l = cur[i], e = 0;
      if (i > 0) e += abs(l - cur[i - 1]); if (j > 0) e += abs(l - up[i]);
      int v = l * 3 / 4 + e * 2; if (v > 255) v = 255;
      int hot = v > 200 ? (v - 200) * 2 : 0;
      gfx_px[Y * GW + X] = (uint32_t)(((v * 22 >> 8) + hot) << 16 | (v > 255 ? 255 : v) << 8 | ((v * 90 >> 8) + hot));
    }
  }
}
void gfx_post(int t) {
  int band = (t * 2) % (GH + 120) - 60;
  for (int y = 0; y < GH; y++) {
    int d = y - band, mul = (y % 3 == 0) ? 200 : 256; if (d > -12 && d < 12) mul += (12 - (d < 0 ? -d : d)) * 3;
    if (mul == 256) continue;
    uint32_t *p = gfx_px + y * GW;
    for (int x = 0; x < GW; x++) {
      uint32_t c = p[x]; if (c >> 24) continue;
      int r = (int)((c >> 16) & 255) * mul >> 8, g = (int)((c >> 8) & 255) * mul >> 8, b = (int)(c & 255) * mul >> 8;
      p[x] = (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b));
    }
  }
}

// ---- a plate turned into the screen (the builder's control windows): draw its contents flat with gfx_plate_begin /
// gfx_plate_end around them; end warps them in perspective onto a quad that turns away towards the screen's middle
// (side -1: on the left, the inner edge recedes to the right; +1 mirrored), with cut corners and glowing edges.
static uint32_t plateBg[GW * 200], plateFg[GW * 200];
static void homography(const float src[4][2], const float dst[4][2], float H[9]) {   // maps src -> dst (8x8 solve)
  float A[8][9];
  for (int k = 0; k < 4; k++) {
    float x = src[k][0], y = src[k][1], u = dst[k][0], v = dst[k][1];
    float r1[9] = { x, y, 1, 0, 0, 0, -u * x, -u * y, u }, r2[9] = { 0, 0, 0, x, y, 1, -v * x, -v * y, v };
    memcpy(A[2 * k], r1, sizeof r1); memcpy(A[2 * k + 1], r2, sizeof r2);
  }
  for (int c = 0; c < 8; c++) {
    int piv = c; for (int r = c + 1; r < 8; r++) if (fabsf(A[r][c]) > fabsf(A[piv][c])) piv = r;
    for (int k = 0; k < 9; k++) { float t = A[c][k]; A[c][k] = A[piv][k]; A[piv][k] = t; }
    for (int r = 0; r < 8; r++) if (r != c) { float f = A[r][c] / A[c][c]; for (int k = c; k < 9; k++) A[r][k] -= f * A[c][k]; }
  }
  for (int k = 0; k < 8; k++) H[k] = A[k][8] / A[k][k];
  H[8] = 1;
}
static void happly(const float H[9], float x, float y, float *u, float *v) {
  float w = H[6] * x + H[7] * y + H[8]; *u = (H[0] * x + H[1] * y + H[2]) / w; *v = (H[3] * x + H[4] * y + H[5]) / w;
}
void gfx_plate_begin(int x, int y, int w, int h) {       // keeps what's behind, then a glass plate to draw on
  if (w > GW || h > 200) return;
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) { int X = x + i, Y = y + j; plateBg[j * w + i] = (X >= 0 && Y >= 0 && X < GW && Y < GH) ? gfx_px[Y * GW + X] : 0; }
  gfx_glass(x, y, w, h);
  for (int j = 0; j < h; j += 4) gfx_add_rect(x, y + j, w, 1, 0x000802);          // faint ruled lines on the glass
}
void gfx_plate_end(int x, int y, int w, int h, int side, float turn) {
  if (w > GW || h > 200) return;
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {   // take the flat plate, put back what was behind
    int X = x + i, Y = y + j; if (X < 0 || Y < 0 || X >= GW || Y >= GH) continue;
    plateFg[j * w + i] = gfx_px[Y * GW + X]; gfx_px[Y * GW + X] = plateBg[j * w + i];
  }
  // the quad: the outer edge full height, the inner edge shorter and pulled in (turned away by `turn`, 0..0.4)
  float sh = h * turn * 0.5f, in = w * turn * 0.35f;
  float src[4][2] = { { 0, 0 }, { (float)w, 0 }, { (float)w, (float)h }, { 0, (float)h } }, dst[4][2];
  if (side < 0) { float d[4][2] = { { 0, 0 }, { w - in, sh }, { w - in, h - sh }, { 0, (float)h } }; memcpy(dst, d, sizeof d); }
  else { float d[4][2] = { { in, sh }, { (float)w, 0 }, { (float)w, (float)h }, { in, h - sh } }; memcpy(dst, d, sizeof d); }
  for (int k = 0; k < 4; k++) { dst[k][0] += x; dst[k][1] += y; }
  float H[9], Hi[9]; homography(dst, src, Hi); homography(src, dst, H);
  int cut = h / 5;                                         // cut corners: top outer and bottom inner
  for (int Y = y; Y < y + h; Y++) for (int X = x; X < x + w; X++) {
    if (X < 0 || Y < 0 || X >= GW || Y >= GH) continue;
    float u, v; happly(Hi, X + .5f, Y + .5f, &u, &v);
    if (u < 0 || v < 0 || u >= w || v >= h) continue;
    float ou = side < 0 ? u : w - u;                       // distance from the outer edge
    if (ou + v < cut || (w - ou) + (h - v) < cut) continue;
    gfx_px[Y * GW + X] = plateFg[(int)v * w + (int)u];
  }
  // the edges, crisp on top: the outline with its cut corners, brighter on the outer edge, ticks along it
  float P[6][2], o = side < 0 ? 0 : (float)w, ii = side < 0 ? (float)w : 0, dir = side < 0 ? 1.f : -1.f;
  float pts[6][2] = { { o, (float)cut }, { o + dir * cut, 0 }, { ii, 0 }, { ii, h - (float)cut }, { ii - dir * cut, (float)h }, { o, (float)h } };
  for (int k = 0; k < 6; k++) happly(H, pts[k][0], pts[k][1], &P[k][0], &P[k][1]);
  for (int k = 0; k < 6; k++) {
    int a = k, b = (k + 1) % 6; uint32_t c = (k == 5 || k == 0) ? GFX_HG : GFX_HG_EDGE;
    gfx_line((int)P[a][0], (int)P[a][1], (int)P[b][0], (int)P[b][1], c);
    if (k == 5) gfx_line((int)P[a][0] + (side < 0 ? 1 : -1), (int)P[a][1], (int)P[b][0] + (side < 0 ? 1 : -1), (int)P[b][1], GFX_HG);   // the outer edge, thicker
  }
  for (int t = 1; t < 6; t++) {                            // ticks along the outer edge
    float ty = cut + (h - cut) * t / 6.0f, a0, b0, a1, b1;
    happly(H, o, ty, &a0, &b0); happly(H, o + dir * 6, ty, &a1, &b1);
    gfx_line((int)a0, (int)b0, (int)a1, (int)b1, GFX_HG_EDGE);
  }
}

// a coverage mask (mw x mh, 0..255) laid onto a screen quad (its corners: the mask's top-left, top-right,
// bottom-right, bottom-left), in colour c: for letters on 3D buttons
void gfx_decal(const uint8_t *mask, int mw, int mh, const float quad[4][2], uint32_t c) {
  float src[4][2] = { { 0, 0 }, { (float)mw, 0 }, { (float)mw, (float)mh }, { 0, (float)mh } }, Hi[9];
  homography(quad, src, Hi);
  float x0 = quad[0][0], x1 = x0, y0 = quad[0][1], y1 = y0;
  for (int k = 1; k < 4; k++) { if (quad[k][0] < x0) x0 = quad[k][0]; if (quad[k][0] > x1) x1 = quad[k][0]; if (quad[k][1] < y0) y0 = quad[k][1]; if (quad[k][1] > y1) y1 = quad[k][1]; }
  for (int Y = (int)y0; Y <= (int)y1; Y++) for (int X = (int)x0; X <= (int)x1; X++) {
    if (X < 0 || Y < 0 || X >= GW || Y >= GH) continue;
    float u, v; happly(Hi, X + .5f, Y + .5f, &u, &v); u -= .5f; v -= .5f;
    if (u < 0 || v < 0 || u >= mw - 1 || v >= mh - 1) continue;
    int iu = (int)u, iv = (int)v; float fu = u - iu, fv = v - iv;   // bilinear
    const uint8_t *m = mask + iv * mw + iu;
    int a = (int)(m[0] * (1 - fu) * (1 - fv) + m[1] * fu * (1 - fv) + m[mw] * (1 - fu) * fv + m[mw + 1] * fu * fv);
    if (a < 8) continue;
    gfx_px[Y * GW + X] = gfx_mix(gfx_px[Y * GW + X] >> 24 ? 0 : gfx_px[Y * GW + X], c, a);
  }
}

// FOLD app: software renderer. See gfx.h.
#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

uint32_t gfx_px[GW * GH];
static float zbuf[GW * GH];
static Mat4 VP;
static uint32_t fogC; static float fogS = 1e9f, fogE = 2e9f;
static int vofsX, vofsY, clx0, cly0, clx1 = GW - 1, cly1 = GH - 1;
static const uint8_t *metal; static int metalW, metalH;
static uint8_t smask[GW * GH];
static uint32_t mix(uint32_t a, uint32_t b, float t) {   // a towards b by t
  if (t <= 0) return a; if (t >= 1) return b;
  int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t), g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t), bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
  return (uint32_t)(r << 16 | g << 8 | bl);
}
static uint32_t fogged(uint32_t c, float z) { return z <= fogS ? c : mix(c, fogC, (z - fogS) / (fogE - fogS) * 0.85f); }

static const char *FONT[128];
static void font_init(void) {
  static int done; if (done) return; done = 1;
  static const struct { char c; const char *b; } F[] = {
    {'A',".###.#...##...#######...##...##...#"},{'B',"110101110101110"},{'C',"011100100100011"},{'D',"110101101101110"},{'E',"111100110100111"},
    {'F',"111100110100100"},{'G',"011100101101011"},{'H',"101101111101101"},{'I',"111010010010111"},{'J',"001001001101010"},
    {'K',"101101110101101"},{'L',"100100100100111"},{'M',"101111111101101"},{'N',"110101101101101"},{'O',"010101101101010"},
    {'P',"110101110100100"},{'Q',"010101101110011"},{'R',"110101110101101"},{'S',"011100010001110"},{'T',"111010010010010"},
    {'U',"101101101101111"},{'V',"101101101101010"},{'W',"#...##...##...##.#.##.#.##.#.#.#.#."},{'X',"101101010101101"},{'Y',"101101010010010"},
    {'Z',"111001010100111"},{'0',"111101101101111"},{'1',"010110010010111"},{'2',"110001010100111"},{'3',"#####...#...#.....#.....##...#.###."},
    {'4',"101101111001001"},{'5',"111100110001110"},{'6',"011100111101111"},{'7',"111001010010010"},{'8',"111101111101111"},
    {'9',"111101111001110"},{'!',"010010010000010"},{':',"000010000010000"},{'-',"...............#####..............."},{'.',"000000000000010"},
    {'/',"001001010100100"},{'+',"000010111010000"},{'>',"100010001010100"},{'<',"001010100010001"},{'?',"110001010000010"},
    {'(',"010100100100010"},{')',"010001001001010"},{'=',"000111000111000"},{',',"000000000010100"},{'#',"101111101111101"},
    {' ',"000000000000000"} };
  for (unsigned i = 0; i < sizeof F / sizeof F[0]; i++) FONT[(int)F[i].c] = F[i].b;
}

const char *gfx_glyph(int ch) { font_init(); if (ch >= 'a' && ch <= 'z') ch -= 32; return ch > 0 && ch < 128 && FONT[ch] ? FONT[ch] : FONT[' ']; }
void gfx_clear(uint32_t c) { for (int i = 0; i < GW * GH; i++) gfx_px[i] = c; }
void gfx_gradient(uint32_t top, uint32_t bottom) {
  for (int y = 0; y < GH; y++) {
    uint32_t row = mix(top, bottom, (float)y / GH);
    for (int x = 0; x < GW; x++) {
      float dx = (x - GW / 2) / (float)(GW / 2), dy = (y - GH / 2) / (float)(GH / 2), v = (dx * dx + dy * dy) * 0.35f;
      gfx_px[y * GW + x] = mix(row, 0x000000, v > 0.6f ? 0.6f : v);
    }
  }
}
void gfx_dim(int x, int y, int w, int h, float k) {
  for (int j = y < 0 ? 0 : y; j < y + h && j < GH; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < GW; i++) gfx_px[j * GW + i] = mix(gfx_px[j * GW + i], 0, k);
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
  gfx_dim(x + 6, y + 8, w, h, 0.45f);                              // its shadow
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
void gfx_zclear(void) { for (int i = 0; i < GW * GH; i++) zbuf[i] = 1e30f; }

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
static int triMode; static float triAlpha;      // 0 normal, 1 see-through (no depth write), 2 shadow mask, 3 lit (u = shade)
static uint32_t shade_lit(uint32_t c, float k) { // up to 1 darkens; above it, a shine towards white
  if (k <= 1) return shade_c(c, k < 0 ? 0 : k);
  float w = k - 1 > 1 ? 1 : k - 1;
  int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
  return (uint32_t)((int)(r + (255 - r) * w) << 16 | (int)(g + (255 - g) * w) << 8 | (int)(b + (255 - b) * w));
}
static void tri(const CV *a, const CV *b, const CV *c, const Tex *t, uint32_t colour, float shade) {
  float X[3], Y[3], IW[3], UW[3], VW[3], Z[3];
  const CV *v[3] = { a, b, c };
  for (int i = 0; i < 3; i++) {
    float iw = 1.0f / v[i]->c.w;
    X[i] = (v[i]->c.x * iw * 0.5f + 0.5f) * GW + vofsX; Y[i] = (0.5f - v[i]->c.y * iw * 0.5f) * GH + vofsY;
    IW[i] = iw; UW[i] = v[i]->u * iw; VW[i] = v[i]->v * iw; Z[i] = v[i]->c.w;
  }
  float area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
  if (fabsf(area) < 1e-6f) return;
  int x0 = (int)floorf(fminf(X[0], fminf(X[1], X[2]))), x1 = (int)ceilf(fmaxf(X[0], fmaxf(X[1], X[2])));
  int y0 = (int)floorf(fminf(Y[0], fminf(Y[1], Y[2]))), y1 = (int)ceilf(fmaxf(Y[0], fmaxf(Y[1], Y[2])));
  if (x0 < clx0) x0 = clx0; if (y0 < cly0) y0 = cly0; if (x1 > clx1) x1 = clx1; if (y1 > cly1) y1 = cly1;
  uint32_t flat = shade_c(colour, shade);
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
    float px = x + 0.5f, py = y + 0.5f;
    float w0 = ((X[1] - px) * (Y[2] - py) - (X[2] - px) * (Y[1] - py)) / area;
    float w1 = ((X[2] - px) * (Y[0] - py) - (X[0] - px) * (Y[2] - py)) / area;
    float w2 = 1 - w0 - w1;
    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
    float iw = w0 * IW[0] + w1 * IW[1] + w2 * IW[2], z = 1.0f / iw;
    int k = y * GW + x;
    if (triMode == 2) { smask[k] = 1; continue; }
    if (z >= zbuf[k]) continue;
    if (triMode == 1) { gfx_px[k] = mix(gfx_px[k], fogged(flat, z), triAlpha); continue; }
    uint32_t col = flat;
    if (triMode == 3) col = shade_lit(colour, (w0 * UW[0] + w1 * UW[1] + w2 * UW[2]) * z);
    else if (t) {
      float u = (w0 * UW[0] + w1 * UW[1] + w2 * UW[2]) * z, vv = (w0 * VW[0] + w1 * VW[1] + w2 * VW[2]) * z;
      int tx = (int)(u * t->w), ty = (int)(vv * t->h);
      if (tx < 0) tx = 0; if (ty < 0) ty = 0; if (tx >= t->w) tx = t->w - 1; if (ty >= t->h) ty = t->h - 1;
      col = shade_c(t->px[ty * t->w + tx], shade);
    }
    zbuf[k] = z; gfx_px[k] = fogged(col, z);
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
void gfx_shadow_begin(void) { memset(smask, 0, sizeof smask); }
void gfx_shadow_quad(const Vec3 p[4]) { triMode = 2; gfx_quad(p, NULL, 0, 1); triMode = 0; }
void gfx_shadow_end(float k) {                  // a soft edge: blur the mask a little while darkening
  for (int y = 1; y < GH - 1; y++) for (int x = 1; x < GW - 1; x++) {
    int i = y * GW + x, n = smask[i] * 4 + smask[i - 1] + smask[i + 1] + smask[i - GW] + smask[i + GW];
    if (n) gfx_px[i] = mix(gfx_px[i], 0x000000, k * n / 8.0f);
  }
}
void gfx_fog(uint32_t c, float start, float end) { fogC = c; fogS = start; fogE = end; }
void gfx_line3(Vec3 a, Vec3 b, uint32_t c, int thick) {
  // a line with depth: drawn where it is in front of (or within a small bias of) what is already there
  V4 A = xform(a), B = xform(b);
  const float NEAR = 0.05f;
  if (A.w < NEAR && B.w < NEAR) return;
  if (A.w < NEAR) { float k = (NEAR - A.w) / (B.w - A.w); A.x += (B.x - A.x) * k; A.y += (B.y - A.y) * k; A.w = NEAR; }
  if (B.w < NEAR) { float k = (NEAR - B.w) / (A.w - B.w); B.x += (A.x - B.x) * k; B.y += (A.y - B.y) * k; B.w = NEAR; }
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
void gfx_metal_texture(const uint8_t *rgb, int w, int h) { metal = rgb; metalW = w; metalH = h; }
void gfx_metal_panel(int x, int y, int w, int h, int r, int mask) {
  gfx_dim(x + 5, y + 6, w, h, 0.5f);                                          // its shadow
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
    int cx = -1, cy = -1;                                                      // inside a rounded corner?
    if (i < r && j < r && (mask & 1)) { cx = r; cy = r; } else if (i >= w - r && j < r && (mask & 2)) { cx = w - r - 1; cy = r; }
    else if (i >= w - r && j >= h - r && (mask & 4)) { cx = w - r - 1; cy = h - r - 1; } else if (i < r && j >= h - r && (mask & 8)) { cx = r; cy = h - r - 1; }
    if (cx >= 0 && (i - cx) * (i - cx) + (j - cy) * (j - cy) > r * r) continue;
    int X = x + i, Y = y + j; if (X < 0 || Y < 0 || X >= GW || Y >= GH) continue;
    uint32_t col = 0xc8ccd0;
    if (metal) { const uint8_t *q = metal + 3 * (((j % metalH) * metalW) + (i % metalW)); col = (uint32_t)q[0] << 16 | q[1] << 8 | q[2]; }
    float k = 1.0f;                                                            // a little bevel: light top, dark bottom
    if (j < 2) k = 1.12f; else if (j >= h - 2) k = 0.7f; else if (i < 2 || i >= w - 2) k = 0.85f;
    int rr = (int)(((col >> 16) & 255) * k), gg = (int)(((col >> 8) & 255) * k), bb = (int)((col & 255) * k);
    gfx_px[Y * GW + X] = (uint32_t)((rr > 255 ? 255 : rr) << 16 | (gg > 255 ? 255 : gg) << 8 | (bb > 255 ? 255 : bb));
  }
}
void gfx_clip(int x, int y, int w, int h) {
  if (w <= 0) { clx0 = cly0 = 0; clx1 = GW - 1; cly1 = GH - 1; return; }
  clx0 = x < 0 ? 0 : x; cly0 = y < 0 ? 0 : y; clx1 = x + w - 1 > GW - 1 ? GW - 1 : x + w - 1; cly1 = y + h - 1 > GH - 1 ? GH - 1 : y + h - 1;
}
void gfx_zclear_rect(int x, int y, int w, int h) {
  for (int j = y < 0 ? 0 : y; j < y + h && j < GH; j++) for (int i = x < 0 ? 0 : x; i < x + w && i < GW; i++) zbuf[j * GW + i] = 1e30f;
}

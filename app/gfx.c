// FOLD app: software renderer. See gfx.h.
#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

uint32_t gfx_px[GW * GH];
static float zbuf[GW * GH];
static Mat4 VP;

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
  *sx = (c.x / c.w * 0.5f + 0.5f) * GW; *sy = (0.5f - c.y / c.w * 0.5f) * GH;
  return 1;
}

// clip-space vertex with texture coords; clipped against the near plane, then rasterised as a fan
typedef struct { V4 c; float u, v; } CV;
static uint32_t shade_c(uint32_t c, float k) {
  int r = (int)(((c >> 16) & 255) * k), g = (int)(((c >> 8) & 255) * k), b = (int)((c & 255) * k);
  return (uint32_t)((r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b));
}
static void tri(const CV *a, const CV *b, const CV *c, const Tex *t, uint32_t colour, float shade) {
  float X[3], Y[3], IW[3], UW[3], VW[3], Z[3];
  const CV *v[3] = { a, b, c };
  for (int i = 0; i < 3; i++) {
    float iw = 1.0f / v[i]->c.w;
    X[i] = (v[i]->c.x * iw * 0.5f + 0.5f) * GW; Y[i] = (0.5f - v[i]->c.y * iw * 0.5f) * GH;
    IW[i] = iw; UW[i] = v[i]->u * iw; VW[i] = v[i]->v * iw; Z[i] = v[i]->c.w;
  }
  float area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
  if (fabsf(area) < 1e-6f) return;
  int x0 = (int)floorf(fminf(X[0], fminf(X[1], X[2]))), x1 = (int)ceilf(fmaxf(X[0], fmaxf(X[1], X[2])));
  int y0 = (int)floorf(fminf(Y[0], fminf(Y[1], Y[2]))), y1 = (int)ceilf(fmaxf(Y[0], fmaxf(Y[1], Y[2])));
  if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > GW - 1) x1 = GW - 1; if (y1 > GH - 1) y1 = GH - 1;
  uint32_t flat = shade_c(colour, shade);
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
    float px = x + 0.5f, py = y + 0.5f;
    float w0 = ((X[1] - px) * (Y[2] - py) - (X[2] - px) * (Y[1] - py)) / area;
    float w1 = ((X[2] - px) * (Y[0] - py) - (X[0] - px) * (Y[2] - py)) / area;
    float w2 = 1 - w0 - w1;
    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
    float iw = w0 * IW[0] + w1 * IW[1] + w2 * IW[2], z = 1.0f / iw;
    int k = y * GW + x;
    if (z >= zbuf[k]) continue;
    uint32_t col = flat;
    if (t) {
      float u = (w0 * UW[0] + w1 * UW[1] + w2 * UW[2]) * z, vv = (w0 * VW[0] + w1 * VW[1] + w2 * VW[2]) * z;
      int tx = (int)(u * t->w), ty = (int)(vv * t->h);
      if (tx < 0) tx = 0; if (ty < 0) ty = 0; if (tx >= t->w) tx = t->w - 1; if (ty >= t->h) ty = t->h - 1;
      col = shade_c(t->px[ty * t->w + tx], shade);
    }
    zbuf[k] = z; gfx_px[k] = col;
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
void gfx_line3(Vec3 a, Vec3 b, uint32_t c, int thick) {
  // a line with depth: drawn where it is in front of (or within a small bias of) what is already there
  V4 A = xform(a), B = xform(b);
  const float NEAR = 0.05f;
  if (A.w < NEAR && B.w < NEAR) return;
  if (A.w < NEAR) { float k = (NEAR - A.w) / (B.w - A.w); A.x += (B.x - A.x) * k; A.y += (B.y - A.y) * k; A.w = NEAR; }
  if (B.w < NEAR) { float k = (NEAR - B.w) / (A.w - B.w); B.x += (A.x - B.x) * k; B.y += (A.y - B.y) * k; B.w = NEAR; }
  float x0 = (A.x / A.w * 0.5f + 0.5f) * GW, y0 = (0.5f - A.y / A.w * 0.5f) * GH;
  float x1 = (B.x / B.w * 0.5f + 0.5f) * GW, y1 = (0.5f - B.y / B.w * 0.5f) * GH;
  int steps = (int)fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) + 1;
  if (steps > 4000) steps = 4000;
  for (int i = 0; i <= steps; i++) {
    float k = (float)i / steps, x = x0 + (x1 - x0) * k, y = y0 + (y1 - y0) * k;
    float iw = (1 - k) / A.w + k / B.w, z = 1.0f / iw;          // perspective-correct depth
    for (int dy = 0; dy < thick; dy++) for (int dx = 0; dx < thick; dx++) {
      int xi = (int)x + dx - thick / 2, yi = (int)y + dy - thick / 2;
      if (xi < 0 || yi < 0 || xi >= GW || yi >= GH) continue;
      int q = yi * GW + xi;
      if (z <= zbuf[q] * 1.002f + 0.002f) gfx_px[q] = c;
    }
  }
}

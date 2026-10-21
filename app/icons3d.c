// FOLD app: the level view's control pictures, in real 3D (the owner's idea): small models on the brushed-metal
// panels, drawn every frame, that move and play up what a control does when it is used: the thumbstick tilts, the
// arrow curling round it that way pops and the planes turn; the D-pad tips and the cursor slides on its grid; face buttons sink and glow;
// the camera turns with the right stick. The stick, D-pad and button models are the Duke controller's parts from the
// VMX film (assets/duke.mesh, made by tools/make_duke.py); until that file loads, simple stand-in shapes are drawn.
#include "icons3d.h"
#include "gfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265f
static Vec3 eye;                               // the current icon's camera (for which side of a quad faces it)
static const float LX = 0.38f, LY = -0.52f, LZ = 0.76f;   // light: up, to the side, a little towards the camera

// smoothed control state, eased each frame towards the pad
static float stX, stY, rsX, rsY, dpX, dpY, spin, spinV = 0.03f, planeRx, planeRz, goalRx, goalRz, press[4], curX, curY, pop[4];   // pop: D-pad up right down left
static unsigned prevHeld; static int tt, prevStick = -1;

void icons_update(const Pad *p) {
  tt++;
  stX += (p->lx - stX) * 0.3f; stY += (p->ly - stY) * 0.3f;
  rsX += (p->rx - rsX) * 0.3f; rsY += (p->ry - rsY) * 0.3f;
  float dx = (p->held & PAD_RIGHT ? 1.f : 0.f) - (p->held & PAD_LEFT ? 1.f : 0.f), dy = (p->held & PAD_DOWN ? 1.f : 0.f) - (p->held & PAD_UP ? 1.f : 0.f);
  dpX += (dx - dpX) * 0.35f; dpY += (dy - dpY) * 0.35f;
  // a flick of the left stick (it rolls the plane) pops that way's arrow and turns the planes
  int sd = p->lx > 0.5f && p->lx >= (p->ly < 0 ? -p->ly : p->ly) ? 1 : p->lx < -0.5f && -p->lx >= (p->ly < 0 ? -p->ly : p->ly) ? 3
         : p->ly < -0.5f ? 0 : p->ly > 0.5f ? 2 : -1;
  if (sd >= 0 && sd != prevStick) {
    if (sd == 0) goalRx -= 90; else if (sd == 1) goalRz += 90; else if (sd == 2) goalRx += 90; else goalRz -= 90;
    pop[sd] = 1;
  }
  prevStick = sd;
  for (int k = 0; k < 4; k++) pop[k] *= 0.88f;
  spinV += ((p->rx != 0 ? p->rx * 0.15f : 0.03f) - spinV) * 0.05f;
  spin += spinV;
  planeRx += (goalRx - planeRx) * 0.18f; planeRz += (goalRz - planeRz) * 0.18f;
  const unsigned bits[4] = { PAD_Y, PAD_X, PAD_B, PAD_A };
  for (int k = 0; k < 4; k++) press[k] += ((p->held & bits[k] ? 1.f : 0.f) - press[k]) * 0.4f;
  curX += (dx - curX) * 0.25f; curY += (-dy - curY) * 0.25f;   // the D-pad slides the cursor on the grid
  prevHeld = p->held;
}

// ---- building blocks ----------------------------------------------------------------------------------------------
typedef struct { float m[9]; Vec3 t; } Xf;      // rotation (rows) then translation
static Xf xf(Vec3 t) { Xf x = { { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, t }; return x; }
static Xf rot(Xf x, int axis, float deg) {      // turn x's frame about its own axis
  float a = deg * PI / 180, c = cosf(a), s = sinf(a), R[9];
  if (axis == 0) { float r[9] = { 1, 0, 0, 0, c, -s, 0, s, c }; memcpy(R, r, sizeof R); }
  else if (axis == 1) { float r[9] = { c, 0, s, 0, 1, 0, -s, 0, c }; memcpy(R, r, sizeof R); }
  else { float r[9] = { c, -s, 0, s, c, 0, 0, 0, 1 }; memcpy(R, r, sizeof R); }
  Xf o = x;
  for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { float v = 0; for (int k = 0; k < 3; k++) v += x.m[i * 3 + k] * R[k * 3 + j]; o.m[i * 3 + j] = v; }
  return o;
}
static Vec3 P(const Xf *x, float a, float b, float c) {
  return v3f(x->t.x + x->m[0] * a + x->m[1] * b + x->m[2] * c, x->t.y + x->m[3] * a + x->m[4] * b + x->m[5] * c, x->t.z + x->m[6] * a + x->m[7] * b + x->m[8] * c);
}
static Vec3 sub3(Vec3 a, Vec3 b) { return v3f(a.x - b.x, a.y - b.y, a.z - b.z); }
// a quad, coloured by which side faces the camera, shaded by the light on that side
static void quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t front, uint32_t back) {
  Vec3 u = sub3(b, a), v = sub3(d, a), n = v3f(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
  if (n.x == 0 && n.y == 0 && n.z == 0) { u = sub3(c, a); n = v3f(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x); }
  Vec3 m = v3f((a.x + c.x) / 2, (a.y + c.y) / 2, (a.z + c.z) / 2), e = sub3(eye, m);
  int facing = n.x * e.x + n.y * e.y + n.z * e.z > 0;
  float l = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z) + 1e-9f, k = (n.x * LX + n.y * LY + n.z * LZ) / l * (facing ? 1 : -1);
  Vec3 Q[4] = { a, b, c, d };
  gfx_quad(Q, NULL, facing ? front : back, 0.42f + 0.62f * (k > 0 ? k : 0));
}
static void box(const Xf *x, float cx, float cy, float cz, float sx, float sy, float sz, uint32_t col) {   // centred
  float X0 = cx - sx / 2, X1 = cx + sx / 2, Y0 = cy - sy / 2, Y1 = cy + sy / 2, Z0 = cz - sz / 2, Z1 = cz + sz / 2;
  quad(P(x, X0, Y0, Z1), P(x, X1, Y0, Z1), P(x, X1, Y1, Z1), P(x, X0, Y1, Z1), col, col);   // top
  quad(P(x, X0, Y0, Z0), P(x, X0, Y1, Z0), P(x, X1, Y1, Z0), P(x, X1, Y0, Z0), col, col);
  quad(P(x, X0, Y0, Z0), P(x, X1, Y0, Z0), P(x, X1, Y0, Z1), P(x, X0, Y0, Z1), col, col);
  quad(P(x, X1, Y1, Z0), P(x, X0, Y1, Z0), P(x, X0, Y1, Z1), P(x, X1, Y1, Z1), col, col);
  quad(P(x, X0, Y1, Z0), P(x, X0, Y0, Z0), P(x, X0, Y0, Z1), P(x, X0, Y1, Z1), col, col);
  quad(P(x, X1, Y0, Z0), P(x, X1, Y1, Z0), P(x, X1, Y1, Z1), P(x, X1, Y0, Z1), col, col);
}
static void cyl(const Xf *x, float r, float z0, float z1, uint32_t side, uint32_t top) {   // upright, along local z
  const int n = 18;
  for (int i = 0; i < n; i++) {
    float a0 = 2 * PI * i / n, a1 = 2 * PI * (i + 1) / n, c0 = cosf(a0) * r, s0 = sinf(a0) * r, c1 = cosf(a1) * r, s1 = sinf(a1) * r;
    quad(P(x, c0, s0, z0), P(x, c1, s1, z0), P(x, c1, s1, z1), P(x, c0, s0, z1), side, side);
    quad(P(x, 0, 0, z1), P(x, c0, s0, z1), P(x, c1, s1, z1), P(x, c1, s1, z1), top, top);
  }
}
static void dome(const Xf *x, float r, float z0, float h, uint32_t col) {   // a button cap: a rounded top
  const int n = 18, rings = 4;
  for (int k = 0; k < rings; k++) {
    float a0 = (PI / 2) * k / rings, a1 = (PI / 2) * (k + 1) / rings;
    float r0 = r * cosf(a0), r1 = r * cosf(a1), h0 = z0 + h * sinf(a0), h1 = z0 + h * sinf(a1);
    for (int i = 0; i < n; i++) {
      float b0 = 2 * PI * i / n, b1 = 2 * PI * (i + 1) / n;
      quad(P(x, cosf(b0) * r0, sinf(b0) * r0, h0), P(x, cosf(b1) * r0, sinf(b1) * r0, h0), P(x, cosf(b1) * r1, sinf(b1) * r1, h1), P(x, cosf(b0) * r1, sinf(b0) * r1, h1), col, col);
    }
  }
}
static void wedge(const Xf *x, float len, float wid, float h, uint32_t col) {   // a flat 3D arrowhead pointing along +x
  Vec3 t0 = P(x, len, 0, 0), l0 = P(x, 0, -wid, 0), r0 = P(x, 0, wid, 0), t1 = P(x, len, 0, h), l1 = P(x, 0, -wid, h), r1 = P(x, 0, wid, h);
  quad(l1, t1, r1, r1, col, col); quad(l0, r0, t0, t0, col, col);
  quad(l0, t0, t1, l1, col, col); quad(t0, r0, r1, t1, col, col); quad(r0, l0, l1, r1, col, col);
}
// a ribbon arrow going round local z: an arc of band from angle a0 to a1 (degrees) at radius r, centred at height zc,
// width w, with a head; one colour for its outside, one for its inside
static void ribbon(const Xf *x, float r, float a0, float a1, float zc, float w, uint32_t out, uint32_t in) {
  const float head = 26; float dir = a1 > a0 ? 1 : -1, ae = a1 - dir * head;
  int n = (int)(fabsf(ae - a0) / 8) + 1;
  for (int i = 0; i < n; i++) {
    float b0 = (a0 + (ae - a0) * i / n) * PI / 180, b1 = (a0 + (ae - a0) * (i + 1) / n) * PI / 180;
    Vec3 p0 = P(x, r * cosf(b0), r * sinf(b0), zc - w / 2), p1 = P(x, r * cosf(b1), r * sinf(b1), zc - w / 2), p2 = P(x, r * cosf(b1), r * sinf(b1), zc + w / 2), p3 = P(x, r * cosf(b0), r * sinf(b0), zc + w / 2);
    if (dir > 0) quad(p0, p1, p2, p3, out, in); else quad(p1, p0, p3, p2, out, in);
  }
  float e = ae * PI / 180, t = a1 * PI / 180, hw = w * 0.95f;     // the head: wider, to a point
  Vec3 b0 = P(x, r * cosf(e), r * sinf(e), zc - hw), b1 = P(x, r * cosf(e), r * sinf(e), zc + hw), tip = P(x, r * cosf(t), r * sinf(t), zc);
  if (dir > 0) quad(b0, tip, tip, b1, out, in); else quad(tip, b0, b1, tip, out, in);
}
// ---- the Duke parts --------------------------------------------------------------------------------------------------
typedef struct { char name[12]; float c[3]; int nv, nt; float *v; uint16_t *t; } Part;   // v: pos[3] normal[3]
enum { DPAD, STICK_L, STICK_R, BTN_A, BTN_B, BTN_X, BTN_Y, NPARTS };
static Part parts[NPARTS]; static int partsTried, partsOk;
static void load_parts(void) {
  partsTried = 1;
  FILE *f = fopen("assets/duke.mesh", "rb"); if (!f) return;
  char magic[4]; unsigned n = 0;
  if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "DUKE", 4) || fread(&n, 4, 1, f) != 1 || n != NPARTS) { fclose(f); return; }
  for (int k = 0; k < NPARTS; k++) {
    Part *q = &parts[k]; unsigned nv, nt;
    if (fread(q->name, 1, 12, f) != 12 || fread(q->c, 4, 3, f) != 3 || fread(&nv, 4, 1, f) != 1 || fread(&nt, 4, 1, f) != 1) { fclose(f); return; }
    q->nv = (int)nv; q->nt = (int)nt; q->v = malloc(nv * 6 * sizeof(float)); q->t = malloc(nt * 3 * sizeof(uint16_t));
    if (fread(q->v, sizeof(float), nv * 6, f) != nv * 6 || fread(q->t, sizeof(uint16_t), nt * 3, f) != nt * 3) { fclose(f); return; }
  }
  fclose(f); partsOk = 1;
}
// a part, at x's frame, scaled from millimetres, its own middle at the frame's origin; lit per corner with a shine
static void part(const Xf *x, int k, float scale, uint32_t col, float glow) {
  static Vec3 W[2048]; static float L[2048];
  const Part *q = &parts[k]; if (q->nv > 2048) return;
  for (int i = 0; i < q->nv; i++) {
    const float *v = q->v + i * 6;
    W[i] = P(x, v[0] * scale, v[1] * scale, v[2] * scale);
    float nx = x->m[0] * v[3] + x->m[1] * v[4] + x->m[2] * v[5], ny = x->m[3] * v[3] + x->m[4] * v[4] + x->m[5] * v[5], nz = x->m[6] * v[3] + x->m[7] * v[4] + x->m[8] * v[5];
    Vec3 e = sub3(eye, W[i]); float el = sqrtf(e.x * e.x + e.y * e.y + e.z * e.z) + 1e-9f;
    float hx = LX + e.x / el, hy = LY + e.y / el, hz = LZ + e.z / el, hl = sqrtf(hx * hx + hy * hy + hz * hz) + 1e-9f;
    float d = nx * LX + ny * LY + nz * LZ, sp = (nx * hx + ny * hy + nz * hz) / hl;
    if (d < 0) d = 0; if (sp < 0) sp = 0;
    sp = sp * sp; sp = sp * sp; sp = sp * sp; sp = sp * sp;              // tight highlight
    L[i] = 0.34f + 0.72f * d + glow + (d > 0 ? 0.9f * sp : 0);
  }
  for (int j = 0; j < q->nt; j++) {
    const uint16_t *t = q->t + j * 3;
    Vec3 a = W[t[0]], b = W[t[1]], c = W[t[2]], u = sub3(b, a), v = sub3(c, a), e = sub3(eye, a);
    Vec3 n = v3f(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
    if (n.x * e.x + n.y * e.y + n.z * e.z <= 0) continue;              // facing away
    Vec3 p3[3] = { a, b, c }; float sh[3] = { L[t[0]], L[t[1]], L[t[2]] };
    gfx_tri_lit(p3, sh, col);
  }
}
static float part_h(int k) {                    // how tall a part stands (mm), from its lowest point to its highest
  float lo = 1e9f, hi = -1e9f; for (int i = 0; i < parts[k].nv; i++) { float z = parts[k].v[i * 6 + 2]; if (z < lo) lo = z; if (z > hi) hi = z; }
  return hi - lo;
}

// an arrow curling round a ball (middle c, radius r): from th0 degrees off its pole f (the side facing the camera)
// towards d, round behind to th1; width w, with a head. Like the four round the D-pad in the owner's sketch.
static void curl(Vec3 c, float r, Vec3 f, Vec3 d, float th0, float th1, float w, uint32_t col) {
  const float head = 30; float ae = th1 - head;
  Vec3 A = v3f(f.y * d.z - f.z * d.y, f.z * d.x - f.x * d.z, f.x * d.y - f.y * d.x);
  #define CP(th, off) v3f(c.x + (cosf((th) * PI / 180) * f.x + sinf((th) * PI / 180) * d.x) * r + A.x * (off), \
                          c.y + (cosf((th) * PI / 180) * f.y + sinf((th) * PI / 180) * d.y) * r + A.y * (off), \
                          c.z + (cosf((th) * PI / 180) * f.z + sinf((th) * PI / 180) * d.z) * r + A.z * (off))
  int n = (int)((ae - th0) / 7) + 1;
  for (int i = 0; i < n; i++) {
    float a0 = th0 + (ae - th0) * i / n, a1 = th0 + (ae - th0) * (i + 1) / n;
    quad(CP(a0, -w / 2), CP(a1, -w / 2), CP(a1, w / 2), CP(a0, w / 2), col, col);
  }
  quad(CP(ae, -w), CP(th1, 0), CP(th1, 0), CP(ae, w), col, col);
  #undef CP
}

// set up a small camera for a panel: the scene's middle at the panel's middle, span units filling its width
static void scene(int x, int y, int w, int h, float dist, float elev, float span) {   // span: units across the panel
  gfx_clip(x, y, w, h); gfx_zclear_rect(x, y, w, h); gfx_fog(0, 1e9f, 2e9f);   // no fog on the icons
  gfx_view_offset(x + w / 2 - GW / 2, y + h / 2 - GH / 2);
  eye = v3f(0, -dist * cosf(elev), dist * sinf(elev));
  gfx_camera(mat_mul(mat_perspective(2 * atanf(span * GH / w / 2 / dist), (float)GW / GH, 0.5f, 100.0f), mat_lookat(eye, v3f(0, 0, 0.2f), v3f(0, 0, 1))));
}
static void done(void) { gfx_clip(0, 0, 0, 0); gfx_view_offset(0, 0); }
static uint32_t brighter(uint32_t c, float k) {
  int r = (int)(((c >> 16) & 255) + (255 - ((c >> 16) & 255)) * k), g = (int)(((c >> 8) & 255) + (255 - ((c >> 8) & 255)) * k), b = (int)((c & 255) + (255 - (c & 255)) * k);
  return (uint32_t)(r << 16 | g << 8 | b);
}
static const uint32_t YEL = 0xf2e01c, INK = 0x161616, INK2 = 0x2c2c2c;
static void thumbstick(Xf base, float tiltX, float tiltY, uint32_t cap, int which) {
  if (!partsTried) load_parts();
  if (partsOk) {                                                        // the Duke stick, tipped about its foot
    const float sc = 0.06f, h = part_h(which) * sc;
    Xf s = rot(rot(base, 1, tiltX * 24), 0, tiltY * 24);
    s.t = P(&s, 0, 0, h / 2);
    part(&s, which, sc, 0x2a2b2e, 0);
    return;
  }
  cyl(&base, 0.82f, -0.06f, 0.06f, INK2, 0x353535);                     // the round base
  Xf s = rot(rot(base, 1, tiltX * 22), 0, tiltY * 22);                  // tipped by the stick
  cyl(&s, 0.2f, 0.06f, 0.55f, INK, INK);
  cyl(&s, 0.56f, 0.55f, 0.72f, cap, 0x262626);
  cyl(&s, 0.42f, 0.72f, 0.75f, 0x3a3a3a, 0x1e1e1e);                     // the dished top
}

// ---- the four panels ----------------------------------------------------------------------------------------------
// four arrows curling round a control (frame b, middle at height zc), one each way; q[] makes one swell
static void curls(const Xf *b, float zc, const float *q) {
  for (int k = 0; k < 4; k++) {
    float dx = k == 1 ? 1.f : k == 3 ? -1.f : 0.f, dy = k == 0 ? 1.f : k == 2 ? -1.f : 0.f;
    Vec3 c = P(b, 0, 0, zc), e = sub3(eye, c); float el = sqrtf(e.x * e.x + e.y * e.y + e.z * e.z);
    Vec3 f0 = v3f(e.x / el, e.y / el, e.z / el), up = v3f(0, f0.z, -f0.y);   // towards the camera; screen up
    const float tc = cosf(0.5f), ts = sinf(0.5f);                       // the ball's pole tipped up-left, to see them wrap
    Vec3 tilt = v3f(-0.6f, 0.8f * up.y, 0.8f * up.z), f = v3f(f0.x * tc + tilt.x * ts, f0.y * tc + tilt.y * ts, f0.z * tc + tilt.z * ts);
    Vec3 d0 = v3f(dx, dy * up.y, dy * up.z); float fd = d0.x * f.x + d0.y * f.y + d0.z * f.z;
    Vec3 d = v3f(d0.x - f.x * fd, d0.y - f.y * fd, d0.z - f.z * fd); float dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    d = v3f(d.x / dl, d.y / dl, d.z / dl);                              // that way on screen, square to the pole
    curl(c, 1.0f + q[k] * 0.2f, f, d, 45 - q[k] * 8, 165 + q[k] * 20, 0.36f + q[k] * 0.12f, brighter(YEL, q[k] * 0.6f));
  }
}
static void draw_stick(int x, int y, int w, int h) {   // the left stick rolls the working plane round the shape
  scene(x, y, w, h, 15, 0.72f, 4.8f);
  float bob = sinf(tt * 0.05f) * 0.03f;
  Xf b = xf(v3f(-1.2f, 0, bob));
  thumbstick(b, stX, -stY, INK, STICK_L);
  curls(&b, 0.45f, pop);
  Xf p = rot(rot(xf(v3f(1.3f, 0, 0.35f)), 0, planeRx), 2, planeRz);    // the planes, turning with each flick
  const float s = 0.55f;
  quad(P(&p, -s, 0, -s), P(&p, s, 0, -s), P(&p, s, 0, s), P(&p, -s, 0, s), 0xe03030, 0xa02020);
  quad(P(&p, 0, -s, -s), P(&p, 0, s, -s), P(&p, 0, s, s), P(&p, 0, -s, s), 0x3050e0, 0x2030a0);
  quad(P(&p, -s, -s, 0), P(&p, s, -s, 0), P(&p, s, s, 0), P(&p, -s, s, 0), 0x52c41a, 0x3a8a14);
  done();
}
static void draw_dpad(int x, int y, int w, int h) {    // the D-pad moves the cursor over the grid
  scene(x, y, w, h, 15, 0.72f, 4.8f);
  Xf b = rot(rot(xf(v3f(-1.25f, 0, 0)), 2, -32), 0, 20);              // stood up, turned away to the left
  Xf d = rot(rot(b, 1, dpX * 16), 0, -dpY * 16);                        // tipped by a press
  if (!partsTried) load_parts();
  if (partsOk) { Xf dd = d; dd.t = P(&d, 0, 0, part_h(DPAD) * 0.062f / 2); part(&dd, DPAD, 0.062f, 0x2a2b2e, 0); }
  else {
    cyl(&d, 0.8f, -0.06f, 0.06f, INK2, 0x2e2e2e);
    box(&d, 0, 0, 0.13f, 1.12f, 0.36f, 0.14f, INK); box(&d, 0, 0, 0.13f, 0.36f, 1.12f, 0.14f, INK);
    cyl(&d, 0.14f, 0.2f, 0.22f, 0x2a2a2a, 0x3a3a3a);
  }
  for (int k = 0; k < 4; k++) {                                         // a yellow arrowhead off each arm
    float ax = k == 0 ? 1.f : k == 1 ? -1.f : 0.f, ay = k == 2 ? 1.f : k == 3 ? -1.f : 0.f;
    float push = ax ? dpX * ax : -dpY * ay; if (push < 0) push = 0;
    Xf a = b; a.t = P(&b, ax * (0.95f + push * 0.25f), ay * (0.95f + push * 0.25f), 0.05f);
    a = rot(a, 2, k == 0 ? 0 : k == 1 ? 180 : k == 2 ? 90 : -90);
    wedge(&a, 0.34f + push * 0.25f, 0.24f + push * 0.1f, 0.12f, brighter(YEL, push * 0.5f));
  }
  Xf g = xf(v3f(1.45f, 0.1f, 0.05f));                                   // the grid, and the cursor on it
  for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) box(&g, i * 0.52f, j * 0.52f, 0, 0.44f, 0.44f, 0.06f, 0x52c41a);
  float cx = curX * 0.52f, cy = curY * 0.52f; const uint32_t CY = 0x22e0ea;
  for (int k = 0; k < 4; k++) {
    float sx = k & 1 ? 1 : -1, sy = k & 2 ? 1 : -1;
    box(&g, cx + sx * 0.25f, cy + sy * 0.18f, 0.08f, 0.05f, 0.16f, 0.06f, CY);
    box(&g, cx + sx * 0.18f, cy + sy * 0.25f, 0.08f, 0.16f, 0.05f, 0.06f, CY);
  }
  done();
}
static void draw_buttons(int x, int y, int w, int h) { // the face buttons, Duke-style: Y X B A
  scene(x, y, w, h, 13, 0.95f, 2.9f);
  struct { float x, y; uint32_t c; const char *l; } fb[4] = { { -0.2f, 0.8f, 0xe8d020, "Y" }, { -0.8f, 0.1f, 0x2050e0, "X" },
                                                            { 0.75f, 0.25f, 0xe02828, "B" }, { 0.15f, -0.6f, 0x52d020, "A" } };
  if (!partsTried) load_parts();
  if (partsOk) {                                 // the Duke's own buttons, where they sit on the pad
    const int id[4] = { BTN_Y, BTN_X, BTN_B, BTN_A }; const float sc = 0.06f; float m[3] = { 0, 0, 0 };
    for (int k = 0; k < 4; k++) for (int i = 0; i < 3; i++) m[i] += parts[id[k]].c[i] / 4;
    for (int k = 0; k < 4; k++) {
      const Part *q = &parts[id[k]];
      fb[k].x = (q->c[0] - m[0]) * sc; fb[k].y = (q->c[1] - m[1]) * sc;
      Xf b = xf(v3f(fb[k].x, fb[k].y, (q->c[2] - m[2]) * sc - press[k] * 0.14f));
      part(&b, id[k], sc, brighter(fb[k].c, press[k] * 0.3f), press[k] * 0.35f);
    }
    for (int k = 0; k < 4; k++) {
      float sx, sy; if (gfx_project(v3f(fb[k].x, fb[k].y, part_h(id[k]) * sc / 2 + 0.02f - press[k] * 0.14f), &sx, &sy)) gfx_text5((int)sx - 2, (int)sy - 3, 1, 0x101010, fb[k].l);
    }
    done(); return;
  }
  for (int k = 0; k < 4; k++) {
    float pz = -press[k] * 0.12f;
    Xf b = xf(v3f(fb[k].x, fb[k].y, pz));
    cyl(&b, 0.36f, -0.08f, 0.04f, INK, INK);                            // the hole round it
    uint32_t c = brighter(fb[k].c, press[k] * 0.45f);
    cyl(&b, 0.31f, 0.0f, 0.12f, c, c); dome(&b, 0.31f, 0.12f, 0.14f, c);
  }
  for (int k = 0; k < 4; k++) {                                         // the letters, on top
    float sx, sy; if (gfx_project(v3f(fb[k].x, fb[k].y, 0.3f - press[k] * 0.12f), &sx, &sy)) gfx_text5((int)sx - 2, (int)sy - 3, 1, 0x101010, fb[k].l);
  }
  done();
}
static void draw_camera(int x, int y, int w, int h) {  // the camera turns with the right stick (and L / R)
  scene(x, y, w, h, 15, 0.5f, 5.0f);
  Xf c = rot(rot(xf(v3f(-1.45f, 0, 0.35f)), 2, 15 + rsX * 30), 1, rsY * 15);   // a film camera, turned by the stick
  box(&c, 0, 0, 0, 0.9f, 0.45f, 0.5f, 0x242424);
  Xf lens = rot(c, 1, -90); lens.t = P(&c, -0.45f, 0, 0);
  cyl(&lens, 0.16f, 0, 0.32f, 0x1a1a1a, 0x5a6a7a);
  for (int k = 0; k < 2; k++) { Xf reel = rot(c, 0, 90); reel.t = P(&c, -0.22f + 0.46f * k, 0, 0.5f); cyl(&reel, 0.27f, -0.07f, 0.07f, 0x303030, 0x8a8e92); }
  Xf a = rot(xf(v3f(0.0f, 0, 0.2f)), 2, 180); wedge(&a, 0.36f, 0.22f, 0.12f, YEL);   // "turns"
  box(&a, -0.25f, 0, 0.06f, 0.3f, 0.16f, 0.12f, YEL);
  Xf b = xf(v3f(1.3f, 0, 0));
  Xf r = rot(b, 0, 12);
  ribbon(&r, 1.0f, -spin * 57 + 30, -spin * 57 + 150, 0.4f, 0.28f, YEL, YEL);
  ribbon(&r, 1.0f, -spin * 57 + 210, -spin * 57 + 330, 0.4f, 0.28f, YEL, YEL);
  thumbstick(b, rsX, -rsY, INK, STICK_R);
  done();
}

// ---- the panels, cached: a panel is drawn again only while its control is in use (or still easing back), or, for the
// two with an idle animation, every other frame; otherwise its last picture is copied back (the software renderer's
// biggest saving on the Xbox)
static uint32_t cache[4][170 * 140]; static int cacheOk[4], cacheX[4], cacheY[4], cacheW[4], cacheH[4], drawN;
static void panel(int k, int x, int y, int w, int h, int active, int idleAnim, void (*draw)(int, int, int, int)) {
  if (k == 0) drawN++;
  int fresh = cacheOk[k] && cacheX[k] == x && cacheY[k] == y && cacheW[k] == w && cacheH[k] == h && w * h <= 170 * 140;
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > GW ? GW : x + w, y1 = y + h > GH ? GH : y + h;
  if (fresh && !active && !(idleAnim && ((drawN + k) & 1))) {
    for (int j = y0; j < y1; j++) memcpy(gfx_px + j * GW + x0, cache[k] + (j - y) * w + (x0 - x), sizeof(uint32_t) * (size_t)(x1 - x0));
    return;
  }
  draw(x, y, w, h);
  if (w * h > 170 * 140) return;
  for (int j = y0; j < y1; j++) memcpy(cache[k] + (j - y) * w + (x0 - x), gfx_px + j * GW + x0, sizeof(uint32_t) * (size_t)(x1 - x0));
  cacheOk[k] = 1; cacheX[k] = x; cacheY[k] = y; cacheW[k] = w; cacheH[k] = h;
}
static int moving(float v) { return v > 0.01f || v < -0.01f; }
void icons_stick(int x, int y, int w, int h) {
  int a = moving(stX) || moving(stY) || moving((planeRx - goalRx) * 0.02f) || moving((planeRz - goalRz) * 0.02f);
  for (int k = 0; k < 4; k++) a |= moving(pop[k]);
  panel(0, x, y, w, h, a, 1, draw_stick);
}
void icons_dpad(int x, int y, int w, int h) { panel(1, x, y, w, h, moving(dpX) || moving(dpY) || moving(curX) || moving(curY), 0, draw_dpad); }
void icons_buttons(int x, int y, int w, int h) { panel(2, x, y, w, h, moving(press[0]) || moving(press[1]) || moving(press[2]) || moving(press[3]), 0, draw_buttons); }
void icons_camera(int x, int y, int w, int h) { panel(3, x, y, w, h, moving(rsX) || moving(rsY), 1, draw_camera); }

// ---- the compass: the world's axes as the level view sees them (dir: from the look point to the camera) ----------
void icons_compass(int x, int y, int w, int h, Vec3 dir, Vec3 up) {
  gfx_clip(x, y, w, h); gfx_zclear_rect(x, y, w, h); gfx_fog(0, 1e9f, 2e9f);
  gfx_view_offset(x + w / 2 - GW / 2, y + h / 2 - GH / 2);
  float l = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z) + 1e-9f, dist = 12, span = 3.4f;
  eye = v3f(dir.x / l * dist, dir.y / l * dist, dir.z / l * dist);
  gfx_camera(mat_mul(mat_perspective(2 * atanf(span * GH / w / 2 / dist), (float)GW / GH, 0.5f, 100.0f), mat_lookat(eye, v3f(0, 0, 0), up)));
  static const uint32_t C[3] = { 0xe84848, 0x58d838, 0x4878f0 };
  static const char *N3[3] = { "E", "N", "U" }, *M3[3] = { "W", "S", "D" };   // x east, y north, z up
  for (int k = 0; k < 3; k++) {
    Xf a = xf(v3f(0, 0, 0));
    if (k == 1) a = rot(a, 2, 90); else if (k == 2) a = rot(a, 1, -90);
    box(&a, 0.55f, 0, 0, 1.1f, 0.12f, 0.12f, C[k]);                 // the shaft, then the head
    Xf hd = a; hd.t = P(&a, 1.05f, 0, -0.06f); wedge(&hd, 0.45f, 0.22f, 0.12f, C[k]);
    box(&a, -0.55f, 0, 0, 1.1f, 0.07f, 0.07f, 0x3a3f3a);            // the minus side, thin and grey
  }
  Xf o = xf(v3f(0, 0, -0.1f)); cyl(&o, 0.16f, 0, 0.2f, 0xd8d8d0, 0xf0f0e8);
  for (int k = 0; k < 3; k++) {                                     // the letters past the heads, and at the other ends
    Vec3 q = v3f(k == 0 ? 1.85f : 0, k == 1 ? 1.85f : 0, k == 2 ? 1.85f : 0); float sx, sy;
    if (gfx_project(q, &sx, &sy)) gfx_text5((int)sx - 2, (int)sy - 3, 1, C[k], N3[k]);
    if (gfx_project(v3f(-q.x * 0.75f, -q.y * 0.75f, -q.z * 0.75f), &sx, &sy)) gfx_text5((int)sx - 2, (int)sy - 3, 1, 0x8a908a, M3[k]);
  }
  done();
}

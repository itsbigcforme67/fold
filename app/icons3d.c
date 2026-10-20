// FOLD app: the level view's control pictures, in real 3D (the owner's idea): small models on the brushed-metal
// panels, drawn every frame, that move and play up what a control does when it is used: the thumbstick tilts and the
// cursor slides on its grid; the D-pad tips, its ribbon arrows (yellow outside, red inside: you see them go round)
// spin and the planes turn; face buttons sink and glow; the camera turns with the right stick.
#include "icons3d.h"
#include "gfx.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265f
static Vec3 eye;                               // the current icon's camera (for which side of a quad faces it)
static const float LX = 0.38f, LY = -0.52f, LZ = 0.76f;   // light: up, to the side, a little towards the camera

// smoothed control state, eased each frame towards the pad
static float stX, stY, rsX, rsY, dpX, dpY, spin, spinV = 0.03f, planeRx, planeRz, goalRx, goalRz, press[4], curX, curY;
static unsigned prevHeld; static int tt;

void icons_update(const Pad *p) {
  tt++;
  stX += (p->lx - stX) * 0.3f; stY += (p->ly - stY) * 0.3f;
  rsX += (p->rx - rsX) * 0.3f; rsY += (p->ry - rsY) * 0.3f;
  float dx = (p->held & PAD_RIGHT ? 1.f : 0.f) - (p->held & PAD_LEFT ? 1.f : 0.f), dy = (p->held & PAD_DOWN ? 1.f : 0.f) - (p->held & PAD_UP ? 1.f : 0.f);
  dpX += (dx - dpX) * 0.35f; dpY += (dy - dpY) * 0.35f;
  unsigned now = p->held & ~prevHeld;           // D-pad presses spin the ribbons and turn the planes
  if (now & PAD_UP) { goalRx -= 90; spinV = 0.25f; }
  if (now & PAD_DOWN) { goalRx += 90; spinV = -0.25f; }
  if (now & PAD_LEFT) { goalRz -= 90; spinV = -0.25f; }
  if (now & PAD_RIGHT) { goalRz += 90; spinV = 0.25f; }
  spinV += ((p->rx != 0 ? p->rx * 0.15f : 0.03f) - spinV) * 0.05f;
  spin += spinV;
  planeRx += (goalRx - planeRx) * 0.18f; planeRz += (goalRz - planeRz) * 0.18f;
  const unsigned bits[4] = { PAD_Y, PAD_X, PAD_B, PAD_A };
  for (int k = 0; k < 4; k++) press[k] += ((p->held & bits[k] ? 1.f : 0.f) - press[k]) * 0.4f;
  float tx = p->lx > 0.5f ? 1.f : p->lx < -0.5f ? -1.f : 0.f, ty = p->ly > 0.5f ? -1.f : p->ly < -0.5f ? 1.f : 0.f;
  curX += (tx - curX) * 0.25f; curY += (ty - curY) * 0.25f;
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
// width w, with a head; yellow on its outside, red on its inside (so its far side shows red)
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
// set up a small camera for a panel: the scene's middle at the panel's middle
static void scene(int x, int y, int w, int h, float dist, float elev) {
  gfx_clip(x, y, w, h); gfx_zclear_rect(x, y, w, h);
  gfx_view_offset(x + w / 2 - GW / 2, y + h / 2 - GH / 2);
  eye = v3f(0, -dist * cosf(elev), dist * sinf(elev));
  gfx_camera(mat_mul(mat_perspective(0.36f, (float)GW / GH, 0.5f, 100.0f), mat_lookat(eye, v3f(0, 0, 0.2f), v3f(0, 0, 1))));
}
static void done(void) { gfx_clip(0, 0, 0, 0); gfx_view_offset(0, 0); }
static uint32_t brighter(uint32_t c, float k) {
  int r = (int)(((c >> 16) & 255) + (255 - ((c >> 16) & 255)) * k), g = (int)(((c >> 8) & 255) + (255 - ((c >> 8) & 255)) * k), b = (int)((c & 255) + (255 - (c & 255)) * k);
  return (uint32_t)(r << 16 | g << 8 | b);
}
static const uint32_t YEL = 0xf2e01c, RED = 0xe02020, INK = 0x161616, INK2 = 0x2c2c2c;
static void thumbstick(Xf base, float tiltX, float tiltY, uint32_t cap) {
  cyl(&base, 0.82f, -0.06f, 0.06f, INK2, 0x353535);                     // the round base
  Xf s = rot(rot(base, 1, tiltX * 22), 0, tiltY * 22);                  // tipped by the stick
  cyl(&s, 0.2f, 0.06f, 0.55f, INK, INK);
  cyl(&s, 0.56f, 0.55f, 0.72f, cap, 0x262626);
  cyl(&s, 0.42f, 0.72f, 0.75f, 0x3a3a3a, 0x1e1e1e);                     // the dished top
}

// ---- the four panels ----------------------------------------------------------------------------------------------
void icons_stick(int x, int y, int w, int h) {   // the left stick moves the cursor over the grid
  scene(x, y, w, h, 15, 0.62f);
  float bob = sinf(tt * 0.05f) * 0.03f;
  Xf b = xf(v3f(-1.3f, 0, bob));
  for (int k = 0; k < 4; k++) {                                         // yellow arrowheads round the base
    float dx = k == 0 ? 1.f : k == 1 ? -1.f : 0.f, dy = k == 2 ? 1.f : k == 3 ? -1.f : 0.f;
    float push = dx ? stX * dx : -stY * dy; if (push < 0) push = 0;
    Xf a = rot(xf(v3f(-1.3f + dx * (1.0f + push * 0.25f), dy * (1.0f + push * 0.25f), 0.02f)), 2, k == 0 ? 0 : k == 1 ? 180 : k == 2 ? 90 : -90);
    wedge(&a, 0.34f + push * 0.25f, 0.24f + push * 0.1f, 0.12f, brighter(YEL, push * 0.5f));
  }
  thumbstick(b, stX, -stY, INK);
  Xf g = rot(xf(v3f(1.45f, 0.1f, 0.05f)), 0, 0);                        // the grid, and the cursor on it
  for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) box(&g, i * 0.52f, j * 0.52f, 0, 0.44f, 0.44f, 0.06f, 0x52c41a);
  float cx = curX * 0.52f, cy = curY * 0.52f; const uint32_t CY = 0x22e0ea;
  for (int k = 0; k < 4; k++) {
    float sx = k & 1 ? 1 : -1, sy = k & 2 ? 1 : -1;
    box(&g, cx + sx * 0.25f, cy + sy * 0.18f, 0.08f, 0.05f, 0.16f, 0.06f, CY);
    box(&g, cx + sx * 0.18f, cy + sy * 0.25f, 0.08f, 0.16f, 0.05f, 0.06f, CY);
  }
  done();
}
void icons_dpad(int x, int y, int w, int h) {    // the D-pad turns the working plane
  scene(x, y, w, h, 15, 0.55f);
  Xf b = xf(v3f(-1.15f, 0, 0));
  Xf d = rot(rot(b, 1, dpX * 16), 0, -dpY * 16);                        // tipped by a press
  cyl(&d, 0.8f, -0.06f, 0.06f, INK2, 0x2e2e2e);
  box(&d, 0, 0, 0.13f, 1.12f, 0.36f, 0.14f, INK); box(&d, 0, 0, 0.13f, 0.36f, 1.12f, 0.14f, INK);
  cyl(&d, 0.14f, 0.2f, 0.22f, 0x2a2a2a, 0x3a3a3a);
  Xf r = rot(b, 0, 14);                                                 // the ribbon arrows round it
  ribbon(&r, 1.05f, spin * 57 + 20, spin * 57 + 140, 0.35f, 0.3f, YEL, RED);
  ribbon(&r, 1.05f, spin * 57 + 200, spin * 57 + 320, 0.35f, 0.3f, YEL, RED);
  Xf p = rot(rot(xf(v3f(1.3f, 0, 0.35f)), 0, planeRx), 2, planeRz);    // the planes, turning with each press
  const float s = 0.55f;
  quad(P(&p, -s, 0, -s), P(&p, s, 0, -s), P(&p, s, 0, s), P(&p, -s, 0, s), 0xe03030, 0xa02020);
  quad(P(&p, 0, -s, -s), P(&p, 0, s, -s), P(&p, 0, s, s), P(&p, 0, -s, s), 0x3050e0, 0x2030a0);
  quad(P(&p, -s, -s, 0), P(&p, s, -s, 0), P(&p, s, s, 0), P(&p, -s, s, 0), 0x52c41a, 0x3a8a14);
  done();
}
void icons_buttons(int x, int y, int w, int h) { // the face buttons, Duke-style: Y X B A
  scene(x, y, w, h, 13, 0.95f);
  struct { float x, y; uint32_t c; const char *l; } fb[4] = { { -0.2f, 0.8f, 0xe8d020, "Y" }, { -0.8f, 0.1f, 0x2050e0, "X" },
                                                            { 0.75f, 0.25f, 0xe02828, "B" }, { 0.15f, -0.6f, 0x52d020, "A" } };
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
void icons_camera(int x, int y, int w, int h) {  // the camera turns with the right stick (and L / R)
  scene(x, y, w, h, 15, 0.5f);
  Xf c = rot(rot(xf(v3f(-1.45f, 0, 0.35f)), 2, 15 + rsX * 30), 1, rsY * 15);   // a film camera, turned by the stick
  box(&c, 0, 0, 0, 0.9f, 0.45f, 0.5f, 0x242424);
  Xf lens = rot(c, 1, -90); lens.t = P(&c, -0.45f, 0, 0);
  cyl(&lens, 0.16f, 0, 0.32f, 0x1a1a1a, 0x5a6a7a);
  for (int k = 0; k < 2; k++) { Xf reel = rot(c, 0, 90); reel.t = P(&c, -0.22f + 0.46f * k, 0, 0.5f); cyl(&reel, 0.27f, -0.07f, 0.07f, 0x303030, 0x8a8e92); }
  Xf a = rot(xf(v3f(0.0f, 0, 0.2f)), 2, 180); wedge(&a, 0.36f, 0.22f, 0.12f, YEL);   // "turns"
  box(&a, -0.25f, 0, 0.06f, 0.3f, 0.16f, 0.12f, YEL);
  Xf b = xf(v3f(1.3f, 0, 0));
  Xf r = rot(b, 0, 12);
  ribbon(&r, 1.0f, -spin * 57 + 30, -spin * 57 + 150, 0.4f, 0.28f, YEL, RED);
  ribbon(&r, 1.0f, -spin * 57 + 210, -spin * 57 + 330, 0.4f, 0.28f, YEL, RED);
  thumbstick(b, rsX, -rsY, INK);
  done();
}

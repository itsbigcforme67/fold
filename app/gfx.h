// FOLD app: a small software renderer into a 640x480 32-bit picture (0xRRGGBB). The same output on PC and the
// original Xbox (the platform layer only shows the picture). Triangles with a depth buffer, flat colour or a
// perspective-correct texture; lines; rectangles; a scalable 3x5 pixel font.
#ifndef GFX_H
#define GFX_H
#include <stdint.h>

#define GW 640
#define GH 480
extern uint32_t gfx_px[GW * GH];

typedef struct { float x, y, z; } Vec3;
typedef struct { float m[16]; } Mat4;      // column-major, like OpenGL

// a texture: w x h of 0xRRGGBB, sampled nearest
typedef struct { int w, h; const uint32_t *px; } Tex;

void gfx_clear(uint32_t c);
void gfx_rect(int x, int y, int w, int h, uint32_t c);
void gfx_frame(int x, int y, int w, int h, uint32_t c);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t c);
void gfx_text(int x, int y, int scale, uint32_t c, const char *s);   // 3x5 font, 4*scale wide per char
int gfx_text_w(int scale, const char *s);
const char *gfx_glyph(int ch);                                         // "010101...": 3 wide, 5 high
void gfx_blit(int x, int y, int scale, const Tex *t);                   // a texture, pixel-scaled

// 3D: set the camera (view-projection) once a frame, then draw
void gfx_camera(Mat4 viewproj);
void gfx_zclear(void);
// a quad p0 p1 p2 p3 (in order round it), textured with uv (0,0) at p0, (1,0) at p1, (1,1) at p2; or flat colour
// if t is NULL. shade multiplies the colour (0..1). Drawn with depth test; depth bias pulls lines / overlays forward.
void gfx_quad(const Vec3 p[4], const Tex *t, uint32_t colour, float shade);
// a triangle shaded smoothly: sh = light at each corner (0..1 darkens, 1..2 shines towards white)
void gfx_tri_lit(const Vec3 p[3], const float sh[3], uint32_t colour);
void gfx_line3(Vec3 a, Vec3 b, uint32_t c, int thick);
int gfx_project(Vec3 p, float *sx, float *sy);                          // 0 if behind the camera

// looks
void gfx_gradient(uint32_t top, uint32_t bottom);                       // background, with a soft vignette
void gfx_fog(uint32_t colour, float start, float end);                  // 3D colours fade towards colour with distance
void gfx_quad_alpha(const Vec3 p[4], uint32_t colour, float alpha);    // see-through flat quad (depth-tested, no write)
void gfx_dim(int x, int y, int w, int h, float k);                      // darken a rectangle (behind panels)
void gfx_panel(int x, int y, int w, int h, const char *title, uint32_t accent);   // a rounded panel with a title bar
void gfx_text5(int x, int y, int scale, uint32_t c, const char *s);    // 5x7 font, 6*scale wide per char
int gfx_text5_w(int scale, const char *s);
int gfx_badge(int x, int y, const char *label);                         // a pad button badge (A green, B red...); width
int gfx_hint(int x, int y, const char *label, const char *what);        // badge + words; returns the width

// shapes for icons
void gfx_disc(int cx, int cy, int r, uint32_t c);
void gfx_ring(int cx, int cy, int r, int w, uint32_t c);
void gfx_ellipse(int cx, int cy, int rx, int ry, float angle, uint32_t c);   // filled, turned by angle (radians)
void gfx_tri2(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);    // a filled 2D triangle
void gfx_thick(int x0, int y0, int x1, int y1, int w, uint32_t c);            // a line w pixels wide
// a brushed-metal panel (set the texture once): rounded corners where mask has them (1 TL, 2 TR, 4 BR, 8 BL)
void gfx_metal_texture(const uint8_t *rgb, int w, int h);
void gfx_metal_panel(int x, int y, int w, int h, int r, int mask);
void gfx_view_offset(int dx, int dy);                                          // move the 3D picture's centre
void gfx_clip(int x, int y, int w, int h);                                     // 3D drawing only inside (w 0 = whole screen)
void gfx_zclear_rect(int x, int y, int w, int h);

Mat4 mat_mul(Mat4 a, Mat4 b);
Mat4 mat_perspective(float fovy, float aspect, float zn, float zf);
Mat4 mat_lookat(Vec3 eye, Vec3 at, Vec3 up);
static inline Vec3 v3f(float x, float y, float z) { Vec3 r = { x, y, z }; return r; }
#endif

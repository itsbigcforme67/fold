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

// an optional GPU for the 3D scene (the Xbox: xbox/gpu.c). Set, gfx_px becomes the 2D layer over the GPU's picture: it
// starts each frame transparent (top byte 0xFF = see-through; anything drawn writes 0x00RRGGBB, opaque; gfx_dim over a
// see-through pixel makes see-through black). Between gfx_scene_begin and gfx_scene_end the scene's triangles and
// lines go to the GPU, already projected and clipped here (clip space), instead of being drawn into gfx_px.
typedef struct { float x, y, z, w, u, v; } GfxVert;              // clip space; u v: 0..1 (mode 3: u = the light)
typedef struct {
  void (*tri)(const GfxVert v[3], const Tex *t, uint32_t colour, float shade, int mode, float alpha);   // mode 0 / 1 see-through / 3 lit
  void (*line)(const GfxVert *a, const GfxVert *b, uint32_t colour, int thick);
  void (*background)(const uint32_t *px);                           // the gradient (GW x GH), under everything
} GfxGpu;
void gfx_set_gpu(const GfxGpu *g);
void gfx_scene_begin(void);
void gfx_scene_end(void);
void gfx_fog_get(uint32_t *colour, float *start, float *end);
void gfx_view_offset_get(int *dx, int *dy);
uint32_t gfx_shade_lit(uint32_t c, float k);                      // the lit colour for a light k (as drawn here)

// the hologram look (VMX World's house style; FOLD BUILDER follows it): see gfx.c
#define GFX_HG 0x5dff7e                                   // hologram green, its dim and edge shades
#define GFX_HG_DIM 0x1f6a34
#define GFX_HG_EDGE 0x2c9a4a
uint32_t gfx_mix(uint32_t a, uint32_t b, int k);            // k/256 of b over a
void gfx_add_rect(int x, int y, int w, int h, uint32_t c);  // additive light
void gfx_glass(int x, int y, int w, int h);                  // dark green see-through glass
void gfx_brackets(int x, int y, int w, int h, uint32_t c, int len);   // bright corner brackets
void gfx_gtext(int x, int y, int scale, uint32_t c, const char *s);   // 5x7 text with a glow
void gfx_hpanel(int x, int y, int w, int h, const char *title, uint32_t accent);   // a glass panel, optional title
void gfx_hsel(int x, int y, int w, int h, int focused);     // a lit row
void gfx_holo_filter(int x, int y, int w, int h);            // turn what's drawn there into green light
void gfx_post(int t);                                        // scanlines + rolling band over the screen
void gfx_decal(const uint8_t *mask, int mw, int mh, const float quad[4][2], uint32_t c);
void gfx_tri_lit_decal(const Vec3 p[3], const float sh[3], const float uv[3][2], uint32_t colour,
                       const uint8_t *mask, int mw, int mh, uint32_t ink);   // gfx_tri_lit with a mask painted on (3D letters)   // a mask laid onto a screen quad
void gfx_plate_begin(int x, int y, int w, int h);           // a control window turned into the screen: draw on it,
void gfx_plate_end(int x, int y, int w, int h, int side, float turn);   // then this warps it (side -1 left, +1 right)

Mat4 mat_mul(Mat4 a, Mat4 b);
Mat4 mat_perspective(float fovy, float aspect, float zn, float zf);
Mat4 mat_lookat(Vec3 eye, Vec3 at, Vec3 up);
static inline Vec3 v3f(float x, float y, float z) { Vec3 r = { x, y, z }; return r; }
#endif

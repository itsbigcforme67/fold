// FOLD on the Xbox's GPU (xbox/gpu.c): the 3D scene on the GPU, the 2D layer (gfx_px) over it.
#ifndef GPU_H
#define GPU_H
#include <stdint.h>
int gpu_init(void);                     // 1 = ready (gfx.c's scene now goes to the GPU)
void gpu_begin(void);                   // each frame, before app_draw
void gpu_end(const uint32_t *px);       // after it: the 2D layer over the scene, then the picture to the TV
#endif

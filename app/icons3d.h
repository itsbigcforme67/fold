// FOLD app: the level view's control pictures as small live 3D models (see icons3d.c).
#ifndef ICONS3D_H
#define ICONS3D_H
#include "app.h"
void icons_update(const Pad *p);               // once a frame: eases the models towards the pad
void icons_stick(int x, int y, int w, int h);  // each draws into its panel's rectangle
void icons_dpad(int x, int y, int w, int h);
void icons_buttons(int x, int y, int w, int h);
void icons_camera(int x, int y, int w, int h);
#endif

// Force-included into the app's C on the Xbox (xbox/Makefile): relative paths ("assets/metal.rgb") open from the
// XBE's folder (D:) and '/' becomes '\'. xbox_fopen is in app/main_xbox.c.
#include <stdio.h>
FILE *xbox_fopen(const char *path, const char *mode);
#define fopen xbox_fopen

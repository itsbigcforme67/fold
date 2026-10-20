// FOLD app: a pad script played back as input, one step per 30 Hz tick: the headless runner's format (wait, hold,
// stick, rstick, zoom, set, button names; other lines are skipped). The Xbox build plays D:\demo.txt this way.
#ifndef PADSCRIPT_H
#define PADSCRIPT_H
#include "app.h"
int padscript_open(const char *path);   // 1 = loaded
int padscript_next(Pad *p);             // the pad for this tick; 0 once the script has ended
#endif

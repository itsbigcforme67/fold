// Will a level play on the VMX? FOLD ROOMS there (vmx-firmware/games/fold, C#) keeps a level in a 128 KB game heap:
// measured, it needs about 62 KB + 1.1 KB a square + 2 bytes a byte of the level's text (the text and the parser's
// line buffer). The same rule is FoldRooms.FitsVmx; FOLD BUILDER warns with it when saving, and VMX World leaves a
// level that fails it off the VMX. Header only, so VMX World can use it without the FOLD core.
#ifndef FOLD_VMXFIT_H
#define FOLD_VMXFIT_H
#include <string.h>
#define FOLD_VMX_BUDGET 64000                  // 1100 * squares + 2 * bytes may come to this much
#define FOLD_VMX_PACK 65536                    // all of a VMX's made levels together (DATA\LEVELS.BIN) at most
// "square" lines in a level text (as FoldLevel.CountSquares)
static inline int fold_count_squares(const char *t, int n) {
  int count = 0, i = 0;
  while (i < n) {
    int j = i; while (j < n && (t[j] == ' ' || t[j] == '\t' || t[j] == '\r')) j++;
    if (j + 6 <= n && !memcmp(t + j, "square", 6)) count++;
    while (i < n && t[i] != '\n') i++;
    i++;
  }
  return count;
}
static inline int fold_fits_vmx(int squares, int bytes) { return 1100 * squares + 2 * bytes <= FOLD_VMX_BUDGET; }
#endif

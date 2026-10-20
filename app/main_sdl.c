// FOLD app in a window (SDL2): game controller or keyboard. Keyboard: WASD = left stick, IJKL = right stick,
// arrows = D-pad, Z/Enter = A, X = B, C = X, V = Y, Q = L, E = R, Esc = START, Tab = BACK, F = right stick click.
#include "app.h"
#include "gfx.h"
#include <SDL.h>

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "levels";
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
  SDL_Window *win = SDL_CreateWindow("FOLD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, GW * 2, GH * 2, SDL_WINDOW_RESIZABLE);
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, 0);
  SDL_RenderSetLogicalSize(ren, GW, GH);
  SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, GW, GH);
  SDL_GameController *pad = NULL;
  app_init(dir);
  Uint32 next = SDL_GetTicks();
  while (!app_quit()) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) return 0;
      if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(e.cdevice.which);
    }
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    Pad p = { 0 };
    p.lx = (float)(k[SDL_SCANCODE_D] - k[SDL_SCANCODE_A]); p.ly = (float)(k[SDL_SCANCODE_S] - k[SDL_SCANCODE_W]);
    p.rx = (float)(k[SDL_SCANCODE_L] - k[SDL_SCANCODE_J]); p.ry = (float)(k[SDL_SCANCODE_K] - k[SDL_SCANCODE_I]);
    if (k[SDL_SCANCODE_UP]) p.held |= PAD_UP; if (k[SDL_SCANCODE_DOWN]) p.held |= PAD_DOWN;
    if (k[SDL_SCANCODE_LEFT]) p.held |= PAD_LEFT; if (k[SDL_SCANCODE_RIGHT]) p.held |= PAD_RIGHT;
    if (k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_RETURN]) p.held |= PAD_A; if (k[SDL_SCANCODE_X]) p.held |= PAD_B;
    if (k[SDL_SCANCODE_C]) p.held |= PAD_X; if (k[SDL_SCANCODE_V]) p.held |= PAD_Y;
    if (k[SDL_SCANCODE_Q]) p.held |= PAD_L; if (k[SDL_SCANCODE_E]) p.held |= PAD_R;
    if (k[SDL_SCANCODE_ESCAPE]) p.held |= PAD_START; if (k[SDL_SCANCODE_TAB]) p.held |= PAD_BACK; if (k[SDL_SCANCODE_F]) p.held |= PAD_RS;
    if (pad) {
      float ax[4]; SDL_GameControllerAxis A[4] = { SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY };
      for (int i = 0; i < 4; i++) { ax[i] = SDL_GameControllerGetAxis(pad, A[i]) / 32767.0f; if (ax[i] > -0.25f && ax[i] < 0.25f) ax[i] = 0; }
      if (ax[0]) p.lx = ax[0]; if (ax[1]) p.ly = ax[1]; if (ax[2]) p.rx = ax[2]; if (ax[3]) p.ry = ax[3];
      static const struct { SDL_GameControllerButton b; unsigned bit; } M[] = {
        { SDL_CONTROLLER_BUTTON_DPAD_UP, PAD_UP }, { SDL_CONTROLLER_BUTTON_DPAD_DOWN, PAD_DOWN }, { SDL_CONTROLLER_BUTTON_DPAD_LEFT, PAD_LEFT },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PAD_RIGHT }, { SDL_CONTROLLER_BUTTON_A, PAD_A }, { SDL_CONTROLLER_BUTTON_B, PAD_B },
        { SDL_CONTROLLER_BUTTON_X, PAD_X }, { SDL_CONTROLLER_BUTTON_Y, PAD_Y }, { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PAD_L },
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PAD_R }, { SDL_CONTROLLER_BUTTON_START, PAD_START }, { SDL_CONTROLLER_BUTTON_BACK, PAD_BACK },
        { SDL_CONTROLLER_BUTTON_RIGHTSTICK, PAD_RS } };
      for (unsigned i = 0; i < sizeof M / sizeof M[0]; i++) if (SDL_GameControllerGetButton(pad, M[i].b)) p.held |= M[i].bit;
      // the Duke's black / white buttons and triggers come through as the shoulders on most drivers
      if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) p.held |= PAD_L;
      if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) p.held |= PAD_R;
    }
    app_frame(&p);
    app_draw();
    SDL_UpdateTexture(tex, NULL, gfx_px, GW * 4);
    SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, NULL, NULL); SDL_RenderPresent(ren);
    next += 33; Uint32 now = SDL_GetTicks(); if (next > now) SDL_Delay(next - now); else next = now;
  }
  return 0;
}

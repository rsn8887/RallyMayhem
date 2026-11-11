// Rally Mayhem - tiny software synthesizer (all sound is generated at runtime)
// by rsn8887
#pragma once
#include <SDL.h>
#include <cstdint>

namespace snd {

struct Engine { float freq = 60, vol = 0, phase = 0, tf = 60, tv = 0, pan = 0.5f, tpan = 0.5f; };   // tf / tv / tpan: target pitch, volume, stereo position

extern Engine eng[5];                    // one engine voice per car
extern SDL_AudioDeviceID dev;
extern float skidTarget, rainTarget;     // volume of the tyre squeal and of the rain / wind noise
extern float skidPan;                    // stereo position of the tyre squeal
float panOf(float x);                    // stereo position (0 left .. 1 right) of a screen x coordinate
extern bool mute;                        // silences game effects (title screen demo)

void init();
void setMusic(bool on);

// sound effects; x is the screen x coordinate the sound comes from (stereo panning), -1 = centre
void beep(bool go);
void crash(float v, float x = -1);
void pickup(float x = -1);
void missile(float x = -1);
void shot(float x = -1);
void explode(float x = -1);
void splash(float x = -1);
void turbo(float x = -1);
void jump(float x = -1);
void land(float x = -1);
void horn(float x = -1);
void bell(float x = -1);
void lapChime();
void finish();
void mineDrop(float x = -1);
void menu();
void join();
void boost(float x = -1);
void rustle(float x = -1);

} // namespace snd

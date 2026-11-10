// Rally Mayhem - tiny software synthesizer (all sound is generated at runtime)
// by rsn8887
#pragma once
#include <SDL.h>
#include <cstdint>

namespace snd {

struct Engine { float freq = 60, vol = 0, phase = 0, tf = 60, tv = 0; };   // tf / tv: target pitch and volume

extern Engine eng[5];                    // one engine voice per car
extern SDL_AudioDeviceID dev;
extern float skidTarget, rainTarget;     // volume of the tyre squeal and of the rain / wind noise
extern bool mute;                        // silences game effects (title screen demo)

void init();
void setMusic(bool on);

// sound effects
void beep(bool go);
void crash(float v);
void pickup();
void missile();
void shot();
void explode();
void splash();
void turbo();
void jump();
void land();
void horn();
void bell();
void lapChime();
void finish();
void mineDrop();
void menu();
void join();
void boost();

} // namespace snd

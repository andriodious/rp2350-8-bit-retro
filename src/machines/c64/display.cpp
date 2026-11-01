#include "board.h"
#include "pico/stdlib.h"
#include "Display.h"
#include "psram_allocator.h"
#include <cstring>
extern "C" void c64_keyboard_poll(uint8_t *,uint8_t *,uint8_t *);
// Two indexed PSRAM buffers allow the VIC and converter to own separate frames.
#if C64_RENDER_OVERLAP
static Display *display_instance;
static uint8_t *spare_pixels;
extern "C" uint8_t *c64_exchange_framebuffer() {
    uint8_t *completed=display_instance->GetFramebuffer();
    display_instance->Update();
    return completed;
}
#endif
Display::Display(C64 *c):the_c64(c) {
    vic_pixels=(uint8_t *)psram_malloc(DISPLAY_X*DISPLAY_Y);
    if(!vic_pixels)panic("C64 framebuffer allocation");
    memset(vic_pixels,0,DISPLAY_X*DISPLAY_Y);
#if C64_RENDER_OVERLAP
    spare_pixels=(uint8_t *)psram_malloc(DISPLAY_X*DISPLAY_Y);
    if(!spare_pixels)panic("C64 second framebuffer allocation");
    memset(spare_pixels,0,DISPLAY_X*DISPLAY_Y);
    display_instance=this;
#endif
}
Display::~Display(){
#if C64_RENDER_OVERLAP
    psram_free(spare_pixels);spare_pixels=nullptr;display_instance=nullptr;
#endif
    psram_free(vic_pixels);
}
uint8_t *Display::BitmapBase(){return vic_pixels;}
int Display::BitmapXMod(){return DISPLAY_X;}
void Display::PollKeyboard(uint8_t *k,uint8_t *r,uint8_t *j){c64_keyboard_poll(k,r,j);}
bool Display::NumLock(){return false;}
void Display::Pause(){}
void Display::Resume(){}
void Display::NewPrefs(const Prefs *){}
void Display::Update(){
#if C64_RENDER_OVERLAP
    // Called only after c64_run_frame returns at raster 0. VIC fetches the new
    // BitmapBase on raster 1, before its next visible line. The previous reader
    // must have finished before this exchange makes its buffer writable again.
    uint8_t *old=vic_pixels;vic_pixels=spare_pixels;spare_pixels=old;
#endif
}
void Display::SetLEDs(int,int,int,int){}
void Display::SetSpeedometer(int){}
void Display::ShowNotification(std::string){}

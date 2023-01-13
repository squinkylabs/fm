// plugin main
#include "Squinky.hpp"
//#include "SqTime.h"
#include "ctrl/SqHelper.h"


// The plugin-wide instance of the Plugin class
Plugin *pluginInstance = nullptr;

/**
 * Here we register the whole plugin, which may have more than one module in it.
 */
void init (::rack::Plugin *p)
{
    pluginInstance = p;
    p->addModel(modelWVCOModule);
}

const NVGcolor SqHelper::COLOR_GREY = nvgRGB(0x80, 0x80, 0x80);
const NVGcolor SqHelper::COLOR_WHITE = nvgRGB(0xff, 0xff, 0xff);
const NVGcolor SqHelper::COLOR_BLACK = nvgRGB(0,0,0);
const NVGcolor SqHelper::COLOR_SQUINKY = nvgRGB(0x30, 0x7d, 0xee);

int soloStateCount = 0;

#ifdef _TIME_DRAWING
#include "SqTime.h"
#ifdef _USE_WINDOWS_PERFTIME
    double SqTime::frequency = 0;
#endif
#endif
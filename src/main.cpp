#include "LiveEffectEngine.h"

int main(int argc, char* argv[]) {
    LiveEffectEngine engine;
    engine.initLV2();
    engine.initPlugins();

    return 0;
}
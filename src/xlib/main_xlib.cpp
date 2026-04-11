// src/xlib/main_xlib.cpp
// Entry point for the Xlib/Xaw frontend of Opiqo.

#include "XlibApp.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    XlibApp app(argc, argv);
    MainWindow win(app);
    app.run();
    return 0;
}

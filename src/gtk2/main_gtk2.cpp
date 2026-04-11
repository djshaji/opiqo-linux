// src/gtk2/main_gtk2.cpp
// GTK2 entry point for Opiqo on Linux.

#include <gtk/gtk.h>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    MainWindow win;

    gtk_widget_show_all(win.window());
    gtk_main();

    return 0;
}

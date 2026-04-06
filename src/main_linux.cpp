// src/main_linux.cpp
// GtkApplication entry point for Opiqo on Linux.

#include <gtk/gtk.h>

#include "linux/MainWindow.h"

// One MainWindow per application lifetime
static MainWindow* g_mainWindow = nullptr;

static void onActivate(GtkApplication* app, gpointer /*user_data*/) {
    // Only one window; raise it if already created
    if (g_mainWindow) {
        gtk_window_present(GTK_WINDOW(g_mainWindow->window()));
        return;
    }
    g_mainWindow = new MainWindow(app);
    gtk_window_present(GTK_WINDOW(g_mainWindow->window()));
}

static void onShutdown(GtkApplication* /*app*/, gpointer /*user_data*/) {
    delete g_mainWindow;
    g_mainWindow = nullptr;
}

int main(int argc, char* argv[]) {
    GtkApplication* app = gtk_application_new("com.djshaji.opiqo",
                                               G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(onActivate), nullptr);
    g_signal_connect(app, "shutdown", G_CALLBACK(onShutdown), nullptr);

    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}

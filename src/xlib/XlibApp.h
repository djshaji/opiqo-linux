// src/xlib/XlibApp.h
// Application wrapper for the Xlib/Xaw frontend.
// Owns the Xt application context, display, top-level shell, and the
// self-pipe that allows the JACK RT thread to wake the Xt event loop.

#pragma once

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

struct AppColors {
    Pixel red    = 0;
    Pixel blue   = 0;
    Pixel grey   = 0;
    Pixel white  = 0;
    Pixel black  = 0;
    Pixel bgDark = 0;  // dark background for main window
    Pixel fgText = 0;  // light foreground text
};

class XlibApp {
public:
    explicit XlibApp(int& argc, char** argv);
    ~XlibApp();

    // Non-copyable / non-movable
    XlibApp(const XlibApp&)            = delete;
    XlibApp& operator=(const XlibApp&) = delete;

    // Accessors
    XtAppContext appContext() const { return appCtx_; }
    Widget       topLevel()   const { return topLevel_; }
    Display*     display()    const { return XtDisplay(topLevel_); }
    const AppColors& colors() const { return colors_; }

    // Run the Xt main event loop.  Returns when XtAppSetExitFlag is called.
    void run();

    // Request a clean quit (sets Xt exit flag).
    void quit();

    // Register a one-shot timer.  Must re-register inside the callback.
    XtIntervalId addTimer(unsigned long ms,
                          XtTimerCallbackProc proc, XtPointer data);
    void         removeTimer(XtIntervalId id);

    // Cross-thread marshal: post a callable to execute on the Xt main thread.
    // Safe to call from any thread (including JACK RT thread).
    void postToMain(std::function<void()> fn);

    // Self-pipe fds (for hot-plug and other out-of-band wakeups).
    int selfPipeReadFd()  const { return pipeFd_[0]; }
    int selfPipeWriteFd() const { return pipeFd_[1]; }

private:
    XtAppContext appCtx_  = nullptr;
    Widget       topLevel_ = nullptr;
    AppColors    colors_;

    int          pipeFd_[2] = {-1, -1};
    XtInputId    pipeInputId_ = 0;

    std::mutex                     marshalMutex_;
    std::vector<std::function<void()>> marshalQueue_;

    void initColors();
    void initSelfPipe();

    // Xt input-source callback — drains the self-pipe and runs marshalQueue_
    static void pipeReady(XtPointer client, int* fd, XtInputId* id);
};

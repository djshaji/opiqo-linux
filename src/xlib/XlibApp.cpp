// src/xlib/XlibApp.cpp
// Application wrapper implementation.

#include "XlibApp.h"

#include <X11/Xlib.h>
#include <X11/IntrinsicP.h>
#include <X11/Shell.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>

// ── Construction / destruction ────────────────────────────────────────────────

XlibApp::XlibApp(int& argc, char** argv) {
    // Fall-back default resources embedded in the binary
    static const char* kFallbackResources[] = {
        "Opiqo*font:         -*-helvetica-medium-r-normal-*-12-*-*-*-*-*-iso8859-1",
        "Opiqo*background:   #2b2b2b",
        "Opiqo*foreground:   #e0e0e0",
        "Opiqo*Command.background: #3c3c3c",
        "Opiqo*Toggle.background:  #3c3c3c",
        "Opiqo*Label.background:   #2b2b2b",
        "Opiqo*borderColor:  #555555",
        nullptr
    };

    topLevel_ = XtVaAppInitialize(&appCtx_,
        "Opiqo",
        nullptr, 0,
        &argc, argv,
        const_cast<String*>(kFallbackResources),
        XtNwidth,  960,
        XtNheight, 680,
        XtNtitle,  "Opiqo",
        nullptr);

    initColors();
    initSelfPipe();
}

XlibApp::~XlibApp() {
    if (pipeInputId_) XtRemoveInput(pipeInputId_);
    if (pipeFd_[0] >= 0) close(pipeFd_[0]);
    if (pipeFd_[1] >= 0) close(pipeFd_[1]);
}

// ── Colour allocation ─────────────────────────────────────────────────────────

void XlibApp::initColors() {
    Display*  dpy  = XtDisplay(topLevel_);
    int       scr  = DefaultScreen(dpy);
    Colormap  cmap = DefaultColormap(dpy, scr);

    auto alloc = [&](const char* name) -> Pixel {
        XColor exact, screen;
        if (XAllocNamedColor(dpy, cmap, name, &screen, &exact))
            return screen.pixel;
        // graceful fallback to black
        return BlackPixel(dpy, scr);
    };

    colors_.red    = alloc("#cc3333");
    colors_.blue   = alloc("#1a6ebd");
    colors_.grey   = alloc("#888888");
    colors_.white  = WhitePixel(dpy, scr);
    colors_.black  = BlackPixel(dpy, scr);
    colors_.bgDark = alloc("#2b2b2b");
    colors_.fgText = alloc("#e0e0e0");
}

// ── Self-pipe (cross-thread wakeup) ───────────────────────────────────────────

void XlibApp::initSelfPipe() {
    if (pipe(pipeFd_) != 0) {
        perror("XlibApp: pipe()");
        return;
    }
    // Make write-end non-blocking so JACK RT thread never blocks
    fcntl(pipeFd_[1], F_SETFL, fcntl(pipeFd_[1], F_GETFL) | O_NONBLOCK);

    pipeInputId_ = XtAppAddInput(appCtx_,
        pipeFd_[0],
        reinterpret_cast<XtPointer>(static_cast<intptr_t>(XtInputReadMask)),
        pipeReady, reinterpret_cast<XtPointer>(this));
}

/*static*/
void XlibApp::pipeReady(XtPointer client, int* /*fd*/, XtInputId* /*id*/) {
    XlibApp* self = reinterpret_cast<XlibApp*>(client);

    // Drain the pipe
    char buf[64];
    while (read(self->pipeFd_[0], buf, sizeof(buf)) > 0) {}

    // Run queued callables on the main thread
    std::vector<std::function<void()>> pending;
    {
        std::lock_guard<std::mutex> lk(self->marshalMutex_);
        pending.swap(self->marshalQueue_);
    }
    for (auto& fn : pending) fn();
}

// ── Public API ────────────────────────────────────────────────────────────────

void XlibApp::run() {
    XtRealizeWidget(topLevel_);
    XtAppMainLoop(appCtx_);
}

void XlibApp::quit() {
    XtAppSetExitFlag(appCtx_);
}

XtIntervalId XlibApp::addTimer(unsigned long ms,
                                XtTimerCallbackProc proc, XtPointer data) {
    return XtAppAddTimeOut(appCtx_, ms, proc, data);
}

void XlibApp::removeTimer(XtIntervalId id) {
    XtRemoveTimeOut(id);
}

void XlibApp::postToMain(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lk(marshalMutex_);
        marshalQueue_.push_back(std::move(fn));
    }
    // Wake the Xt event loop
    char byte = 1;
    write(pipeFd_[1], &byte, 1);
}

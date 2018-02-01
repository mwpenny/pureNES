#ifndef EMULATION_THREAD_H
#define EMULATION_THREAD_H

#include <string>

#include <wx/wx.h>

extern "C" {
    #include "../core/nes.h"
}

#include "Canvas.h"

class EmuFrame;

class EmulationThread : public wxThread {
    public:
        EmulationThread(EmuFrame* parentFrame, Canvas* renderCanvas, std::string romPath);
        virtual wxThread::ExitCode Entry();
        void updateController(int wxKey, bool pressed);
        bool isRunning();
        void terminate();
    private:
        NES nes;
        wxMutex emuMutex;
        bool running, stoppingEmulation;

        static ControllerButton resolveNESButton(int wxKey);
}; 

#endif

#ifndef EMUFRAME_H
#define EMUFRAME_H

#include <string>

#include "Canvas.h"

extern "C" {
    #include "../core/nes.h"
}

#include <wx/wx.h>

class EmulationThread : public wxThread {
    public:
        EmulationThread(Canvas* renderCanvas, std::string romPath);
        virtual wxThread::ExitCode Entry();
        void updateController(int wxKey, bool pressed);
        bool isRunning();
        void terminate();
    private:
        NES nes;
        wxMutex mutex;
        bool running, stoppingEmulation;

        static ControllerButton resolveNESButton(int wxKey);
};

class EmuFrame : public wxFrame {
	public:
		EmuFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	private:
        Canvas* canvas;
        EmulationThread* emuThread;
        void startEmulation(std::string romPath);
        void stopEmulation();
        void exit();

        void onKeyDown(wxKeyEvent& evt);
        void onKeyUp(wxKeyEvent& evt);
        void onFileOpen(wxCommandEvent& evt);
        void onDropFiles(wxDropFilesEvent& evt);
		void onExit(wxCommandEvent& evt);
        void onClose(wxCloseEvent& evt);
		wxDECLARE_EVENT_TABLE();
}; 

#endif
#ifndef EMUFRAME_H
#define EMUFRAME_H

#include "Canvas.h"

extern "C" {
    #include "../core/nes.h"
}

#include <wx/wx.h>

class EmulationThread : public wxThread {
    public:
        EmulationThread(Canvas* renderCanvas);
        virtual wxThread::ExitCode Entry();
        void updateController(int wxKey, bool pressed);
    private:
        NES nes;
        Canvas* renderCanvas;
        wxMutex mutex;

        static ControllerButton resolveNESButton(int wxKey);
};

class EmuFrame : public wxFrame {
	public:
		EmuFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	private:
        EmulationThread* emuThread;
        void onKeyDown(wxKeyEvent& evt);
        void onKeyUp(wxKeyEvent& evt);
		void onExit(wxCommandEvent& evt);
		wxDECLARE_EVENT_TABLE();
}; 

#endif
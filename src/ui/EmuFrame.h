#ifndef EMUFRAME_H
#define EMUFRAME_H

#include <cstdint>
#include <string>

#include "Canvas.h"
#include "EmulationThread.h"

#include <wx/wx.h>

class EmuFrame : public wxFrame {
	public:
        EmuFrame(const wxString& title, const wxPoint& pos, const wxSize& size, std::string romPath);
        virtual ~EmuFrame();
        void setAudioBuf(uint16_t* buf, uint32_t bufSize);
        void outputAudio(uint16_t* stream, int len);
	private:
        Canvas* canvas;
        EmulationThread* emuThread;
        uint16_t* bufferedAudio;
        uint32_t audioBufSize;
        uint32_t audioBufPos;
        wxMutex audioMutex;

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
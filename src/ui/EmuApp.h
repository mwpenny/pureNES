#ifndef EMU_APP_H
#define EMU_APP_H

#include <string>

#include <wx/wx.h>

class EmuApp : public wxApp {
	public:
		virtual bool OnInit();
	private:
		std::string romPath;
		virtual void OnInitCmdLine(wxCmdLineParser& parser);
		virtual bool OnCmdLineParsed(wxCmdLineParser& parser);
		virtual int OnExit();
};

#endif
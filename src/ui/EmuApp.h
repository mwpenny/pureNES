#ifndef EMU_APP_H
#define EMU_APP_H

#include <wx/wx.h>

class EmuApp : public wxApp {
	public:
		virtual bool OnInit();
	private:
		virtual int OnExit();
};

#endif
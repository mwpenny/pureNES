#include "EmuApp.h"
#include "EmuFrame.h"

bool EmuApp::OnInit()
{
	EmuFrame* frame = new EmuFrame("pNES", wxPoint(50, 50), wxSize(256*4, 240*4));
    frame->Show();
    return true;
}

int EmuApp::OnExit()
{
	// Cleanup code goes here
	return 0;
}

wxIMPLEMENT_APP(EmuApp);
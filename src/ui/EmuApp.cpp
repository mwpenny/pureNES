#include <wx/cmdline.h>

#include "EmuApp.h"
#include "EmuFrame.h"

static const wxCmdLineEntryDesc cmdLineUsage[] =
{
	{ wxCMD_LINE_OPTION, "p", "path", "path to an NES ROM file to load on startup",
	  wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, "h", "help", "displays this usage information",
	  wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_NONE }
};

bool EmuApp::OnInit()
{
	if (!wxApp::OnInit())
		return false;
	EmuFrame* frame = new EmuFrame("pNES", wxPoint(50, 50), wxSize(256*4, 240*4), romPath);
    frame->Show();
    return true;
}

int EmuApp::OnExit()
{
	// Cleanup code goes here
	return 0;
}

void EmuApp::OnInitCmdLine(wxCmdLineParser& parser)
{
	parser.SetDesc(cmdLineUsage);
	parser.SetSwitchChars("-");
}

bool EmuApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
	wxString romPath;
	if (parser.Found("path", &romPath))
		this->romPath = romPath.c_str();
	return true;
}

int main(int argc, char** argv)
{
	wxEntry(argc, argv);
}

wxIMPLEMENT_APP_NO_MAIN(EmuApp);

#include "EmuFrame.h"

static void frameUpdateCallback(uint32_t* frame, void* userdata)
{
    static_cast<Canvas*>(userdata)->updateFrame(frame);
}

EmulationThread::EmulationThread(Canvas* renderCanvas)
{
    this->renderCanvas = renderCanvas;
}

wxThread::ExitCode EmulationThread::Entry()
{
    // TODO: error checking and clean exit
    NESInitInfo init_info = {frameUpdateCallback, renderCanvas};
    nes_init(&nes, &init_info);
    nes_load_rom(&nes, "mario.nes");
    wxLongLong startMS = wxGetUTCTimeMillis();
    long long cyclesPerMS = CPU_CLOCK_RATE / 1000;
    long long cyclesEmulated = 0;

    while (true)
    {
        // TODO: better frame limiting
        wxLongLong cyclesNeeded = (wxGetUTCTimeMillis() - startMS) * cyclesPerMS;
        mutex.Lock();
        while (cyclesEmulated < cyclesNeeded)
            cyclesEmulated += nes_update(&nes);
        mutex.Unlock();
    }
    return NULL;
}

ControllerButton EmulationThread::resolveNESButton(int wxKey)
{
    // TODO: make this user configurable
    switch (wxKey)
    {
        case WXK_UP:
            return CONTROLLER_UP;
        case WXK_DOWN:
            return CONTROLLER_DOWN;
        case WXK_LEFT:
            return CONTROLLER_LEFT;
        case WXK_RIGHT:
            return CONTROLLER_RIGHT;
        case 'Z':
            return CONTROLLER_B;
        case 'X':
            return CONTROLLER_A;
        case WXK_SHIFT:
            return CONTROLLER_SELECT;
        case WXK_RETURN:
            return CONTROLLER_START;
        default:
            return CONTROLLER_NONE;
    }
}

void EmulationThread::updateController(int wxKey, bool pressed)
{
    // TODO: multiple controllers
    ControllerButton btn = resolveNESButton(wxKey);
    if (btn != CONTROLLER_NONE)
    {
        mutex.Lock();
        controller_set_button(&nes.c1, btn, pressed);
        mutex.Unlock();
    }
}


EmuFrame::EmuFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size)
{
	wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_EXIT, "E&xit");

    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);

    Canvas* canvas = new Canvas(this, 256, 240, 4);
    emuThread = new EmulationThread(canvas);

    // TODO: error checking
    emuThread->Create();
    emuThread->Run();
}

void EmuFrame::onKeyDown(wxKeyEvent& evt)
{
    emuThread->updateController(evt.GetKeyCode(), true);
    evt.Skip();
}

void EmuFrame::onKeyUp(wxKeyEvent& evt)
{
    emuThread->updateController(evt.GetKeyCode(), false);
    evt.Skip();
}

void EmuFrame::onExit(wxCommandEvent& evt)
{
	Destroy();
} 

wxBEGIN_EVENT_TABLE(EmuFrame, wxFrame)
    EVT_MENU(wxID_EXIT, EmuFrame::onExit)
    //EVT_MENU(wxID_ABOUT, EmuFrame::OnAbout)
    EVT_KEY_DOWN(EmuFrame::onKeyDown)
    EVT_KEY_UP(EmuFrame::onKeyUp)
wxEND_EVENT_TABLE()
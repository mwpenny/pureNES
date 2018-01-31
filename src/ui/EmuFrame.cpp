#include "EmuFrame.h"

static void frameUpdateCallback(uint32_t* frame, void* userdata)
{
    static_cast<Canvas*>(userdata)->updateFrame(frame);
}

EmulationThread::EmulationThread(Canvas* renderCanvas, std::string romPath)
    : wxThread(wxTHREAD_JOINABLE)
{
    // TODO: error checking
    NESInitInfo init_info = {frameUpdateCallback, renderCanvas};
    nes_init(&nes, &init_info);
    nes_load_rom(&nes, const_cast<char*>(romPath.c_str()));
    this->stoppingEmulation = false;
}

wxThread::ExitCode EmulationThread::Entry()
{
    // TODO: error checking
    wxLongLong startMS = wxGetUTCTimeMillis();
    long long cyclesPerMS = CPU_CLOCK_RATE / 1000;
    long long cyclesEmulated = 0;
    running = true;

    while (running)
    {
        // TODO: better frame limiting
        wxLongLong cyclesNeeded = (wxGetUTCTimeMillis() - startMS) * cyclesPerMS;
        mutex.Lock();
        while (cyclesEmulated < cyclesNeeded)
            cyclesEmulated += nes_update(&nes);
        running = !stoppingEmulation;
        mutex.Unlock();
    }
    nes_unload_rom(&nes);
    nes_cleanup(&nes);
    running = false;
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

bool EmulationThread::isRunning()
{
    bool running;
    mutex.Lock();
    running = this->running;
    mutex.Unlock();
    return running;
}

void EmulationThread::terminate()
{
    mutex.Lock();
    stoppingEmulation = true;
    mutex.Unlock();
    Wait(wxTHREAD_WAIT_BLOCK);
}


EmuFrame::EmuFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size)
{
	wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_OPEN, "&Open");
    menuFile->Append(wxID_EXIT, "E&xit");

    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);

    DragAcceptFiles(true);

    canvas = new Canvas(this, 256, 240, 4);
    emuThread = NULL;
}

void EmuFrame::startEmulation(std::string romPath)
{
    // TODO: error checking (file actually NES ROM)
    stopEmulation();
    emuThread = new EmulationThread(canvas, romPath);
    emuThread->Run();
}

void EmuFrame::stopEmulation()
{
    if (emuThread)
    {
        if (emuThread->isRunning())
            emuThread->terminate();
        delete emuThread;
        emuThread = NULL;
    }
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

void EmuFrame::onFileOpen(wxCommandEvent& evt)
{
    long style = wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR;
    std::string wildcard = "NES ROM files (*.nes)|*.nes";
    wxFileDialog diag(this, "Select NES ROM file", "", "", wildcard, style);
    if (diag.ShowModal() == wxID_OK)
        startEmulation(diag.GetPath().ToStdString());
}

void EmuFrame::onDropFiles(wxDropFilesEvent& evt)
{
    std::string path = evt.GetFiles()[0].ToStdString();
    startEmulation(path);
}

void EmuFrame::exit()
{
    stopEmulation();
    Destroy();
}

void EmuFrame::onExit(wxCommandEvent& evt)
{
    exit();
}

void EmuFrame::onClose(wxCloseEvent& evt)
{
    exit();
}

wxBEGIN_EVENT_TABLE(EmuFrame, wxFrame)
    EVT_KEY_DOWN(EmuFrame::onKeyDown)
    EVT_KEY_UP(EmuFrame::onKeyUp)
    EVT_MENU(wxID_OPEN, EmuFrame::onFileOpen)
    EVT_DROP_FILES(EmuFrame::onDropFiles)
    EVT_MENU(wxID_EXIT, EmuFrame::onExit)
    EVT_CLOSE(EmuFrame::onClose)
    //EVT_MENU(wxID_ABOUT, EmuFrame::OnAbout)
wxEND_EVENT_TABLE()
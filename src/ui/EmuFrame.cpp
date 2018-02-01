extern "C" {
    #include <SDL.h>
}

#include "EmuFrame.h"

static void sdlAudioCallback(void* userdata, uint8_t* stream, int len)
{
    static_cast<EmuFrame*>(userdata)->outputAudio((uint16_t*)stream, len/2);
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

    // TODO: adjustable in GUI
    SDL_AudioSpec desired, obtained;
    desired.freq = 44100;
    desired.format = AUDIO_U16SYS;
    desired.channels = 1;  // TODO: stereo experimentation
    desired.samples = 2048;
    desired.callback = sdlAudioCallback;
    desired.userdata = this;
    bufferedAudio = NULL;

    // TODO: error checking
    SDL_Init(SDL_INIT_AUDIO);
    SDL_OpenAudio(&desired, &obtained);
    SDL_PauseAudio(0);
}

EmuFrame::~EmuFrame()
{
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SDL_Quit();
}

void EmuFrame::startEmulation(std::string romPath)
{
    // TODO: error checking (file actually NES ROM)
    stopEmulation();
    emuThread = new EmulationThread(this, canvas, romPath);
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

void EmuFrame::exit()
{
    stopEmulation();
    Destroy();
}

void EmuFrame::setAudioBuf(uint16_t* buf, uint32_t bufSize)
{
    audioMutex.Lock();
    bufferedAudio = buf;
    audioBufSize = bufSize;
    audioBufPos = 0;
    audioMutex.Unlock();
}

void EmuFrame::outputAudio(uint16_t* stream, int len)
{
    audioMutex.Lock();
    if (bufferedAudio)
    {
        for (int i = 0; i < len; ++i)
        {
            stream[i] = bufferedAudio[audioBufPos];
            audioBufPos = (audioBufPos + 1) % audioBufSize;
        }
    }
    else
    {
        memset(stream, 0, len * 2);
    }
    audioMutex.Unlock();
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
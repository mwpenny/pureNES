#include "EmuFrame.h"
#include "EmulationThread.h"

static void frameUpdateCallback(uint32_t* frame, void* userdata)
{
    static_cast<Canvas*>(userdata)->updateFrame(frame);
}

static void emuAudioCallback(uint16_t* readBuf, uint32_t bufSize, void* userdata)
{
    static_cast<EmuFrame*>(userdata)->setAudioBuf(readBuf, bufSize);
}

EmulationThread::EmulationThread(EmuFrame* parentFrame, Canvas* renderCanvas, std::string romPath)
    : wxThread(wxTHREAD_JOINABLE)
{
    // TODO: error checking
    NESInitInfo init_info;
    init_info.render_cb = frameUpdateCallback;
    init_info.render_userdata = renderCanvas;
    init_info.snd_cb = emuAudioCallback;
    init_info.snd_userdata = parentFrame;
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
        emuMutex.Lock();
        while (cyclesEmulated < cyclesNeeded)
            cyclesEmulated += nes_update(&nes);
		//wxMilliSleep(1); // TODO: experiment
        running = !stoppingEmulation;
        emuMutex.Unlock();
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
        emuMutex.Lock();
        controller_set_button(&nes.c1, btn, pressed);
        emuMutex.Unlock();
    }
}

bool EmulationThread::isRunning()
{
    bool running;
    emuMutex.Lock();
    running = this->running;
    emuMutex.Unlock();
    return running;
}

void EmulationThread::terminate()
{
    emuMutex.Lock();
    stoppingEmulation = true;
    emuMutex.Unlock();
    Wait(wxTHREAD_WAIT_BLOCK);
} 

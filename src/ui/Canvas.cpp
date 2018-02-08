#include "Canvas.h"

#ifdef __WXMSW__
	#include <GL/gl.h>
	#include <GL/glext.h>
#elif __WXOSX__
	#include <OpenGL/gl.h>
#else
	#include <GL/gl.h>
#endif

Canvas::Canvas(wxFrame* parent, uint16_t frameWidth, uint16_t frameHeight, uint8_t bpp)
	: wxGLCanvas(parent, wxID_ANY, NULL)
{
	glInitialized = false;
	glCtx = new wxGLContext(this);
    framebuffer = new uint32_t[frameWidth * frameHeight * bpp];
    this->frameWidth = frameWidth;
    this->frameHeight = frameHeight;
    this->bpp = bpp;
} 

Canvas::~Canvas()
{
    delete glCtx;
    delete[] framebuffer;
    glInitialized = false;
}

void Canvas::updateFrame(uint32_t* frame)
{
    mutex.Lock();
    memcpy(framebuffer, frame, frameWidth * frameHeight * bpp);
    mutex.Unlock();
    Refresh();
}

void Canvas::initGl()
{
    GLuint tid;
	SetCurrent(*glCtx); // TODO: error checking for this

    // Initialize the texture that will be used to display the frame buffer
	glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &tid);
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, -1, 1);

	glInitialized = true;
}

void Canvas::onSize(wxSizeEvent& evt)
{ 
    wxSize size = GetSize();
    glViewport(0, 0, size.x, size.y);
    Refresh();
}

void Canvas::onPaint(wxPaintEvent& evt)
{
	if (!IsShown())
		return;
	else if (!glInitialized)
		initGl();

	wxPaintDC(this);

    mutex.Lock();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight,
                 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, framebuffer);
    mutex.Unlock();
 
    glBegin(GL_QUADS);
        glTexCoord2i(0, 0); glVertex2i(0, 0);
        glTexCoord2i(1, 0); glVertex2i(1, 0);
        glTexCoord2i(1, 1); glVertex2i(1, 1);
        glTexCoord2i(0, 1); glVertex2i(0, 1);
    glEnd();

    //glFlush();
    SwapBuffers();
}

void Canvas::onKeyEvent(wxKeyEvent& evt)
{
    // We want key events to be handled in the parent window
    evt.ResumePropagation(1);
    evt.Skip();
}

BEGIN_EVENT_TABLE(Canvas, wxGLCanvas)
	EVT_SIZE(Canvas::onSize)
	EVT_PAINT(Canvas::onPaint)
    EVT_KEY_DOWN(Canvas::onKeyEvent)
    EVT_KEY_UP(Canvas::onKeyEvent)
END_EVENT_TABLE()

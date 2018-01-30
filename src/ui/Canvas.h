#ifndef CANVAS_H
#define CANVAS_H

#include <cstdint>

#include <wx/wx.h>
#include <wx/glcanvas.h>

class Canvas : public wxGLCanvas {
	public:
		Canvas(wxFrame* parent, uint16_t frameWidth, uint16_t frameHeight, uint8_t bpp);
		virtual ~Canvas();

		void updateFrame(uint32_t* frame);

		DECLARE_EVENT_TABLE();
	private:
		wxGLContext* glCtx;
		bool glInitialized;
		uint32_t* framebuffer;
		uint16_t frameWidth, frameHeight;
		uint8_t bpp;

		wxMutex mutex;

		void initGl();
		void onSize(wxSizeEvent& evt);
		void onPaint(wxPaintEvent& evt);
		void onKeyEvent(wxKeyEvent& evt);
};

#endif
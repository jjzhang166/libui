// 27 april 2015
#include "uipriv_windows.h"

struct window {
	uiWindow w;
	HWND hwnd;
	uiContainer *bin;
	int hidden;
	BOOL shownOnce;
	int (*onClosing)(uiWindow *, void *);
	void *onClosingData;
	int margined;
};

// TODO window class and init functions

static void windowDestroy(uiControl *c)
{
	struct window *w = (struct window *) c;

	// first hide ourselves
	ShowWindow(w->hwnd, SW_HIDE);
	// now destroy the bin
	// the bin has no parent, so we can just call uiControlDestroy()
	uiControlDestroy(uiControl(w->bin));
	// TODO menus
	// now destroy ourselves
	if (DestroyWindow(w->hwnd) == 0)
		logLastError("error destroying uiWindow in windowDestroy()");
	uiFree(w);
}

static uintptr_t windowHandle(uiControl *c)
{
	struct window *w = (struct window *) c;

	return (uintptr_t) (w->hwnd);
}

static void windowSetParent(uiControl *c)
{
	complain("attempt to give the uiWindow at %p a parent", c);
}

static void windowPreferredSize(uiControl *c, uiSizing *d, intmax_t *width, intmax_t *height)
{
	complain("attempt to get the preferred size of the uiWindow at %p", c);
}

static void windowResize(uiControl *c, intmax_t x, intmax_t y, intmax_t width, intmax_t height, uiSizing *d)
{
	complain("attempt to resize the uiWindow at %p", c);
}

static int windowVisible(uiControl *c)
{
	struct window *w = (struct window *) c;

	return !w->hidden;
}

static void windowShow(uiControl *w)
{
	struct window *w = (struct window *) c;

	if (w->shownOnce) {
		ShowWindow(w->hwnd, SW_SHOW);
		w->hidden = 0;
		return;
	}
	w->shownOnce = TRUE;
	ShowWindow(w->hwnd, nCmdShow);
	if (UpdateWindow(w->hwnd) == 0)
		logLastError("error calling UpdateWindow() after showing uiWindow for the first time in windowShow()");
	w->hidden = 0;
}

static void windowHide(uiControl *w)
{
	struct window *w = (struct window *) c;

	ShowWindow(w->hwnd, SW_HIDE);
	w->hidden = 1;
}

static void windowEnable(uiControl *c)
{
	struct window *w = (struct window *) c;

	EnableWindow(w->hwnd, TRUE);
}

static void windowDisable(uiControl *c)
{
	struct window *w = (struct window *) c;

	EnableWindow(w->hwnd, FALSE);
}

static char *windowTitle(uiWindow *ww)
{
	struct window *w = (struct window *) ww;
	WCHAR *wtext;
	char *text;

	wtext = windowText(w->hwnd);
	text = toUTF8(wtext);
	uiFree(wtext);
	return text;
}

static void windowSetTitle(uiWindow *ww, const char *title)
{
	struct window *w = (struct window *) ww;
	WCHAR *wtext;

	wtext = toUTF16(text);
	if (SetWindowTextW(w->hwnd, wtext) == 0)
		logLastError("error setting window title in uiWindowSetTitle()");
	uiFree(wtext);
}

static void windowOnClosing(uiWindow *ww, int (*f)(uiWindow *, void *), void *data)
{
	struct window *w = (struct window *) ww;

	w->onClosing = f;
	w->onClosingData = data;
}

static void windowSetChild(uiWindow *ww, uiControl *child)
{
	struct window *w = (struct window *) ww;

	binSetMainControl(w->bin, child);
}

static int windowMargined(uiWindow *ww)
{
	struct window *w = (struct window *) ww;

	return w->margined;
}

// from https://msdn.microsoft.com/en-us/library/windows/desktop/dn742486.aspx#sizingandspacing
#define windowMargin 7

static void windowSetMargined(uiWindow *ww, int margined)
{
	struct window *w = (struct window *) ww;

	w->margined = margined;
	if (w->margined)
		binSetMargins(w->bin, windowMargin, windowMargin, windowMargin, windowMargin);
	else
		binSetMargins(w->bin, 0, 0, 0, 0);
}
// 22 april 2015
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_40
#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_40
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_10
#define GDK_VERSION_MAX_ALLOWED GDK_VERSION_3_10
#include <gtk/gtk.h>
#include <math.h>
#include <dlfcn.h>		// see drawtext.c
#include <langinfo.h>
#include <string.h>
#include "../ui.h"
#include "../ui_unix.h"
#include "../common/uipriv.h"

#define gtkXMargin 12
#define gtkYMargin 12
#define gtkXPadding 12
#define gtkYPadding 6

// areaeventhandler.c
struct areaEventHandler {
	clickCounter cc;
	uiAreaEventHandler *eh;
};
typedef struct areaEventHandler areaEventHandler;

void loadAreaSize(GtkWidget *w, double *width, double *height, int providesize);
gboolean areaEventHandler_button_press_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventButton *e, int providesize);
gboolean areaEventHandler_button_release_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventButton *e, int providesize);
gboolean areaEventHandler_motion_notify_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventMotion *e, int providesize);
gboolean areaEventHandler_enter_notify_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventCrossing *e);
gboolean areaEventHandler_leave_notify_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventCrossing *e);
gboolean areaEventHandler_key_press_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventKey *e);
gboolean areaEventHandler_key_release_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventKey *e);

// menu.c
extern GtkWidget *makeMenubar(uiWindow *);
extern void freeMenubar(GtkWidget *);
extern void uninitMenus(void);

// alloc.c
extern void initAlloc(void);
extern void uninitAlloc(void);

// util.c
extern void setMargined(GtkContainer *, int);

// child.c
extern struct child *newChild(uiControl *child, uiControl *parent, GtkContainer *parentContainer);
extern struct child *newChildWithBox(uiControl *child, uiControl *parent, GtkContainer *parentContainer, int margined);
extern void childRemove(struct child *c);
extern void childDestroy(struct child *c);
extern GtkWidget *childWidget(struct child *c);
extern int childFlag(struct child *c);
extern void childSetFlag(struct child *c, int flag);
extern GtkWidget *childBox(struct child *c);
extern void childSetMargined(struct child *c, int margined);

// draw.c
extern uiDrawContext *newContext(cairo_t *);
extern void freeContext(uiDrawContext *);

// drawtext.c
extern uiDrawTextFont *mkTextFont(PangoFont *f, gboolean add);
extern PangoFont *pangoDescToPangoFont(PangoFontDescription *pdesc);

// graphemes.c
extern ptrdiff_t *graphemes(const char *text, PangoContext *context);

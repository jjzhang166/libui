// 4 september 2015
#include "uipriv_unix.h"

// notes:
// - G_DECLARE_DERIVABLE/FINAL_INTERFACE() requires glib 2.44 and that's starting with debian stretch (testing) (GTK+ 3.18) and ubuntu 15.04 (GTK+ 3.14) - debian jessie has 2.42 (GTK+ 3.14)
#define areaWidgetType (areaWidget_get_type())
#define areaWidget(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), areaWidgetType, areaWidget))
#define isAreaWidget(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), areaWidgetType))
#define areaWidgetClass(class) (G_TYPE_CHECK_CLASS_CAST((class), areaWidgetType, areaWidgetClass))
#define isAreaWidgetClass(class) (G_TYPE_CHECK_CLASS_TYPE((class), areaWidget))
#define getAreaWidgetClass(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), areaWidgetType, areaWidgetClass))

typedef struct areaWidget areaWidget;
typedef struct areaWidgetClass areaWidgetClass;

struct areaWidget {
	GtkDrawingArea parent_instance;
	uiArea *a;
};

struct areaWidgetClass {
	GtkDrawingAreaClass parent_class;
};

struct uiArea {
	uiUnixControl c;
	GtkWidget *widget;		// either swidget or areaWidget depending on whether it is scrolling

	GtkWidget *swidget;
	GtkContainer *scontainer;
	GtkScrolledWindow *sw;

	GtkWidget *areaWidget;
	GtkDrawingArea *drawingArea;
	areaWidget *area;

	gboolean scrolling;
	intmax_t scrollWidth;
	intmax_t scrollHeight;

	uiAreaHandler *ah;
	areaEventHandler eh;
};

G_DEFINE_TYPE(areaWidget, areaWidget, GTK_TYPE_DRAWING_AREA)

static void areaWidget_init(areaWidget *aw)
{
	// for events
	gtk_widget_add_events(GTK_WIDGET(aw),
		GDK_POINTER_MOTION_MASK |
		GDK_BUTTON_MOTION_MASK |
		GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK |
		GDK_KEY_PRESS_MASK |
		GDK_KEY_RELEASE_MASK |
		GDK_ENTER_NOTIFY_MASK |
		GDK_LEAVE_NOTIFY_MASK);

	gtk_widget_set_can_focus(GTK_WIDGET(aw), TRUE);
}

static void areaWidget_dispose(GObject *obj)
{
	G_OBJECT_CLASS(areaWidget_parent_class)->dispose(obj);
}

static void areaWidget_finalize(GObject *obj)
{
	G_OBJECT_CLASS(areaWidget_parent_class)->finalize(obj);
}

static void areaWidget_size_allocate(GtkWidget *w, GtkAllocation *allocation)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	// GtkDrawingArea has a size_allocate() implementation; we need to call it
	// this will call gtk_widget_set_allocation() for us
	GTK_WIDGET_CLASS(areaWidget_parent_class)->size_allocate(w, allocation);

	if (!a->scrolling)
		// we must redraw everything on resize because Windows requires it
		gtk_widget_queue_resize(w);
}

static gboolean areaWidget_draw(GtkWidget *w, cairo_t *cr)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	uiAreaDrawParams dp;
	double clipX0, clipY0, clipX1, clipY1;

	dp.Context = newContext(cr);

	loadAreaSize(w, &(dp.AreaWidth), &(dp.AreaHeight), !a->scrolling);

	cairo_clip_extents(cr, &clipX0, &clipY0, &clipX1, &clipY1);
	dp.ClipX = clipX0;
	dp.ClipY = clipY0;
	dp.ClipWidth = clipX1 - clipX0;
	dp.ClipHeight = clipY1 - clipY0;

	// no need to save or restore the graphics state to reset transformations; GTK+ does that for us
	(*(a->ah->Draw))(a->ah, a, &dp);

	freeContext(dp.Context);
	return FALSE;
}

// to do this properly for scrolling areas, we need to
// - return the same value for min and nat
// - call gtk_widget_queue_resize() when the size changes
// thanks to Company in irc.gimp.net/#gtk+
static void areaWidget_get_preferred_height(GtkWidget *w, gint *min, gint *nat)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	// always chain up just in case
	GTK_WIDGET_CLASS(areaWidget_parent_class)->get_preferred_height(w, min, nat);
	if (a->scrolling) {
		*min = a->scrollHeight;
		*nat = a->scrollHeight;
	}
}

static void areaWidget_get_preferred_width(GtkWidget *w, gint *min, gint *nat)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;

	// always chain up just in case
	GTK_WIDGET_CLASS(areaWidget_parent_class)->get_preferred_width(w, min, nat);
	if (a->scrolling) {
		*min = a->scrollWidth;
		*nat = a->scrollWidth;
	}
}

static gboolean areaWidget_button_press_event(GtkWidget *w, GdkEventButton *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	return areaEventHandler_button_press_event(&a->eh, uiControl(a), w, e, !a->scrolling);
}

static gboolean areaWidget_button_release_event(GtkWidget *w, GdkEventButton *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	return areaEventHandler_button_release_event(&a->eh, uiControl(a), w, e, !a->scrolling);
}

static gboolean areaWidget_motion_notify_event(GtkWidget *w, GdkEventMotion *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	return areaEventHandler_motion_notify_event(&a->eh, uiControl(a), w, e, !a->scrolling);
}

static gboolean areaWidget_enter_notify_event(GtkWidget *w, GdkEventCrossing *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	return areaEventHandler_enter_notify_event(&a->eh, uiControl(a), w, e);
}

static gboolean areaWidget_leave_notify_event(GtkWidget *w, GdkEventCrossing *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	return areaEventHandler_leave_notify_event(&a->eh, uiControl(a), w, e);
}

static gboolean areaWidget_key_press_event(GtkWidget *w, GdkEventKey *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	return areaEventHandler_key_press_event(&a->eh, uiControl(a), w, e);
}

static gboolean areaWidget_key_release_event(GtkWidget *w, GdkEventKey *e)
{
	areaWidget *aw = areaWidget(w);
	uiArea *a = aw->a;
	return areaEventHandler_key_release_event(&a->eh, uiControl(a), w, e);
}

enum {
	pArea = 1,
	nProps,
};

static GParamSpec *pspecArea;

static void areaWidget_set_property(GObject *obj, guint prop, const GValue *value, GParamSpec *pspec)
{
	areaWidget *aw = areaWidget(obj);

	switch (prop) {
	case pArea:
		aw->a = (uiArea *) g_value_get_pointer(value);
		return;
	}
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
}

static void areaWidget_get_property(GObject *obj, guint prop, GValue *value, GParamSpec *pspec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop, pspec);
}

static void areaWidget_class_init(areaWidgetClass *class)
{
	G_OBJECT_CLASS(class)->dispose = areaWidget_dispose;
	G_OBJECT_CLASS(class)->finalize = areaWidget_finalize;
	G_OBJECT_CLASS(class)->set_property = areaWidget_set_property;
	G_OBJECT_CLASS(class)->get_property = areaWidget_get_property;

	GTK_WIDGET_CLASS(class)->size_allocate = areaWidget_size_allocate;
	GTK_WIDGET_CLASS(class)->draw = areaWidget_draw;
	GTK_WIDGET_CLASS(class)->get_preferred_height = areaWidget_get_preferred_height;
	GTK_WIDGET_CLASS(class)->get_preferred_width = areaWidget_get_preferred_width;
	GTK_WIDGET_CLASS(class)->button_press_event = areaWidget_button_press_event;
	GTK_WIDGET_CLASS(class)->button_release_event = areaWidget_button_release_event;
	GTK_WIDGET_CLASS(class)->motion_notify_event = areaWidget_motion_notify_event;
	GTK_WIDGET_CLASS(class)->enter_notify_event = areaWidget_enter_notify_event;
	GTK_WIDGET_CLASS(class)->leave_notify_event = areaWidget_leave_notify_event;
	GTK_WIDGET_CLASS(class)->key_press_event = areaWidget_key_press_event;
	GTK_WIDGET_CLASS(class)->key_release_event = areaWidget_key_release_event;

	pspecArea = g_param_spec_pointer("libui-area",
		"libui-area",
		"uiArea.",
		G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
	g_object_class_install_property(G_OBJECT_CLASS(class), pArea, pspecArea);
}

// control implementation

uiUnixControlAllDefaults(uiArea)

void uiAreaSetSize(uiArea *a, intmax_t width, intmax_t height)
{
	if (!a->scrolling)
		userbug("You cannot call uiAreaSetSize() on a non-scrolling uiArea. (area: %p)", a);
	a->scrollWidth = width;
	a->scrollHeight = height;
	gtk_widget_queue_resize(a->areaWidget);
}

void uiAreaQueueRedrawAll(uiArea *a)
{
	gtk_widget_queue_draw(a->areaWidget);
}

void uiAreaScrollTo(uiArea *a, double x, double y, double width, double height)
{
	// TODO
	// TODO adjust adjustments and find source for that
}

uiArea *uiNewArea(uiAreaHandler *ah)
{
	uiArea *a;

	uiUnixNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = FALSE;

	a->eh.eh = &ah->eh;
	clickCounterReset(&(a->eh.cc));

	a->areaWidget = GTK_WIDGET(g_object_new(areaWidgetType,
		"libui-area", a,
		NULL));
	a->drawingArea = GTK_DRAWING_AREA(a->areaWidget);
	a->area = areaWidget(a->areaWidget);

	a->widget = a->areaWidget;

	return a;
}

uiArea *uiNewScrollingArea(uiAreaHandler *ah, intmax_t width, intmax_t height)
{
	uiArea *a;

	uiUnixNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = TRUE;
	a->scrollWidth = width;
	a->scrollHeight = height;

	a->eh.eh = &ah->eh;
	clickCounterReset(&(a->eh.cc));

	a->swidget = gtk_scrolled_window_new(NULL, NULL);
	a->scontainer = GTK_CONTAINER(a->swidget);
	a->sw = GTK_SCROLLED_WINDOW(a->swidget);

	a->areaWidget = GTK_WIDGET(g_object_new(areaWidgetType,
		"libui-area", a,
		NULL));
	a->drawingArea = GTK_DRAWING_AREA(a->areaWidget);
	a->area = areaWidget(a->areaWidget);

	a->widget = a->swidget;

	gtk_container_add(a->scontainer, a->areaWidget);
	// and make the area visible; only the scrolled window's visibility is controlled by libui
	gtk_widget_show(a->areaWidget);

	return a;
}

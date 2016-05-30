// 2 june 2016
#include "uipriv_unix.h"

void loadAreaSize(GtkWidget *w, double *width, double *height, int providesize)
{
	GtkAllocation allocation;

	*width = 0;
	*height = 0;
	// don't provide size information for scrolling areas
	if (providesize) {
		gtk_widget_get_allocation(w, &allocation);
		// these are already in drawing space coordinates
		// for drawing, the size of drawing space has the same value as the widget allocation
		// thanks to tristan in irc.gimp.net/#gtk+
		*width = allocation.width;
		*height = allocation.height;
	}
}

static guint translateModifiers(guint state, GdkWindow *window)
{
	GdkModifierType statetype;

	// GDK doesn't initialize the modifier flags fully; we have to explicitly tell it to (thanks to Daniel_S and daniels (two different people) in irc.gimp.net/#gtk+)
	statetype = state;
	gdk_keymap_add_virtual_modifiers(
		gdk_keymap_get_for_display(gdk_window_get_display(window)),
		&statetype);
	return statetype;
}

static uiModifiers toModifiers(guint state)
{
	uiModifiers m;

	m = 0;
	if ((state & GDK_CONTROL_MASK) != 0)
		m |= uiModifierCtrl;
	if ((state & GDK_META_MASK) != 0)
		m |= uiModifierAlt;
	if ((state & GDK_MOD1_MASK) != 0)		// GTK+ itself requires this to be Alt (just read through gtkaccelgroup.c)
		m |= uiModifierAlt;
	if ((state & GDK_SHIFT_MASK) != 0)
		m |= uiModifierShift;
	if ((state & GDK_SUPER_MASK) != 0)
		m |= uiModifierSuper;
	return m;
}

// capture on drag is done automatically on GTK+
static void finishMouseEvent(uiControl *c, GtkWidget *w, areaEventHandler *eh, uiAreaMouseEvent *me, guint mb, gdouble x, gdouble y, guint state, GdkWindow *window, int providesize)
{
	// on GTK+, mouse buttons 4-7 are for scrolling; if we got here, that's a mistake
	if (mb >= 4 && mb <= 7)
		return;
	// if the button ID >= 8, continue counting from 4, as in the MouseEvent spec
	if (me->Down >= 8)
		me->Down -= 4;
	if (me->Up >= 8)
		me->Up -= 4;

	state = translateModifiers(state, window);
	me->Modifiers = toModifiers(state);

	// the mb != # checks exclude the Up/Down button from Held
	me->Held1To64 = 0;
	if (mb != 1 && (state & GDK_BUTTON1_MASK) != 0)
		me->Held1To64 |= 1 << 0;
	if (mb != 2 && (state & GDK_BUTTON2_MASK) != 0)
		me->Held1To64 |= 1 << 1;
	if (mb != 3 && (state & GDK_BUTTON3_MASK) != 0)
		me->Held1To64 |= 1 << 2;
	// don't check GDK_BUTTON4_MASK or GDK_BUTTON5_MASK because those are for the scrolling buttons mentioned above
	// GDK expressly does not support any more buttons in the GdkModifierType; see https://git.gnome.org/browse/gtk+/tree/gdk/x11/gdkdevice-xi2.c#n763 (thanks mclasen in irc.gimp.net/#gtk+)

	// these are already in drawing space coordinates
	// the size of drawing space has the same value as the widget allocation
	// thanks to tristan in irc.gimp.net/#gtk+
	me->X = x;
	me->Y = y;

	loadAreaSize(w, &me->AreaWidth, &me->AreaHeight, providesize);

	(*(eh->eh->MouseEvent))(eh->eh, c, me);
}

gboolean areaEventHandler_button_press_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventButton *e, int providesize)
{
	gint maxTime, maxDistance;
	GtkSettings *settings;
	uiAreaMouseEvent me;

	// clicking doesn't automatically transfer keyboard focus; we must do so manually (thanks tristan in irc.gimp.net/#gtk+)
	gtk_widget_grab_focus(w);

	// we handle multiple clicks ourselves here, in the same way as we do on Windows
	if (e->type != GDK_BUTTON_PRESS)
		// ignore GDK's generated double-clicks and beyond
		return GDK_EVENT_PROPAGATE;
	settings = gtk_widget_get_settings(w);
	g_object_get(settings,
		"gtk-double-click-time", &maxTime,
		"gtk-double-click-distance", &maxDistance,
		NULL);
	// don't unref settings; it's transfer-none (thanks gregier in irc.gimp.net/#gtk+)
	me.Count = clickCounterClick(&eh->cc, me.Down,
		e->x, e->y,
		e->time, maxTime,
		maxDistance, maxDistance);

	me.Down = e->button;
	me.Up = 0;
	finishMouseEvent(c, w, eh, &me, e->button, e->x, e->y, e->state, e->window, providesize);
	return GDK_EVENT_PROPAGATE;
}

gboolean areaEventHandler_button_release_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventButton *e, int providesize)
{
	uiAreaMouseEvent me;

	me.Down = 0;
	me.Up = e->button;
	me.Count = 0;
	finishMouseEvent(c, w, eh, &me, e->button, e->x, e->y, e->state, e->window, providesize);
	return GDK_EVENT_PROPAGATE;
}

gboolean areaEventHandler_motion_notify_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventMotion *e, int providesize)
{
	uiAreaMouseEvent me;

	me.Down = 0;
	me.Up = 0;
	me.Count = 0;
	finishMouseEvent(c, w, eh, &me, 0, e->x, e->y, e->state, e->window, providesize);
	return GDK_EVENT_PROPAGATE;
}

// we want switching away from the control to reset the double-click counter, like with WM_ACTIVATE on Windows
// according to tristan in irc.gimp.net/#gtk+, doing this on both enter-notify-event and leave-notify-event is correct (and it seems to be true in my own tests; plus the events DO get sent when switching programs with the keyboard (just pointing that out))
static gboolean onCrossing(areaEventHandler *eh, uiControl *c, int left)
{
	(*(eh->eh->MouseCrossed))(eh->eh, c, left);
	clickCounterReset(&eh->cc);
	return GDK_EVENT_PROPAGATE;
}

gboolean areaEventHandler_enter_notify_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventCrossing *e)
{
	return onCrossing(eh, c, 0);
}

gboolean areaEventHandler_leave_notify_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventCrossing *e)
{
	return onCrossing(eh, c, 1);
}

// note: there is no equivalent to WM_CAPTURECHANGED on GTK+; there literally is no way to break a grab like that (at least not on X11 and Wayland)
// even if I invoke the task switcher and switch processes, the mouse grab will still be held until I let go of all buttons
// therefore, no DragBroken()

// we use GDK_KEY_Print as a sentinel because libui will never support the print screen key; that key belongs to the user

static const struct {
	guint keyval;
	uiExtKey extkey;
} extKeys[] = {
	{ GDK_KEY_Escape, uiExtKeyEscape },
	{ GDK_KEY_Insert, uiExtKeyInsert },
	{ GDK_KEY_Delete, uiExtKeyDelete },
	{ GDK_KEY_Home, uiExtKeyHome },
	{ GDK_KEY_End, uiExtKeyEnd },
	{ GDK_KEY_Page_Up, uiExtKeyPageUp },
	{ GDK_KEY_Page_Down, uiExtKeyPageDown },
	{ GDK_KEY_Up, uiExtKeyUp },
	{ GDK_KEY_Down, uiExtKeyDown },
	{ GDK_KEY_Left, uiExtKeyLeft },
	{ GDK_KEY_Right, uiExtKeyRight },
	{ GDK_KEY_F1, uiExtKeyF1 },
	{ GDK_KEY_F2, uiExtKeyF2 },
	{ GDK_KEY_F3, uiExtKeyF3 },
	{ GDK_KEY_F4, uiExtKeyF4 },
	{ GDK_KEY_F5, uiExtKeyF5 },
	{ GDK_KEY_F6, uiExtKeyF6 },
	{ GDK_KEY_F7, uiExtKeyF7 },
	{ GDK_KEY_F8, uiExtKeyF8 },
	{ GDK_KEY_F9, uiExtKeyF9 },
	{ GDK_KEY_F10, uiExtKeyF10 },
	{ GDK_KEY_F11, uiExtKeyF11 },
	{ GDK_KEY_F12, uiExtKeyF12 },
	// numpad numeric keys and . are handled in events.c
	{ GDK_KEY_KP_Enter, uiExtKeyNEnter },
	{ GDK_KEY_KP_Add, uiExtKeyNAdd },
	{ GDK_KEY_KP_Subtract, uiExtKeyNSubtract },
	{ GDK_KEY_KP_Multiply, uiExtKeyNMultiply },
	{ GDK_KEY_KP_Divide, uiExtKeyNDivide },
	{ GDK_KEY_Print, 0 },
};

static const struct {
	guint keyval;
	uiModifiers mod;
} modKeys[] = {
	{ GDK_KEY_Control_L, uiModifierCtrl },
	{ GDK_KEY_Control_R, uiModifierCtrl },
	{ GDK_KEY_Alt_L, uiModifierAlt },
	{ GDK_KEY_Alt_R, uiModifierAlt },
	{ GDK_KEY_Meta_L, uiModifierAlt },
	{ GDK_KEY_Meta_R, uiModifierAlt },
	{ GDK_KEY_Shift_L, uiModifierShift },
	{ GDK_KEY_Shift_R, uiModifierShift },
	{ GDK_KEY_Super_L, uiModifierSuper },
	{ GDK_KEY_Super_R, uiModifierSuper },
	{ GDK_KEY_Print, 0 },
};

int areaKeyEvent(areaEventHandler *eh, uiControl *c, int up, GdkEventKey *e)
{
	uiAreaKeyEvent ke;
	guint state;
	int i;

	ke.Key = 0;
	ke.ExtKey = 0;
	ke.Modifier = 0;

	state = translateModifiers(e->state, e->window);
	ke.Modifiers = toModifiers(state);

	ke.Up = up;

	for (i = 0; extKeys[i].keyval != GDK_KEY_Print; i++)
		if (extKeys[i].keyval == e->keyval) {
			ke.ExtKey = extKeys[i].extkey;
			goto keyFound;
		}

	for (i = 0; modKeys[i].keyval != GDK_KEY_Print; i++)
		if (modKeys[i].keyval == e->keyval) {
			ke.Modifier = modKeys[i].mod;
			// don't include the modifier in ke.Modifiers
			ke.Modifiers &= ~ke.Modifier;
			goto keyFound;
		}

	if (fromScancode(e->hardware_keycode - 8, &ke))
		goto keyFound;

	// no supported key found; treat as unhandled
	return 0;

keyFound:
	return (*(eh->eh->KeyEvent))(eh->eh, c, &ke);
}

gboolean areaEventHandler_key_press_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventKey *e)
{
	if (areaKeyEvent(eh, c, 0, e))
		return GDK_EVENT_STOP;
	return GDK_EVENT_PROPAGATE;
}

gboolean areaEventHandler_key_release_event(areaEventHandler *eh, uiControl *c, GtkWidget *w, GdkEventKey *e)
{
	if (areaKeyEvent(eh, c, 1, e))
		return GDK_EVENT_STOP;
	return GDK_EVENT_PROPAGATE;
}


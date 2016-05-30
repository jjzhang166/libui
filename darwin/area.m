// 9 september 2015
#import "uipriv_darwin.h"

// 10.8 fixups
#define NSEventModifierFlags NSUInteger

@interface areaView : NSView<areaKeyHandler> {
	uiArea *libui_a;
	NSSize libui_ss;
	BOOL libui_enabled;
}
- (id)initWithFrame:(NSRect)r area:(uiArea *)a;
- (void)doMouseEvent:(NSEvent *)e;
- (int)sendKeyEvent:(uiAreaKeyEvent *)ke;
- (int)doKeyDownUp:(NSEvent *)e up:(BOOL)up;
- (int)doFlagsChanged:(NSEvent *)e;
- (void)setScrollingSize:(NSSize)s;
- (BOOL)isEnabled;
- (void)setEnabled:(BOOL)e;
@end

struct uiArea {
	uiDarwinControl c;
	NSView *view;			// either sv or area depending on whether it is scrolling
	NSScrollView *sv;
	areaView *area;
	struct scrollViewData *d;
	uiAreaHandler *ah;
	areaEventHandler *eh;
	BOOL scrolling;
};

@implementation areaView

- (id)initWithFrame:(NSRect)r area:(uiArea *)a
{
	self = [super initWithFrame:r];
	if (self) {
		self->libui_a = a;
		self->libui_ss = r.size;
		self->libui_enabled = YES;

		[self->libui_a->eh updateTrackingAreaForView:self];
	}
	return self;
}

- (void)drawRect:(NSRect)r
{
	uiArea *a = self->libui_a;
	CGContextRef c;
	uiAreaDrawParams dp;

	c = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
	// see draw.m under text for why we need the height
	dp.Context = newContext(c, [self bounds].size.height);

	dp.AreaWidth = 0;
	dp.AreaHeight = 0;
	if (!a->scrolling) {
		dp.AreaWidth = [self frame].size.width;
		dp.AreaHeight = [self frame].size.height;
	}

	dp.ClipX = r.origin.x;
	dp.ClipY = r.origin.y;
	dp.ClipWidth = r.size.width;
	dp.ClipHeight = r.size.height;

	// no need to save or restore the graphics state to reset transformations; Cocoa creates a brand-new context each time
	(*(a->ah->Draw))(a->ah, a, &dp);

	freeContext(dp.Context);
}

- (BOOL)isFlipped
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)updateTrackingAreas
{
	uiArea *a = self->libui_a;
	[a->eh updateTrackingAreaForView:self];
}

// capture on drag is done automatically on OS X
- (void)doMouseEvent:(NSEvent *)e
{
	uiArea *a = self->libui_a;
	uiAreaMouseEvent me = [a->eh doMouseEvent:e inView:self scrolling:a->scrolling];
	if (self->libui_enabled)
		(*(a->ah->eh.MouseEvent))(&a->ah->eh, &a->c.c, &me);
}

uiImplMouseEvents

- (void)mouseEntered:(NSEvent *)e
{
	uiArea *a = self->libui_a;
	if (self->libui_enabled)
		(*(a->ah->eh.MouseCrossed))(&a->ah->eh, &a->c.c, 0);
}

- (void)mouseExited:(NSEvent *)e
{
	uiArea *a = self->libui_a;
	if (self->libui_enabled)
		(*(a->ah->eh.MouseCrossed))(&a->ah->eh, &a->c.c, 1);
}

// note: there is no equivalent to WM_CAPTURECHANGED on Mac OS X; there literally is no way to break a grab like that
// even if I invoke the task switcher and switch processes, the mouse grab will still be held until I let go of all buttons
// therefore, no DragBroken()

- (int)sendKeyEvent:(uiAreaKeyEvent *)ke
{
	uiArea *a = self->libui_a;
	return (*(a->ah->eh.KeyEvent))(&a->ah->eh, &a->c.c, ke);
}

- (int)doKeyDownUp:(NSEvent *)e up:(BOOL)up
{
	uiArea *a = self->libui_a;
	uiAreaKeyEvent ke;
	if (![a->eh doKeyDownUp:e up:up keyEvent:&ke])
		return 0;
	return [self sendKeyEvent:&ke];
}

- (int)doFlagsChanged:(NSEvent *)e
{
	uiArea *a = self->libui_a;
	uiAreaKeyEvent ke;
	if (![a->eh doFlagsChanged:e keyEvent:&ke])
		return 0;
	return [self sendKeyEvent:&ke];
}

- (void)setFrameSize:(NSSize)size
{
	uiArea *a = self->libui_a;

	[super setFrameSize:size];
	if (!a->scrolling)
		// we must redraw everything on resize because Windows requires it
		[self setNeedsDisplay:YES];
}

- (void)setScrollingSize:(NSSize)s
{
	self->libui_ss = s;
	[self invalidateIntrinsicContentSize];
}

- (NSSize)intrinsicContentSize
{
	if (!self->libui_a->scrolling)
		return [super intrinsicContentSize];
	return self->libui_ss;
}

- (BOOL)becomeFirstResponder
{
	return [self isEnabled];
}

- (BOOL)isEnabled
{
	return self->libui_enabled;
}

- (void)setEnabled:(BOOL)e
{
	self->libui_enabled = e;
	if (!self->libui_enabled && [self window] != nil)
		if ([[self window] firstResponder] == self)
			[[self window] makeFirstResponder:nil];
}

@end

uiDarwinControlAllDefaultsExceptDestroy(uiArea, view)

static void uiAreaDestroy(uiControl *c)
{
	uiArea *a = uiArea(c);

	if (a->scrolling)
		scrollViewFreeData(a->sv, a->d);
	[a->area release];
	if (a->scrolling)
		[a->sv release];
	uiFreeControl(uiControl(a));
}

void uiAreaSetSize(uiArea *a, intmax_t width, intmax_t height)
{
	if (!a->scrolling)
		userbug("You cannot call uiAreaSetSize() on a non-scrolling uiArea. (area: %p)", a);
	[a->area setScrollingSize:NSMakeSize(width, height)];
}

void uiAreaQueueRedrawAll(uiArea *a)
{
	[a->area setNeedsDisplay:YES];
}

void uiAreaScrollTo(uiArea *a, double x, double y, double width, double height)
{
	if (!a->scrolling)
		userbug("You cannot call uiAreaScrollTo() on a non-scrolling uiArea. (area: %p)", a);
	[a->area scrollRectToVisible:NSMakeRect(x, y, width, height)];
	// don't worry about the return value; it just says whether scrolling was needed
}

uiArea *uiNewArea(uiAreaHandler *ah)
{
	uiArea *a;

	uiDarwinNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = NO;

	a->area = [[areaView alloc] initWithFrame:NSZeroRect area:a];

	a->view = a->area;

	return a;
}

uiArea *uiNewScrollingArea(uiAreaHandler *ah, intmax_t width, intmax_t height)
{
	uiArea *a;
	struct scrollViewCreateParams p;

	uiDarwinNewControl(uiArea, a);

	a->ah = ah;
	a->scrolling = YES;

	a->eh = [[areaEventHandler alloc] init];

	a->area = [[areaView alloc] initWithFrame:NSMakeRect(0, 0, width, height)
		area:a];

	memset(&p, 0, sizeof (struct scrollViewCreateParams));
	p.DocumentView = a->area;
	p.BackgroundColor = [NSColor controlColor];
	p.DrawsBackground = 1;
	p.Bordered = NO;
	p.HScroll = YES;
	p.VScroll = YES;
	a->sv = mkScrollView(&p, &(a->d));

	a->view = a->sv;

	return a;
}

// 25 may 2016
#import "uipriv_darwin.h"

@interface openGLAreaView : NSView {
	uiOpenGLArea *libui_a;
	BOOL libui_enabled;
}

@end

#define ATTRIBUTE_LIST_SIZE	256

struct uiOpenGLArea {
	uiDarwinControl c;
	openGLAreaView *view;
	uiOpenGLAreaHandler *ah;
	areaEventHandler *eh;
	CGLPixelFormatObj pix;
	GLint npix;
	NSOpenGLContext *ctx;
	BOOL initialized;
};

// This functionality is wrapped up here to guard against buffer overflows in the attribute list.
static void assignNextPixelFormatAttribute(CGLPixelFormatAttribute *as, unsigned *ai, CGLPixelFormatAttribute a)
{
	if (*ai >= ATTRIBUTE_LIST_SIZE)
		implbug("Too many pixel format attributes; increase ATTRIBUTE_LIST_SIZE!");
	as[*ai] = a;
	(*ai)++;
}

@implementation openGLAreaView

- (id)initWithFrame:(NSRect)r area:(uiOpenGLArea *)a attributes:(uiOpenGLAttributes *)attribs
{
	self = [super initWithFrame:r];
	if (self) {
		self->libui_a = a;
		self->libui_enabled = YES;

		[self->libui_a->eh updateTrackingAreaForView:self];

		CGLPixelFormatAttribute pfAttributes[ATTRIBUTE_LIST_SIZE];
		unsigned pfAttributeIndex = 0;
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFAColorSize);
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, attribs->RedBits + attribs->GreenBits + attribs->BlueBits);
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFAAlphaSize);
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, attribs->AlphaBits);
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFADepthSize);
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, attribs->DepthBits);
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFAStencilSize);
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, attribs->StencilBits);
		if (attribs->Stereo)
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFAStereo);
		if (attribs->Samples > 0) {
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFAMultisample);
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFASamples);
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, attribs->Samples);
		}
		if (attribs->DoubleBuffer)
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFADoubleBuffer);
		if (attribs->MajorVersion < 3) {
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFAOpenGLProfile);
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, (CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy);
		} else {
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, kCGLPFAOpenGLProfile);
			assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core);
		}
		assignNextPixelFormatAttribute(pfAttributes, &pfAttributeIndex, 0);

		if (CGLChoosePixelFormat(pfAttributes, &self->libui_a->pix, &self->libui_a->npix) != kCGLNoError)
			userbug("No available pixel format!");

		CGLContextObj ctx;
		if (CGLCreateContext(self->libui_a->pix, NULL, &ctx) != kCGLNoError)
			userbug("Couldn't create OpenGL context!");
		self->libui_a->ctx = [[NSOpenGLContext alloc] initWithCGLContextObj:ctx];
		[[NSNotificationCenter defaultCenter] addObserver:self selector: @selector(viewBoundsDidChange:) name:NSViewFrameDidChangeNotification object:self];
	}
	return self;
}

- (void)viewBoundsDidChange:(NSNotification *)notification
{
	[self->libui_a->ctx setView:self];
	[self->libui_a->ctx update];
}

- (void)drawRect:(NSRect)r
{
	uiOpenGLArea *a = self->libui_a;
	uiOpenGLAreaMakeCurrent(a);

	if (!a->initialized) {
		(*(a->ah->InitGL))(a->ah, a);
		a->initialized = YES;
	}
	(*(a->ah->DrawGL))(a->ah, a);
}

- (void)doMouseEvent:(NSEvent *)e
{
	uiOpenGLArea *a = self->libui_a;
	uiAreaMouseEvent me = [a->eh doMouseEvent:e inView:self scrolling:NO];
	if (self->libui_enabled)
		(*(a->ah->eh.MouseEvent))(&a->ah->eh, &a->c.c, &me);
}

uiImplMouseEvents

- (void)mouseEntered:(NSEvent *)e
{
	uiOpenGLArea *a = self->libui_a;

	if (self->libui_enabled)
		(*(a->ah->eh.MouseCrossed))(&a->ah->eh, &a->c.c, 0);
}

- (void)mouseExited:(NSEvent *)e
{
	uiOpenGLArea *a = self->libui_a;

	if (self->libui_enabled)
		(*(a->ah->eh.MouseCrossed))(&a->ah->eh, &a->c.c, 1);
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)updateTrackingAreas
{
	uiOpenGLArea *a = self->libui_a;
	[a->eh updateTrackingAreaForView:self];
}

- (int)sendKeyEvent:(uiAreaKeyEvent *)ke
{
	uiOpenGLArea *a = self->libui_a;
	return (*(a->ah->eh.KeyEvent))(&a->ah->eh, &a->c.c, ke);
}

- (int)doKeyDownUp:(NSEvent *)e up:(BOOL)up
{
	uiOpenGLArea *a = self->libui_a;
	uiAreaKeyEvent ke;
	if (![a->eh doKeyDownUp:e up:up keyEvent:&ke])
		return 0;
	return [self sendKeyEvent:&ke];
}

- (int)doFlagsChanged:(NSEvent *)e
{
	uiOpenGLArea *a = self->libui_a;
	uiAreaKeyEvent ke;
	if (![a->eh doFlagsChanged:e keyEvent:&ke])
		return 0;
	return [self sendKeyEvent:&ke];
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

uiDarwinControlAllDefaultsExceptDestroy(uiOpenGLArea, view)

static void uiOpenGLAreaDestroy(uiControl *c)
{
	uiOpenGLArea *a = uiOpenGLArea(c);

	[a->view release];
	[a->ctx release];
	CGLReleasePixelFormat(a->pix);
	uiFreeControl(uiControl(a));
}

void uiOpenGLAreaGetSize(uiOpenGLArea *a, int *width, int *height)
{
	NSRect rect = [a->view frame];
	*width = rect.size.width;
	*height = rect.size.height;
}

void uiOpenGLAreaSetSwapInterval(uiOpenGLArea *a, int si)
{
	if (!CGLSetParameter([a->ctx CGLContextObj], kCGLCPSwapInterval, &si) != kCGLNoError)
		userbug("Couldn't set the swap interval!");
}

void uiOpenGLAreaQueueRedrawAll(uiOpenGLArea *a)
{
	[a->view setNeedsDisplay:YES];
}

void uiOpenGLAreaMakeCurrent(uiOpenGLArea *a)
{
	CGLSetCurrentContext([a->ctx CGLContextObj]);
}

void uiOpenGLAreaSwapBuffers(uiOpenGLArea *a)
{
	CGLFlushDrawable([a->ctx CGLContextObj]);
}

uiOpenGLArea *uiNewOpenGLArea(uiOpenGLAreaHandler *ah, uiOpenGLAttributes *attribs)
{
	uiOpenGLArea *a;
	uiDarwinNewControl(uiOpenGLArea, a);
	a->initialized = NO;
	a->ah = ah;
	a->eh = [[areaEventHandler alloc] init];
	a->view = [[openGLAreaView alloc] initWithFrame:NSZeroRect area:a attributes:attribs];
	return a;
}


/* 
 * Copyright (c) 2004, Laminar Research.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 *
 */
#include "XWin.h"
#include "XUtils.h"

static	bool	MacCanAcceptDrag(DragReference theDrag, vector<string> * outFiles);
static	bool	sIniting = false;

static EventHandlerUPP			mEventHandler = NewEventHandlerUPP(XWin::MacEventHandler);
static DragTrackingHandlerUPP	mTrackingHandler = NewDragTrackingHandlerUPP(XWin::MacTrackingHandler);
static DragReceiveHandlerUPP	mReceiveHandler = NewDragReceiveHandlerUPP(XWin::MacReceiveHandler);
static EventLoopTimerUPP		mTimerHandler = NewEventLoopTimerUPP(XWin::MacTimer);

XWin::XWin()
{
	sIniting = true;

		Rect	bounds;
		
	bounds = (**(::GetMainDevice())).gdRect;
	bounds.top += GetMBarHeight();
	
	OSStatus	err = CreateNewWindow(kPlainWindowClass, kWindowStandardHandlerAttribute,
						&bounds,
						&mWindow);
	if (err != noErr) throw err;
	
		EventTypeSpec	events[] = { 
//			kEventClassMouse,	kEventMouseDown,
			kEventClassMouse,	kEventMouseUp,
			kEventClassMouse,	kEventMouseDragged,
			kEventClassMouse,	kEventMouseWheelMoved,
			kEventClassWindow, 	kEventWindowHandleContentClick,
			kEventClassWindow,	kEventWindowDrawContent,
			kEventClassWindow,	kEventWindowBoundsChanged,
			kEventClassWindow,	kEventWindowResizeCompleted,
			kEventClassWindow,	kEventWindowClose,
			kEventClassWindow,	kEventWindowClosed,
			kEventClassKeyboard,kEventRawKeyDown,
			kEventClassCommand,	kEventCommandProcess
		};			
		
	
	err = InstallEventHandler(GetWindowEventTarget(mWindow),
		mEventHandler, GetEventTypeCount(events), events, 
			reinterpret_cast<void *>(this),
			NULL);
	if (err != noErr) throw err;
	
	err = InstallTrackingHandler(mTrackingHandler, mWindow, reinterpret_cast<void *>(this));
	if (err != noErr) throw err;
	
	err = InstallReceiveHandler(mReceiveHandler, mWindow, reinterpret_cast<void *>(this));
	if (err != noErr) throw err;

	ShowWindow(mWindow);	
	sIniting = false;
	
	err = InstallEventLoopTimer(GetMainEventLoop(), kDurationForever, kDurationForever, mTimerHandler, 
		reinterpret_cast<void *>(this), &mTimer);
	if (err != noErr) throw err;
}


XWin::XWin(
	const char * 	inTitle,
	int				inX,
	int				inY,
	int				inWidth,
	int				inHeight)
{
	sIniting = true;

		Rect	bounds;
	
	bounds.left = inX;
	bounds.right = inX + inWidth;
	bounds.top = inY;
	bounds.bottom = inY + inHeight;
	
	OSStatus	err = CreateNewWindow(kDocumentWindowClass,
						kWindowStandardDocumentAttributes + kWindowStandardHandlerAttribute,
						&bounds,
						&mWindow);
	if (err != noErr) throw err;
	
		EventTypeSpec	events[] = { 
//			kEventClassMouse,	kEventMouseDown,
			kEventClassMouse,	kEventMouseUp,
			kEventClassMouse,	kEventMouseDragged,
			kEventClassMouse,	kEventMouseWheelMoved,
			kEventClassWindow, 	kEventWindowHandleContentClick,
			kEventClassWindow,	kEventWindowDrawContent,
			kEventClassWindow,	kEventWindowBoundsChanged,
			kEventClassWindow,	kEventWindowResizeCompleted,
			kEventClassWindow,	kEventWindowClose,
			kEventClassWindow,	kEventWindowClosed,
			kEventClassKeyboard,kEventRawKeyDown,
			kEventClassCommand,	kEventCommandProcess
		};			
		
	
	err = InstallEventHandler(GetWindowEventTarget(mWindow),
		mEventHandler, GetEventTypeCount(events), events, 
			reinterpret_cast<void *>(this),
			NULL);
	if (err != noErr) throw err;
	
	err = InstallTrackingHandler(mTrackingHandler, mWindow, reinterpret_cast<void *>(this));
	if (err != noErr) throw err;
	
	err = InstallReceiveHandler(mReceiveHandler, mWindow, reinterpret_cast<void *>(this));
	if (err != noErr) throw err;

	ShowWindow(mWindow);	
	sIniting = false;
	
	SetTitle(inTitle);
	
	err = InstallEventLoopTimer(GetMainEventLoop(), kDurationForever, kDurationForever, mTimerHandler, 
		reinterpret_cast<void *>(this), &mTimer);
	if (err != noErr) throw err;
}
	
XWin::~XWin()
{
	RemoveEventLoopTimer(mTimer);
	if (mWindow)
		DisposeWindow(mWindow);
}

#pragma mark -

void			XWin::SetTitle(const char * inTitle)
{
	Str255	title;
	title[0] = strlen(inTitle);
	memcpy(title+1,inTitle, title[0]);
	::SetWTitle(mWindow, title);
}

void			XWin::MoveTo(int inX, int inY)
{
	::MoveWindow(mWindow, inX, inY, true);
}

void			XWin::Resize(int inWidth, int inHeight)
{
	::SizeWindow(mWindow, inWidth, inHeight, true);
}

void			XWin::ForceRefresh(void)
{
	Rect	bounds;

	::GetWindowBounds(mWindow, kWindowContentRgn, &bounds);
	::OffsetRect(&bounds, -bounds.left, -bounds.top);
	::InvalWindowRect(mWindow, &bounds);
}

void			XWin::SetTimerInterval(double seconds)
{
	RemoveEventLoopTimer(mTimer);
	OSErr err = InstallEventLoopTimer(GetMainEventLoop(), seconds ? seconds : kDurationForever, seconds ? seconds : kDurationForever, mTimerHandler, 
		reinterpret_cast<void *>(this), &mTimer);
}		
	
void			XWin::GetBounds(int * outX, int * outY)
{
	Rect	bounds;

	::GetWindowBounds(mWindow, kWindowContentRgn, &bounds);
	::OffsetRect(&bounds, -bounds.left, -bounds.top);
	
	if (outX) *outX = bounds.right;
	if (outY) *outY = bounds.bottom;
}

#pragma mark -

pascal OSStatus	XWin::MacEventHandler(
						EventHandlerCallRef inHandlerCallRef, EventRef 
						inEvent, 
						void *inUserData)
{
	if (sIniting)
		return eventNotHandledErr;
		
	XWin * me = reinterpret_cast<XWin *>(inUserData);

	Point				pt;
	EventMouseButton	btn;
	EventMouseWheelAxis	axis;
	UInt32				delta;
	char				macChar;
	UInt32				modifiers;

	Rect		bounds;
	::GetWindowBounds(me->mWindow, kWindowContentRgn, &bounds);
	::OffsetRect(&bounds, -bounds.left, -bounds.top);

	GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(pt), NULL, &pt);
	GetEventParameter(inEvent, kEventParamMouseButton, typeMouseButton, NULL, sizeof(btn), NULL, &btn);
	GetEventParameter(inEvent, kEventParamMouseWheelAxis, typeMouseWheelAxis, NULL, sizeof(axis), NULL, &axis);
	GetEventParameter(inEvent, kEventParamMouseWheelDelta, typeLongInteger, NULL, sizeof(delta), NULL, &delta);
	GetEventParameter(inEvent, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(macChar), NULL, &macChar);
	GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifiers), NULL, &modifiers);
	
	EventRecord	e;

	UInt32	clss = ::GetEventClass(inEvent);	
	UInt32	kind = ::GetEventKind(inEvent);
	
	SetPortWindowPort(me->mWindow);
	GlobalToLocal(&pt);
	
	static	bool	got_down = false;
	
	switch(clss) {
	case kEventClassMouse:
		if ((btn == 1) && (modifiers & controlKey))	btn = 2;
		switch(kind) {
		case kEventMouseDown:
			got_down = true;
			me->ClickDown(pt.h, pt.v, btn - 1);
			return noErr;
		case kEventMouseUp:
			if (got_down)
				me->ClickUp(pt.h, pt.v, btn - 1);
			got_down = false;
			return noErr;
		case kEventMouseDragged:
			if (got_down)
				me->ClickDrag(pt.h, pt.v, btn - 1);				
			return noErr;
		case kEventMouseWheelMoved:
			me->MouseWheel(pt.h, pt.v, delta, (axis == kEventMouseWheelAxisY) ? 0 : 1);
			return noErr;
		default:
			return eventNotHandledErr;
		}
		break;
	case kEventClassWindow:
		switch(kind) {
		case kEventWindowHandleContentClick:
			if ((btn == 1) && (modifiers & controlKey))	btn = 2;
			got_down = true;
			me->ClickDown(pt.h, pt.v, btn - 1);
			return noErr;
		case kEventWindowDrawContent:
			me->Update(NULL);
			return noErr;
		case kEventWindowBoundsChanged:
			me->Resized(bounds.right, bounds.bottom);
			return noErr;
		case kEventWindowResizeCompleted:
			me->Resized(bounds.right, bounds.bottom);
			return noErr;
		case kEventWindowClose:
			if (me->Closed())
				return eventNotHandledErr;
			else
				return noErr;
		case kEventWindowClosed:
			me->mWindow = NULL;
			delete me;
			return noErr;
		default:
			return eventNotHandledErr;
		}
		break;
	case kEventClassKeyboard:
		switch(kind) {
		case kEventRawKeyDown:
			{
				ConvertEventRefToEventRecord(inEvent, &e);
				if (!me->KeyPressed(macChar, e.what, e.message, e.modifiers))
				{
					if (macChar == '=')
						me->MouseWheel(e.where.h, e.where.v, 1, 0);
					else if (macChar == '-')
						me->MouseWheel(e.where.h, e.where.v, -1, 0);
				}
			}
			return noErr;
		default:
			return eventNotHandledErr;
		}
	case kEventClassCommand:
		switch(kind) {
		case kEventCommandProcess:
			{
				HICommand cmd;
				GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(cmd), NULL, &cmd);
				if (cmd.menu.menuRef && me->HandleMenuCmd(cmd.menu.menuRef, cmd.menu.menuItemIndex - 1))
					return noErr;
				else
					return eventNotHandledErr;
			}
		default:
			return eventNotHandledErr;
		}		
	default:
		return eventNotHandledErr;
	}
}						

pascal OSErr	XWin::MacTrackingHandler(
					DragTrackingMessage message, 
					WindowRef theWindow, 
					void *handlerRefCon, 
					DragRef theDrag)
{
	XWin * me = reinterpret_cast<XWin *>(handlerRefCon);

	Rect		bounds;
	RgnHandle	rgn;
	::GetWindowBounds(me->mWindow, kWindowContentRgn, &bounds);
	::OffsetRect(&bounds, -bounds.left, -bounds.top);
	Point	p, p2;
	GetDragMouse(theDrag, &p, &p2);
	SetPortWindowPort(me->mWindow);
	GlobalToLocal(&p);

	switch(message) {
	case kDragTrackingEnterWindow:
		if (!MacCanAcceptDrag(theDrag, NULL))
			return dragNotAcceptedErr;
		rgn = NewRgn();
		RectRgn(rgn, &bounds);
		ShowDragHilite(theDrag, rgn, true);
		DisposeRgn(rgn);
		me->DragEnter(p.h, p.v);
		me->ForceRefresh();
		return noErr;
	case kDragTrackingInWindow:
		me->DragOver(p.h, p.v);
		me->ForceRefresh();
		return noErr;
 	case kDragTrackingLeaveWindow:
		SetPortWindowPort(me->mWindow);
 		HideDragHilite(theDrag);
		me->DragLeave();
		me->ForceRefresh();
		me->ForceRefresh();
 		return noErr;
	}
	return noErr;	
}
					
pascal OSErr	XWin::MacReceiveHandler(
					WindowRef theWindow, 
					void *handlerRefCon, 
					DragRef theDrag)
{
	XWin * me = reinterpret_cast<XWin *>(handlerRefCon);
	
	vector<string>	files;
	if (MacCanAcceptDrag(theDrag, &files))
	{
		Point	p, p2;
		GetDragMouse(theDrag, &p, &p2);
		SetPortWindowPort(me->mWindow);
		GlobalToLocal(&p);
		me->ReceiveFiles(files, p.h, p.v);
		me->ForceRefresh();
		return noErr;
	} else
		return dragNotAcceptedErr;
}					
		
bool	MacCanAcceptDrag(DragReference theDrag, vector<string> * outFiles)
{
	UInt16			count, index;
	DragItemRef		item;
	FlavorFlags		flags;
	HFSFlavor		hfsFlavor;
	Size			dataSize;
	
	if (::CountDragItems(theDrag, &count) != noErr) return false;
	
	for (index = 1; index <= count; ++index)
	{
		if (::GetDragItemReferenceNumber(theDrag, index, &item) != noErr) return false;
		
		if ((::GetFlavorFlags(theDrag, item, kDragFlavorTypeHFS, &flags) == noErr) &&
			((flags & flavorSenderOnly) == 0))
		{
			dataSize = sizeof(hfsFlavor);
			if ((::GetFlavorData(theDrag, item, kDragFlavorTypeHFS, &hfsFlavor, &dataSize, 0L) != noErr) ||
				(dataSize != sizeof(hfsFlavor)))
			{
				return false;
			}
			
			if (outFiles)
			{
				string	foo;
				FSSpec_2_String(hfsFlavor.fileSpec, foo);
				outFiles->push_back(foo);
			}
			
		} else 
			return false;
	
		
	}
	
	return true;
}

pascal void		XWin::MacTimer(EventLoopTimerRef inTimer, void *inUserData)
{
	XWin * me = reinterpret_cast<XWin *>(inUserData);
	me->Timer();
}

xmenu XWin::GetMenuBar(void)
{
	return NULL;
}

#pragma mark -

xmenu	XWin::CreateMenu(xmenu parent, int item, const char * inTitle)
{
	static	int gID = 1000;
	Str255	pStr;
	if (inTitle)
	{
		pStr[0] = strlen(inTitle);
		memcpy(pStr+1, inTitle, pStr[0]);
	} else
		pStr[0] = 0;
		
	xmenu new_menu = ::NewMenu(gID++, pStr);
	
	if (parent)
	{
		SetMenuItemHierarchicalID(parent,item + 1, GetMenuID(new_menu));
	}
	
	InsertMenu(new_menu,0);
	
	return new_menu;
}

int		XWin::AppendMenuItem(xmenu menu, const char * inTitle)
{
	Str255	pStr;
	pStr[0] = strlen(inTitle);
	memcpy(pStr+1,inTitle, pStr[0]);
	AppendMenuItemText(menu, pStr);
	return CountMenuItems(menu) - 1;	
}

int		XWin::AppendSeparator(xmenu menu)
{
	return XWin::AppendMenuItem(menu, "-");
}

void	XWin::CheckMenuItem(xmenu menu, int item, bool inCheck)
{
	::CheckMenuItem(menu, item+1, inCheck);
}

void	XWin::EnableMenuItem(xmenu menu, int item, bool inEnable)
{
	if (inEnable)
		::EnableMenuItem(menu, item+1);
	else
		::DisableMenuItem(menu, item+1);
}

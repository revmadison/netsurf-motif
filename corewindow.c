/*
 * Copyright 2017 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * framebuffer generic core window interface.
 *
 * Provides interface for core renderers to the framebufefr toolkit
 * drawable area.
 *
 * This module is an object that must be encapsulated. Client users
 * should embed a struct motif_corewindow at the beginning of their
 * context for this display surface, fill in relevant data and then
 * call motif_corewindow_init()
 *
 * The fb core window structure requires the callback for draw, key and
 * mouse operations.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/utf8.h"
#include "utils/nsoption.h"
#include "netsurf/keypress.h"
#include "netsurf/mouse.h"
#include "netsurf/plot_style.h"
#include "netsurf/plotters.h"

#include <X11/keysym.h>
#include <Xm/AtomMgr.h>
#include <Xm/CutPaste.h>
#include <Xm/DialogS.h>
#include <Xm/DrawingA.h>
#include <Xm/DrawnB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/MainW.h>
#include <Xm/ScrollBar.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

#include "motif/gui.h"
#include "motif/drawing.h"
#include "motif/corewindow.h"

extern Widget motifWindow;
extern Display *motifDisplay;
extern Visual *motifVisual;
extern int motifDepth;
extern int motifDoubleBuffered;
extern Colormap motifColormap;

// TODO: This mainly only supports the bookmarks (hotlist) for now
//		 Need to add support for local history and settings still
void coreDrawingAreaRedrawCallback(Widget widget, XtPointer client_data, XtPointer call_data);
void coreDrawingAreaInputCallback(Widget widget, XtPointer client_data, XtPointer call_data);



/* toolkit event handlers that do generic things and call internal callbacks */

/**
 * callback from core to request a redraw
 */
static nserror
fb_cw_invalidate(struct core_window *cw, const struct rect *r)
{
	struct motif_corewindow *motif_cw = (struct motif_corewindow *)cw;

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &fb_plotters,
		.priv = motif_cw
	};
	Dimension width, height;

	XtVaGetValues(motif_cw->drawingArea, XmNwidth, &width, XmNheight, &height, NULL);

	// There seems to be a bug when deleting bookmarks
	// so we'll just redraw the whole window instead of the specified rect
	struct rect clip;
	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = width;
	clip.y1 = height;
	hotlist_redraw(0, 0, &clip, &ctx);

	return NSERROR_OK;
}


static nserror
fb_cw_update_size(struct core_window *cw, int width, int height)
{
	struct motif_corewindow *motif_cw = (struct motif_corewindow *)cw;
	XtVaSetValues(motif_cw->layout, XmNheight, height, NULL);
	XtVaSetValues(motif_cw->drawingArea, XmNheight, height, NULL);

	return NSERROR_OK;
}


static nserror
fb_cw_set_scroll(struct core_window *cw, int x, int y)
{
//printf("fb_cw_set_scroll\n");
	return NSERROR_OK;
}


static nserror
fb_cw_get_scroll(const struct core_window *cw, int *x, int *y)
{
//printf("fb_cw_get_scroll\n");
	return NSERROR_NOT_IMPLEMENTED;
}


static nserror
fb_cw_get_window_dimensions(const struct core_window *cw,
		int *width, int *height)
{
	struct motif_corewindow *motif_cw = (struct motif_corewindow *)cw;
	Dimension w, h;	
	XtVaGetValues(motif_cw->drawingArea, XmNwidth, &w, XmNheight, &h, NULL);

	*width = w; 
	*height = h; 
	return NSERROR_OK;
}


static nserror
fb_cw_drag_status(struct core_window *cw, core_window_drag_status ds)
{
//printf("fb_cw_drag_status\n");
	struct motif_corewindow *fb_cw = (struct motif_corewindow *)cw;
	fb_cw->drag_status = ds;

	return NSERROR_OK;
}


struct core_window_callback_table motif_cw_cb_table = {
	.invalidate = fb_cw_invalidate,
	.update_size = fb_cw_update_size,
	.set_scroll = fb_cw_set_scroll,
	.get_scroll = fb_cw_get_scroll,
	.get_window_dimensions = fb_cw_get_window_dimensions,
	.drag_status = fb_cw_drag_status
};

extern uint64_t timestamp();

// Used for core windows (bookmarks, history, etc.)
void coreDrawingAreaRedrawCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	Dimension width, height;
	int scrollX, scrollY;
	int x;
	int y;
	struct rect clip;

	struct motif_corewindow *cw;
	XtVaGetValues(widget, XmNuserData, &cw, NULL);
	if(!cw) {
		printf("Widget with null userData!\n");
		return;
	}


	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &fb_plotters,
		.priv = cw
	};
	XEvent *event = NULL;
	XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *)call_data;

	if(cbs) {
		event = cbs->event;
	}

	XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height, NULL);
	//XtVaGetValues(cw->vertScrollBar, XmNvalue, &scrollY, NULL);
	//XtVaGetValues(cw->horizScrollBar, XmNvalue, &scrollX, NULL);

	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = width;
	clip.y1 = height;

	if(event && !motifDoubleBuffered) {
		clip.x0 = event->xexpose.x;
		clip.y0 = event->xexpose.y;
		clip.x1 = clip.x0 + event->xexpose.width;
		clip.y1 = clip.y0 + event->xexpose.height;
	}

	hotlist_redraw(0, 0, &clip, &ctx);
}

void coreDrawingAreaInputCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	XEvent *event = NULL;
	XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *)call_data;
	event = cbs->event;

	if(!event)
	{
		return;
	}
	struct motif_corewindow *cw;

	Dimension curWidth, curHeight;
	XtVaGetValues(widget, XmNwidth, &curWidth, XmNheight, &curHeight, XmNuserData, &cw, NULL);
	if(!cw) {
		printf("Drawing widget with null userData!\n");
		return;
	}

	if(event->xany.type == KeyPress) {
		KeySym keySym;// = XLookupKeysym((XKeyEvent *)event, 0);
		char buffer[8];
		XLookupString((XKeyEvent *)event, buffer, 7, &keySym, NULL);

		switch (keySym) {

		case XK_Delete:
			hotlist_keypress(NS_KEY_DELETE_RIGHT);
			break;

		case XK_BackSpace:
			hotlist_keypress(NS_KEY_DELETE_LEFT);
			break;

		case XK_Right:
			hotlist_keypress(NS_KEY_RIGHT);
			break;

		case XK_Left:
			hotlist_keypress(NS_KEY_LEFT);
			break;

		case XK_Up:
			hotlist_keypress(NS_KEY_UP);
			break;

		case XK_Down:
			hotlist_keypress(NS_KEY_DOWN);
			break;

		case XK_Return:
			hotlist_keypress(13);	// Send the return key in
			break;

		case XK_Tab:
			hotlist_keypress(9);	// Send the tab key in
			break;

		// These are key-specific checks that ALL must fully check as they fall through
		case XK_A:
		case XK_a:
			if ((keySym == XK_a || keySym == XK_A) &&
					(event->xkey.state & ControlMask)) {
				/* A pressed with CTRL held */
				hotlist_keypress(NS_KEY_ESCAPE);
				hotlist_keypress(NS_KEY_ESCAPE);
				hotlist_keypress(NS_KEY_SELECT_ALL);
				break;
			}
			// If not ctrl-A, fall through
		case XK_E:
		case XK_e:
			if ((keySym == XK_e || keySym == XK_E) &&
					(event->xkey.state & ControlMask)) {
				/* E pressed with CTRL held */
				hotlist_edit_selection();
				break;
			}
			// If not ctrl-E, fall through
			/* Fall through */

		default:
			if(buffer[0] >= 32 && buffer[0] <= 255) {
				hotlist_keypress(buffer[0]);
			}
			break;
		}
	} else if(event->xany.type == KeyRelease) {
	}
}

void coreMouseAction(Widget widget, XEvent *event, String *args, Cardinal *num_args)
{
	XButtonEvent *bevent = (XButtonEvent *)event;
	int scrollX, scrollY;
	struct motif_corewindow *cw;

	XtVaGetValues(widget, XmNuserData, &cw, NULL);
	if(!cw) {
		printf("Null userData in widget!\n");
		return;
	}

	int x, y;
	browser_mouse_state mouse;
	uint64_t time_now = timestamp();
	static struct {
		enum { CLICK_SINGLE, CLICK_DOUBLE, CLICK_TRIPLE } type;
		uint64_t time;
	} last_click;

	x = event->xbutton.x;
	y = event->xbutton.y;

	if(*num_args != 1)
	{
		return;
	}

	if(!strcmp(args[0], "down"))
	{
		hotlist_mouse_action(BROWSER_MOUSE_PRESS_1, x, y);
		cw->mousePressed = 1;
		cw->mouseX = x;
		cw->mouseY = y;

	} else if(!strcmp(args[0], "up")) {
		/* This is a click;
		 * clear PRESSED state and pass to core */
		mouse = BROWSER_MOUSE_CLICK_1;

		/* Determine if it's a double or triple click, allowing
		 * 0.5 seconds (500ms) between clicks
		 */
		if(time_now < (last_click.time + 500)) {
			if (last_click.type == CLICK_SINGLE) {
				/* Set double click */
				mouse |= BROWSER_MOUSE_DOUBLE_CLICK;
				last_click.type = CLICK_DOUBLE;

			} else if (last_click.type == CLICK_DOUBLE) {
				/* Set triple click */
				mouse |= BROWSER_MOUSE_TRIPLE_CLICK;
				last_click.type = CLICK_TRIPLE;
			} else {
				/* Set normal click */
				last_click.type = CLICK_SINGLE;
			}
		} else {
			last_click.type = CLICK_SINGLE;
		}

		if(cw->mouseDragging) {
			mouse = BROWSER_MOUSE_HOVER;
		}

		if (mouse) {
			hotlist_mouse_action(mouse, x, y);
		}

		cw->mousePressed = 0;
		cw->mouseDragging = 0;

		last_click.time = time_now;
	} else if(!strcmp(args[0], "move")) {
		browser_mouse_state mouse = 0;

		x = event->xmotion.x;
		y = event->xmotion.y;

		if(!cw->mousePressed) {
			hotlist_mouse_action(BROWSER_MOUSE_HOVER, x, y);
		} else if(cw->mouseDragging) {
			hotlist_mouse_action(BROWSER_MOUSE_DRAG_1, x, y);
		} else {
			if(abs(cw->mouseX - x) < 5 && abs(cw->mouseY - y) < 5) {
				// Do nothing...
			} else {
				cw->mouseDragging = 1;
				hotlist_mouse_action(BROWSER_MOUSE_DRAG_1|BROWSER_MOUSE_DRAG_ON, x, y);
			}
		}
	} else {
		printf("coreMouseAction(%s)\n", args[0]);
	}
}

void bookmarksMenuSimpleCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	if((int)client_data == 0) {
		// Add new folder
		hotlist_add_folder(NULL, false, 0);
	} else if ((int)client_data == 1) {
		// Edit bookmark
		hotlist_edit_selection();
	} else if ((int)client_data == 2) {
		// Delete bookmark
		hotlist_keypress(NS_KEY_DELETE_LEFT);
	}
}

/**
 * Setup a dialog window for a corewindow
 */
void motif_corewindow_init(struct motif_corewindow *cw) {
	Arg	args[32];
	int n;
	Widget dialog, layout;
	String			translations = "<Btn1Down>: coreMouseAction(down) ManagerGadgetArm()\n<Btn1Up>: coreMouseAction(up) ManagerGadgetActivate()\n<Btn1Motion>: coreMouseAction(move) ManagerGadgetButtonMotion()\n<Motion>: coreMouseAction(move)\n<KeyDown>: DrawingAreaInput() ManagerGadgetKeyInput()\n<KeyUp>: DrawingAreaInput()\n<Btn2Down>: coreMouseAction(btn2down)\n<Btn2Up>: coreMouseAction(btn2up)\n<Btn3Down>: coreMouseAction(btn3down)\n<Btn4Down>: coreMouseAction(btn4down)\n<Btn5Down>: coreMouseAction(btn5down)";

	cw->mousePressed = 0;
	cw->mouseDragging = 0;

	n = 0;
	XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++;
	XtSetArg(args[n], XmNcolormap, motifColormap); n++;
	cw->dialog = XmCreateDialogShell(motifWindow, "Bookmarks", args, n);

	n = 0;
	XtSetArg(args[n], XmNwidth, 512); n++;
	XtSetArg(args[n], XmNheight, 512); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	XtSetArg(args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++;
	XtSetArg(args[n], XmNcolormap, motifColormap); n++;
	Widget mainWindow = XmCreateMainWindow(cw->dialog, "bookmarksMainWindow", args, n);
	XtManageChild(mainWindow);

	cw->layout = XtVaCreateWidget("dialogLayout", xmFormWidgetClass, mainWindow, XmNdepth, motifDepth, XmNvisual, motifVisual, XmNcolormap, motifColormap, XmNwidth, 512, XmNheight, 512, NULL);
	XtManageChild(cw->layout);

	n = 0;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNresizable, 1); n++;
	XtSetArg(args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
	XtSetArg(args[n], XmNtranslations, XtParseTranslationTable(translations));  n++;
	XtSetArg(args[n], XmNuserData, cw); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	XtSetArg(args[n], XmNwidth, 512); n++; 
	XtSetArg(args[n], XmNheight, 512); n++; 
	cw->drawingArea = XmCreateDrawingArea(cw->layout, "coreDrawingArea", args, n);
	XtManageChild(cw->drawingArea);

	XtAddCallback(cw->drawingArea, XmNexposeCallback, coreDrawingAreaRedrawCallback, NULL);
	XtAddCallback(cw->drawingArea, XmNinputCallback, coreDrawingAreaInputCallback, NULL);

	XGCValues		gcv;
	gcv.background = BlackPixelOfScreen(XtScreen(cw->drawingArea));
	gcv.foreground = WhitePixelOfScreen(XtScreen(cw->drawingArea));
	cw->gc = XCreateGC(XtDisplay(cw->drawingArea), XtWindow(motifWindow), GCForeground|GCBackground, &gcv);


	switch(cw->windowType) {
	case MOTIF_BOOKMARKS:
		{
			XmString menuButtons[5];
			XmString bookmarks = XmStringCreateLocalized("Bookmarks");
			Widget motifMenubar = XmVaCreateSimpleMenuBar(mainWindow, "cwMenubar", 
				XmVaCASCADEBUTTON, bookmarks, 'B', NULL);
			XtVaSetValues(motifMenubar, XmNvisual, motifVisual, XmNcolormap, motifColormap, NULL);

			XmStringFree(bookmarks);

			menuButtons[0] = XmStringCreateLocalized("Add new folder");
			menuButtons[1] = XmStringCreateLocalized("Edit bookmark");
			menuButtons[2] = XmStringCreateLocalized("Delete bookmark");
			menuButtons[3] = XmStringCreateLocalized("Ctrl+N");
			menuButtons[4] = XmStringCreateLocalized("Ctrl+E");

			XmVaCreateSimplePulldownMenu(motifMenubar, "cwBookmarksMenu", 
				0, bookmarksMenuSimpleCallback,
				XmVaPUSHBUTTON, menuButtons[0], 'A', "Ctrl<Key>N", menuButtons[3],
				XmVaPUSHBUTTON, menuButtons[1], 'E', "Ctrl<Key>E", menuButtons[4],
				XmVaPUSHBUTTON, menuButtons[2], 'D', NULL, NULL,
				NULL);

			XmStringFree(menuButtons[0]);
			XmStringFree(menuButtons[1]);
			XmStringFree(menuButtons[2]);
			XmStringFree(menuButtons[3]);
			XmStringFree(menuButtons[4]);

			XtManageChild(motifMenubar);
			XtVaSetValues(mainWindow, XmNmenuBar, motifMenubar, XmNworkWindow, cw->layout, 	NULL);
			break;
		}
	}
}

/* exported interface documented in fb/corewindow.h */
nserror motif_corewindow_fini(struct motif_corewindow *fb_cw)
{
	return NSERROR_OK;
}

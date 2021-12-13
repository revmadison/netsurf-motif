/*
 * Copyright 2008, 2014 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http:f//www.netsurf-browser.org/
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

#include <stdint.h>
#include <limits.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <nsutils/time.h>

#include "utils/utils.h"
#include "utils/nsoption.h"
#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"
#include "desktop/browser_history.h"
#include "netsurf/plotters.h"
#include "netsurf/window.h"
#include "netsurf/misc.h"
#include "netsurf/netsurf.h"
#include "netsurf/cookie_db.h"
#include "content/fetch.h"

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
#include <X11/cursorfont.h>
#include <X11/xpm.h>
#include <sys/time.h>

#include "motif/icons/icon_back.xpm"
#include "motif/icons/icon_back_d.xpm"
#include "motif/icons/icon_forward.xpm"
#include "motif/icons/icon_forward_d.xpm"
#include "motif/icons/icon_stop.xpm"
#include "motif/icons/icon_stop_d.xpm"

#include "motif/gui.h"
#include "motif/drawing.h"
#include "motif/schedule.h"
#include "motif/findfile.h"
#include "motif/font.h"
#include "motif/clipboard.h"
#include "motif/fetch.h"
#include "motif/bitmap.h"
#include "motif/local_history.h"
#include "motif/download.h"
#include "motif/corewindow.h"

#ifdef NSMOTIF_USE_GL
#include "/usr/include/GL/glxtokens.h"
#include <GL/glx.h>
#endif

#define MAX_MENU_ITEMS 64

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

String fallbacks[] = {
        "*sgiMode: true",
        "*useSchemes: all",
		"*fontList: -*-helvetica-bold-o-normal-*-12-*-*-*-*-*-iso8859-*",
		"*XmLabel*fontList: -*-helvetica-bold-r-normal-*-12-*-*-*-*-*-iso8859-*",
		"*XmTextField*fontList: -*-helvetica-medium-r-normal-*-12-*-*-*-*-*-iso8859-*",
        NULL
};

static XtAppContext app;
static Widget mainLayout;
static Widget contentHolder;
Widget motifWindow;
Display *motifDisplay = NULL;
Visual *motifVisual = NULL;
#ifdef NSMOTIF_USE_GL
GLXContext motifGLContext;
#endif
int motifDepth;
int motifDoubleBuffered = 0;
Colormap motifColormap;

Widget motifMenubar;

Widget tabHolder;
Widget newTabButton;
static int numTabs = 0;

static long color808080 = -1;
static long colorc4c4c4 = -1;

Widget motifStatusLabel;

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;

struct gui_window *currentTab = NULL;

Cursor handCursor = None;
Cursor textCursor = None;
Cursor waitCursor = None;
Cursor moveCursor = None;

static const char *feurl;

static Pixmap leftArrowPixmap = None;
static Pixmap leftArrowDisabled = None;
static Pixmap rightArrowPixmap = None;
static Pixmap rightArrowDisabled = None;
static Pixmap stopButtonPixmap = None;
static Pixmap stopButtonDisabled = None;
static int triedLoadingPixmaps = 0;

static struct gui_drag {
	enum state {
		GUI_DRAG_NONE,
		GUI_DRAG_PRESSED,
		GUI_DRAG_DRAG
	} state;
	int button;
	int x;
	int y;
	bool grabbed_pointer;
} gui_drag;

void drawingAreaRedrawCallback(Widget widget, XtPointer client_data, XtPointer call_data);

void setupBookmarksMenu();
static void setupTabs();
void windowClosedCallback(Widget widget, XtPointer client_data, XtPointer call_data);

static void gui_window_remove_from_window_list(struct gui_window *gw);

void scheduled_browser_destroy(void *bw) {
	if(bw) {
		browser_window_destroy((struct browser_window *)bw);
	}
}

/**
 * Cause an abnormal program termination.
 *
 * \note This never returns and is intended to terminate without any cleanup.
 *
 * \param error The message to display to the user.
 */
static void die(const char *error)
{
	fprintf(stderr, "%s\n", error);
	exit(1);
}


/**
 * Warn the user of an event.
 *
 * \param[in] warning A warning looked up in the message translation table
 * \param[in] detail Additional text to be displayed or NULL.
 * \return NSERROR_OK on success or error code if there was a
 *           faliure displaying the message to the user.
 */
static nserror fb_warn_user(const char *warning, const char *detail)
{
	NSLOG(netsurf, INFO, "%s %s", warning, detail);
	return NSERROR_OK;
}


/* queue a window scroll */
static void
widget_scroll_y(struct gui_window *gw, int y, bool abs)
{
	int scrollY;
	int maximum, slider;

	XtVaGetValues(gw->vertScrollBar, XmNmaximum, &maximum, XmNsliderSize, &slider, NULL);
	if(abs) {
		scrollY = y;
	} else {
		XtVaGetValues(gw->vertScrollBar, XmNvalue, &scrollY, NULL);
		scrollY += y;
	}

	if(scrollY >= (maximum-slider)) {
		scrollY = maximum-slider-1;
	}
	if(scrollY < 0) {
		scrollY = 0;
	}

	XtVaSetValues(gw->vertScrollBar, XmNvalue, scrollY, NULL);

	drawingAreaRedrawCallback(gw->drawingArea, NULL, NULL);
}

/* queue a window scroll */
static void
widget_scroll_x(struct gui_window *gw, int x, bool abs)
{
	int scrollX;
	int maximum, slider;

	XtVaGetValues(gw->horizScrollBar, XmNmaximum, &maximum, XmNsliderSize, &slider, NULL);
	if(abs) {
		scrollX = x;
	} else {
		XtVaGetValues(gw->horizScrollBar, XmNvalue, &scrollX, NULL);
		scrollX += x;
	}

	if(scrollX >= (maximum-slider)) {
		scrollX = maximum-slider-1;
	}
	if(scrollX < 0) {
		scrollX = 0;
	}

	XtVaSetValues(gw->horizScrollBar, XmNvalue, scrollX, NULL);

	drawingAreaRedrawCallback(gw->drawingArea, NULL, NULL);
}

void scrollbarChangedCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	struct gui_window *gw;
	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Widget with null userData!\n");
		return;
	}

	drawingAreaRedrawCallback(gw->drawingArea, NULL, NULL);
}

void drawingAreaResizeCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	struct gui_window *gw;
	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Widget with null userData!\n");
		return;
	}

#ifdef NSMOTIF_USE_GL
	Dimension width, height;
	XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height, NULL);

	gw->winWidth = (int)width;
	gw->winHeight = (int)height;

	{
		float orthoMtx[16] = {
			2.0f / width, 0.0f, 0.0f, 0.0f,
			0.0f, 2.0f / -height, 0.0f, 0.0f,
			0.0f, 0.0f, -2.0f / 999.0f, 0.0f,
			-1.0f, 1.0f, -1001.0f / 999.0f, 1.0f
		};
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glLoadMatrixf(&orthoMtx[0]);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glViewport(0, 0, width, height);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_SCISSOR_TEST);
	}
		
#endif
	if(gw->bw)
	{
		browser_window_schedule_reformat(gw->bw);
	}
}

void newTabButtonCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	nsurl *url;
	nserror ret;
	struct browser_window *bw;

	ret = nsurl_create(feurl, &url);
	if (ret == NSERROR_OK) {
		ret = browser_window_create(BW_CREATE_HISTORY|BW_CREATE_TAB,
			url, NULL, NULL, &bw);
		nsurl_unref(url);
	}

}

void activateTab(struct gui_window *gw) {
	if(!gw) {
		printf("Activating a null tab?!?\n");
		return;
	}

	if(currentTab) {
		XtVaSetValues(currentTab->tab, XmNbackground, color808080, NULL);
		XtUnmapWidget(currentTab->layout);
	}
	XtVaSetValues(gw->tab, XmNbackground, colorc4c4c4, NULL);
	XtMapWidget(gw->layout);
	XmProcessTraversal(gw->drawingArea, XmTRAVERSE_CURRENT);
	XmProcessTraversal(gw->drawingArea, XmTRAVERSE_CURRENT);

#ifdef NSMOTIF_USE_GL
	glXMakeCurrent(motifDisplay, XtWindow(gw->drawingArea), motifGLContext);
	drawingAreaResizeCallback(gw->drawingArea, NULL, NULL);
#endif

	XtVaSetValues(motifWindow, XmNtitle, gw->tabTitle, NULL);
	browser_window_schedule_reformat(gw->bw);

	currentTab = gw;
}

void switchToTabButtonCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	struct gui_window *gw;
	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Widget with null userData!\n");
		return;
	}

	if(gw != currentTab) {
		activateTab(gw);
	} else {
		XmProcessTraversal(gw->drawingArea, XmTRAVERSE_CURRENT);
		XmProcessTraversal(gw->drawingArea, XmTRAVERSE_CURRENT);
	}
}
void tabExposeCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	struct gui_window *gw;
	Dimension width, height;
	XtVaGetValues(widget, XmNuserData, &gw, XmNwidth, &width, XmNheight, &height, NULL);
	if(!gw) {
		return;
	}
	XSetForeground(motifDisplay, gw->gc, gw==currentTab?colorc4c4c4:color808080);
	XSetFillStyle(motifDisplay, gw->gc, FillSolid);
	XFillRectangle(motifDisplay, XtWindow(widget), gw->gc, 0, 0, width, height);

}

static void motif_update_back_forward(struct gui_window *gw)
{
	if(leftArrowPixmap != None) {
		struct browser_window *bw = gw->bw;
		XtVaSetValues(gw->backButton, XmNlabelPixmap, (browser_window_back_available(bw)) ? leftArrowPixmap : leftArrowDisabled, NULL);
		XtVaSetValues(gw->forwardButton, XmNlabelPixmap, (browser_window_forward_available(bw)) ? rightArrowPixmap : rightArrowDisabled, NULL);
	}
}


void motifBackButtonCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	struct gui_window *gw;
	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		return;
	}

	if (browser_window_back_available(gw->bw))
		browser_window_history_back(gw->bw, false);

	motif_update_back_forward(gw);

}
void motifForwardButtonCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	struct gui_window *gw;
	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		return;
	}

	if (browser_window_forward_available(gw->bw))
		browser_window_history_forward(gw->bw, false);

	motif_update_back_forward(gw);
}
void motifStopReloadButtonCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	struct gui_window *gw;
	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Widget with null userData!\n");
		return;
	}

	browser_window_stop(gw->bw);
}

void drawingAreaRedrawCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	Dimension width, height;
	int scrollX, scrollY;
	int x;
	int y;
	struct rect clip;

	struct gui_window *gw;
	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Widget with null userData!\n");
		return;
	}
	if(gw != currentTab) {
		//printf("redraw callback for background tab, ignoring\n");
		return;
	}

#ifdef NSMOTIF_USE_GL
	glXMakeCurrent(motifDisplay, XtWindow(gw->drawingArea), motifGLContext);
#endif

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
#ifdef NSMOTIF_USE_GL
		.plot = &motifgl_plotters,
#else
		.plot = &fb_plotters,
#endif
		.priv = gw
	};
	XEvent *event = NULL;
	XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *)call_data;

	if(cbs) {
		event = cbs->event;
	}

	XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height, NULL);
	XtVaGetValues(gw->vertScrollBar, XmNvalue, &scrollY, NULL);
	XtVaGetValues(gw->horizScrollBar, XmNvalue, &scrollX, NULL);

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

	browser_window_redraw(gw->bw,
			-scrollX,
			-scrollY,
			&clip, &ctx);

	if(gw->caretEnabled) {
		plot_style_t style;
		struct rect line;

		style.stroke_type = PLOT_OP_TYPE_SOLID;
		style.stroke_colour = 0xffff00ff;
		style.stroke_width = plot_style_int_to_fixed(2);

		line.x0 = gw->caretX-scrollX;
		line.y0 = gw->caretY-scrollY;
		line.x1 = gw->caretX-scrollX;
		line.y1 = gw->caretY+gw->caretH-scrollY;

#ifdef NSMOTIF_USE_GL
		motifgl_plotters
#else
		fb_plotters
#endif
			.line(&ctx, &style, &line);
	}

#ifdef NSMOTIF_USE_GL
	if(motifDoubleBuffered) {
		glXSwapBuffers(motifDisplay, XtWindow(gw->drawingArea));
	}
#endif
}

void drawingAreaInputCallback(Widget widget, XtPointer client_data, XtPointer call_data)
{
	XEvent *event = NULL;
	XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *)call_data;
	event = cbs->event;

	if(!event)
	{
		return;
	}
	struct gui_window *gw;

	Dimension curWidth, curHeight;
	XtVaGetValues(widget, XmNwidth, &curWidth, XmNheight, &curHeight, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Drawing widget with null userData!\n");
		return;
	}

	if(event->xany.type == KeyPress) {
		KeySym keySym;// = XLookupKeysym((XKeyEvent *)event, 0);
		char buffer[8];
		XLookupString((XKeyEvent *)event, buffer, 7, &keySym, NULL);

		switch (keySym) {

		case XK_Delete:
			browser_window_key_press(gw->bw, NS_KEY_DELETE_RIGHT);
			break;

		case XK_BackSpace:
			browser_window_key_press(gw->bw, NS_KEY_DELETE_LEFT);
			break;

		case XK_Page_Up:
			if (browser_window_key_press(gw->bw,
					NS_KEY_PAGE_UP) == false) {
				widget_scroll_y(gw, -curHeight, false);
			}
			break;

		case XK_Page_Down:
			if (browser_window_key_press(gw->bw,
					NS_KEY_PAGE_DOWN) == false) {
				widget_scroll_y(gw, curHeight, false);
			}
			break;

		case XK_Right:
			if (event->xkey.state & ControlMask) {
				/* CTRL held */
				if (browser_window_key_press(gw->bw,
						NS_KEY_LINE_END) == false) {
					widget_scroll_x(gw, INT_MAX, true);
				}
			} else if (event->xkey.state & ShiftMask) {
				/* SHIFT held */
				if (browser_window_key_press(gw->bw,
						NS_KEY_WORD_RIGHT) == false) {
					widget_scroll_x(gw, curWidth, false);
				}
			} else {
				/* no modifier */
				if (browser_window_key_press(gw->bw,
						NS_KEY_RIGHT) == false) {
					widget_scroll_x(gw, 100, false);
				}
			}
			break;

		case XK_Left:
			if (event->xkey.state & ControlMask) {
				/* CTRL held */
				if (browser_window_key_press(gw->bw,
						NS_KEY_LINE_START) == false) {
					widget_scroll_x(gw, 0, true);
				}

			} else if (event->xkey.state & ShiftMask) {
				/* SHIFT held */
				if (browser_window_key_press(gw->bw,
						NS_KEY_WORD_LEFT) == false) {
					widget_scroll_x(gw, -curWidth, false);
				}

			} else {
				/* no modifier */
				if (browser_window_key_press(gw->bw,
						NS_KEY_LEFT) == false) {
					widget_scroll_x(gw, -100, false);
				}
			}
			break;

		case XK_Up:
			if (browser_window_key_press(gw->bw,
					NS_KEY_UP) == false) {
				widget_scroll_y(gw, -100, false);
			}
			break;

		case XK_Down:
			if (browser_window_key_press(gw->bw,
					NS_KEY_DOWN) == false) {
				widget_scroll_y(gw, 100, false);
			}
			break;
		case XK_Return:
			browser_window_key_press(gw->bw, 13);	// Send the return key in
			break;

		case XK_Tab:
			browser_window_key_press(gw->bw, 9);	// Send the tab key in
			break;

		default:
			if(buffer[0] >= 32 && buffer[0] <= 255) {
				browser_window_key_press(gw->bw, buffer[0]);
			}
			break;
		}
	} else if(event->xany.type == KeyRelease) {
	}
}

static void cleanup_browsers()
{
	struct gui_window *gw = window_list;
	while(gw) {
		if(gw->bw) {
			browser_window_destroy(gw->bw);
		}
		gw = gw->next;
	}
}

static bool
process_cmdline(int argc, char** argv)
{
	int opt;
	int option_index;
	NSLOG(netsurf, INFO, "argc %d, argv %p", argc, argv);

	if ((nsoption_charp(homepage_url) != NULL) && 
	    (nsoption_charp(homepage_url)[0] != '\0')) {
		feurl = nsoption_charp(homepage_url);
	} else {
		feurl = NETSURF_HOMEPAGE;
	}

	int lastArg = argc-1;
	if (lastArg > 0) {
		feurl = argv[optind];
	}

	return true;
}

/**
 * Set option defaults for framebuffer frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
	/* Set defaults for absent option strings */
	const char *home = getenv("HOME");
	if(home) {
		char *buffer = (char *)malloc(strlen(home)+strlen("/.netsurf/Cookies")+1);
		strcpy(buffer, home);
		strcat(buffer, "/.netsurf");
		mkdir(buffer, 00700);
		strcat(buffer, "/Cookies");
		nsoption_setnull_charp(cookie_file, strdup(buffer));
		nsoption_setnull_charp(cookie_jar, strdup(buffer));
		nsoption_setnull_charp(downloads_directory, strdup(home));

		strcpy(buffer, home);
		strcat(buffer, "/.netsurf/URLs");
		nsoption_setnull_charp(url_file, strdup(buffer));

		strcpy(buffer, home);
		strcat(buffer, "/.netsurf/Hotlist");
		nsoption_setnull_charp(hotlist_file, strdup(buffer));

		free(buffer);
	}

	if (nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL || 
		nsoption_charp(downloads_directory) == NULL || 
		nsoption_charp(url_file) == NULL || 
		nsoption_charp(hotlist_file) == NULL) {
		NSLOG(netsurf, INFO, "Failed initialising default paths");
		return NSERROR_BAD_PARAMETER;
	}

	/* set system colours for framebuffer ui */
	nsoption_set_colour(sys_colour_ActiveBorder, 0x00000000);
	nsoption_set_colour(sys_colour_ActiveCaption, 0x00ddddcc);
	nsoption_set_colour(sys_colour_AppWorkspace, 0x00eeeeee);
	nsoption_set_colour(sys_colour_Background, 0x00aa0000);
	nsoption_set_colour(sys_colour_ButtonFace, 0x00dddddd);
	nsoption_set_colour(sys_colour_ButtonHighlight, 0x00cccccc);
	nsoption_set_colour(sys_colour_ButtonShadow, 0x00bbbbbb);
	nsoption_set_colour(sys_colour_ButtonText, 0x00000000);
	nsoption_set_colour(sys_colour_CaptionText, 0x00000000);
	nsoption_set_colour(sys_colour_GrayText, 0x00777777);
	nsoption_set_colour(sys_colour_Highlight, 0x00ee0000);
	nsoption_set_colour(sys_colour_HighlightText, 0x00000000);
	nsoption_set_colour(sys_colour_InactiveBorder, 0x00000000);
	nsoption_set_colour(sys_colour_InactiveCaption, 0x00ffffff);
	nsoption_set_colour(sys_colour_InactiveCaptionText, 0x00cccccc);
	nsoption_set_colour(sys_colour_InfoBackground, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_InfoText, 0x00000000);
	nsoption_set_colour(sys_colour_Menu, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_MenuText, 0x00000000);
	nsoption_set_colour(sys_colour_Scrollbar, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_ThreeDDarkShadow, 0x00555555);
	nsoption_set_colour(sys_colour_ThreeDFace, 0x00dddddd);
	nsoption_set_colour(sys_colour_ThreeDHighlight, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_ThreeDLightShadow, 0x00999999);
	nsoption_set_colour(sys_colour_ThreeDShadow, 0x00777777);
	nsoption_set_colour(sys_colour_Window, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_WindowFrame, 0x00000000);
	nsoption_set_colour(sys_colour_WindowText, 0x00000000);

	return NSERROR_OK;
}


/**
 * Ensures output logging stream is correctly configured
 */
static bool nslog_stream_configure(FILE *fptr)
{
        /* set log stream to be non-buffering */
	setbuf(fptr, NULL);

	return true;
}

static int alreadyRanGuiQuit = 0;

static void gui_quit(void)
{
	if(alreadyRanGuiQuit) {
		return;
	}
	alreadyRanGuiQuit = 1;
	NSLOG(netsurf, INFO, "gui_quit");
//printf("gui_quit\n");
	urldb_save_cookies(nsoption_charp(cookie_jar));
	urldb_save(nsoption_charp(url_file));
	hotlist_fini();
}

/* called back when click in browser window */
static int
fb_browser_window_click(void *widget, void *cbi)
{
#if 0 //MOTIF? Need to move the rest of the non-mouse-1 logic into our input handling
	struct gui_window *gw = cbi->context;
	struct browser_widget_s *bwidget = fbtk_get_userpw(widget);
	browser_mouse_state mouse;
	int x = cbi->x + bwidget->scrollx;
	int y = cbi->y + bwidget->scrolly;
	uint64_t time_now;
	static struct {
		enum { CLICK_SINGLE, CLICK_DOUBLE, CLICK_TRIPLE } type;
		uint64_t time;
	} last_click;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN &&
	    cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	NSLOG(netsurf, DEEPDEBUG, "browser window clicked at %d,%d",
			cbi->x, cbi->y);

	switch (cbi->event->type) {
	case NSFB_EVENT_KEY_DOWN:
		switch (cbi->event->value.keycode) {
		case NSFB_KEY_MOUSE_1:
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_PRESS_1, x, y);
			gui_drag.state = GUI_DRAG_PRESSED;
			gui_drag.button = 1;
			gui_drag.x = x;
			gui_drag.y = y;
			break;

		case NSFB_KEY_MOUSE_3:
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_PRESS_2, x, y);
			gui_drag.state = GUI_DRAG_PRESSED;
			gui_drag.button = 2;
			gui_drag.x = x;
			gui_drag.y = y;
			break;

		case NSFB_KEY_MOUSE_4:
			/* scroll up */
			if (browser_window_scroll_at_point(gw->bw,
							   x, y,
							   0, -100) == false)
				widget_scroll_y(gw, -100, false);
			break;

		case NSFB_KEY_MOUSE_5:
			/* scroll down */
			if (browser_window_scroll_at_point(gw->bw,
							   x, y,
							   0, 100) == false)
				widget_scroll_y(gw, 100, false);
			break;

		default:
			break;

		}

		break;
	case NSFB_EVENT_KEY_UP:

		mouse = 0;
		nsu_getmonotonic_ms(&time_now);

		switch (cbi->event->value.keycode) {
		case NSFB_KEY_MOUSE_1:
			if (gui_drag.state == GUI_DRAG_DRAG) {
				/* End of a drag, rather than click */

				if (gui_drag.grabbed_pointer) {
					/* need to ungrab pointer */
					fbtk_tgrab_pointer(widget);
					gui_drag.grabbed_pointer = false;
				}

				gui_drag.state = GUI_DRAG_NONE;

				/* Tell core */
				browser_window_mouse_track(gw->bw, 0, x, y);
				break;
			}
			/* This is a click;
			 * clear PRESSED state and pass to core */
			gui_drag.state = GUI_DRAG_NONE;
			mouse = BROWSER_MOUSE_CLICK_1;
			break;

		case NSFB_KEY_MOUSE_3:
			if (gui_drag.state == GUI_DRAG_DRAG) {
				/* End of a drag, rather than click */
				gui_drag.state = GUI_DRAG_NONE;

				if (gui_drag.grabbed_pointer) {
					/* need to ungrab pointer */
					fbtk_tgrab_pointer(widget);
					gui_drag.grabbed_pointer = false;
				}

				/* Tell core */
				browser_window_mouse_track(gw->bw, 0, x, y);
				break;
			}
			/* This is a click;
			 * clear PRESSED state and pass to core */
			gui_drag.state = GUI_DRAG_NONE;
			mouse = BROWSER_MOUSE_CLICK_2;
			break;

		default:
			break;

		}

		/* Determine if it's a double or triple click, allowing
		 * 0.5 seconds (500ms) between clicks
		 */
		if ((time_now < (last_click.time + 500)) &&
		    (cbi->event->value.keycode != NSFB_KEY_MOUSE_4) &&
		    (cbi->event->value.keycode != NSFB_KEY_MOUSE_5)) {
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

		if (mouse) {
			browser_window_mouse_click(gw->bw, mouse, x, y);
		}

		last_click.time = time_now;

		break;
	default:
		break;

	}
#endif
	return 1;
}


/* reload icon click routine */
static int
fb_reload_click(void *widget, void *cbi)
{
/* MOTIF?
	struct browser_window *bw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	browser_window_reload(bw, true);
*/
	return 1;
}

void urlFieldActivatedCallback(Widget textField, XtPointer client_data, XtPointer call_data)
{
	char *text = XmTextFieldGetString(textField);
	struct gui_window *gw;
	XtVaGetValues(textField, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Widget with null userData!\n");
		return;
	}

	if (!text || !*text) {
		XtFree(text); /* XtFree() checks for NULL */
		return;
	}

	struct browser_window *bw = gw->bw;
	nsurl *url;
	nserror error;

	error = nsurl_create(text, &url);
	if (error != NSERROR_OK) {
		fb_warn_user("Errorcode:", messages_get_errorcode(error));
	} else {
		browser_window_navigate(bw, url, NULL, BW_NAVIGATE_HISTORY,
				NULL, NULL, NULL);
		nsurl_unref(url);
	}

	XtFree(text);

	XmProcessTraversal(gw->drawingArea, XmTRAVERSE_CURRENT);
	XmProcessTraversal(gw->drawingArea, XmTRAVERSE_CURRENT);

}

static int
fb_localhistory_btn_clik(void *widget, void *cbi)
{
/* MOTIF?
	struct gui_window *gw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	fb_local_history_present(fbtk, gw->bw);
*/
	return 0;
}

#if 0 // MOTIF?
/** Create a toolbar window and populate it with buttons. 
 *
 * The toolbar layout uses a character to define buttons type and position:
 * b - back
 * l - local history
 * f - forward
 * s - stop 
 * r - refresh
 * u - url bar expands to fit remaining space
 * t - throbber/activity indicator
 * c - close the current window
 *
 * The default layout is "blfsrut" there should be no more than a
 * single url bar entry or behaviour will be undefined.
 *
 * @param gw Parent window 
 * @param toolbar_height The height in pixels of the toolbar
 * @param padding The padding in pixels round each element of the toolbar
 * @param frame_col Frame colour.
 * @param toolbar_layout A string defining which buttons and controls
 *                       should be added to the toolbar. May be empty
 *                       string to disable the bar..
 * 
 */
static void *
create_toolbar(struct gui_window *gw, 
	       int toolbar_height, 
	       int padding, 
	       colour frame_col,
	       const char *toolbar_layout)
{
	void *toolbar;
	void *widget;

	int xpos; /* The position of the next widget. */
	int xlhs = 0; /* extent of the left hand side widgets */
	int xdir = 1; /* the direction of movement + or - 1 */
	const char *itmtype; /* type of the next item */

	if (toolbar_layout == NULL) {
		toolbar_layout = NSFB_TOOLBAR_DEFAULT_LAYOUT;
	}

	NSLOG(netsurf, INFO, "Using toolbar layout %s", toolbar_layout);

	itmtype = toolbar_layout;

	/* check for the toolbar being disabled */
	if ((*itmtype == 0) || (*itmtype == 'q')) {
		return NULL;
	}

	toolbar = fbtk_create_window(gw->window, 0, 0, 0, 
				     toolbar_height, 
				     frame_col);

	if (toolbar == NULL) {
		return NULL;
	}

	xpos = padding;

	/* loop proceeds creating widget on the left hand side until
	 * it runs out of layout or encounters a url bar declaration
	 * wherupon it works backwards from the end of the layout
	 * untill the space left is for the url bar
	 */
	while ((itmtype >= toolbar_layout) && 
	       (*itmtype != 0) && 
	       (xdir !=0)) {

		NSLOG(netsurf, INFO, "toolbar adding %c", *itmtype);


		switch (*itmtype) {

		case 'b': /* back */
			widget = fbtk_create_button(toolbar, 
						    (xdir == 1) ? xpos : 
						     xpos - left_arrow.width, 
						    padding, 
						    left_arrow.width, 
						    -padding, 
						    frame_col, 
						    &left_arrow, 
						    fb_leftarrow_click, 
						    gw);
			gw->back = widget; /* keep reference */
			break;

		case 'l': /* local history */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1) ? xpos : 
						     xpos - history_image.width,
						    padding,
						    history_image.width,
						    -padding,
						    frame_col,
						    &history_image,
						    fb_localhistory_btn_clik,
						    gw);
			gw->history = widget;
			break;

		case 'f': /* forward */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - right_arrow.width,
						    padding,
						    right_arrow.width,
						    -padding,
						    frame_col,
						    &right_arrow,
						    fb_rightarrow_click,
						    gw);
			gw->forward = widget;
			break;

		case 'c': /* close the current window */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - stop_image_g.width,
						    padding,
						    stop_image_g.width,
						    -padding,
						    frame_col,
						    &stop_image_g,
						    fb_close_click,
						    gw->bw);
			gw->close = widget;
			break;

		case 's': /* stop  */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - stop_image.width,
						    padding,
						    stop_image.width,
						    -padding,
						    frame_col,
						    &stop_image,
						    fb_stop_click,
						    gw->bw);
			gw->stop = widget;
			break;

		case 'r': /* reload */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - reload.width,
						    padding,
						    reload.width,
						    -padding,
						    frame_col,
						    &reload,
						    fb_reload_click,
						    gw->bw);
			gw->reload = widget;
			break;

		case 't': /* throbber/activity indicator */
			widget = fbtk_create_bitmap(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - throbber0.width,
						    padding,
						    throbber0.width,
						    -padding,
						    frame_col, 
						    &throbber0);
			gw->throbber = widget;
			break;


		case 'u': /* url bar*/
			if (xdir == -1) {
				/* met the u going backwards add url
				 * now we know available extent 
				 */ 

				widget = fbtk_create_writable_text(toolbar,
						   xlhs,
						   padding,
						   xpos - xlhs,
						   -padding,
						   FB_COLOUR_WHITE,
						   FB_COLOUR_BLACK,
						   true,
						   fb_url_enter,
						   gw->bw);

				fbtk_set_handler(widget, 
						 FBTK_CBT_POINTERENTER, 
						 fb_url_move, gw->bw);

				gw->url = widget; /* keep reference */

				/* toolbar is complete */
				xdir = 0;
				break;
			}
			/* met url going forwards, note position and
			 * reverse direction 
			 */
			itmtype = toolbar_layout + strlen(toolbar_layout);
			xdir = -1;
			xlhs = xpos;
			xpos = (2 * fbtk_get_width(toolbar));
			widget = toolbar;
			break;

		default:
			widget = NULL;
			xdir = 0;
			NSLOG(netsurf, INFO,
			      "Unknown element %c in toolbar layout",
			      *itmtype);
		        break;

		}

		if (widget != NULL) {
			xpos += (xdir * (fbtk_get_width(widget) + padding));
		}

		NSLOG(netsurf, INFO, "xpos is %d", xpos);

		itmtype += xdir;
	}

	fbtk_set_mapping(toolbar, true);
	return toolbar;
}
#endif


static void
create_normal_browser_window(struct gui_window *gw)
{
	int statusbar_width = 0;

//printf("create_normal_browser_window\n");

	NSLOG(netsurf, INFO, "Normal window");


	Widget layout;
	Widget drawingArea;
	Widget vertScrollBar;
	Widget horizScrollBar;
	Widget backButton;
	Widget forwardButton;
	Widget stopButton;
	Widget urlTextField;

	Arg         	args[32];
	int         	n = 0;
	String			translations = "<Btn1Down>: mouseAction(down) ManagerGadgetArm()\n<Btn1Up>: mouseAction(up) ManagerGadgetActivate()\n<Btn1Motion>: mouseAction(move) ManagerGadgetButtonMotion()\n<Motion>: mouseAction(move)\n<KeyDown>: DrawingAreaInput() ManagerGadgetKeyInput()\n<KeyUp>: DrawingAreaInput()\n<Btn2Down>: mouseAction(btn2down)\n<Btn2Up>: mouseAction(btn2up)\n<Btn3Down>: mouseAction(btn3down)\n<Btn4Down>: mouseAction(btn4down)\n<Btn5Down>: mouseAction(btn5down)";
	XtActionsRec	actions;
	XmString buttonLabel;

	if(leftArrowPixmap == None && !triedLoadingPixmaps) {
		triedLoadingPixmaps = 1;

		XpmAttributes attribs;
		attribs.valuemask = XpmVisual | XpmColormap;
		attribs.visual = motifVisual;
		attribs.colormap = motifColormap;

		XpmCreatePixmapFromData(XtDisplay(contentHolder), XtWindow(contentHolder), icon_back, &leftArrowPixmap, NULL, &attribs);
		XpmCreatePixmapFromData(XtDisplay(contentHolder), XtWindow(contentHolder), icon_back_d, &leftArrowDisabled, NULL, &attribs);
		XpmCreatePixmapFromData(XtDisplay(contentHolder), XtWindow(contentHolder), icon_forward, &rightArrowPixmap, NULL, &attribs);
		XpmCreatePixmapFromData(XtDisplay(contentHolder), XtWindow(contentHolder), icon_forward_d, &rightArrowDisabled, NULL, &attribs);
		XpmCreatePixmapFromData(XtDisplay(contentHolder), XtWindow(contentHolder), icon_stop, &stopButtonPixmap, NULL, &attribs);
		XpmCreatePixmapFromData(XtDisplay(contentHolder), XtWindow(contentHolder), icon_stop_d, &stopButtonDisabled, NULL, &attribs);
	}

	gw->layout = XtVaCreateWidget("windowLayout", xmFormWidgetClass, contentHolder, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_FORM, XmNdepth, motifDepth, XmNvisual, motifVisual, XmNcolormap, motifColormap, NULL);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNwidth, 30); n++;
	XtSetArg(args[n], XmNheight, 34); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	if(leftArrowPixmap != None) {
		XtSetArg(args[n], XmNlabelType, XmPIXMAP); n++; 
		XtSetArg(args[n], XmNlabelPixmap, leftArrowPixmap); n++;
	}
	XtSetArg(args[n], XmNuserData, gw); n++;
	gw->backButton = (Widget)XmCreatePushButton(gw->layout, "<-", args, n);
	XtManageChild(gw->backButton);
	XtAddCallback(gw->backButton, XmNactivateCallback, motifBackButtonCallback, NULL);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNwidth, 30); n++;
	XtSetArg(args[n], XmNheight, 34); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, gw->backButton); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	if(rightArrowPixmap != None) {
		XtSetArg(args[n], XmNlabelType, XmPIXMAP); n++; 
		XtSetArg(args[n], XmNlabelPixmap, rightArrowPixmap); n++;
	}
	XtSetArg(args[n], XmNuserData, gw); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	gw->forwardButton = (Widget)XmCreatePushButton(gw->layout, "->", args, n);
	XtManageChild(gw->forwardButton);
	XtAddCallback(gw->forwardButton, XmNactivateCallback, motifForwardButtonCallback, NULL);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNwidth, 30); n++;
	XtSetArg(args[n], XmNheight, 34); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, gw->forwardButton); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	if(stopButtonPixmap != None) {
		XtSetArg(args[n], XmNlabelType, XmPIXMAP); n++; 
		XtSetArg(args[n], XmNlabelPixmap, stopButtonPixmap); n++;
	}
	XtSetArg(args[n], XmNuserData, gw); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	gw->stopButton = (Widget)XmCreatePushButton(gw->layout, "Rld", args, n);
	XtManageChild(gw->stopButton);
	XtAddCallback(gw->stopButton, XmNactivateCallback, motifStopReloadButtonCallback, NULL);

	gw->urlTextField = XtVaCreateManagedWidget("motifURLTextField", xmTextFieldWidgetClass, gw->layout, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, gw->stopButton, XmNrightAttachment, XmATTACH_FORM, XmNtopAttachment, XmATTACH_FORM, XmNuserData, gw, NULL);
	XtAddCallback(gw->urlTextField, XmNactivateCallback, urlFieldActivatedCallback, NULL);

	n = 0;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomOffset, 20); n++;
	XtSetArg(args[n], XmNwidth, 20); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, gw->urlTextField); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg(args[n], XmNincrement, 10); n++;
	XtSetArg(args[n], XmNuserData, gw); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	gw->vertScrollBar = XmCreateScrollBar(gw->layout, "motifVertScrollBar", args, n);
	XtManageChild(gw->vertScrollBar);

	n = 0;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightOffset, 20); n++;
	XtSetArg(args[n], XmNheight, 20); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNincrement, 10); n++;
	XtSetArg(args[n], XmNuserData, gw); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	gw->horizScrollBar = XmCreateScrollBar(gw->layout, "motifHorizScrollBar", args, n);
	XtManageChild(gw->horizScrollBar);


	n = 0;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomOffset, 20); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, gw->urlTextField); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, gw->vertScrollBar); n++;
	XtSetArg(args[n], XmNresizable, 1); n++;
	XtSetArg(args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
	XtSetArg(args[n], XmNtranslations, XtParseTranslationTable(translations));  n++;
	XtSetArg(args[n], XmNuserData, gw); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	gw->drawingArea = XmCreateDrawingArea(gw->layout, "motifDrawingArea", args, n);
	XtManageChild(gw->drawingArea);

	XGCValues		gcv;
	gcv.background = BlackPixelOfScreen(XtScreen(gw->drawingArea));
	gcv.foreground = WhitePixelOfScreen(XtScreen(gw->drawingArea));
	gw->gc = XCreateGC(XtDisplay(gw->drawingArea), XtWindow(motifWindow), GCForeground|GCBackground, &gcv);

	XtAddCallback(gw->drawingArea, XmNexposeCallback, drawingAreaRedrawCallback, NULL);
	XtAddCallback(gw->drawingArea, XmNresizeCallback, drawingAreaResizeCallback, NULL);
	XtAddCallback(gw->drawingArea, XmNinputCallback, drawingAreaInputCallback, NULL);

	XtAddCallback(gw->vertScrollBar, XmNvalueChangedCallback, scrollbarChangedCallback, NULL);
	XtAddCallback(gw->horizScrollBar, XmNvalueChangedCallback, scrollbarChangedCallback, NULL);

	XtManageChild(gw->drawingArea);
	XtManageChild(gw->layout);
	XtUnmapWidget(gw->layout);

	// And now the tab view...
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopOffset, 2); n++;
//	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
//	XtSetArg(args[n], XmNtopAttachment, XmATTACH_OPPOSITE_FORM); n++;
//	XtSetArg(args[n], XmNtopOffset, -28); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg(args[n], XmNleftPosition, 0); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg(args[n], XmNrightPosition, 1); n++;
	XtSetArg(args[n], XmNheight, 32); n++;
	XtSetArg(args[n], XmNresizable, 0); n++; 
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg(args[n], XmNuserData, gw); n++;
	XtSetArg(args[n], XmNrecomputeSize, 0); n++; 
	XtSetArg(args[n], XmNshadowType, XmSHADOW_OUT); n++;
	XtSetArg(args[n], XmNbackground, color808080); n++;
	XtSetArg(args[n], XmNforeground, BlackPixelOfScreen(XtScreen(gw->drawingArea))); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	gw->tab = XmCreateDrawnButton(tabHolder, "(loading...)", args, n);
	XtManageChild(gw->tab);
	XtAddCallback(gw->tab, XmNactivateCallback, switchToTabButtonCallback, NULL);
	//XtAddCallback(gw->tab, XmNexposeCallback, tabExposeCallback, NULL);
}

static void setupTabs() {
	struct gui_window *gw = window_list;
	int i = 0;

	while(gw) {
		XtVaSetValues(gw->tab, XmNleftPosition, i, XmNrightPosition, i+1, NULL);
		i++;
		gw = gw->next;
	}

	numTabs = i;

	XtVaSetValues(tabHolder, XmNfractionBase, MAX(i+1, 4), NULL);
}



static void gui_window_add_to_window_list(struct gui_window *gw)
{
	gw->next = NULL;
	gw->prev = NULL;

	if (window_list == NULL) {
		window_list = gw;
	} else {
		struct gui_window *last = window_list;
		while(last->next) {
			last = last->next;
		}
		last->next = gw;
		gw->prev = last;
	}
	setupTabs();
}

static void gui_window_remove_from_window_list(struct gui_window *gw)
{
	struct gui_window *list;

	for (list = window_list; list != NULL; list = list->next) {
		if (list != gw)
			continue;

		if (list == window_list) {
			window_list = list->next;
			if (window_list != NULL)
				window_list->prev = NULL;
		} else {
			list->prev->next = list->next;
			if (list->next != NULL) {
				list->next->prev = list->prev;
			}
		}
		break;
	}

	if(gw->tab) {
		XtUnmanageChild(gw->tab);
	}

	setupTabs();
}


static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	struct gui_window *gw;

//printf("gui_window_create\n");

	gw = calloc(1, sizeof(struct gui_window));

	if (gw == NULL)
		return NULL;

	gw->deleting = false;

	/* associate the gui window with the underlying browser window
	 */
	gw->bw = bw;
	gw->tabTitle = strdup("");

	create_normal_browser_window(gw);

	/* Add it to the window list */
	gui_window_add_to_window_list(gw);

	return gw;
}

static void
gui_window_destroy(struct gui_window *gw)
{
	gw->deleting = true;

	gui_window_remove_from_window_list(gw);

	XFreeGC(XtDisplay(gw->drawingArea), gw->gc);

	XtUnmanageChild(gw->layout);
	XtUnmanageChild(gw->tab);
	XtDestroyWidget(gw->tab);
	XtDestroyWidget(gw->layout);

	if(gw->tabTitle) {
		free(gw->tabTitle);
	}

	free(gw);
}


#define MAX_INVALIDATED_REGIONS 16
static struct {
	struct gui_window *gw;
	struct rect r[MAX_INVALIDATED_REGIONS];
	int rectCount;
	bool fullWindow;
	bool scheduled;
} ScheduledRedrawData;

void scheduled_redraw(void *arg) {
	Dimension width, height;
	int scrollX, scrollY;
	int x;
	int y;
	struct rect clip;

	struct gui_window *gw = ScheduledRedrawData.gw;
	if(!gw) {
		printf("scheduled_redraw with null gui_window!\n");
		return;
	}
	if(gw != currentTab) {
		return;
	}

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
#ifdef NSMOTIF_USE_GL
		.plot = &motifgl_plotters,
#else
		.plot = &fb_plotters,
#endif
		.priv = gw
	};

	XtVaGetValues(gw->drawingArea, XmNwidth, &width, XmNheight, &height, NULL);
	XtVaGetValues(gw->horizScrollBar, XmNvalue, &scrollX, NULL);
	XtVaGetValues(gw->vertScrollBar, XmNvalue, &scrollY, NULL);

	if(ScheduledRedrawData.fullWindow || motifDoubleBuffered) {
		ScheduledRedrawData.r[0].x0 = 0;
		ScheduledRedrawData.r[0].y0 = 0;
		ScheduledRedrawData.r[0].x1 = width;
		ScheduledRedrawData.r[0].y1 = height;
		ScheduledRedrawData.rectCount = 1;
	}

	for(int i = 0; i < ScheduledRedrawData.rectCount; i++) {
		clip.x0 = ScheduledRedrawData.r[i].x0;
		clip.y0 = ScheduledRedrawData.r[i].y0;
		clip.x1 = ScheduledRedrawData.r[i].x1;
		clip.y1 = ScheduledRedrawData.r[i].y1;

		if((clip.x0 > width) || (clip.x1 < 0) || (clip.y0 > height) || (clip.y1 < 0)) {
			// Off screen!
			continue;
		}

//printf("Scheduled redraw %d,%d -> %d,%d in %dx%d window\n", data->r.x0, data->r.y0, data->r.x1, data->r.y1, (int)width, (int)height);

		browser_window_redraw(gw->bw,
			-scrollX,
			-scrollY,
			&clip, &ctx);
	}

	ScheduledRedrawData.rectCount = 0;
	ScheduledRedrawData.scheduled = 0;
	ScheduledRedrawData.fullWindow = 0;

	if(gw->caretEnabled) {
		plot_style_t style;
		struct rect line;

		style.stroke_type = PLOT_OP_TYPE_SOLID;
		style.stroke_colour = 0xffff00ff;
		style.stroke_width = plot_style_int_to_fixed(2);

		line.x0 = gw->caretX-scrollX;
		line.y0 = gw->caretY-scrollY;
		line.x1 = gw->caretX-scrollX;
		line.y1 = gw->caretY+gw->caretH-scrollY;

#ifdef NSMOTIF_USE_GL
		motifgl_plotters
#else
		fb_plotters
#endif
			.line(&ctx, &style, &line);
	}

#ifdef NSMOTIF_USE_GL
	if(motifDoubleBuffered) {
		glXSwapBuffers(motifDisplay, XtWindow(gw->drawingArea));
	}
#endif
}

/**
 * Invalidates an area of a framebuffer browser window
 *
 * \param g The netsurf window being invalidated.
 * \param rect area to redraw or NULL for the entire window area
 * \return NSERROR_OK on success or appropriate error code
 */
static nserror
gui_window_invalidate_area(struct gui_window *g, const struct rect *rect)
{
	if(g != currentTab) {
		// Not the current tab, do nada
		return NSERROR_OK;
	}

	if(ScheduledRedrawData.gw != g) {
		// Set the window and reset any invalidated rects
		ScheduledRedrawData.gw = g;
		ScheduledRedrawData.rectCount = 0;
	}

	if(rect && !ScheduledRedrawData.fullWindow) {
		int scrollX, scrollY;
		XtVaGetValues(g->horizScrollBar, XmNvalue, &scrollX, NULL);
		XtVaGetValues(g->vertScrollBar, XmNvalue, &scrollY, NULL);

		if(ScheduledRedrawData.rectCount < MAX_INVALIDATED_REGIONS) {
			int i = ScheduledRedrawData.rectCount;
			ScheduledRedrawData.r[i].x0 = rect->x0-scrollX;
			ScheduledRedrawData.r[i].y0 = rect->y0-scrollY;
			ScheduledRedrawData.r[i].x1 = rect->x1-scrollX;
			ScheduledRedrawData.r[i].y1 = rect->y1-scrollY;
			ScheduledRedrawData.rectCount++;
		} else {
			// Too many individual rects, just render it all...
			ScheduledRedrawData.fullWindow = 1;
		}
	} else {
		ScheduledRedrawData.fullWindow = 1;
	}

	if(!ScheduledRedrawData.scheduled) {
		ScheduledRedrawData.scheduled = 1;
		motif_schedule(1, scheduled_redraw, NULL);
	}

	return NSERROR_OK;
}

static bool
gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	XtVaGetValues(g->horizScrollBar, XmNvalue, sx, NULL);
	XtVaGetValues(g->vertScrollBar, XmNvalue, sy, NULL);
	return true;
}

/**
 * Set the scroll position of a framebuffer browser window.
 *
 * Scrolls the viewport to ensure the specified rectangle of the
 *   content is shown. The framebuffer implementation scrolls the contents so
 *   the specified point in the content is at the top of the viewport.
 *
 * \param gw gui_window to scroll
 * \param rect The rectangle to ensure is shown.
 * \return NSERROR_OK on success or apropriate error code.
 */
static nserror
gui_window_set_scroll(struct gui_window *gw, const struct rect *rect)
{
	int scrollY;
	int maximumY, sliderY;
	int scrollX;
	int maximumX, sliderX;

	XtVaGetValues(gw->horizScrollBar, XmNmaximum, &maximumX, XmNsliderSize, &sliderX, NULL);
	XtVaGetValues(gw->vertScrollBar, XmNmaximum, &maximumY, XmNsliderSize, &sliderY, NULL);

	scrollX = rect->x0;
	scrollY = rect->y0;

	if(scrollX >= (maximumX-sliderX)) {
		scrollX = maximumX-sliderX-1;
	}
	if(scrollX < 0) {
		scrollX = 0;
	}

	if(scrollY >= (maximumY-sliderY)) {
		scrollY = maximumY-sliderY-1;
	}
	if(scrollY < 0) {
		scrollY = 0;
	}

	XtVaSetValues(gw->horizScrollBar, XmNvalue, scrollX, NULL);
	XtVaSetValues(gw->vertScrollBar, XmNvalue, scrollY, NULL);

	drawingAreaRedrawCallback(gw->drawingArea, NULL, NULL);

	return NSERROR_OK;
}


/**
 * Find the current dimensions of a framebuffer browser window content area.
 *
 * \param gw The gui window to measure content area of.
 * \param width receives width of window
 * \param height receives height of window
 * \return NSERROR_OK on sucess and width and height updated.
 */
static nserror
gui_window_get_dimensions(struct gui_window *gw, int *width, int *height)
{
	Dimension curWidth, curHeight;
	XtVaGetValues(gw->drawingArea, XmNwidth, &curWidth, XmNheight, &curHeight, NULL);	
	*width = curWidth;
	*height = curHeight;
	return NSERROR_OK;
}

static void
gui_window_update_extent(struct gui_window *gw)
{
	Dimension winWidth, winHeight;
	int w, h;
	int scrollX, scrollY;

	XtVaGetValues(gw->horizScrollBar, XmNvalue, &scrollX, NULL);
	XtVaGetValues(gw->vertScrollBar, XmNvalue, &scrollY, NULL);

	browser_window_get_extents(gw->bw, true, &w, &h);

	XtVaGetValues(gw->drawingArea, XmNwidth, &winWidth, XmNheight, &winHeight, NULL);

//printf("gui_window_update_extent = (%d, %d)\n", w, h);

	if(winWidth > w) winWidth = w;
	if(winHeight > h) winHeight = h;

	if(scrollX >= w-winWidth) {
		scrollX = w-winWidth-1;
	}
	if(scrollX < 0) {
		scrollX = 0;
	}
	if(scrollY >= h-winHeight) {
		scrollY = h-winHeight-1;
	}
	if(scrollY < 0) {
		scrollY = 0;
	}

	XtVaSetValues(gw->horizScrollBar, XmNvalue, 0, XmNmaximum, w, XmNsliderSize, winWidth, XmNpageIncrement, winWidth>>1, XmNvalue, scrollX, NULL);
	XtVaSetValues(gw->vertScrollBar, XmNvalue, 0, XmNmaximum, h, XmNsliderSize, winHeight, XmNpageIncrement, winHeight>>1, XmNvalue, scrollY, NULL);
}

static void
gui_window_set_status(struct gui_window *g, const char *text)
{
	XmString statusLabel = XmStringCreate(text, "STATUS");
	XtVaSetValues(motifStatusLabel, XmNlabelString, statusLabel, NULL);
	XmStringFree(statusLabel);
}

static void
gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	Display *display = XtDisplay(g->drawingArea);
	Window window = XtWindow(g->drawingArea);
	Cursor cursorToUse = None;

	switch (shape) {
	case GUI_POINTER_POINT:
		if(handCursor == None) {
			handCursor = XCreateFontCursor(display, XC_hand2);
		}
		cursorToUse = handCursor;
		break;

	case GUI_POINTER_CARET:
		if(textCursor == None) {
			textCursor = XCreateFontCursor(display, XC_xterm);
		}
		cursorToUse = textCursor;
		break;

	case GUI_POINTER_MENU:
		// Do we want this one? motif_set_cursor(&menu_image);
		printf("GUI_POINTER_MENU\n");
		break;

	case GUI_POINTER_PROGRESS:
		if(waitCursor == None) {
			waitCursor = XCreateFontCursor(display, XC_watch);
		}
		cursorToUse = waitCursor;
		break;

	case GUI_POINTER_MOVE:
		if(moveCursor == None) {
			moveCursor = XCreateFontCursor(display, XC_fleur);
		}
		cursorToUse = moveCursor;
		break;

	default:
		XUndefineCursor(display, window);
		return;
		break;
	}

	XDefineCursor(display, window, cursorToUse);
}

static void gui_window_set_title(struct gui_window *g, const char *title) {
	if(title != NULL && title[0] != 0) {
		free(g->tabTitle);
		g->tabTitle = stringToUTF8FreeString(title, strlen(title));
		if(g == currentTab) {
			XtVaSetValues(motifWindow, XmNtitle, g->tabTitle, NULL);
		}
		XmString str = XmStringCreate(g->tabTitle, "TABLBL");
		XtVaSetValues(g->tab, XmNlabelString, str, NULL);
		XmStringFree(str);
	}
}

static nserror
gui_window_set_url(struct gui_window *g, nsurl *url)
{
	XmTextFieldSetString(g->urlTextField, nsurl_access(url));
	return NSERROR_OK;
}

static void
throbber_advance(void *pw)
{
/* MOTIF?
	struct gui_window *g = pw;
	struct fbtk_bitmap *image;

	switch (g->throbber_index) {
	case 0:
		image = &throbber1;
		g->throbber_index = 1;
		break;

	case 1:
		image = &throbber2;
		g->throbber_index = 2;
		break;

	case 2:
		image = &throbber3;
		g->throbber_index = 3;
		break;

	case 3:
		image = &throbber4;
		g->throbber_index = 4;
		break;

	case 4:
		image = &throbber5;
		g->throbber_index = 5;
		break;

	case 5:
		image = &throbber6;
		g->throbber_index = 6;
		break;

	case 6:
		image = &throbber7;
		g->throbber_index = 7;
		break;

	case 7:
		image = &throbber8;
		g->throbber_index = 0;
		break;

	default:
		return;
	}

	if (g->throbber_index >= 0) {
		fbtk_set_bitmap(g->throbber, image);
		framebuffer_schedule(100, throbber_advance, g);
	}
*/
}

static void
gui_window_start_throbber(struct gui_window *g)
{
//printf("gui_window_start_throbber\n");
	// MOTIF?g->throbber_index = 0;
	// MOTIF?framebuffer_schedule(100, throbber_advance, g);
}

static void
gui_window_stop_throbber(struct gui_window *gw)
{
//printf("gui_window_stop_throbber\n");
	// MOTIF?gw->throbber_index = -1;
/* MOTIF?
	fbtk_set_bitmap(gw->throbber, &throbber0);
*/
	motif_update_back_forward(gw);

}

static void
gui_window_place_caret(struct gui_window *gw, int x, int y, int height,
		const struct rect *clip)
{
	struct rect r;

	if(gw->caretEnabled) {
		r.x0 = MIN(gw->caretX, x)-1;
		r.y0 = MIN(gw->caretY, y)-1;
		r.x1 = MAX(gw->caretX, x)+1;
		r.y1 = MAX(gw->caretY+gw->caretH, y+height)+1;
	} else {
		r.x0 = x-1;
		r.y0 = y-1;
		r.x1 = x+1;
		r.y1 = y+height+1;
	}

	gw->caretX = x;
	gw->caretY = y;
	gw->caretH = height;
	gw->caretEnabled = true;

	gui_window_invalidate_area(gw, &r);
}

static void
gui_window_remove_caret(struct gui_window *gw)
{
	if(gw->caretEnabled) {
		struct rect r;
		r.x0 = gw->caretX-1;
		r.y0 = gw->caretY-1;
		r.x1 = gw->caretX+1;
		r.y1 = gw->caretY+gw->caretH+1;
		gw->caretEnabled = false;
		gui_window_invalidate_area(gw, &r);
	}
}

/**
 * process miscellaneous window events
 *
 * \param gw The window receiving the event.
 * \param event The event code.
 * \return NSERROR_OK when processed ok
 */
static nserror
gui_window_event(struct gui_window *gw, enum gui_window_event event)
{
//printf("gui_window_event %d\n", (int)event);
	switch (event) {
	case GW_EVENT_UPDATE_EXTENT:
		gui_window_update_extent(gw);
		break;

	case GW_EVENT_REMOVE_CARET:
		gui_window_remove_caret(gw);
		break;

	case GW_EVENT_START_THROBBER:
		gui_window_start_throbber(gw);
		break;

	case GW_EVENT_STOP_THROBBER:
		gui_window_stop_throbber(gw);
		break;

	default:
		break;
	}
	return NSERROR_OK;
}

static struct gui_window_table motif_window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.invalidate = gui_window_invalidate_area,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.event = gui_window_event,

	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
	.set_pointer = gui_window_set_pointer,
	.place_caret = gui_window_place_caret,
};


static struct gui_misc_table motif_misc_table = {
	.schedule = motif_schedule,

	.quit = gui_quit,
};

static void scheduleTimerCallback(XtPointer clientData, XtIntervalId *timer)
{
	XtAppContext * app = (XtAppContext *)clientData;

	schedule_run();
	XtAppAddTimeOut(*app, 20, scheduleTimerCallback, app);
}

uint64_t timestamp() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t millis = (tv.tv_sec*1000) + (tv.tv_usec / 1000);
	return millis;
}

static void mouseAction(Widget widget, XEvent *event, String *args, Cardinal *num_args)
{
	XButtonEvent *bevent = (XButtonEvent *)event;
	int scrollX, scrollY;
	struct gui_window *gw;

	XtVaGetValues(widget, XmNuserData, &gw, NULL);
	if(!gw) {
		printf("Null userData in widget!\n");
		return;
	}
	XtVaGetValues(gw->horizScrollBar, XmNvalue, &scrollX, NULL);
	XtVaGetValues(gw->vertScrollBar, XmNvalue, &scrollY, NULL);
	int x, y;
	browser_mouse_state mouse;
	uint64_t time_now = timestamp();
	static struct {
		enum { CLICK_SINGLE, CLICK_DOUBLE, CLICK_TRIPLE } type;
		uint64_t time;
	} last_click;

	x = event->xbutton.x + scrollX;
	y = event->xbutton.y + scrollY;

	if(*num_args != 1)
	{
		return;
	}

	if(!strcmp(args[0], "down"))
	{
		browser_window_mouse_click(gw->bw, BROWSER_MOUSE_PRESS_1, x, y);
		gui_drag.state = GUI_DRAG_PRESSED;
		gui_drag.button = 1;
		gui_drag.x = x;
		gui_drag.y = y;

	} else if(!strcmp(args[0], "up")) {
		if (gui_drag.state == GUI_DRAG_DRAG) {
			/* End of a drag, rather than click */

			if (gui_drag.grabbed_pointer) {
				/* need to ungrab pointer */
				// MOTIF?fbtk_tgrab_pointer(widget);
				gui_drag.grabbed_pointer = false;
			}

			gui_drag.state = GUI_DRAG_NONE;

			/* Tell core */
			browser_window_mouse_track(gw->bw, 0, x, y);
			return;
		}
		/* This is a click;
		 * clear PRESSED state and pass to core */
		gui_drag.state = GUI_DRAG_NONE;
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

		if (mouse) {
			browser_window_mouse_click(gw->bw, mouse, x, y);
		}

		last_click.time = time_now;
	} else if(!strcmp(args[0], "move")) {
		browser_mouse_state mouse = 0;

		x = event->xmotion.x + scrollX;
		y = event->xmotion.y + scrollY;

		if (gui_drag.state == GUI_DRAG_PRESSED &&
				(abs(x - gui_drag.x) > 5 ||
				 abs(y - gui_drag.y) > 5)) {
			/* Drag started */
			if (gui_drag.button == 1) {
				browser_window_mouse_click(gw->bw,
						BROWSER_MOUSE_DRAG_1,
						gui_drag.x, gui_drag.y);
			} else {
				browser_window_mouse_click(gw->bw,
						BROWSER_MOUSE_DRAG_2,
						gui_drag.x, gui_drag.y);
			}
			gui_drag.grabbed_pointer = 0; // MOTIF? fbtk_tgrab_pointer(widget);
			gui_drag.state = GUI_DRAG_DRAG;
		}

		if (gui_drag.state == GUI_DRAG_DRAG) {
			/* set up mouse state */
			mouse |= BROWSER_MOUSE_DRAG_ON;

			if (gui_drag.button == 1)
				mouse |= BROWSER_MOUSE_HOLDING_1;
			else
				mouse |= BROWSER_MOUSE_HOLDING_2;
		}

		browser_window_mouse_track(gw->bw, mouse, x, y);
	} else if(!strcmp("btn4down", args[0])) {
		widget_scroll_y(gw, -100, false);
	} else if(!strcmp("btn5down", args[0])) {
		widget_scroll_y(gw, 100, false);
	} else if(!strcmp("btn2down", args[0])) {
		// TODO: Open link in new tab
		browser_window_mouse_click(gw->bw, BROWSER_MOUSE_PRESS_2, x, y);
		gui_drag.state = GUI_DRAG_PRESSED;
		gui_drag.button = 2;
		gui_drag.x = x;
		gui_drag.y = y;
	} else if(!strcmp("btn2up", args[0])) {
		gui_drag.state = GUI_DRAG_NONE;
		mouse = BROWSER_MOUSE_CLICK_2;

		if (mouse) {
			browser_window_mouse_click(gw->bw, mouse, x, y);
		}
	} else {
		// printf("mouseAction(%s)\n", args[0]);
	}
}

void windowClosedCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	gui_quit();
	XtAppSetExitFlag(app);
}

void fileMenuSimpleCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	if((int)client_data == 0) {
		struct gui_window *gw = currentTab;
		char *text = XmTextFieldGetString(gw->urlTextField);
		XmTextSetSelection(gw->urlTextField, 0, strlen(text), XtLastTimestampProcessed(XtDisplay(gw->urlTextField)));
		XtFree(text);
		XmProcessTraversal(gw->urlTextField, XmTRAVERSE_CURRENT);
		XmProcessTraversal(gw->urlTextField, XmTRAVERSE_CURRENT);
	} else if ((int)client_data == 1) {
		windowClosedCallback(NULL, NULL, NULL);
	}
}

void editMenuSimpleCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	if((int)client_data == 0) {
		// Copy
		browser_window_key_press(currentTab->bw, NS_KEY_COPY_SELECTION);
	} else if((int)client_data == 1) {
		// Undo
		browser_window_key_press(currentTab->bw, NS_KEY_UNDO);
	} else if((int)client_data == 2) {
		// Redo
		browser_window_key_press(currentTab->bw, NS_KEY_REDO);
	}
}

void windowMenuSimpleCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	if((int)client_data == 0) {
		// New tab
		newTabButtonCallback(NULL, NULL, NULL);
	} else if((int)client_data == 1) {
		// Close tab
		if(currentTab && currentTab->bw) {
			struct gui_window *toClose = currentTab;
			if(toClose->prev) {
				activateTab(toClose->prev);
				gui_window_remove_from_window_list(toClose);
				motif_schedule(100, scheduled_browser_destroy, toClose->bw);
				//browser_window_destroy(toClose->bw);
			} else if(toClose->next) {
				activateTab(toClose->next);
				gui_window_remove_from_window_list(toClose);
				motif_schedule(100, scheduled_browser_destroy, toClose->bw);
				//browser_window_destroy(toClose->bw);
			} else {
				// Quit the whole app
				windowClosedCallback(NULL, NULL, NULL);
			}
		}
	} else if((int)client_data == 2) {
		// Zoom in
		browser_window_set_scale(currentTab->bw, 0.1, false);
	} else if((int)client_data == 3) {
		// Zoom out
		browser_window_set_scale(currentTab->bw, -0.1, false);
	} else if((int)client_data == 4) {
		// Reset zoom
		browser_window_set_scale(currentTab->bw, 1.0, true);
	}
}

#define MAX_BOOKMARKS_IN_FOLDER 100

typedef struct BookmarkMenuContext {
	int index;
	XmString menuButtons[MAX_BOOKMARKS_IN_FOLDER];
	XmButtonType menuButtonTypes[MAX_BOOKMARKS_IN_FOLDER];
	KeySym menuKeySyms[MAX_BOOKMARKS_IN_FOLDER];
	char *accelerators[MAX_BOOKMARKS_IN_FOLDER];
	XmString acceleratorTexts[MAX_BOOKMARKS_IN_FOLDER];

	int urlIndex;
} BookmarkMenuContext;

static struct nsurl *bookmarkURLs[MAX_BOOKMARKS_IN_FOLDER];

static void bookmarkCoreWindowDestroyed(Widget widget, XtPointer client_data, XtPointer call_data) {
	setupBookmarksMenu();
	hotlist_manager_fini();
}

static void bookmarksMenuSimpleCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	if((int)client_data == 0) {
		// Add bookmark
		struct nsurl *url = browser_window_access_url(currentTab->bw);
		if(!hotlist_has_url(url)) {
			hotlist_add_url(url);
			setupBookmarksMenu();
		}
	} else if((int)client_data == 1) {
		// Edit bookmarks...
		static struct motif_corewindow hotlist_corewindow;
		struct motif_corewindow *cw = &hotlist_corewindow;
		cw->windowType = MOTIF_BOOKMARKS;
		motif_corewindow_init(cw);
		XtAddCallback(cw->dialog, XmNdestroyCallback, bookmarkCoreWindowDestroyed, NULL);
		hotlist_manager_init(&motif_cw_cb_table, (struct core_window *)cw);
	} else {
		browser_window_navigate(currentTab->bw, bookmarkURLs[(int)client_data], NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
	}
}

static nserror hotlistFolderEnter(void *ctx, char *title) {
	BookmarkMenuContext *context = 	(BookmarkMenuContext *)ctx;

	if(context->index+2 >= MAX_BOOKMARKS_IN_FOLDER) {
		return NSERROR_OK;
	}

	context->menuButtons[context->index] = XmStringCreateLocalized("");
	context->menuButtonTypes[context->index] = XmSEPARATOR;
	context->menuKeySyms[context->index] = 0;
	context->accelerators[context->index] = NULL;
	context->acceleratorTexts[context->index] = NULL;
	context->menuButtons[context->index+1] = XmStringCreateLocalized(title);
	context->menuButtonTypes[context->index+1] = XmTITLE;
	context->menuKeySyms[context->index+1] = 0;
	context->accelerators[context->index+1] = NULL;
	context->acceleratorTexts[context->index+1] = NULL;
	context->index += 2;
	return NSERROR_OK;
}
static nserror hotlistAddress(void *ctx, struct nsurl *url, char *title) {
	BookmarkMenuContext *context = 	(BookmarkMenuContext *)ctx;

	if(context->index+1 >= MAX_BOOKMARKS_IN_FOLDER) {
		return NSERROR_OK;
	}

	context->menuButtons[context->index] = XmStringCreateLocalized(title);
	context->menuButtonTypes[context->index] = XmPUSHBUTTON;
	context->menuKeySyms[context->index] = 0;
	context->accelerators[context->index] = NULL;
	context->acceleratorTexts[context->index] = NULL;
	bookmarkURLs[context->urlIndex] = url;
	nsurl_ref(url);
	context->index++;
	context->urlIndex++;
	return NSERROR_OK;
}
static nserror hotlistFolderExit(void *ctx) {
	// do nothing until we support nested folders
	return NSERROR_OK;
}

void setupBookmarksMenu() {
	BookmarkMenuContext context;
	Arg args[32];
	int n;

	context.menuButtons[0] = XmStringCreateLocalized("Add bookmark");
	context.menuButtonTypes[0] = XmPUSHBUTTON;
	context.menuKeySyms[0] = 'A';
	context.accelerators[0] = "Ctrl<Key>D";
	context.acceleratorTexts[0] = XmStringCreateLocalized("Ctrl+D");
	context.menuButtons[1] = XmStringCreateLocalized("Edit bookmarks");
	context.menuButtonTypes[1] = XmPUSHBUTTON;
	context.menuKeySyms[1] = 'E';
	context.accelerators[1] = "<Key>F6";
	context.acceleratorTexts[1] = XmStringCreateLocalized("F6");

	context.index = 2;
	context.urlIndex = 2;

	hotlist_iterate(&context, hotlistFolderEnter, hotlistAddress, hotlistFolderExit);

	n = 0;
	XtSetArg(args[n], XmNbuttonCount, context.index); n++;
	XtSetArg(args[n], XmNbuttons, context.menuButtons); n++;
	XtSetArg(args[n], XmNbuttonMnemonics, context.menuKeySyms); n++;
	XtSetArg(args[n], XmNbuttonType, context.menuButtonTypes); n++;
	XtSetArg(args[n], XmNbuttonAccelerators, context.accelerators); n++;
	XtSetArg(args[n], XmNbuttonAcceleratorText, context.acceleratorTexts); n++;
	XtSetArg(args[n], XmNpostFromButton, 2); n++;
	XtSetArg(args[n], XmNsimpleCallback, bookmarksMenuSimpleCallback); n++;
	XtSetArg(args[n], XmNvisual, motifVisual); n++; 
	XtSetArg(args[n], XmNdepth, motifDepth); n++; 
	XtSetArg(args[n], XmNcolormap, motifColormap); n++; 
	XmCreateSimplePulldownMenu(motifMenubar, "bookmarksMenu", args, n);
	for(int i = 0; i < context.index; i++) {
		XmStringFree(context.menuButtons[i]);
		if(context.acceleratorTexts[i]) {
			XmStringFree(context.acceleratorTexts[i]);
		}
	}
}

/**
 * Entry point from OS.
 *
 * /param argc The number of arguments in the string vector.
 * /param argv The argument string vector.
 * /return The return code to the OS
 */
int
main(int argc, char** argv)
{
	struct browser_window *bw;
	char *options;
	char *messages;
	nsurl *url;
	nserror ret;
	Arg         	args[32];
	int         	n = 0;
	XtActionsRec	actions;
	XmString buttonLabel;
	XmString menuButtons[MAX_MENU_ITEMS];
	KeySym menuKeySyms[MAX_MENU_ITEMS];
	XmButtonType menuButtonTypes[MAX_MENU_ITEMS];

	ScheduledRedrawData.rectCount = 0;
	ScheduledRedrawData.scheduled = 0;
	ScheduledRedrawData.fullWindow = 0;

	struct netsurf_table motif_table = {
		.misc = &motif_misc_table,
		.window = &motif_window_table,
		.clipboard = motif_clipboard_table,
		.fetch = motif_fetch_table,
		.utf8 = motif_utf8_table,
		.bitmap = motif_bitmap_table,
		.layout = motif_layout_table,
		.download = motif_download_table,
	};

	ret = netsurf_register(&motif_table);
	if (ret != NSERROR_OK) {
		die("NetSurf operation table failed registration");
	}

	respaths = fb_init_resource_path(NETSURF_MOTIF_RESPATH":"NETSURF_MOTIF_FONTPATH":res:frontends/motif/res");

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(nslog_stream_configure, &argc, argv);

	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		die("Options failed to initialise");
	}
	options = filepath_find(respaths, "Choices");
	nsoption_read(options, nsoptions);
	free(options);
	nsoption_commandline(&argc, argv, nsoptions);

	/* message init */
	messages = filepath_find(respaths, "Messages");
        ret = messages_add_from_file(messages);
	free(messages);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "Message translations failed to load\n");
	}

    XtSetLanguageProc(NULL, NULL, NULL);
    motifWindow = XtVaAppInitialize(&app, "netsurf-motif", NULL, 0, &argc, argv, fallbacks, NULL);
	Widget mainWindow = XmCreateMainWindow(motifWindow, "main_window", NULL, 0);
	XtManageChild(mainWindow);

	// mainWindow here was motifWindow
	mainLayout = XtVaCreateWidget("mainLayout", xmFormWidgetClass, mainWindow, XmNwidth, 800, XmNheight, 720, NULL);

	motifDisplay = XtDisplay(motifWindow);

	// Request a 24-bit truecolor visual
	{
#ifdef NSMOTIF_USE_GL
		int attrList[] = {
			GLX_RGBA,
			GLX_DOUBLEBUFFER,
			GLX_RED_SIZE, 1,
			GLX_BLUE_SIZE, 1,
			GLX_GREEN_SIZE, 1,
			None
		};
		int backupAttrList[] = {
			GLX_RGBA,
			GLX_RED_SIZE, 2,
			GLX_BLUE_SIZE, 2,
			GLX_GREEN_SIZE, 2,
			None
		};
		int screen;
		XVisualInfo *vi = glXChooseVisual(motifDisplay, DefaultScreen(motifDisplay), attrList);
		if(!vi) {
			printf("Failed to find a GLX visual\n");
			return 1;
		}
		motifDoubleBuffered = 1;
		if(vi->depth < 8) {
			printf("GLX Visual only %dbit, switching to single buffered\n", vi->depth);
			vi = glXChooseVisual(motifDisplay, DefaultScreen(motifDisplay), backupAttrList);
			if(!vi) {
				printf("Failed to find a backup GLX visual\n");
				return 1;
			}
			motifDoubleBuffered = 0;
		}
		screen = vi->screen;
		motifDepth = vi->depth;
printf("Selected GL visual depth %d\n", motifDepth);
		motifVisual = vi->visual;
		motifColormap = XCreateColormap(motifDisplay, RootWindow(motifDisplay, screen), motifVisual, AllocNone);

		motifGLContext = glXCreateContext(motifDisplay, vi, 0, GL_TRUE);
#else
		XVisualInfo template;
		long mask = VisualClassMask | VisualDepthMask;
		XVisualInfo *visuals;
		int visualCount;

		template.class = TrueColor;
		template.depth = 24;

		visuals = XGetVisualInfo(motifDisplay, mask, &template, &visualCount);
		if(visualCount == 0) {
			printf("This system doesn't support truecolor visuals. Sorry!\n");
			return 1;
		}
		
		motifVisual = visuals[0].visual;
		motifDepth = visuals[0].depth;
		XFree(visuals);

		motifColormap = XCreateColormap(motifDisplay, DefaultRootWindow(motifDisplay), motifVisual, AllocNone);
#endif

		if(motifDepth < 24) {
			XColor xc808080;
			XColor xcc4c4c4;

			xc808080.red = xc808080.green = xc808080.blue = 32768;
			xc808080.flags = DoRed | DoGreen | DoBlue;
			XAllocColor(motifDisplay, motifColormap, &xc808080);
			color808080 = xc808080.pixel;

			xcc4c4c4.red = xcc4c4c4.green = xcc4c4c4.blue = 50372;	// 0xc4/0xff * 65535
			xcc4c4c4.flags = DoRed | DoGreen | DoBlue;
			XAllocColor(motifDisplay, motifColormap, &xcc4c4c4);
			colorc4c4c4 = xcc4c4c4.pixel;
		} else {
			color808080 = 0x00808080;
			colorc4c4c4 = 0x00c4c4c4;
		}

		XtVaSetValues(motifWindow, XmNdepth, motifDepth, XmNvisual, motifVisual, XmNcolormap, motifColormap, NULL);
		XtVaSetValues(mainWindow, XmNdepth, motifDepth, XmNvisual, motifVisual, XmNcolormap, motifColormap, NULL);
		XtVaSetValues(mainLayout, XmNdepth, motifDepth, XmNvisual, motifVisual, XmNcolormap, motifColormap, NULL);
		
	}

	actions.string = "mouseAction";
	actions.proc = mouseAction;
	XtAppAddActions(app, &actions, 1);
	actions.string = "coreMouseAction";
	actions.proc = coreMouseAction;
	XtAppAddActions(app, &actions, 1);

	tabHolder = XtVaCreateWidget("tabHolder", xmFormWidgetClass, mainLayout, XmNheight, 28, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNresizable, 0, NULL);
	XtVaSetValues(tabHolder, XmNbackground, color808080, NULL);

	XtManageChild(tabHolder);

	n = 0;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNheight, 16); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNresizable, 0); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	motifStatusLabel = XmCreateLabel(mainLayout, "motifStatusLabel", args, n);
	XtManageChild(motifStatusLabel);

	contentHolder = XtVaCreateWidget("contentHolder", xmFormWidgetClass, mainLayout, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, tabHolder, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM, XmNbottomAttachment, XmATTACH_WIDGET, XmNbottomWidget, motifStatusLabel, 		XmNdepth, motifDepth, XmNvisual, motifVisual, XmNcolormap, motifColormap, NULL);

	XtManageChild(contentHolder);
	XtManageChild(mainLayout);
	XtRealizeWidget(motifWindow);

	Atom protocolsAtom = XmInternAtom(motifDisplay, "WM_PROTOCOLS", False);
	Atom deleteWindowAtom = XmInternAtom(motifDisplay, "WM_DELETE_WINDOW", True);
	if(deleteWindowAtom) {
		XmAddProtocols(motifWindow, protocolsAtom, &deleteWindowAtom, 1);
		XmAddProtocolCallback(motifWindow, protocolsAtom, deleteWindowAtom, windowClosedCallback, NULL);
	}

	// Setup the menu bar
	XmString file = XmStringCreateLocalized("File");
	XmString edit = XmStringCreateLocalized("Edit");
	XmString bookmarks = XmStringCreateLocalized("Bookmarks");
	XmString windowString = XmStringCreateLocalized("Window");
	motifMenubar = XmVaCreateSimpleMenuBar(mainWindow, "menubar", 
		XmVaCASCADEBUTTON, file, 'F',
		XmVaCASCADEBUTTON, edit, 'E',
		XmVaCASCADEBUTTON, bookmarks, 'B',
		XmVaCASCADEBUTTON, windowString, 'W', 
		NULL);
	XmStringFree(file);
	XmStringFree(edit);
	XmStringFree(bookmarks);
	XtManageChild(motifMenubar);

	// Setup the File menu
	{
		menuButtons[0] = XmStringCreateLocalized("Go to URL");
		menuButtons[1] = XmStringCreateLocalized("Ctrl+L");
		menuButtons[2] = XmStringCreateLocalized("Exit");
		XmVaCreateSimplePulldownMenu(motifMenubar, "fileMenu", 
			0, fileMenuSimpleCallback,
			XmVaPUSHBUTTON, menuButtons[0], 'G', "Ctrl<Key>L", menuButtons[1],
			XmVaSEPARATOR,
			XmVaPUSHBUTTON, menuButtons[2], 'x', "Alt<Key>F4", NULL,
			NULL);
		XmStringFree(menuButtons[0]);
		XmStringFree(menuButtons[1]);
		XmStringFree(menuButtons[2]);
	}

	// Setup the Edit menu
	{
		menuButtons[0] = XmStringCreateLocalized("Copy");
		menuButtons[1] = XmStringCreateLocalized("Ctrl+C");
		menuButtons[2] = XmStringCreateLocalized("Undo");
		menuButtons[3] = XmStringCreateLocalized("Ctrl+Z");
		menuButtons[4] = XmStringCreateLocalized("Redo");
		menuButtons[5] = XmStringCreateLocalized("Ctrl+Shift+Z");

		XmVaCreateSimplePulldownMenu(motifMenubar, "windowMenu", 
			1, editMenuSimpleCallback,
			XmVaPUSHBUTTON, menuButtons[0], 'C', "Ctrl<Key>C", menuButtons[1],
			XmVaSEPARATOR,
			XmVaPUSHBUTTON, menuButtons[2], 'U', "Ctrl<Key>Z", menuButtons[3],
			XmVaPUSHBUTTON, menuButtons[4], 'R', "Ctrl Shift<Key>Z", menuButtons[5],
			NULL);
		XmStringFree(menuButtons[0]);
		XmStringFree(menuButtons[1]);
		XmStringFree(menuButtons[2]);
		XmStringFree(menuButtons[3]);
		XmStringFree(menuButtons[4]);
		XmStringFree(menuButtons[5]);
	}

	// Setup the Window menu
	{
		menuButtons[0] = XmStringCreateLocalized("New tab");
		menuButtons[1] = XmStringCreateLocalized("Close tab");

		menuButtons[3] = XmStringCreateLocalized("Zoom in");
		menuButtons[4] = XmStringCreateLocalized("Zoom out");
		menuButtons[5] = XmStringCreateLocalized("Reset zoom");

		menuButtons[6] = XmStringCreateLocalized("Ctrl+T");
		menuButtons[7] = XmStringCreateLocalized("Ctrl+W");
		menuButtons[8] = XmStringCreateLocalized("Ctrl++");
		menuButtons[9] = XmStringCreateLocalized("Ctrl+-");
		menuButtons[10] = XmStringCreateLocalized("Ctrl+0");

		XmVaCreateSimplePulldownMenu(motifMenubar, "windowMenu", 
			3, windowMenuSimpleCallback,
			XmVaPUSHBUTTON, menuButtons[0], 'N', "Ctrl<Key>T", menuButtons[6],
			XmVaPUSHBUTTON, menuButtons[1], 'C', "Ctrl<Key>W", menuButtons[7],
			XmVaSEPARATOR,
			XmVaPUSHBUTTON, menuButtons[3], 'i', "Ctrl<Key>equal", menuButtons[8],
			XmVaPUSHBUTTON, menuButtons[4], 'o', "Ctrl<Key>minus", menuButtons[9],
			XmVaPUSHBUTTON, menuButtons[5], 'R', "Ctrl<Key>0", menuButtons[10],
			NULL);
		XmStringFree(menuButtons[0]);
		XmStringFree(menuButtons[1]);
		XmStringFree(menuButtons[3]);
		XmStringFree(menuButtons[4]);
		XmStringFree(menuButtons[5]);

		XmStringFree(menuButtons[6]);
		XmStringFree(menuButtons[7]);
		XmStringFree(menuButtons[8]);
		XmStringFree(menuButtons[9]);
		XmStringFree(menuButtons[10]);
	}

	XtVaSetValues(mainWindow, XmNmenuBar, motifMenubar, XmNworkWindow, mainLayout, NULL);

	/* common initialisation */
	ret = netsurf_init(NULL);
	if (ret != NSERROR_OK) {
		die("NetSurf failed to initialise");
	}

	/* Override, since we have no support for non-core SELECT menu */
	nsoption_set_bool(core_select_menu, true);

	if (process_cmdline(argc,argv) != true)
		die("unable to process command line.\n");

	urldb_load(nsoption_charp(url_file));
	urldb_load_cookies(nsoption_charp(cookie_file));
	hotlist_init(nsoption_charp(hotlist_file), nsoption_charp(hotlist_file));

	// Setup the Bookmarks menu, AFTER everything else has been initialized
	setupBookmarksMenu();

	/* create an initial browser window */

	NSLOG(netsurf, INFO, "calling browser_window_create");

	ret = nsurl_create(feurl, &url);
	if (ret == NSERROR_OK) {
		ret = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      &bw);
		nsurl_unref(url);
	}

	if (ret != NSERROR_OK) {
		fb_warn_user("Errorcode:", messages_get_errorcode(ret));
	} else {
		XtAppAddTimeOut(app, 20, scheduleTimerCallback, &app);
		activateTab(window_list);
		XtAppMainLoop(app);
	}

	// Is this even necessary?
	//cleanup_browsers();

	//netsurf_exit();

	/* finalise options */
	nsoption_finalise(nsoptions, nsoptions_default);

	/* finalise logging */
	//nslog_finalise();

	return 0;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

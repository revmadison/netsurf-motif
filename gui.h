/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef NETSURF_MOTIF_GUI_H
#define NETSURF_MOTIF_GUI_H

#include <X11/Intrinsic.h>

//#define NSMOTIF_USE_GL 1

/* bounding box */
typedef struct nsfb_bbox_s bbox_t;

struct gui_window {
	// NOTE: These must be the first entries to match motif_corewindow 
	// because I haven't yet abstracted this all out yet
	Widget drawingArea;
	GC gc;

	struct browser_window *bw;

	bool deleting;

	Widget layout;
	Widget vertScrollBar;
	Widget horizScrollBar;
	Widget backButton;
	Widget forwardButton;
	Widget stopButton;
	Widget urlTextField;

	Widget tab;
	char *tabTitle;

	int throbber_index;

	bool caretEnabled;
	int caretX, caretY, caretH;

	int winWidth, winHeight;

	struct gui_window *next;
	struct gui_window *prev;
};


extern struct gui_window *window_list;

void gui_resize(void *root, int width, int height);

#endif /* NETSURF_MOTIF_GUI_H */

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

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

#ifndef MOTIF_COREWINDOW_H
#define MOTIF_COREWINDOW_H

#include "netsurf/core_window.h"

#include <X11/Intrinsic.h>

enum motif_core_window_type {
	MOTIF_BOOKMARKS,
	MOTIF_SETTINGS
};

/**
 * fb core window state
 */
struct motif_corewindow {
	// NOTE: These must be the first entries to match gui_window 
	// because I haven't yet abstracted this all out yet
	Widget drawingArea;
	GC gc;

	int windowType;

	int scrollx, scrolly; /**< scroll offsets. */

	int mousePressed, mouseX, mouseY, mouseDragging;

	Widget dialog;
	Widget layout;

	/** drag status set by core */
	core_window_drag_status drag_status;

	/** table of callbacks for core window operations */
	struct core_window_callback_table *cb_table;

	/**
	 * callback to draw on drawable area of fb core window
	 *
	 * \param fb_cw The fb core window structure.
	 * \param r The rectangle of the window that needs updating.
	 * \return NSERROR_OK on success otherwise apropriate error code
	 */
	nserror (*draw)(struct motif_corewindow *fb_cw, struct rect *r);

	/**
	 * callback for keypress on fb core window
	 *
	 * \param fb_cw The fb core window structure.
	 * \param nskey The netsurf key code.
	 * \return NSERROR_OK if key processed,
	 *         NSERROR_NOT_IMPLEMENTED if key not processed
	 *         otherwise apropriate error code
	 */
	nserror (*key)(struct motif_corewindow *fb_cw, uint32_t nskey);

	/**
	 * callback for mouse event on fb core window
	 *
	 * \param fb_cw The fb core window structure.
	 * \param mouse_state mouse state
	 * \param x location of event
	 * \param y location of event
	 * \return NSERROR_OK on sucess otherwise apropriate error code.
	 */
	nserror (*mouse)(struct motif_corewindow *fb_cw, browser_mouse_state mouse_state, int x, int y);
};


void motif_corewindow_init(struct motif_corewindow *cw);

/**
 * finalise elements of fb core window.
 *
 * \param fb_cw A fb core window structure to initialise
 * \return NSERROR_OK on successful finalisation otherwise error code.
 */
nserror motif_corewindow_fini(struct motif_corewindow *fb_cw);

void coreMouseAction(Widget widget, XEvent *event, String *args, Cardinal *num_args);

extern struct core_window_callback_table motif_cw_cb_table;

#endif

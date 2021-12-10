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

/**
 * \file
 * Motif (for IRIX specifically) implementation of generic bitmap interface.
 */

#include <inttypes.h>
#include <sys/types.h>
#include <stdbool.h>
#include <assert.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "netsurf/bitmap.h"
#include "netsurf/plotters.h"
#include "netsurf/content.h"

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <Xm/DrawingA.h>
#include <stdlib.h>

#include "motif/gui.h"
#include "motif/drawing.h"
#include "motif/bitmap.h"

extern Display *motifDisplay;
extern Visual *motifVisual;
extern Widget motifWindow;

/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */
static void *bitmap_create(int width, int height, unsigned int state)
{
//printf("bitmap_create %dx%d %d\n", width, height, state);
	MotifBitmap * bmp = (MotifBitmap *)malloc(sizeof(MotifBitmap));
	if(!bmp) return NULL;

	bmp->buffer = (char *)malloc(width*height*4);
	bmp->width = width;
	bmp->height = height;
	bmp->bpp = 4;
	bmp->stride = width*4;
	bmp->opaque = state & BITMAP_OPAQUE ? 1 : 0;
	memset(bmp->buffer, 0, width*height*4);
#ifndef NSMOTIF_USE_GL
	bmp->ximage = XCreateImage(motifDisplay, motifVisual, 24, ZPixmap, 0, bmp->buffer, width, height, 32, width*4);

	bmp->pixmap = XCreatePixmap(motifDisplay, XtWindow(motifWindow), width, height, 24);
	bmp->gc = XCreateGC(motifDisplay, bmp->pixmap, 0, 0);
#else
	bmp->ximage = NULL;
	bmp->pixmap = None;
	bmp->gc = NULL;
#endif
	bmp->mask = None;
	bmp->hasMask = 0;
	return bmp;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */
static unsigned char *bitmap_get_buffer(void *bitmap)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	return (unsigned char *)bmp->buffer;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */
static size_t bitmap_get_rowstride(void *bitmap)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	return bmp->stride;	
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
static void bitmap_destroy(void *bitmap)
{
//printf("bitmap_destroy\n");
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	if(bmp == NULL) return;

#ifndef NSMOTIF_USE_GL
	XDestroyImage(bmp->ximage);
	XFreePixmap(motifDisplay, bmp->pixmap);
	if(bmp->mask != None) {
		XFreePixmap(motifDisplay, bmp->mask);
		bmp->mask = None;
	}
	XFreeGC(motifDisplay, bmp->gc);
#else
	//XDestroyImage above frees this when not using GL
	free(bmp->buffer);
#endif
	free(bmp);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \param flags flags controlling how the bitmap is saved.
 * \return true on success, false on error and error reported
 */
static bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
static void bitmap_modified(void *bitmap) {
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	int *pixels = (int *)bmp->buffer;
	Display *display = motifDisplay;
//printf("bitmap_modified %x %d\n", bitmap, bmp->opaque);

#ifndef NSMOTIF_USE_GL
	int i = 0;
	if(bmp->opaque) {
		// We're opaque (jpeg, for instance) so just convert the color data
		bmp->hasMask = 0;
		for(int y = 0; y < bmp->height; y++) {
			for(int x = 0; x < bmp->width; x++) {
				pixels[i] = ((pixels[i]>>24)&0x000000ff)|((pixels[i]>>8)&0x0000ff00)|((pixels[i]&0x0000ff00)<<8);
				i++;
			}
		}
	} else {
#define PUSH_MASK_VALUE(v)						\
		maskValue = (maskValue>>1)|(v);			\
		maskBit++;								\
		if(maskBit == 8) {						\
			maskBuffer[maskOffset] = maskValue;	\
			maskValue = 0;						\
			maskBit = 0;						\
			maskOffset++;						\
		}
#define MASK_NEXT_LINE							\
		if(maskBit > 0) {						\
			maskValue >>= (8-maskBit);			\
			maskBuffer[maskOffset] = maskValue;	\
			maskValue = 0;						\
			maskBit = 0;						\
			maskOffset++;						\
		}

		char *maskBuffer = (char *)malloc(bmp->width*bmp->height);
		int maskBit = 0;
		int maskValue = 0;
		int maskOffset = 0;
		int hasMask = 0;

		// We do the extra work to generate a mask here since we could have alpha
		for(int y = 0; y < bmp->height; y++) {
			for(int x = 0; x < bmp->width; x++) {
				unsigned int p;
				if(pixels[i] & 0x00000080)
				{
					p = ((pixels[i]>>24)&0x000000ff)|((pixels[i]>>8)&0x0000ff00)|((pixels[i]&0x0000ff00)<<8)|((pixels[i]&0x000000ff)<<24);
					PUSH_MASK_VALUE(0x0080);
				} else {
					p = 0;
					hasMask = 1;
					PUSH_MASK_VALUE(0);
				}
				pixels[i] = p;
				i++;
			}
			MASK_NEXT_LINE;
		}

		if(hasMask) {
			if(bmp->mask != None) {
				XFreePixmap(motifDisplay, bmp->mask);
			}
			bmp->mask = XCreatePixmapFromBitmapData(motifDisplay, XtWindow(motifWindow), maskBuffer, bmp->width, bmp->height, 1, 0, 1);
		} else {
			if(bmp->mask != None) {
				XFreePixmap(motifDisplay, bmp->mask);
				bmp->mask = None;
			}
		}
		bmp->hasMask = hasMask;
		free(maskBuffer);

	}

	XPutImage(display, bmp->pixmap, bmp->gc, bmp->ximage, 0, 0, 0, 0, bmp->width, bmp->height);
#endif

}

/**
 * Sets wether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
static void bitmap_set_opaque(void *bitmap, bool opaque)
{
//printf("bitmap_set_opaque %x\n", bitmap);
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	bmp->opaque = opaque?1:0;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
static bool bitmap_test_opaque(void *bitmap)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	int *pixels = (int *)bmp->buffer;
//printf("bitmap_test_opaque %x\n", bitmap);

	if(bmp->hasMask) {
		return false;
	}

	int i = 0;
	for(int y = 0; y < bmp->height; y++) {
		for(int x = 0; x < bmp->width; x++) {
			unsigned int p;
			if((pixels[i] & 0x00000080)) {
				// Value > 50% alpha
			} else {
				return false;
			}
			i++;
		}
	}

	return true;
}


/**
 * Gets weather a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	return bmp->opaque;
}

static int bitmap_get_width(void *bitmap)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	return bmp->width;
}

static int bitmap_get_height(void *bitmap)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	return bmp->height;
}

/* get bytes per pixel */
static size_t bitmap_get_bpp(void *bitmap)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	return bmp->bpp;
}

/**
 * Render content into a bitmap.
 *
 * \param bitmap the bitmap to draw to
 * \param content content structure to render
 * \return true on success and bitmap updated else false
 */
static nserror
bitmap_render(struct bitmap *bitmap,
	      struct hlcache_handle *content)
{
	MotifBitmap * bmp = (MotifBitmap *)bitmap;
	if(bmp) {
		//printf("bitmap_render into %dx%d bitmap\n", bmp->width, bmp->height);
	} else {
		//printf("bitmap_render null bitmap\n");
	}

#if 0 // MOTIF? Is this just used for rendering snapshots for the history page?
	nsfb_t *tbm = (nsfb_t *)bitmap; /* target bitmap */
	nsfb_t *bm; /* temporary bitmap */
	nsfb_t *current; /* current main fb */
	int width, height; /* target bitmap width height */
	int cwidth, cheight;/* content width /height */
	nsfb_bbox_t loc;

	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &fb_plotters
	};

	nsfb_get_geometry(tbm, &width, &height, NULL);

	NSLOG(netsurf, INFO, "width %d, height %d", width, height);

	/* Calculate size of buffer to render the content into */
	/* We get the width from the largest of the bitmap width and the content
	 * width, unless it exceeds 1024, in which case we use 1024. This means
	 * we never create excessively large render buffers for huge contents,
	 * which would eat memory and cripple performance. */
	cwidth = max(width, min(content_get_width(content), 1024));
	/* The height is set in proportion with the width, according to the
	 * aspect ratio of the required thumbnail. */
	cheight = ((cwidth * height) + (width / 2)) / width;

	/* create temporary surface */
	bm = nsfb_new(NSFB_SURFACE_RAM);
	if (bm == NULL) {
		return NSERROR_NOMEM;
	}

	nsfb_set_geometry(bm, cwidth, cheight, NSFB_FMT_XBGR8888);

	if (nsfb_init(bm) == -1) {
		nsfb_free(bm);
		return NSERROR_NOMEM;
	}

	current = framebuffer_set_surface(bm);

	/* render the content into temporary surface */
	content_scaled_redraw(content, cwidth, cheight, &ctx);

	framebuffer_set_surface(current);

	loc.x0 = 0;
	loc.y0 = 0;
	loc.x1 = width;
	loc.y1 = height;

	nsfb_plot_copy(bm, NULL, tbm, &loc);

	nsfb_free(bm);
#endif
	return NSERROR_OK;
}

static struct gui_bitmap_table bitmap_table = {
	.create = bitmap_create,
	.destroy = bitmap_destroy,
	.set_opaque = bitmap_set_opaque,
	.get_opaque = bitmap_get_opaque,
	.test_opaque = bitmap_test_opaque,
	.get_buffer = bitmap_get_buffer,
	.get_rowstride = bitmap_get_rowstride,
	.get_width = bitmap_get_width,
	.get_height = bitmap_get_height,
	.get_bpp = bitmap_get_bpp,
	.save = bitmap_save,
	.modified = bitmap_modified,
	.render = bitmap_render,
};

struct gui_bitmap_table *motif_bitmap_table = &bitmap_table;


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

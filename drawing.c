/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer interface
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils/utils.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "netsurf/browser_window.h"
#include "netsurf/plotters.h"
#include "netsurf/bitmap.h"

#include <X11/Xlib.h>
#include <Xm/DrawingA.h>

#include "motif/gui.h"
#include "motif/drawing.h"
#include "motif/font.h"
#include "motif/bitmap.h"

extern Display *motifDisplay;
extern Visual *motifVisual;
extern Widget motifWindow;
extern int motifDepth;

static XRectangle clipRect;

#define TARGET XtWindow(gw->drawingArea)

/**
 * \brief Sets a clip rectangle for subsequent plot operations.
 *
 * \param ctx The current redraw context.
 * \param clip The rectangle to limit all subsequent plot
 *              operations within.
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_clip(const struct redraw_context *ctx, const struct rect *clip)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

	clipRect.x = clip->x0;
	clipRect.y = clip->y0;
	clipRect.width = clip->x1-clip->x0;
	clipRect.height = clip->y1-clip->y0;

	XSetClipRectangles(motifDisplay, gw->gc, 0, 0, &clipRect, 1, Unsorted);

	return NSERROR_OK;
}


/**
 * Plots an arc
 *
 * plot an arc segment around (x,y), anticlockwise from angle1
 *  to angle2. Angles are measured anticlockwise from
 *  horizontal, in degrees.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the arc plot.
 * \param x The x coordinate of the arc.
 * \param y The y coordinate of the arc.
 * \param radius The radius of the arc.
 * \param angle1 The start angle of the arc.
 * \param angle2 The finish angle of the arc.
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_arc(const struct redraw_context *ctx,
	       const plot_style_t *style,
	       int x, int y, int radius, int angle1, int angle2)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

//printf("framebuffer_plot_arc\n");
	Display *display = motifDisplay;
	GC gc = gw->gc;

	XSetForeground(display, gc, style->fill_colour);
	XDrawArc(display, TARGET, gc, x, y, radius*2, radius*2, angle1<<5, angle2<<5);
	return NSERROR_OK;
}


/**
 * Plots a circle
 *
 * Plot a circle centered on (x,y), which is optionally filled.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the circle plot.
 * \param x x coordinate of circle centre.
 * \param y y coordinate of circle centre.
 * \param radius circle radius.
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_disc(const struct redraw_context *ctx,
		const plot_style_t *style,
		int x, int y, int radius)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

//printf("framebuffer_plot_disc\n");
	Display *display = motifDisplay;
	GC gc = gw->gc;

	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		XSetForeground(display, gc, style->fill_colour);
		XFillArc(display, TARGET, gc, x, y, radius*2, radius*2, 0<<5, 360<<5);
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		XSetForeground(display, gc, style->stroke_colour);
		XDrawArc(display, TARGET, gc, x, y, radius*2, radius*2, 0<<5, 360<<5);
	}
	return NSERROR_OK;
}


/**
 * Plots a line
 *
 * plot a line from (x0,y0) to (x1,y1). Coordinates are at
 *  centre of line width/thickness.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the line plot.
 * \param line A rectangle defining the line to be drawn
 * \return NSERROR_OK on success else error code.
 */
static nserror
motif_plot_line(const struct redraw_context *ctx,
		const plot_style_t *style,
		const struct rect *line)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

//printf("motif_plot_line\n");

	/*
	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if (style->stroke_type == PLOT_OP_TYPE_DOT) {
			pen.stroke_type = NFSB_PLOT_OPTYPE_PATTERN;
			pen.stroke_pattern = 0xAAAAAAAA;
		} else if (style->stroke_type == PLOT_OP_TYPE_DASH) {
			pen.stroke_type = NFSB_PLOT_OPTYPE_PATTERN;
			pen.stroke_pattern = 0xF0F0F0F0;
		} else {
			pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
		}
	}
	*/

	Display *display = motifDisplay;
	GC gc = gw->gc;
	// TODO: Support dotted lines
	bool dotted = false;
	bool dashed = false;

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if (style->stroke_type == PLOT_OP_TYPE_DOT) {
			dotted = true;
		}

		if (style->stroke_type == PLOT_OP_TYPE_DASH) {
			dashed = true;
		}

		XSetForeground(display, gc, style->stroke_colour);
		XSetLineAttributes(display, gc, plot_style_fixed_to_int(style->stroke_width), dashed?LineOnOffDash:LineSolid, CapNotLast, JoinMiter);
		XDrawLine(display, TARGET, gc, line->x0, line->y0, line->x1, line->y1);
	}

	return NSERROR_OK;
}


/**
 * Plots a rectangle.
 *
 * The rectangle can be filled an outline or both controlled
 *  by the plot style The line can be solid, dotted or
 *  dashed. Top left corner at (x0,y0) and rectangle has given
 *  width and height.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the rectangle plot.
 * \param nsrect A rectangle defining the line to be drawn
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_rectangle(const struct redraw_context *ctx,
		     const plot_style_t *style,
		     const struct rect *nsrect)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

//printf("framebuffer_plot_rectangle (%d, %d)->(%d, %d) @ %x/%x\n", nsrect->x0, nsrect->y0, nsrect->x1, nsrect->y1, style->fill_colour, style->stroke_colour);
	Display *display = motifDisplay;
	GC gc = gw->gc;
	XRectangle xrect;
	// TODO: Support dotted lines
	bool dotted = false;
	bool dashed = false;

	xrect.x = nsrect->x0;
	xrect.y = nsrect->y0;
	xrect.width = nsrect->x1-nsrect->x0;
	xrect.height = nsrect->y1-nsrect->y0;


	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		XSetForeground(display, gc, style->fill_colour);
		XSetFillStyle(display, gc, FillSolid);
		XFillRectangle(display, TARGET, gc, xrect.x, xrect.y, xrect.width, xrect.height);
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if (style->stroke_type == PLOT_OP_TYPE_DOT) {
			dotted = true;
		}

		if (style->stroke_type == PLOT_OP_TYPE_DASH) {
			dashed = true;
		}

		XSetForeground(display, gc, style->stroke_colour);
		XSetLineAttributes(display, gc, plot_style_fixed_to_int(style->stroke_width), dashed?LineOnOffDash:LineSolid, CapNotLast, JoinMiter);
		XDrawRectangle(display, TARGET, gc, xrect.x, xrect.y, xrect.width, xrect.height);
	}

	return NSERROR_OK;
}


/**
 * Plot a polygon
 *
 * Plots a filled polygon with straight lines between
 * points. The lines around the edge of the ploygon are not
 * plotted. The polygon is filled with the non-zero winding
 * rule.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the polygon plot.
 * \param p verticies of polygon
 * \param n number of verticies.
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_polygon(const struct redraw_context *ctx,
		   const plot_style_t *style,
		   const int *p,
		   unsigned int n)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}


	Display *display = motifDisplay;
	GC gc = gw->gc;

//printf("framebuffer_plot_polygon\n");
	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		XPoint *points = (XPoint *)malloc(n*sizeof(XPoint));
		for(int i = 0; i < n; i++)
		{
			points[i].x = p[(i<<1)+0];
			points[i].y = p[(i<<1)+1];
		}

		XSetForeground(display, gc, style->fill_colour);
		XSetFillStyle(display, gc, FillSolid);
		XFillPolygon(display, TARGET, gc, points, n, Complex, CoordModeOrigin);
		free(points);
	}

	return NSERROR_OK;
}


/**
 * Plots a path.
 *
 * Path plot consisting of cubic Bezier curves. Line and fill colour is
 *  controlled by the plot style.
 *
 * \param ctx The current redraw context.
 * \param pstyle Style controlling the path plot.
 * \param p elements of path
 * \param n nunber of elements on path
 * \param transform A transform to apply to the path.
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_path(const struct redraw_context *ctx,
		const plot_style_t *pstyle,
		const float *p,
		unsigned int n,
		const float transform[6])
{
	NSLOG(netsurf, INFO, "path unimplemented");
//NEEDGL
	return NSERROR_OK;
}

#ifdef NSMOTIF_USE_GL
static Pixmap createPixmap(MotifBitmap *bmp) {
	int *dest = (int *)malloc(bmp->width*bmp->height*4);
	int *pixels = (int *)bmp->buffer;
	Display *display = motifDisplay;
//printf("bitmap_modified %x %d\n", bitmap, bmp->opaque);

	if(motifDepth < 24) return None;

	int i = 0;

#define PUSH_MASK_VALUE(v)					\
	maskValue = (maskValue>>1)|(v);			\
	maskBit++;								\
	if(maskBit == 8) {						\
		maskBuffer[maskOffset] = maskValue;	\
		maskValue = 0;						\
		maskBit = 0;						\
		maskOffset++;						\
	}
#define MASK_NEXT_LINE						\
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
				p = ((pixels[i]>>8) & 0x00ffffff) |
					((pixels[i]&0x00ff) << 24);
				PUSH_MASK_VALUE(0x0080);
			} else {
				p = 0;
				hasMask = 1;
				PUSH_MASK_VALUE(0);
			}
			dest[i] = p;
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

#undef PUSH_MASK_VALUE
#undef MASK_NEXT_LINE

	XImage *ximage = XCreateImage(display, motifVisual, 24, ZPixmap, 0, (char *)dest, bmp->width, bmp->height, 32, bmp->width*4);
	Pixmap pixmap = XCreatePixmap(motifDisplay, XtWindow(motifWindow), bmp->width, bmp->height, motifDepth < 24 ? motifDepth : 24);
	GC gc = XCreateGC(motifDisplay, pixmap, 0, 0);

	XPutImage(display, pixmap, gc, ximage, 0, 0, 0, 0, bmp->width, bmp->height);
	
	XDestroyImage(ximage);
	XFreeGC(motifDisplay, gc);

	bmp->pixmap = pixmap;
	return pixmap;
}
#endif


static Pixmap scaleBitmap(MotifBitmap *bmp, int x, int y, int w, int h, int scaledW, int scaledH, float scaleX, float scaleY, int drawX, int drawY, GC drawingGC) {
	unsigned int * src = (unsigned int *)bmp->buffer;
	unsigned int * dest = (unsigned int *)malloc(scaledW*scaledH*4);

	Display *display = motifDisplay;
//printf("scaling bitmap source %d,%d from %dx%d to %dx%d factor %f,%f to %d,%d\n", x, y, w, h, scaledW, scaledH, (1.0f/scaleX), (1.0f/scaleY), drawX, drawY);

	XImage *ximage = XCreateImage(display, motifVisual, 24, ZPixmap, 0, (unsigned char *)dest, scaledW, scaledH, 32, scaledW*4);

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

	char *maskBuffer = (char *)malloc(scaledW*scaledH);
	int maskBit = 0;
	int maskValue = 0;
	int maskOffset = 0;
	int hasMask = 0;

	float curX = (x*scaleX);
	float curY = (y*scaleY);

	int i = 0;
	int curYOff = ((int)curY)*w;

	for(int iy = 0; iy < scaledH; iy++) {
		curX = (x*scaleX);
		for(int ix = 0; ix < scaledW; ix++) {
			unsigned int p = src[curYOff+((int)curX)];
			dest[i] = p;
			curX += scaleX;
			if(p & 0xff000000) {
				PUSH_MASK_VALUE(0x00000080);
			} else {
				PUSH_MASK_VALUE(0);
			}
			i++;
		}
		MASK_NEXT_LINE;

		curY += scaleY;
		curYOff = ((int)curY)*w;
	}

	if(bmp->hasMask) {
		Pixmap maskPixmap = XCreatePixmapFromBitmapData(display, XtWindow(motifWindow), maskBuffer, scaledW, scaledH, 1, 0, 1);
		XSetClipOrigin(display, drawingGC, drawX, drawY);
		XSetClipMask(display, drawingGC, maskPixmap);
		XFreePixmap(display, maskPixmap);
	}
	free(maskBuffer);

	Pixmap scaledPixmap = XCreatePixmap(display, XtWindow(motifWindow), scaledW, scaledH, motifDepth < 24 ? motifDepth : 24);
	GC gc = XCreateGC(display, scaledPixmap, 0, 0);
	XPutImage(display, scaledPixmap, gc, ximage, 0, 0, 0, 0, scaledW, scaledH);
	XDestroyImage(ximage);
	XFreeGC(display, gc);
	return scaledPixmap;
}

/**
 * Plot a bitmap
 *
 * Tiled plot of a bitmap image. (x,y) gives the top left
 * coordinate of an explicitly placed tile. From this tile the
 * image can repeat in all four directions -- up, down, left
 * and right -- to the extents given by the current clip
 * rectangle.
 *
 * The bitmap_flags say whether to tile in the x and y
 * directions. If not tiling in x or y directions, the single
 * image is plotted. The width and height give the dimensions
 * the image is to be scaled to.
 *
 * \param ctx The current redraw context.
 * \param bitmap The bitmap to plot
 * \param x The x coordinate to plot the bitmap
 * \param y The y coordiante to plot the bitmap
 * \param width The width of area to plot the bitmap into
 * \param height The height of area to plot the bitmap into
 * \param bg the background colour to alpha blend into
 * \param flags the flags controlling the type of plot operation
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_bitmap(const struct redraw_context *ctx,
		  struct bitmap *bitmap,
		  int x, int y,
		  int width,
		  int height,
		  colour bg,
		  bitmap_flags_t flags)
{
#define mymin(a,b) (a<b)?a:b
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}


	MotifBitmap *bmp = (MotifBitmap *)bitmap;
//printf("framebuffer_plot_bitmap: (%d,%d) %dx%d\n", x, y, width, height);

#ifdef NSMOTIF_USE_GL
	if(bmp->pixmap == None) {
		if(!createPixmap(bmp)) {
			printf("Failed to create pixmap\n");
			return NSERROR_OK;
		}
	}
#endif

	if(bmp->hasMask) {
		XSetClipOrigin(motifDisplay, gw->gc, x, y);
		XSetClipMask(motifDisplay, gw->gc, bmp->mask);
	}

	int srcX = 0;
	int srcY = 0;
	int srcW = bmp->width;
	int srcH = bmp->height;
	int drawX = x;
	int drawY = y;
	int drawW = width;
	int drawH = height;

	if(width == 0 || height == 0) {
		return NSERROR_OK;
	}

	bool repeatX = (flags & BITMAPF_REPEAT_X);
	bool repeatY = (flags & BITMAPF_REPEAT_Y);

	if (repeatX) {
		srcX += (clipRect.x - drawX)%srcW;
		drawX = clipRect.x;
		drawW = clipRect.width;
	}
	if (repeatY) {
		srcY += (clipRect.y - drawY)%srcH;
		drawY = clipRect.y;
		drawH = clipRect.height;
	}

	if(drawX < clipRect.x) {
		srcX += (clipRect.x - drawX);
		drawW -= (clipRect.x - drawX);
		drawX = clipRect.x;
	}
	if(drawY < clipRect.y) {
		srcY += (clipRect.y - drawY);
		drawH -= (clipRect.y - drawY);
		drawY = clipRect.y;
	}

	if(drawX+drawW>clipRect.x+clipRect.width) {
		drawW = (clipRect.x+clipRect.width)-drawX;
	}
	if(drawY+drawH>clipRect.y+clipRect.height) {
		drawH = (clipRect.y+clipRect.height)-drawY;
	}

	// Fast-track the repeating here if possible?
	// Need to look into this, it's crashing w/ BadPixmap
	/* if((repeatX || repeatY) && !bmp->hasMask && width == bmp->width && height == bmp->height) {
printf("Fast-trackign fill of %dx%d region with %dx%d image\n", clipRect.width, clipRect.height, bmp->width, bmp->height);
		XRectangle fillRect;
		fillRect.x = x;
		fillRect.y = y;
		fillRect.width = (clipRect.x + clipRect.width)-x;
		fillRect.height = (clipRect.y + clipRect.height)-y;

		XSetFillStyle(motifDisplay, gw->gc, FillTiled);
		XSetTile(motifDisplay, gw->gc, bmp->pixmap);
		
		XFillRectangle(motifDisplay, TARGET, gw->gc, fillRect.x, fillRect.y, fillRect.width, fillRect.height);

		XSetFillStyle(motifDisplay, gw->gc, FillSolid);
		XSetTile(motifDisplay, gw->gc, None);
		
		return NSERROR_OK;
	}*/

	if((repeatX && drawW>(srcW-srcX)) || (repeatY && drawH>(srcH-srcY))) {
//		printf("drawing repeating %dx%d image into %dx%d area, starting offset %d,%d\n", srcW, srcH, drawW, drawH, srcX, srcY);
//		printf("clip is %d,%d sized %dx%d\n", clipRect.x, clipRect.y, clipRect.width, clipRect.height);
		int curSrcY = srcY;
		for(int rpy = 0; rpy < drawH; rpy += srcH) {
			int curSrcX = srcX;
			for(int rpx = 0; rpx < drawW; rpx += srcW) {
				int actualW = srcW;
				int actualH = srcH;
				if(rpx+srcW > drawW) {
					actualW = (drawW-rpx);
				}
				if(rpy+srcH > drawH) {
					actualH = (drawH-rpy);
				}

				if(actualW > 0 && actualH > 0) {
//printf("drawing rpx=%d rpy=%d\n", rpx, rpy);
					XCopyArea(motifDisplay, bmp->pixmap, TARGET, gw->gc, curSrcX, curSrcY, actualW, actualH, drawX+rpx, drawY+rpy);
				}
				curSrcX = 0;
				if(rpx == 0) rpx -= srcX;
			}
			curSrcY = 0;
			if(rpy == 0) rpy -= srcY;
		}
	} else {
		// Simple boring single-draw
		if(drawW > 0 && drawH > 0) {
			bool hasScale = ((bmp->width != width) || (bmp->height != height)) && (flags == BITMAPF_NONE);

			if(!hasScale) {
				XCopyArea(motifDisplay, bmp->pixmap, TARGET, gw->gc, srcX, srcY, drawW, drawH, drawX, drawY);
			} else {
				float scaleX = ((float)bmp->width)/((float)width);
				float scaleY = ((float)bmp->height)/((float)height);

				Pixmap scaledPixmap = scaleBitmap(bmp, srcX, srcY, srcW, srcH, drawW, drawH, scaleX, scaleY, drawX, drawY, gw->gc);
				XCopyArea(motifDisplay, scaledPixmap, TARGET, gw->gc, 0, 0, drawW, drawH, drawX, drawY);
				XFreePixmap(motifDisplay, scaledPixmap);
			}
		}
	}

	if(bmp->hasMask) {
		XSetClipOrigin(motifDisplay, gw->gc, 0, 0);
		XSetClipMask(motifDisplay, gw->gc, None);
		XSetClipRectangles(motifDisplay, gw->gc, 0, 0, &clipRect, 1, Unsorted);
	}

	return NSERROR_OK;
}


/**
 * Text plotting.
 *
 * \param ctx The current redraw context.
 * \param fstyle plot style for this text
 * \param x x coordinate
 * \param y y coordinate
 * \param text UTF-8 string to plot
 * \param length length of string, in bytes
 * \return NSERROR_OK on success else error code.
 */
static nserror
framebuffer_plot_text(const struct redraw_context *ctx,
		const struct plot_font_style *fstyle,
		int x,
		int y,
		const char *text,
		size_t length)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

	const char *utf8Free = stringToUTF8FreeString(text, length);
	XFontStruct *fontStruct = fontStructForFontStyle(fstyle);

//printf("framebuffer_plot_text (%d, %d): %s\n", x, y, text);
	XSetForeground(motifDisplay, gw->gc, fstyle->foreground);
	XSetFont(motifDisplay, gw->gc, fontStruct->fid);
	XDrawString(motifDisplay, TARGET, gw->gc, x, y, utf8Free, strlen(utf8Free));
	//XDrawString(motifDisplay, TARGET, gw->gc, x, y, text, length);

	free(utf8Free);

	return NSERROR_OK;

}


/** framebuffer plot operation table */
const struct plotter_table fb_plotters = {
	.clip = framebuffer_plot_clip,
	.arc = framebuffer_plot_arc,
	.disc = framebuffer_plot_disc,
	.line = motif_plot_line,
	.rectangle = framebuffer_plot_rectangle,
	.polygon = framebuffer_plot_polygon,
	.path = framebuffer_plot_path,
	.bitmap = framebuffer_plot_bitmap,
	.text = framebuffer_plot_text,
	.option_knockout = true,
};

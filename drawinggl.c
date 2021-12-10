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

#ifdef NSMOTIF_USE_GL

#include <GL/glx.h>

extern Display *motifDisplay;
extern Visual *motifVisual;
extern Widget motifWindow;

static XRectangle clipRect;

#define TARGET XtWindow(gw->drawingArea)

// OpenGL Helper functions / vars
static XFontStruct **cachedFontStructs;
static unsigned int *cachedFontDisplayLists;
static int cachedFontSize = 0;
static int cachedFontCount = 0;

static void set_glcolor(int color) {
		unsigned char a = (color>>24) & 0x00ff;
		unsigned char b = (color>>16) & 0x00ff;
		unsigned char g = (color>>8)&0x00ff;
		unsigned char r = (color>>0)&0x00ff;
		glColor4f(((float)r)/255.0f, ((float)g)/255.0f, ((float)b)/255.0f, 1.0f);
}

/**
 * \brief Sets a clip rectangle for subsequent plot operations.
 *
 * \param ctx The current redraw context.
 * \param clip The rectangle to limit all subsequent plot
 *              operations within.
 * \return NSERROR_OK on success else error code.
 */
static nserror
motifgl_plot_clip(const struct redraw_context *ctx, const struct rect *clip)
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

	glScissor(clip->x0, gw->winHeight-clip->y1, clipRect.width, clipRect.height);
	//printf("glScissor(%d, %d, %d, %d) from %d,%d,%d,%d with win %d,%d\n", clip->x0, gw->winHeight-clip->y1, clipRect.width, clipRect.height, clip->x0, clip->y0, clip->x1, clip->y1, gw->winWidth, gw->winHeight);

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
motifgl_plot_arc(const struct redraw_context *ctx,
	       const plot_style_t *style,
	       int x, int y, int radius, int angle1, int angle2)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

//printf("motifgl_plot_arc\n");
	Display *display = motifDisplay;

//NEEDGL
//	GC gc = gw->gc;
//	XSetForeground(display, gc, style->fill_colour);
//	XDrawArc(display, TARGET, gc, x, y, radius*2, radius*2, angle1<<5, angle2<<5);
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
motifgl_plot_disc(const struct redraw_context *ctx,
		const plot_style_t *style,
		int x, int y, int radius)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

//printf("motifgl_plot_disc\n");
	Display *display = motifDisplay;

//NEEDGL
//	GC gc = gw->gc;
/*	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		XSetForeground(display, gc, style->fill_colour);
		XFillArc(display, TARGET, gc, x, y, radius*2, radius*2, 0<<5, 360<<5);
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		XSetForeground(display, gc, style->stroke_colour);
		XDrawArc(display, TARGET, gc, x, y, radius*2, radius*2, 0<<5, 360<<5);
	}*/
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
motifgl_plot_line(const struct redraw_context *ctx,
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
	//GC gc = gw->gc;
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

		set_glcolor(style->stroke_colour);

		if(dotted) {
			glLineStipple(1, 0xC0C0);
			glEnable(GL_LINE_STIPPLE);
		} else if(dashed) {
			glLineStipple(1, 0xFFF0);
			glEnable(GL_LINE_STIPPLE);
		} else {
			glDisable(GL_LINE_STIPPLE);
		}

		glBegin(GL_LINES);
		glVertex3f(line->x0, line->y0, -2.0f);
		glVertex3f(line->x1, line->y1, -2.0f);
		glEnd();
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
motifgl_plot_rectangle(const struct redraw_context *ctx,
		     const plot_style_t *style,
		     const struct rect *nsrect)
{
	struct gui_window *gw = ctx->priv;
	if(!gw) {
		printf("Null gui_window in redraw_context\n");
		return NSERROR_OK;
	}

//printf("motifgl_plot_rectangle (%d, %d)->(%d, %d) @ %x/%x\n", nsrect->x0, nsrect->y0, nsrect->x1, nsrect->y1, style->fill_colour, style->stroke_colour);
	Display *display = motifDisplay;
	//GC gc = gw->gc;
	XRectangle xrect;
	// TODO: Support dotted lines
	bool dotted = false;
	bool dashed = false;

	xrect.x = nsrect->x0;
	xrect.y = nsrect->y0;
	xrect.width = nsrect->x1-nsrect->x0;
	xrect.height = nsrect->y1-nsrect->y0;


	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		set_glcolor(style->fill_colour);
		glBegin(GL_TRIANGLE_STRIP);
		glVertex3f(xrect.x, xrect.y, -2.0f);
		glVertex3f(xrect.x, xrect.y+xrect.height, -2.0f);
		glVertex3f(xrect.x+xrect.width, xrect.y, -2.0f);
		glVertex3f(xrect.x+xrect.width, xrect.y+xrect.height, -2.0f);
		glEnd();
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		if (style->stroke_type == PLOT_OP_TYPE_DOT) {
			dotted = true;
		}

		if (style->stroke_type == PLOT_OP_TYPE_DASH) {
			dashed = true;
		}

		set_glcolor(style->stroke_colour);

		if(dotted) {
			glLineStipple(1, 0xC0C0);
			glEnable(GL_LINE_STIPPLE);
		} else if(dashed) {
			glLineStipple(1, 0xFFF0);
			glEnable(GL_LINE_STIPPLE);
		} else {
			glDisable(GL_LINE_STIPPLE);
		}

		glBegin(GL_LINE_STRIP);
		glVertex3f(xrect.x, xrect.y, -2.0f);
		glVertex3f(xrect.x+xrect.width, xrect.y, -2.0f);
		glVertex3f(xrect.x+xrect.width, xrect.y+xrect.height, -2.0f);
		glVertex3f(xrect.x, xrect.y+xrect.height, -2.0f);
		glVertex3f(xrect.x, xrect.y, -2.0f);
		glEnd();
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
motifgl_plot_polygon(const struct redraw_context *ctx,
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
	//GC gc = gw->gc;

//printf("motifgl_plot_polygon\n");
	if (style->fill_type != PLOT_OP_TYPE_NONE) {
//NEEDGL
/*		XPoint *points = (XPoint *)malloc(n*sizeof(XPoint));
		for(int i = 0; i < n; i++)
		{
			points[i].x = p[(i<<1)+0];
			points[i].y = p[(i<<1)+1];
		}

		XSetForeground(display, gc, style->fill_colour);
		XSetFillStyle(display, gc, FillSolid);
		XFillPolygon(display, TARGET, gc, points, n, Complex, CoordModeOrigin);
		free(points);*/
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
motifgl_plot_path(const struct redraw_context *ctx,
		const plot_style_t *pstyle,
		const float *p,
		unsigned int n,
		const float transform[6])
{
	NSLOG(netsurf, INFO, "path unimplemented");
//NEEDGL
	return NSERROR_OK;
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
motifgl_plot_bitmap(const struct redraw_context *ctx,
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
//printf("motifgl_plot_bitmap: (%d,%d) %dx%d\n", x, y, width, height);

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

	if(bmp->opaque) {
		glDisable(GL_BLEND);
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

	glPixelStorei(GL_UNPACK_ROW_LENGTH, bmp->width);

	if((repeatX && drawW>(srcW-srcX)) || (repeatY && drawH>(srcH-srcY))) {
//		printf("drawing repeating %dx%d image into %dx%d area, starting offset %d,%d\n", srcW, srcH, drawW, drawH, srcX, srcY);
//		printf("clip is %d,%d sized %dx%d\n", clipRect.x, clipRect.y, clipRect.width, clipRect.height);

		glPixelZoom(1.0f, -1.0f);

		int curSrcY = srcY;
		for(int rpy = 0; rpy < drawH; rpy += srcH) {
			int curSrcX = srcX;
			for(int rpx = 0; rpx < drawW; rpx += srcW) {
				int actualW = srcW;
				int actualH = srcH;
				unsigned char *data = (unsigned char *)bmp->buffer;

				if(rpx+srcW > drawW) {
					actualW = (drawW-rpx);
				}
				if(rpy+srcH > drawH) {
					actualH = (drawH-rpy);
				}

				if(actualW <= 0 || actualH <= 0) {
					continue;
				}

//printf("drawing rpx=%d rpy=%d\n", rpx, rpy);
				if(curSrcX || curSrcY) {
					data = data + (((bmp->width*curSrcY) + curSrcX)*4);
				}

				glRasterPos3f((float)drawX+rpx, (float)drawY+rpy, -2.0f);
				glDrawPixels(actualW, actualH, GL_RGBA, GL_UNSIGNED_BYTE, data);

				curSrcX = 0;
				if(rpx == 0) rpx -= srcX;
			}
			curSrcY = 0;
			if(rpy == 0) rpy -= srcY;
		}
	} else if(drawW > 0 && drawH > 0) {
		// Simple boring single-draw
		
		bool hasScale = ((bmp->width != width) || (bmp->height != height)) && (flags == BITMAPF_NONE);
		unsigned char *data = (unsigned char *)bmp->buffer;

		if(hasScale) {
			float scaleX = ((float)bmp->width)/((float)width);
			float scaleY = ((float)bmp->height)/((float)height);
			glPixelZoom(1.0f/scaleX, -1.0f/scaleY);

			int scaledW = (int)(((float)drawW) * scaleX);
			int scaledH = (int)(((float)drawH) * scaleY);
			srcX = (int)(((float)srcX) * scaleX);
			srcY = (int)(((float)srcY) * scaleY);

			drawW = (int)(((float)drawW) * scaleX);
			drawH = (int)(((float)drawH) * scaleY);
		} else {
			glPixelZoom(1.0f, -1.0f);
		}
		glRasterPos3f((float)drawX, (float)drawY, -2.0f);

		if(srcX > 0 || srcY > 0) {
			data = data + (((bmp->width*srcY) + srcX)*4);
		}
		glDrawPixels(drawW, drawH, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}

	glEnable(GL_BLEND);

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
motifgl_plot_text(const struct redraw_context *ctx,
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

	if(cachedFontSize == 0) {
		cachedFontStructs = (XFontStruct **)malloc(8 * sizeof(XFontStruct *));
		cachedFontDisplayLists = (unsigned int *)malloc(8 * sizeof(unsigned int));
		cachedFontSize = 8;
		cachedFontCount = 0;
	}

	int found = -1;
	for(int i = 0; i < cachedFontCount; i++) {
		if(cachedFontStructs[i] == fontStruct) {
			found = i;
			break;
		}
	}

	if(found == -1) {
		if(cachedFontCount == cachedFontSize) {
			cachedFontSize <<= 1;
			cachedFontStructs = (XFontStruct **)realloc(cachedFontStructs, cachedFontSize * sizeof(XFontStruct *));
			cachedFontDisplayLists = (unsigned int *)realloc(cachedFontDisplayLists, cachedFontSize * sizeof(unsigned int));
		}
		int first = fontStruct->min_char_or_byte2;
		int last = fontStruct->max_char_or_byte2;
		cachedFontStructs[cachedFontCount] = fontStruct;
		cachedFontDisplayLists[cachedFontCount] = glGenLists(last+1);

//		printf("Allocated list base %d for fontStruct %x\n", cachedFontDisplayLists[cachedFontCount], fontStruct);
		glXUseXFont(fontStruct->fid, first, last-first+1, cachedFontDisplayLists[cachedFontCount]+first);

		found = cachedFontCount;
		cachedFontCount++;
	}

	set_glcolor(fstyle->foreground);
	glRasterPos3f(x, y, -2.0f);

//printf("Drawing %s with color 0x%x", utf8Free, fstyle->foreground);
	glListBase(cachedFontDisplayLists[found]);
	glCallLists(strlen(utf8Free), GL_UNSIGNED_BYTE, (unsigned char *)utf8Free);

	free(utf8Free);

	return NSERROR_OK;

}


/** framebuffer plot operation table */
const struct plotter_table motifgl_plotters = {
	.clip = motifgl_plot_clip,
	.arc = motifgl_plot_arc,
	.disc = motifgl_plot_disc,
	.line = motifgl_plot_line,
	.rectangle = motifgl_plot_rectangle,
	.polygon = motifgl_plot_polygon,
	.path = motifgl_plot_path,
	.bitmap = motifgl_plot_bitmap,
	.text = motifgl_plot_text,
	.option_knockout = true,
};

#endif

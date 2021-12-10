/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 *           2008 Vincent Sanders <vince@simtec.co.uk>
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

#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "netsurf/utf8.h"
#include "netsurf/layout.h"
#include "netsurf/plot_style.h"

#include <X11/Xlib.h>
#include <Xm/DrawingA.h>

#include "motif/gui.h"
#include "motif/font.h"

extern Display *motifDisplay;

static int fontsLoaded = 0;

XFontStruct *sansSmallFontStruct = NULL;
XFontStruct *sansMediumFontStruct = NULL;
XFontStruct *sansLargeFontStruct = NULL;
XFontStruct *sansXLargeFontStruct = NULL;

XFontStruct *sansBSmallFontStruct = NULL;
XFontStruct *sansBMediumFontStruct = NULL;
XFontStruct *sansBLargeFontStruct = NULL;
XFontStruct *sansBXLargeFontStruct = NULL;

XFontStruct *serifSmallFontStruct = NULL;
XFontStruct *serifMediumFontStruct = NULL;
XFontStruct *serifLargeFontStruct = NULL;
XFontStruct *serifXLargeFontStruct = NULL;

XFontStruct *serifBSmallFontStruct = NULL;
XFontStruct *serifBMediumFontStruct = NULL;
XFontStruct *serifBLargeFontStruct = NULL;
XFontStruct *serifBXLargeFontStruct = NULL;

XFontStruct *fallbackFontStruct = NULL;

#define XLARGE_CUTOFF 32
#define LARGE_CUTOFF 22
#define MEDIUM_CUTOFF 16

#define GLYPH_LEN		16

uint8_t glyph_x2[GLYPH_LEN * 4];

bool fb_font_init(void)
{
	return true;
}

bool fb_font_finalise(void)
{
	return true;
}

enum fb_font_style
fb_get_font_style(const plot_font_style_t *fstyle)
{
	enum fb_font_style style = FB_REGULAR;

	if (fstyle->weight >= 700)
		style |= FB_BOLD;
	if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE))
		style |= FB_ITALIC;

	return style;
}

int
fb_get_font_size(const plot_font_style_t *fstyle)
{
	int size = fstyle->size * 10 /
			(((nsoption_int(font_min_size) * 3 +
			   nsoption_int(font_size)) / 4) * PLOT_STYLE_SCALE);
	if (size > 2)
		size = 2;
	else if (size <= 0)
		size = 1;

	return size;
}

/** Lookup table to scale 4 bits to 8 bits, so e.g. 0101 --> 00110011 */
const uint8_t glyph_lut[16] = {
		0x00, 0x03, 0x0c, 0x0f,
		0x30, 0x33, 0x3c, 0x3f,
		0xc0, 0xc3, 0xcc, 0xcf,
		0xf0, 0xf3, 0xfc, 0xff
};

static nserror utf8_to_local(const char *string,
				       size_t len,
				       char **result)
{
//printf("utf8_to_local\n");
	return utf8_to_enc(string, "CP1252", len, result);

}

static nserror utf8_from_local(const char *string,
					size_t len,
					char **result)
{
//printf("utf8_from_local\n");
	*result = malloc(len + 1);
	if (*result == NULL) {
		return NSERROR_NOMEM;
	}

	memcpy(*result, string, len);

	(*result)[len] = '\0';

	return NSERROR_OK;
}

inline unsigned int nextCharFromUTF8String(const char *string, int *i) {
	unsigned int c;
	int l = *i;
	if(!(string[l] & 0x80)) {
		c = string[l];
	} else if((string[l] & 0xe0) == 0xc0) {
		c = ((string[l]&0x1f)<<6) + (string[l+1] &0x3f);
		l++;
	} else if((string[l] & 0xf0) == 0xe0) {
		c = ((string[l]&0x0f)<<12) + ((string[l+1] &0x3f)<<6) + (string[l+2] &0x3f);
		l+=2;
	} else if((string[l] & 0xf8) == 0xf0) {
		c = ((string[l]&0x07)<<18) + ((string[l+1] &0x3f)<<12) + ((string[l+2] &0x3f)<<6) + (string[l+3] &0x3f);
		l+=3;
	}

	if(c == 0x2022 || c == 0x2027 || c == 0x2024) {
		c = 0xb7;
	} else if(c >= 0x2010 && c <= 0x2015) {
		c = '-';
	} else if(c == 0x2025) {
		c = 0xa8;
	} else if(c == 0x2018 || c == 0x2019 || c == 0x201b || c == 0x201a) {
		c = '\'';
	} else if(c == 0x201c || c == 0x201d || c == 0x201e || c == 0x201f) {
		c = '"';
	} else if(c >= 0x100) {
// printf("Unknown out-of-bounds character %08x\n", c);
//		c = ' ';
	}

	*i = l;
	return c;
}

const char *stringToUTF8FreeString(const char *string, int length) {
	char *utf8Free = (char *)malloc(length+1);
	int at = 0;
	for(int i = 0 ; i < length; i++) {
		unsigned int c = nextCharFromUTF8String(string, &i);
		if(c < 0x100) {
			utf8Free[at] = c;
			at++;
		}
	}
	utf8Free[at] = 0;
	return utf8Free;
}


/* exported interface documented in framebuffer/freetype_font.h */
nserror
motif_font_width(const plot_font_style_t *fstyle,
	      const char *string,
	      size_t length,
	      int *width)
{
	XFontStruct *fontStruct = fontStructForFontStyle(fstyle);

	const char *utf8Free = stringToUTF8FreeString(string, length);
	*width = XTextWidth(fontStruct, utf8Free, strlen(utf8Free));
	free(utf8Free);

	//*width = XTextWidth(fontStruct, string, length);
//printf("motif_font_width: %s = %d\n", string, *width);
	/*
        while (nxtchr < length) {
		uint32_t ucs4;
		ucs4 = utf8_to_ucs4(string + nxtchr, length - nxtchr);
		if (codepoint_displayable(ucs4)) {
			*width += FB_FONT_WIDTH;
		}

		nxtchr = utf8_next(string, length, nxtchr);
        }

	*width *= fb_get_font_size(fstyle);
	*/
	return NSERROR_OK;
}


/* exported interface documented in framebuffer/freetype_font.h */
nserror
motif_font_position(const plot_font_style_t *fstyle,
		 const char *string,
		 size_t length,
		 int x,
		 size_t *char_offset,
		 int *actual_x)
{
// printf("motif_font_position: %s\n", string);
	int runningWidth = 0;
	int newWidth = 0;

	XFontStruct *fontStruct = fontStructForFontStyle(fstyle);

	for(int i = 0; i < length; i++) {
		int cur = i;
		unsigned int c = nextCharFromUTF8String(string, &i);
		if(c < 0x100) {
			unsigned char ch = c;
			newWidth = XTextWidth(fontStruct, &ch, 1);

			if(runningWidth <= x && runningWidth+newWidth >= x) {
				*char_offset = cur;
				*actual_x = runningWidth;
				return NSERROR_OK;
			}
			runningWidth += newWidth;
		}
	}

	*actual_x = x;
	*char_offset = length;
	return NSERROR_OK;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  fstyle       style for this text
 * \param  string       UTF-8 string to measure
 * \param  length       length of string, in bytes
 * \param  x            width available
 * \param  char_offset  updated to offset in string of actual_x, [1..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 *
 * On exit, char_offset indicates first character after split point.
 *
 * Note: char_offset of 0 should never be returned.
 *
 *   Returns:
 *     char_offset giving split point closest to x, where actual_x <= x
 *   else
 *     char_offset giving split point closest to x, where actual_x > x
 *
 * Returning char_offset == length means no split possible
 */
static nserror
motif_font_split(const plot_font_style_t *fstyle,
	      const char *string,
	      size_t length,
	      int x,
	      size_t *char_offset,
	      int *actual_x)
{
//printf("motif_font_split %s\n", string);

	int linewidth = 0;
	int lastspace = 0;
	int endoftext = 0;
	size_t nxtchr = 0;
	int last_space_x = 0;
	int last_space_idx = 0;
	XFontStruct *fontStruct = fontStructForFontStyle(fstyle);

	int runningWidth = 0;
	int newWidth = 0;

	for(int i = 0; i < length; i++) {
		int cur = i;
		unsigned int c = nextCharFromUTF8String(string, &i);
		if(c < 0x100) {
			if(c == ' ' || c == '\t' || c == '\n') lastspace = cur;
			unsigned char ch = c;
			newWidth = XTextWidth(fontStruct, &ch, 1);

			if(runningWidth <= x && runningWidth+newWidth >= x) {
				*char_offset = lastspace;
				*actual_x = runningWidth;
				return NSERROR_OK;
			}
			runningWidth += newWidth;
		}
	}

	/*
	while(XTextWidth(fontStruct, string, linewidth) < x)
	{
		linewidth++;
		if(string[linewidth] == ' ') lastspace = linewidth;
		if(string[linewidth] == '\t') lastspace = linewidth;
		if(string[linewidth] == '\n') lastspace = linewidth;
		if(linewidth >= length) {
			endoftext = 1;
			break;
		}
	}

	if(endoftext)
	{
		// do nothing
	} else if(lastspace > 0) {
		linewidth = lastspace;
	} else {
		// No space to break on...
		*actual_x = x;
		*char_offset = length;
		return NSERROR_OK;
	}
	*/
	*actual_x = runningWidth;
	*char_offset = length;

//	*actual_x = XTextWidth(fontStruct, string, linewidth);
//	*char_offset = linewidth;

	return NSERROR_OK;
}

XFontStruct *fontStructForFontStyle(plot_font_style_t *style)
{
	if (!fontsLoaded) {
		fontsLoaded = 1;

		fallbackFontStruct = XLoadQueryFont(motifDisplay, "screen14" );
if(!fallbackFontStruct) printf("Failed to load fallback font\n");

		sansSmallFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*" );
if(!sansSmallFontStruct) printf("Failed to load small sans font\n");
		sansMediumFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-medium-r-normal-*-18-*-*-*-*-*-*-*" );
if(!sansMediumFontStruct) printf("Failed to load medium sans font\n");
		sansLargeFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-medium-r-normal-*-24-*-*-*-*-*-*-*" );
if(!sansLargeFontStruct) printf("Failed to load large sans font\n");
		sansXLargeFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-medium-r-normal-*-34-*-*-*-*-*-*-*" );
if(!sansXLargeFontStruct) printf("Failed to load xlarge sans font\n");
		sansBSmallFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-bold-r-normal-*-12-*-*-*-*-*-*-*" );
if(!sansBSmallFontStruct) printf("Failed to load small bold sans font\n");
		sansBMediumFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-bold-r-normal-*-18-*-*-*-*-*-*-*" );
if(!sansBMediumFontStruct) printf("Failed to load medium bold sans font\n");
		sansBLargeFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-bold-r-normal-*-24-*-*-*-*-*-*-*" );
if(!sansBLargeFontStruct) printf("Failed to load large bold sans font\n");
		sansBXLargeFontStruct = XLoadQueryFont(motifDisplay, "*-helvetica-bold-r-normal-*-34-*-*-*-*-*-*-*" );
if(!sansBXLargeFontStruct) printf("Failed to load xlarge bold sans font\n");


		serifSmallFontStruct = XLoadQueryFont(motifDisplay, "*-times-medium-r-normal-*-12-*-*-*-*-*-*-*" );
if(!sansSmallFontStruct) printf("Failed to load small serif font\n");
		serifMediumFontStruct = XLoadQueryFont(motifDisplay, "*-times-medium-r-normal-*-18-*-*-*-*-*-*-*" );
if(!serifMediumFontStruct) printf("Failed to load medium serif font\n");
		serifLargeFontStruct = XLoadQueryFont(motifDisplay, "*-times-medium-r-normal-*-24-*-*-*-*-*-*-*" );
if(!serifLargeFontStruct) printf("Failed to load large serif font\n");
		serifXLargeFontStruct = XLoadQueryFont(motifDisplay, "*-times-medium-r-normal-*-34-*-*-*-*-*-*-*" );
if(!serifXLargeFontStruct) printf("Failed to load xlarge serif font\n");

		serifBSmallFontStruct = XLoadQueryFont(motifDisplay, "*-times-bold-r-normal-*-12-*-*-*-*-*-*-*" );
if(!serifBSmallFontStruct) printf("Failed to load small bold serif font\n");
		serifBMediumFontStruct = XLoadQueryFont(motifDisplay, "*-times-bold-r-normal-*-18-*-*-*-*-*-*-*" );
if(!serifBMediumFontStruct) printf("Failed to load medium bold serif font\n");
		serifBLargeFontStruct = XLoadQueryFont(motifDisplay, "*-times-bold-r-normal-*-24-*-*-*-*-*-*-*" );
if(!serifBLargeFontStruct) printf("Failed to load large bold serif font\n");
		serifBXLargeFontStruct = XLoadQueryFont(motifDisplay, "*-times-bold-r-normal-*-34-*-*-*-*-*-*-*" );
if(!serifBXLargeFontStruct) printf("Failed to load xlarge bold serif font\n");
	}

	if(!style) {
		// Safe enough default?
		return sansMediumFontStruct;
	}

	int fsize = plot_style_fixed_to_int(style->size);

	if(style->family == PLOT_FONT_FAMILY_SANS_SERIF) {
		if(style->weight >= 700) {	// cutoff for bold
			if(fsize >= XLARGE_CUTOFF && sansBXLargeFontStruct) return sansBXLargeFontStruct;
			else if(fsize >= LARGE_CUTOFF && sansBLargeFontStruct) return sansBLargeFontStruct;
			else if(fsize >= MEDIUM_CUTOFF && sansBMediumFontStruct) return sansBMediumFontStruct;
			else if(sansBSmallFontStruct) return sansBSmallFontStruct;
		} else {
			if(fsize >= XLARGE_CUTOFF && sansXLargeFontStruct) return sansXLargeFontStruct;
			else if(fsize >= LARGE_CUTOFF && sansLargeFontStruct) return sansLargeFontStruct;
			else if(fsize >= MEDIUM_CUTOFF && sansMediumFontStruct) return sansMediumFontStruct;
			else if(sansSmallFontStruct) return sansSmallFontStruct;
		}
	} else {
		if(style->weight >= 700) {	// cutoff for bold
			if(fsize >= XLARGE_CUTOFF && serifBXLargeFontStruct) return serifBXLargeFontStruct;
			else if(fsize >= LARGE_CUTOFF && serifBLargeFontStruct) return serifBLargeFontStruct;
			else if(fsize >= MEDIUM_CUTOFF && serifBMediumFontStruct) return serifBMediumFontStruct;
			else if(serifBSmallFontStruct) return serifBSmallFontStruct;
		} else {
			if(fsize >= XLARGE_CUTOFF && serifXLargeFontStruct) return serifXLargeFontStruct;
			else if(fsize >= LARGE_CUTOFF && serifLargeFontStruct) return serifLargeFontStruct;
			else if(fsize >= MEDIUM_CUTOFF && serifMediumFontStruct) return serifMediumFontStruct;
			else if(serifSmallFontStruct) return serifSmallFontStruct;
		}
	}

	printf("Failed to find loaded font for size %d weight %d, returning fallback screen14 (%x)\n", fsize, style->weight, fallbackFontStruct);
	return fallbackFontStruct;
}



static struct gui_layout_table layout_table = {
	.width = motif_font_width,
	.position = motif_font_position,
	.split = motif_font_split,
};

struct gui_layout_table *motif_layout_table = &layout_table;


static struct gui_utf8_table utf8_table = {
	.utf8_to_local = utf8_to_local,
	.local_to_utf8 = utf8_from_local,
};

struct gui_utf8_table *motif_utf8_table = &utf8_table;


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

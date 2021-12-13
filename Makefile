#
# Makefile for NetSurf Framebuffer frontend
#
# This file is part of NetSurf 
#
# ----------------------------------------------------------------------------
# Framebuffer flag setup (using pkg-config)
# ----------------------------------------------------------------------------
NETSURF_FEATURE_RSVG_CFLAGS := -DWITH_RSVG

INCLUDE_DIRS += /usr/include /usr/sgug/include

CFLAGS += -std=c99 -g -v -nostdinc \
	  -Dmotif -Dnsmotif -Dsmall -DNO_IPV6 -Wno-error \
          -Wno-discarded-qualifiers -Wno-missing-prototypes \
          -Wno-implicit-function-declaration -Wno-implicit-fallthrough \
          -Wno-unused-variable -Wno-unused-function -Wno-nested-externs \
          -Wno-missing-declarations \
          -I/usr/sgug/include/libdicl-0.1 -D_SGI_SOURCE \
          -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS 

#CFLAGS += -std=c99 -g -nostdinc \
#	  -Dmotif -Dsmall -DNO_IPV6 -Wno-error -I/usr/include -I/usr/sgug/include/libdicl-0.1 -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS 

$(eval $(call pkg_config_find_and_add_enabled,RSVG,librsvg-2.0,SVG))

#resource path
CFLAGS += '-DNETSURF_MOTIF_RESPATH="$(NETSURF_MOTIF_RESPATH)"'

# compile time font locations
CFLAGS += '-DNETSURF_MOTIF_FONTPATH="$(NETSURF_MOTIF_FONTPATH)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_SANS_SERIF="$(NETSURF_MOTIF_FONT_SANS_SERIF)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_SANS_SERIF_BOLD="$(NETSURF_MOTIF_FONT_SANS_SERIF_BOLD)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_SANS_SERIF_ITALIC="$(NETSURF_MOTIF_FONT_SANS_SERIF_ITALIC)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_SANS_SERIF_ITALIC_BOLD="$(NETSURF_MOTIF_FONT_SANS_SERIF_ITALIC_BOLD)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_SERIF="$(NETSURF_MOTIF_FONT_SERIF)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_SERIF_BOLD="$(NETSURF_MOTIF_FONT_SERIF_BOLD)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_MONOSPACE="$(NETSURF_MOTIF_FONT_MONOSPACE)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_MONOSPACE_BOLD="$(NETSURF_MOTIF_FONT_MONOSPACE_BOLD)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_CURSIVE="$(NETSURF_MOTIF_FONT_CURSIVE)"'
CFLAGS += '-DNETSURF_MOTIF_FONT_FANTASY="$(NETSURF_MOTIF_FONT_FANTASY)"'

#LDFLAGS += -lXm -lXt -lXpm -lX11 -lXext -lPW -lm -ldicl-0.1 -liconv -Wl,--allow-shlib-undefined
# For GL frontend 
LDFLAGS += /usr/lib32/libX11.so.1 /usr/lib32/libXext.a /usr/lib32/libXt.a /usr/lib32/libXm.so.1 /usr/lib32/libXpm.so.1 -lGL -lGLcore -lPW -lm -ldicl-0.1 -liconv -Wl,--allow-shlib-undefined -Wl,-rpath-link=/usr/lib32 -Wl,-rpath=/usr/lib32:/usr/sgug/lib32

# ---------------------------------------------------------------------------
# Target setup
# ---------------------------------------------------------------------------

# The filter and target for split messages
MESSAGES_FILTER=fb
MESSAGES_TARGET=$(FRONTEND_RESOURCES_DIR)

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# S_FRONTEND are sources purely for the motif build
S_FRONTEND := gui.c drawing.c drawinggl.c schedule.c bitmap.c fetch.c download.c \
	findfile.c corewindow.c local_history.c clipboard.c font_internal.c

# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND) $(S_IMAGES) $(S_FONTS)
EXETARGET := nsmotif

# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

NETSURF_MOTIF_RESOURCE_LIST := adblock.css credits.html	\
	default.css internal.css licence.html			\
	netsurf.png quirks.css welcome.html

install-motif:
	$(VQ)echo " INSTALL: $(DESTDIR)$(PREFIX)"
	$(Q)$(INSTALL) -d $(DESTDIR)$(NETSURF_MOTIF_BIN)
	$(Q)$(INSTALL) -T $(EXETARGET) $(DESTDIR)$(NETSURF_MOTIF_BIN)/netsurf-motif
	$(Q)$(INSTALL) -d $(DESTDIR)$(NETSURF_MOTIF_RESOURCES)
	$(Q)for F in $(NETSURF_MOTIF_RESOURCE_LIST); do $(INSTALL) -m 644 $(FRONTEND_RESOURCES_DIR)/$$F $(DESTDIR)/$(NETSURF_MOTIF_RESOURCES); done
	$(Q)$(MKDIR) -p $(DESTDIR)$(NETSURF_MOTIF_RESOURCES)/icons
	$(Q)$(INSTALL) -d $(FRONTEND_RESOURCES_DIR)icons/* $(DESTDIR)$(NETSURF_MOTIF_RESOURCES)icons/
	$(Q)$(MKDIR) -p $(DESTDIR)$(NETSURF_MOTIF_RESOURCES)throbber
	$(Q)$(INSTALL) -m 0644 $(FRONTEND_RESOURCES_DIR)/throbber/*.png $(DESTDIR)$(NETSURF_MOTIF_RESOURCES)throbber/
	$(Q)$(RM) $(DESTDIR)$(NETSURF_MOTIF_RESOURCES)Messages
	$(Q)$(SPLIT_MESSAGES) -l en -p fb -f messages -o $(DESTDIR)$(NETSURF_MOTIF_RESOURCES)Messages -z resources/FatMessages

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package-motif:

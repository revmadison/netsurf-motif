#
# Makefile for NetSurf Framebuffer frontend
#
# This file is part of NetSurf 
#
# ----------------------------------------------------------------------------
# Framebuffer flag setup (using pkg-config)
# ----------------------------------------------------------------------------
NETSURF_FEATURE_RSVG_CFLAGS := -DWITH_RSVG

CFLAGS += -std=c99 -g \
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

#LDFLAGS := -Wl,-rpath=/usr/lib32 -L/usr/lib32 -lSgm -lXm -lXt -lX11 -lGLcore -lXext -lPW -L/usr/sgug/lib32 -ljpeg -liconv -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lcss -lparserutils -lwapcaplet -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -ldom -lexpat -lhubbub -lparserutils -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lnsutils -lz -lcurl -l:libssl.so.1.1 -l:libcrypto.so.1.1 -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lutf8proc  -lwebp  -lpng16 -lz  -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lnsbmp  -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lnsgif  -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lsvgtiny -ldom -lexpat -lhubbub -lparserutils  -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lnspsl  -L/usr/people/bcs/rpmbuild/BUILD/netsurf-all-3.10/inst-motif/lib -lnslog -lm -ldicl-0.1 -liconv -Wl,--allow-shlib-undefined

#LDFLAGS += /usr/lib32/libXext.so /usr/lib32/libXt.so /usr/lib32/libXmu.so /usr/lib32/libX11.so.1 /usr/lib32/libSgm.so /usr/lib32/libXm.so.1 /usr/lib32/libGLcore.so -lPW -lm -ldicl-0.1 -liconv -Wl,--allow-shlib-undefined
LDFLAGS += -lXm -lXt -lXpm -lX11 -lXext -lPW -lm -ldicl-0.1 -liconv -Wl,--allow-shlib-undefined

#TEMPLD = $(LDFLAGS)
#LDFLAGS := -L/usr/lib32 -lXt -lSgm -l:libXm.so.1 -l:libX11.so.1 -lGLcore -lPW -L/usr/sgug/lib32 -lm -ldicl-0.1 -liconv $(TEMPLD)



# ---------------------------------------------------------------------------
# Target setup
# ---------------------------------------------------------------------------

# The filter and target for split messages
MESSAGES_FILTER=fb
MESSAGES_TARGET=$(FRONTEND_RESOURCES_DIR)

# ---------------------------------------------------------------------------
# HOST specific feature flags
# ---------------------------------------------------------------------------

# enable POSIX and XSI feature flasg except:
#   - the default set on freebsd already has them enabled
#   - openbsd does not require the default source flags
#ifneq ($(HOST),FreeBSD)
#  ifneq ($(HOST),OpenBSD)
#    CFLAGS += -D_POSIX_C_SOURCE=200809L \
	      -D_XOPEN_SOURCE=700 \
	      -D_BSD_SOURCE \
	      -D_DEFAULT_SOURCE \
	      -D_NETBSD_SOURCE
 # else
  #  CFLAGS += -D_POSIX_C_SOURCE=200809L
  #endif
#endif

# ----------------------------------------------------------------------------
# built-in resource setup
# ----------------------------------------------------------------------------

FB_IMAGE_left_arrow := icons/back.png
FB_IMAGE_right_arrow := icons/forward.png
FB_IMAGE_reload := icons/reload.png
FB_IMAGE_stop_image := icons/stop.png
FB_IMAGE_history_image := icons/history.png

FB_IMAGE_left_arrow_g := icons/back_g.png
FB_IMAGE_right_arrow_g := icons/forward_g.png
FB_IMAGE_reload_g := icons/reload_g.png
FB_IMAGE_stop_image_g := icons/stop_g.png
FB_IMAGE_history_image_g := icons/history_g.png

FB_IMAGE_scrolll := icons/scrolll.png
FB_IMAGE_scrollr := icons/scrollr.png
FB_IMAGE_scrollu := icons/scrollu.png
FB_IMAGE_scrolld := icons/scrolld.png

FB_IMAGE_osk_image := icons/osk.png

FB_IMAGE_pointer_image := pointers/default.png
FB_IMAGE_hand_image := pointers/point.png
FB_IMAGE_caret_image := pointers/caret.png
FB_IMAGE_menu_image := pointers/menu.png
FB_IMAGE_progress_image := pointers/progress.png
FB_IMAGE_move_image := pointers/move.png

FB_IMAGE_throbber0 := throbber/throbber0.png
FB_IMAGE_throbber1 := throbber/throbber1.png
FB_IMAGE_throbber2 := throbber/throbber2.png
FB_IMAGE_throbber3 := throbber/throbber3.png
FB_IMAGE_throbber4 := throbber/throbber4.png
FB_IMAGE_throbber5 := throbber/throbber5.png
FB_IMAGE_throbber6 := throbber/throbber6.png
FB_IMAGE_throbber7 := throbber/throbber7.png
FB_IMAGE_throbber8 := throbber/throbber8.png

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# S_FRONTEND are sources purely for the framebuffer build
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

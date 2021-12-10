/*
 * Copyright 2008 Michael Lester <element3260@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/nsurl.h"
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/string.h"
#include "desktop/download.h"
#include "netsurf/download.h"

#include <Xm/Xm.h>
#include <Xm/FileSB.h>

#include "motif/download.h"

struct download_context;

extern Widget motifWindow;

typedef enum {
	MOTIF_DOWNLOAD_NONE,
	MOTIF_DOWNLOAD_SELECTING_DOWNLOADING,	// Name not yet selected, file still downloading
	MOTIF_DOWNLOAD_SELECTED_DOWNLOADING,	// Name selected, file still downloading
	MOTIF_DOWNLOAD_SELECTING_DOWNLOADED,	// Name not yet selected, file downloaded
	MOTIF_DOWNLOAD_ERROR,
	MOTIF_DOWNLOAD_COMPLETE,
	MOTIF_DOWNLOAD_CANCELED
} motif_download_status;

/**
 * context for each download.
 */
struct gui_download_window {
	struct download_context *ctx;
	motif_download_status status;

	char *temporaryName;
	char *selectedName;
	unsigned long long int totalSize;
	unsigned long long int downloaded;

	FILE *fp;

	Widget dialog;
};


static void completeDownload(struct gui_download_window *dw) {
	if(dw->fp) {
		fclose(dw->fp);
		dw->fp = NULL;
	}
	dw->status = MOTIF_DOWNLOAD_COMPLETE;
	if(rename(dw->temporaryName, dw->selectedName)) {
		printf("Failed to rename %s to %s. Download may be lost.\n", dw->temporaryName, dw->selectedName);
		// TODO: This can / will fail if selecting a different filesystem, need to support a fallback of opening old file writing contents into new file, etc.
	} else {
		printf("Successfully saved as %s\n", dw->selectedName);
	}
}


static void freeDownloadWindow(struct gui_download_window *dw) {
	XtUnmanageChild(dw->dialog);
	XtDestroyWidget(dw->dialog);

	download_context_destroy(dw->ctx);

	if(dw->fp) {
		fclose(dw->fp);
		dw->fp = NULL;
		if(dw->temporaryName) {
			remove(dw->temporaryName);
		}
	}
	if(dw->temporaryName) {
		free(dw->temporaryName);
		dw->temporaryName = NULL;
	}
	if(dw->selectedName) {
		free(dw->selectedName);
		dw->selectedName = NULL;
	}
	free(dw);
}

static char *stringFromXmString(XmString xmString) {
	XmStringContext context;
	char buffer[1024];
	char *text;		
	XmStringCharSet charset;
	XmStringDirection direction;
	XmStringComponentType unknownTag;
	unsigned short *unknownLen;
	unsigned char *unknownData;
	XmStringComponentType type;

	if(!XmStringInitContext(&context, xmString)) {
		printf("Failed to convert compound string\n");
		return strdup("");
	}

	buffer[0] = 0;
	while((type = XmStringGetNextComponent(context, &text, &charset, &direction, &unknownTag, &unknownLen, &unknownData)) != XmSTRING_COMPONENT_END) {
		if(type == XmSTRING_COMPONENT_TEXT || type == XmSTRING_COMPONENT_LOCALE_TEXT) {
			//printf("Text component: '%s'\n", text);
			if(strlen(buffer)+strlen(text) > 1023) {
				XtFree(text);
				break;
			}
			strcat(buffer, text);
			XtFree(text);
		} else if(type == XmSTRING_COMPONENT_SEPARATOR) {
			if(strlen(buffer) >= 1023) {
				break;
			}
			strcat(buffer, "\n");
		}
	}

	XmStringFreeContext(context);
	return strdup(buffer);
}


static void filenameSelectedCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	struct gui_download_window *dw;
	XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;
	XtVaGetValues(widget, XmNuserData, &dw, NULL);
	if(!dw) {
		printf("Selected filename for download with null data.\n");
		return;
	}

	char *dirname = stringFromXmString(cbs->dir);
	char *filename = stringFromXmString(cbs->value);

	//printf("Selected filename '%s''%s'\n", dirname, filename);
	
	dw->selectedName = (char *)malloc(strlen(dirname)+strlen(filename)+2);
	if(dirname[strlen(dirname)-1] != '/') {
		sprintf(dw->selectedName, "%s/%s", dirname, filename);
	} else {
		sprintf(dw->selectedName, "%s%s", dirname, filename);
	}
/* OLDXM
	char *filename = (char *)XmStringUnparse(cbs->value, XmFONTLIST_DEFAULT_TAG, XmCHARSET_TEXT, XmCHARSET_TEXT, NULL, 0, XmOUTPUT_ALL);
	if(!filename) {
		printf("Error receiving the filename from the file selection box!n");
		return;
	}
	dw->selectedName = strdup(filename);
	XtFree(filename);
*/
	printf("Selected to download to %s\n", dw->selectedName);

	if(dw->status == MOTIF_DOWNLOAD_SELECTING_DOWNLOADING) {
		dw->status = MOTIF_DOWNLOAD_SELECTED_DOWNLOADING;
	} else if(dw->status == MOTIF_DOWNLOAD_SELECTING_DOWNLOADED) {
		completeDownload(dw);
		freeDownloadWindow(dw);
	}

}

static void downloadCancelCallback(Widget widget, XtPointer client_data, XtPointer call_data) {
	struct gui_download_window *dw;
	XtVaGetValues(widget, XmNuserData, &dw, NULL);
	if(!dw) {
		printf("Canceled download with null data.\n");
		return;
	}

	dw->status = MOTIF_DOWNLOAD_CANCELED;
	printf("Chose to cancel the download.\n");

	download_context_abort(dw->ctx);
	freeDownloadWindow(dw);
}


/**
 * core callback on creating a new download
 */
static struct gui_download_window *
gui_download_window_create(download_context *ctx, struct gui_window *gui)
{
	nsurl *url;
	unsigned long long int total_size;
	char *domain;
	char *destination;
	bool unknown_size;
	struct gui_download_window *download;
	const char *size;
	Arg args[32];
	int n = 0;
	char buffer[1024];

	url = download_context_get_url(ctx);
	total_size = download_context_get_total_length(ctx);
	unknown_size = total_size == 0;

	download = malloc(sizeof *download);
	if (download == NULL) {
		return NULL;
	}

	download->status = MOTIF_DOWNLOAD_SELECTING_DOWNLOADING;
	download->selectedName = NULL;

	/* set the domain to the host component of the url if it exists */
	if (nsurl_has_component(url, NSURL_HOST)) {
		domain = strdup(nsurl_get_component(url, NSURL_HOST));
	}

	n = 0;
	const char *homedir = getenv("HOME");
	XmString dir = XmStringCreate(homedir, "homedir");
	XtSetArg(args[n], XmNdirectory, dir); n++;
	XtSetArg(args[n], XmNfileTypeMask, XmFILE_REGULAR); n++;
// OLDXM	XtSetArg(args[n], XmNpathMode, XmPATH_MODE_RELATIVE); n++;
	XtSetArg(args[n], XmNuserData, download); n++;
	Widget dialog = XmCreateFileSelectionDialog(motifWindow, "filesb", args, n);
	XtAddCallback(dialog, XmNcancelCallback, downloadCancelCallback, NULL);
	XtAddCallback(dialog, XmNokCallback, filenameSelectedCallback, NULL);

	Widget textField = XmFileSelectionBoxGetChild(dialog, XmDIALOG_TEXT);
	if(textField) {
		XmTextFieldSetString(textField, download_context_get_filename(ctx));
	}

	XtManageChild(dialog);
	XmStringFree(dir);

	download->dialog = dialog;

	/* show the dialog */

	download->ctx = ctx;
	download->temporaryName = strdup(tmpnam(buffer));
	download->selectedName = NULL;//strdup(download_context_get_filename(ctx));
	download->totalSize = total_size;
	download->downloaded = 0;
	download->fp = fopen(download->temporaryName, "wb");

	printf("Downloading %s to %s for now, size %lld bytes\n", download_context_get_filename(ctx), download->temporaryName, total_size);

	free(destination);

	return download;
}


/**
 * core callback on receipt of data
 */
static nserror
gui_download_window_data(struct gui_download_window *dw,
			 const char *data,
			 unsigned int size)
{
	if(dw->status == MOTIF_DOWNLOAD_CANCELED || dw->status == MOTIF_DOWNLOAD_ERROR) {
		// We've canceled this, ignore any data incoming;
		return NSERROR_INVALID;
	}
	if(!dw->fp) {
		printf("Received data for closed download.\n");
		return NSERROR_INVALID;
	}

	fwrite(data, 1, size, dw->fp);
	dw->downloaded += size;
	printf("received %d more bytes, %lld/%lld downloaded\n", size, dw->downloaded, dw->totalSize);

	return NSERROR_OK;
}


/**
 * core callback on error
 */
static void
gui_download_window_error(struct gui_download_window *dw, const char *error_msg)
{
	printf("Failed to download %s\n", error_msg);
	dw->status = MOTIF_DOWNLOAD_ERROR;
	freeDownloadWindow(dw);
}


/**
 * core callback when core download is complete
 */
static void gui_download_window_done(struct gui_download_window *dw)
{
	fclose(dw->fp);
	printf("Completed download. %lld total bytes.\n", dw->downloaded);

	if(dw->status == MOTIF_DOWNLOAD_SELECTING_DOWNLOADING) {
		dw->status = MOTIF_DOWNLOAD_SELECTING_DOWNLOADED;
	} else if(dw->status == MOTIF_DOWNLOAD_SELECTED_DOWNLOADING) {
		completeDownload(dw);
		freeDownloadWindow(dw);
	}
}


static struct gui_download_table download_table = {
	.create = gui_download_window_create,
	.data = gui_download_window_data,
	.error = gui_download_window_error,
	.done = gui_download_window_done,
};

struct gui_download_table *motif_download_table = &download_table;

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2005 Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libbonoboui.h>
#include <libgnomevfs/gnome-vfs-monitor.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <glade/glade.h>

#include "auto-completion.h"
#include "bookmarks.h"
#include "catalog.h"
#include "catalog-list.h"
#include "comments.h"
#include "dir-list.h"
#include "dlg-bookmarks.h"
#include "dlg-categories.h"
#include "dlg-comment.h"
#include "dlg-file-utils.h"
#include "dlg-image-prop.h"
#include "dlg-save-image.h"
#include "file-utils.h"
#include "gconf-utils.h"
#include "glib-utils.h"
#include "gth-browser.h"
#include "gth-browser-ui.h"
#include "gth-exif-data-viewer.h"
#include "gth-exif-utils.h"
#include "gth-file-list.h"
#include "gth-file-view.h"
#include "gth-pixbuf-op.h"
#include "gth-toggle-button.h"
#include "gthumb-info-bar.h"
#include "gthumb-preloader.h"
#include "gthumb-stock.h"
#include "gtk-utils.h"
#include "image-viewer.h"
#include "jpeg-utils.h"
#include "main.h"
#include "nav-window.h"
#include "pixbuf-utils.h"
#include "thumb-cache.h"

#ifdef HAVE_LIBEXIF
#include <libexif/exif-data.h>
#include "jpegutils/jpeg-data.h"
#endif /* HAVE_LIBEXIF */

#include "icons/pixbufs.h"
#include "icons/nav_button.xpm"

#define GCONF_NOTIFICATIONS 18

typedef enum {
	GTH_BROWSER_GO_TO,
	GTH_BROWSER_GO_BACK,
	GTH_BROWSER_GO_FORWARD
} WindowGoOp;

typedef enum {
	MONITOR_EVENT_FILE_CREATED = 0,
	MONITOR_EVENT_FILE_DELETED,
	MONITOR_EVENT_DIR_CREATED,
	MONITOR_EVENT_DIR_DELETED,
	MONITOR_EVENT_FILE_CHANGED,
	MONITOR_EVENT_NUM
} MonitorEventType;

struct _GthBrowserPrivateData {
	BonoboUIComponent  *ui_component;
	GtkUIManager       *ui;
	GtkActionGroup     *actions;
	GtkActionGroup     *bookmark_actions;
	GtkActionGroup     *history_actions;
	guint               sidebar_merge_id;
	guint               toolbar_merge_id;
	guint               bookmarks_merge_id;
	guint               history_merge_id;

	GtkToolItem        *go_back_tool_item;

	GtkWidget          *toolbar;
	GtkWidget          *statusbar;

	GtkWidget          *viewer;
	GtkWidget          *viewer_container;  /* Container widget for the 
						* viewer.  Used by fullscreen 
						* in order to reparent the 
						* viewer.*/
	GtkWidget          *main_pane;
	GtkWidget          *content_pane;
	GtkWidget          *image_pane;
	GtkWidget          *dir_list_pane;
	GtkWidget          *file_list_pane;
	GtkWidget          *notebook;
	GtkWidget          *location_entry;
	GtkWidget          *viewer_vscr;
	GtkWidget          *viewer_hscr;
	GtkWidget          *viewer_event_box;
	GtkWidget          *show_folders_toolbar_button;
	GtkWidget          *show_catalog_toolbar_button;

	GtkWidget          *file_popup_menu;
	GtkWidget          *image_popup_menu;
	GtkWidget          *fullscreen_image_popup_menu;
	GtkWidget          *catalog_popup_menu;
	GtkWidget          *library_popup_menu;
	GtkWidget          *dir_popup_menu;
	GtkWidget          *dir_list_popup_menu;
	GtkWidget          *catalog_list_popup_menu;
	GtkWidget          *history_list_popup_menu;


	GtkWidget          *image_comment;
	GtkWidget          *exif_data_viewer;

	GtkWidget          *progress;              /* statusbar widgets. */
	GtkWidget          *image_info;
	GtkWidget          *image_info_frame;
	GtkWidget          *zoom_info;
	GtkWidget          *zoom_info_frame;

	GtkWidget          *image_prop_dlg;        /* no-modal dialogs. */
	GtkWidget          *comment_dlg;
	GtkWidget          *categories_dlg;
	GtkWidget          *bookmarks_dlg;

	GtkWidget          *info_bar;
	GtkWidget          *info_combo;
	GtkWidget          *info_icon;

	char                sidebar_content;       /* SidebarContent values. */
	int                 sidebar_width;
	gboolean            sidebar_visible;
	guint               layout_type : 2;

	gboolean            image_pane_visible;

	GtkWidget          *preview_widget_image;
	GtkWidget          *preview_widget_data_comment;
	GtkWidget          *preview_widget_data;
	GtkWidget          *preview_widget_comment;

	GtkWidget          *preview_button_image;
	GtkWidget          *preview_button_data;
	GtkWidget          *preview_button_comment;

	gboolean            preview_visible;
	GthPreviewContent   preview_content;
	gboolean            image_data_visible;

	/* bookmarks & history */

	int                 bookmarks_length;
	Bookmarks          *history;
	GList              *history_current;
	int                 history_length;
	WindowGoOp          go_op;

	/* browser stuff */

	GthFileList        *file_list;
	DirList            *dir_list;
	CatalogList        *catalog_list;
	char               *catalog_path;       /* The catalog file we are 
						 * showing in the file list. */
	char               *image_path;         /* The image file we are 
						 * showing in the image 
						 * viewer. */
	char               *new_image_path;     /* The image to load after
						 * asking whether to save
						 * the current image. */
	time_t              image_mtime;        /* Modification time of loaded
						 * image, used to reload the 
						 * image only when needed.*/
	char               *image_catalog;      /* The catalog the current 
						 * image belongs to, NULL if 
						 * the image is not from a 
						 * catalog. */
	gboolean            image_error;        /* An error occurred loading 
						 * the current image. */

	gfloat              dir_load_progress;
	int                 activity_ref;       /* when > 0 some activity
						 * is present. */

	char               *initial_location;

	gboolean            image_modified;
	ImageSavedFunc      image_saved_func;
	gboolean            setting_file_list;
	gboolean            can_set_file_list;
	gboolean            changing_directory;
	gboolean            can_change_directory;
	gboolean            refreshing;         /* true if we are refreshing
						 * the file list.  Used to 
						 * handle the refreshing case 
						 * in a special way. */
	gboolean            saving_modified_image;

	guint               activity_timeout;   /* activity timeout handle. */
	guint               load_dir_timeout;
	guint               sel_change_timeout;
	guint               busy_cursor_timeout;
	guint               auto_load_timeout;
	guint               update_layout_timeout;
	
	GThumbPreloader    *preloader;

	/* viewer stuff */

	gboolean            fullscreen;         /* whether the fullscreen mode
						 * is active. */
	guint               view_image_timeout; /* timer for the 
						 * view_image_at_pos function.
						 */
	guint               slideshow_timeout;  /* slideshow timer. */
	gboolean            slideshow;          /* whether the slideshow is 
						 * active. */
	GList              *slideshow_set;      /* FileData list of the 
						 * images to display in the 
						 * slideshow. */
	GList              *slideshow_random_set;
	GList              *slideshow_first;
	GList              *slideshow_current;

#ifdef HAVE_LIBEXIF
	ExifData           *exif_data;
#endif /* HAVE_LIBEXIF */

	/* monitor stuff */

	GnomeVFSMonitorHandle *monitor_handle;
	guint                  monitor_enabled : 1;
	guint                  update_changes_timeout;
	GList                 *monitor_events[MONITOR_EVENT_NUM]; /* char * lists */

	/* misc */

	guint                  cnxn_id[GCONF_NOTIFICATIONS];
	GthPixbufOp           *pixop;
	
	GladeXML              *progress_gui;
	GtkWidget             *progress_dialog;
	GtkWidget             *progress_progressbar;
	GtkWidget             *progress_info;
	guint                  progress_timeout;

	GtkTooltips           *tooltips;
	guint                  help_message_cid;
	guint                  list_info_cid;

	gboolean               focus_location_entry;

	gboolean               first_time_show;
};

static GthWindowClass *parent_class = NULL;


#define ACTIVITY_DELAY         200
#define BUSY_CURSOR_DELAY      200
#define DISPLAY_PROGRESS_DELAY 750
#define HIDE_SIDEBAR_DELAY     150
#define LOAD_DIR_DELAY         75
#define PANE_MIN_SIZE          60
#define PROGRESS_BAR_WIDTH     60
#define SEL_CHANGED_DELAY      150
#define UPDATE_DIR_DELAY       500
#define VIEW_IMAGE_DELAY       25
#define AUTO_LOAD_DELAY        750
#define UPDATE_LAYOUT_DELAY    250

#define DEF_WIN_WIDTH          690
#define DEF_WIN_HEIGHT         460
#define DEF_SIDEBAR_SIZE       255
#define DEF_SIDEBAR_CONT_SIZE  190
#define DEF_SLIDESHOW_DELAY    4
#define PRELOADED_IMAGE_MAX_SIZE (1.5*1024*1024)
#define PRELOADED_IMAGE_MAX_DIM1 (3000*3000)
#define PRELOADED_IMAGE_MAX_DIM2 (1500*1500)

#define GLADE_EXPORTER_FILE    "gthumb_png_exporter.glade"
#define HISTORY_LIST_MENU      "/MenuBar/Go/HistoryList"
#define HISTORY_LIST_POPUP     "/HistoryListPopup"

enum {
	NAME_COLUMN,
	VALUE_COLUMN,
	POS_COLUMN,
	NUM_COLUMNS
};

enum {
	TARGET_PLAIN,
	TARGET_PLAIN_UTF8,
	TARGET_URILIST,
};


static GtkTargetEntry target_table[] = {
	{ "text/uri-list", 0, TARGET_URILIST },
	/*
	  { "text/plain;charset=UTF-8",    0, TARGET_PLAIN_UTF8 },
	  { "text/plain",    0, TARGET_PLAIN }
	*/
};

/* FIXME
   static GtkTargetEntry same_app_target_table[] = {
   { "text/uri-list", GTK_TARGET_SAME_APP, TARGET_URILIST },
   { "text/plain",    GTK_TARGET_SAME_APP, TARGET_PLAIN }
   };
*/


static GtkTreePath  *dir_list_tree_path = NULL;
static GtkTreePath  *catalog_list_tree_path = NULL;
static GList        *gth_file_list_drag_data = NULL;
static char         *dir_list_drag_data = NULL;





static void 
set_action_sensitive (GthBrowser  *browser,
		      const char  *action_name,
		      gboolean     sensitive)
{
	GtkAction *action;

	action = gtk_action_group_get_action (browser->priv->actions, action_name);
	g_object_set (action, "sensitive", sensitive, NULL);
}


static void
set_action_active (GthBrowser  *browser,
		   char        *action_name,
		   gboolean     is_active)
{
	GtkAction *action;

	action = gtk_action_group_get_action (browser->priv->actions, action_name);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), is_active);
}


static void
set_action_active_if_different (GthBrowser  *browser,
				char        *action_name,
				gboolean     is_active)
{
	GtkAction       *action;
	GtkToggleAction *toggle_action;

	action = gtk_action_group_get_action (browser->priv->actions, action_name);
	toggle_action = GTK_TOGGLE_ACTION (action);

	if (gtk_toggle_action_get_active (toggle_action) != is_active)
		gtk_toggle_action_set_active (toggle_action, is_active);
}


static void
set_action_important (GthBrowser  *browser,
		      char        *action_name,
		      gboolean     is_important)
{
	GtkAction *action;

	action = gtk_ui_manager_get_action (browser->priv->ui, action_name);
	if (action != NULL) {
		g_object_set (action, "is_important", is_important, NULL);
		g_object_unref (action);
	}
}





static void window_update_zoom_sensitivity (GthBrowser *browser);


static void
window_update_statusbar_zoom_info (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	const char            *path;
	gboolean               image_is_visible;
	int                    zoom;
	char                  *text;

	window_update_zoom_sensitivity (browser);

	/**/

	path = priv->image_path;

	image_is_visible = (path != NULL) && !priv->image_error && ((priv->sidebar_visible && priv->image_pane_visible && priv->preview_content == GTH_PREVIEW_CONTENT_IMAGE) || ! priv->sidebar_visible);

	if (! image_is_visible) {
		if (! GTK_WIDGET_VISIBLE (priv->zoom_info_frame))
			return;
		gtk_widget_hide (priv->zoom_info_frame);
		return;
	} 

	if (! GTK_WIDGET_VISIBLE (priv->zoom_info_frame)) 
		gtk_widget_show (priv->zoom_info_frame);

	zoom = (int) (IMAGE_VIEWER (priv->viewer)->zoom_level * 100.0);
	text = g_strdup_printf (" %d%% ", zoom);
	gtk_label_set_markup (GTK_LABEL (priv->zoom_info), text);
	g_free (text);
}


static void
window_update_statusbar_image_info (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *text;
	char                   time_txt[50], *utf8_time_txt;
	char                  *size_txt;
	char                  *file_size_txt;
	const char            *path;
	int                    width, height;
	time_t                 timer = 0;
	struct tm             *tm;
	gdouble                sec;

	path = priv->image_path;

	if ((path == NULL) || priv->image_error) {
		gtk_label_set_text (GTK_LABEL (priv->image_info), "");
		return;
	}

	if (!image_viewer_is_void (IMAGE_VIEWER (priv->viewer))) {
		width = image_viewer_get_image_width (IMAGE_VIEWER (priv->viewer));
		height = image_viewer_get_image_height (IMAGE_VIEWER (priv->viewer)); 
	} else {
		width = 0;
		height = 0;
	}

#ifdef HAVE_LIBEXIF
	timer = get_exif_time (path);
#endif
	if (timer == 0)
		timer = get_file_mtime (path);
	tm = localtime (&timer);
	strftime (time_txt, 50, _("%d %B %Y, %H:%M"), tm);
	utf8_time_txt = g_locale_to_utf8 (time_txt, -1, 0, 0, 0);
	sec = g_timer_elapsed (image_loader_get_timer (IMAGE_VIEWER (priv->viewer)->loader),  NULL);

	size_txt = g_strdup_printf (_("%d x %d pixels"), width, height);
	file_size_txt = gnome_vfs_format_file_size_for_display (get_file_size (path));

	/**/

	if (! priv->image_modified)
		text = g_strdup_printf (" %s - %s - %s     ",
					size_txt,
					file_size_txt,
					utf8_time_txt);
	else
		text = g_strdup_printf (" %s - %s     ", 
					_("Modified"),
					size_txt);

	gtk_label_set_markup (GTK_LABEL (priv->image_info), text);

	/**/

	g_free (size_txt);
	g_free (file_size_txt);
	g_free (text);
	g_free (utf8_time_txt);
}


static void
update_image_comment (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	CommentData   *cdata;
	char          *comment;
	GtkTextBuffer *text_buffer;

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->image_comment));

	if (priv->image_path == NULL) {
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		gtk_text_buffer_delete (text_buffer, &start, &end);
                return;
	}

	cdata = comments_load_comment (priv->image_path);

	if (cdata == NULL) {
		GtkTextIter  iter;
		const char  *click_here = _("[Press 'c' to add a comment]");
		GtkTextIter  start, end;
		GtkTextTag  *tag;

		gtk_text_buffer_set_text (text_buffer, click_here, strlen (click_here));
		gtk_text_buffer_get_iter_at_line (text_buffer, &iter, 0);
		gtk_text_buffer_place_cursor (text_buffer, &iter);

		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		tag = gtk_text_buffer_create_tag (text_buffer, NULL, 
						  "style", PANGO_STYLE_ITALIC,
						  NULL);
		gtk_text_buffer_apply_tag (text_buffer, tag, &start, &end);

		return;
	}

	comment = comments_get_comment_as_string (cdata, "\n\n", " - ");

	if (comment != NULL) {
		GtkTextIter iter;
		gtk_text_buffer_set_text (text_buffer, comment, strlen (comment));
		gtk_text_buffer_get_iter_at_line (text_buffer, &iter, 0);
		gtk_text_buffer_place_cursor (text_buffer, &iter);
	} else {
		GtkTextIter start, end;
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		gtk_text_buffer_delete (text_buffer, &start, &end);
	}

	g_free (comment);
	comment_data_free (cdata);
}


static void
window_update_image_info (GthBrowser *browser)
{
	window_update_statusbar_image_info (browser);
	window_update_statusbar_zoom_info (browser);
	gth_exif_data_viewer_update (GTH_EXIF_DATA_VIEWER (browser->priv->exif_data_viewer), 
				     IMAGE_VIEWER (browser->priv->viewer),
				     browser->priv->image_path);
	update_image_comment (browser);
}


static void
window_update_infobar (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char       *text;
	const char *path;
	char       *escaped_name;
	char       *utf8_name;
	int         images, current;

	path = priv->image_path;
	if (path == NULL) {
		gthumb_info_bar_set_text (GTHUMB_INFO_BAR (priv->info_bar), NULL, NULL);
		return;
	}

	images = gth_file_list_get_length (priv->file_list);

	current = gth_file_list_pos_from_path (priv->file_list, path) + 1;

	utf8_name = g_filename_to_utf8 (file_name_from_path (path), -1, 0, 0, 0);
	escaped_name = g_markup_escape_text (utf8_name, -1);

	text = g_strdup_printf ("%d/%d - <b>%s</b> %s", 
				current, 
				images, 
				escaped_name,
				priv->image_modified ? _("[modified]") : "");

	gthumb_info_bar_set_text (GTHUMB_INFO_BAR (priv->info_bar), text, NULL);

	g_free (utf8_name);
	g_free (escaped_name);
	g_free (text);
}


static void 
window_update_title (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char *info_txt = NULL;
	char *path;
	char *modified;

	path = priv->image_path;
	modified = priv->image_modified ? _("[modified]") : "";

	if (path == NULL) {
		if ((priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		    && (priv->dir_list->path != NULL)) {
			char *path = g_filename_to_utf8 (priv->dir_list->path, -1, NULL, NULL, NULL);
			if (path == NULL)
				path = g_strdup (_("(Invalid Name)"));
			info_txt = g_strconcat (path, " ", modified, NULL);
			g_free (path);

		} else if ((priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST)
			   && (priv->catalog_path != NULL)) {
			const char *cat_name;
			char       *cat_name_no_ext;

			cat_name = file_name_from_path (priv->catalog_path);
			cat_name_no_ext = g_strdup (cat_name);
			
			/* Cut out the file extension. */
			cat_name_no_ext[strlen (cat_name_no_ext) - 4] = 0;
			
			info_txt = g_filename_to_utf8 (cat_name_no_ext, -1, 0, 0, 0);
			g_free (cat_name_no_ext);
		} else
			info_txt = g_strdup_printf ("%s", _("gThumb"));
	} else {
		const char *image_name = g_filename_to_utf8 (file_name_from_path (path), -1, 0, 0, 0);

		if (image_name == NULL)
			image_name = "";

		if (priv->image_catalog != NULL) {
			char *cat_name = g_filename_to_utf8 (file_name_from_path (priv->image_catalog), -1, 0, 0, 0);

			/* Cut out the file extension. */
			cat_name[strlen (cat_name) - 4] = 0;
			
			info_txt = g_strdup_printf ("%s %s - %s", image_name, modified, cat_name);
			g_free (cat_name);
		} else 
			info_txt = g_strdup_printf ("%s %s", image_name, modified);
	}

	gtk_window_set_title (GTK_WINDOW (browser), info_txt);
	g_free (info_txt);
}


static void
window_update_statusbar_list_info (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *info, *size_txt, *sel_size_txt;
	char                  *total_info, *selected_info;
	int                    tot_n, sel_n;
	GnomeVFSFileSize       tot_size, sel_size;
	GList                 *scan;
	GList                 *selection;

	tot_n = 0;
	tot_size = 0;

	for (scan = priv->file_list->list; scan; scan = scan->next) {
		FileData *fd = scan->data;
		tot_n++;
		tot_size += fd->size;
	}

	sel_n = 0;
	sel_size = 0;
	selection = gth_file_list_get_selection_as_fd (priv->file_list);

	for (scan = selection; scan; scan = scan->next) {
		FileData *fd = scan->data;
		sel_n++;
		sel_size += fd->size;
	}

	file_data_list_free (selection);

	size_txt = gnome_vfs_format_file_size_for_display (tot_size);
	sel_size_txt = gnome_vfs_format_file_size_for_display (sel_size);

	if (tot_n == 0)
		total_info = g_strdup (_("No image"));
	else if (tot_n == 1)
		total_info = g_strdup_printf (_("1 image (%s)"),
					      size_txt);
	else
		total_info = g_strdup_printf (_("%d images (%s)"),
					      tot_n,
					      size_txt); 

	if (sel_n == 0)
		selected_info = g_strdup (" ");
	else if (sel_n == 1)
		selected_info = g_strdup_printf (_("1 selected (%s)"), 
						 sel_size_txt);
	else
		selected_info = g_strdup_printf (_("%d selected (%s)"), 
						 sel_n, 
						 sel_size_txt);

	info = g_strconcat (total_info, 
			    ((sel_n == 0) ? NULL : ", "),
			    selected_info, 
			    NULL);

	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), 
			   priv->list_info_cid);
	gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar),
			    priv->list_info_cid, info);

	g_free (total_info);
	g_free (selected_info);
	g_free (size_txt);
	g_free (sel_size_txt);
	g_free (info);
}


static void
window_update_go_sensitivity (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	gboolean               sensitive;

	sensitive = (priv->history_current != NULL) && (priv->history_current->next != NULL);
	set_action_sensitive (browser, "Go_Back", sensitive);

	sensitive = (priv->history_current != NULL) && (priv->history_current->prev != NULL);
	set_action_sensitive (browser, "Go_Forward", sensitive);
}


static void
window_update_zoom_sensitivity (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	gboolean               image_is_visible;
	gboolean               image_is_void;
	gboolean               fit;
	int                    zoom;

	image_is_visible = (priv->image_path != NULL) && ((priv->sidebar_visible && priv->image_pane_visible && priv->preview_content == GTH_PREVIEW_CONTENT_IMAGE) || ! priv->sidebar_visible);
	image_is_void = image_viewer_is_void (IMAGE_VIEWER (priv->viewer));
	zoom = (int) (IMAGE_VIEWER (priv->viewer)->zoom_level * 100.0);
	fit = image_viewer_is_zoom_to_fit (IMAGE_VIEWER (priv->viewer)) || image_viewer_is_zoom_to_fit_if_larger (IMAGE_VIEWER (priv->viewer));

	set_action_sensitive (browser, 
			      "View_Zoom100",
			      image_is_visible && !image_is_void && (zoom != 100));
	set_action_sensitive (browser, 
			      "View_ZoomIn",
			      image_is_visible && !image_is_void && (zoom != 10000));
	set_action_sensitive (browser, 
			      "View_ZoomOut",
			      image_is_visible && !image_is_void && (zoom != 5));
	set_action_sensitive (browser, 
			      "View_ZoomFit",
			      image_is_visible && !image_is_void);
}


static void
window_update_sensitivity (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GtkTreeIter iter;
	int         sidebar_content = priv->sidebar_content;
	gboolean    sel_not_null;
	gboolean    only_one_is_selected;
	gboolean    image_is_void;
	gboolean    image_is_ani;
	gboolean    playing;
	gboolean    viewing_dir;
	gboolean    viewing_catalog;
	gboolean    is_catalog;
	gboolean    is_search;
	gboolean    not_fullscreen;
	gboolean    image_is_visible;
	int         image_pos;

	sel_not_null = gth_file_view_selection_not_null (priv->file_list->view);
	only_one_is_selected = gth_file_view_only_one_is_selected (priv->file_list->view);
	image_is_void = image_viewer_is_void (IMAGE_VIEWER (priv->viewer));
	image_is_ani = image_viewer_is_animation (IMAGE_VIEWER (priv->viewer));
	playing = image_viewer_is_playing_animation (IMAGE_VIEWER (priv->viewer));
	viewing_dir = sidebar_content == GTH_SIDEBAR_DIR_LIST;
	viewing_catalog = sidebar_content == GTH_SIDEBAR_CATALOG_LIST; 
	not_fullscreen = ! priv->fullscreen;
	image_is_visible = ! image_is_void && ((priv->sidebar_visible && priv->image_pane_visible && (priv->preview_content == GTH_PREVIEW_CONTENT_IMAGE)) || ! priv->sidebar_visible);

	if (priv->image_path != NULL)
		image_pos = gth_file_list_pos_from_path (priv->file_list, priv->image_path);
	else
		image_pos = -1;

	window_update_go_sensitivity (browser);

	/* Image popup menu. */

	set_action_sensitive (browser, "Image_OpenWith", ! image_is_void && not_fullscreen);
	set_action_sensitive (browser, "Image_Rename", ! image_is_void && not_fullscreen);
	set_action_sensitive (browser, "Image_Delete", ! image_is_void && not_fullscreen);
	set_action_sensitive (browser, "Image_Copy", ! image_is_void && not_fullscreen);
	set_action_sensitive (browser, "Image_Move", ! image_is_void && not_fullscreen);

	/* File menu. */

	set_action_sensitive (browser, "File_OpenWith", sel_not_null && not_fullscreen);

	set_action_sensitive (browser, "File_SaveAs", ! image_is_void);
	set_action_sensitive (browser, "File_Revert", ! image_is_void && priv->image_modified);
	set_action_sensitive (browser, "File_Print", ! image_is_void || sel_not_null);

	/* Edit menu. */

	set_action_sensitive (browser, "Edit_RenameFile", only_one_is_selected && not_fullscreen);
	set_action_sensitive (browser, "Edit_DuplicateFile", sel_not_null && not_fullscreen);
	set_action_sensitive (browser, "Edit_DeleteFiles", sel_not_null && not_fullscreen);
	set_action_sensitive (browser, "Edit_CopyFiles", sel_not_null && not_fullscreen);
	set_action_sensitive (browser, "Edit_MoveFiles", sel_not_null && not_fullscreen);

	set_action_sensitive (browser, "AlterImage_Rotate90", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Rotate90CC", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Flip", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Mirror", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Desaturate", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Resize", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_ColorBalance", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_HueSaturation", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_BrightnessContrast", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Invert", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Posterize", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Equalize", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_AdjustLevels", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_StretchContrast", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Normalize", ! image_is_void && ! image_is_ani && image_is_visible);
	set_action_sensitive (browser, "AlterImage_Crop", ! image_is_void && ! image_is_ani && image_is_visible);

	window_update_zoom_sensitivity(browser);
	set_action_sensitive (browser, "View_PlayAnimation", image_is_ani);
	set_action_sensitive (browser, "View_StepAnimation", image_is_ani && ! playing);

	set_action_sensitive (browser, "Edit_EditComment", sel_not_null);
	set_action_sensitive (browser, "Edit_DeleteComment", sel_not_null);
	set_action_sensitive (browser, "Edit_EditCategories", sel_not_null);

	set_action_sensitive (browser, "Edit_AddToCatalog", sel_not_null);
	set_action_sensitive (browser, "Edit_RemoveFromCatalog", viewing_catalog && sel_not_null);

	set_action_sensitive (browser, "Go_ToContainer", not_fullscreen && viewing_catalog && only_one_is_selected);

	set_action_sensitive (browser, "Go_Stop", 
			       ((priv->activity_ref > 0) 
				|| priv->setting_file_list
				|| priv->changing_directory
				|| priv->file_list->doing_thumbs));

	/* Edit Catalog menu. */

	if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) { 
		char *view_catalog;
		char *view_search;

		is_catalog = (viewing_catalog && catalog_list_get_selected_iter (priv->catalog_list, &iter));

		set_action_sensitive (browser, "EditCatalog_Rename", is_catalog);
		set_action_sensitive (browser, "EditCatalog_Delete", is_catalog);
		set_action_sensitive (browser, "EditCatalog_Move", is_catalog && ! catalog_list_is_dir (priv->catalog_list, &iter));
		
		is_search = (is_catalog && (catalog_list_is_search (priv->catalog_list, &iter)));
		set_action_sensitive (browser, "EditCatalog_EditSearch", is_search);
		set_action_sensitive (browser, "EditCatalog_RedoSearch", is_search);

		/**/

		is_catalog = (priv->catalog_path != NULL) && (viewing_catalog && catalog_list_get_iter_from_path (priv->catalog_list, priv->catalog_path, &iter));

		set_action_sensitive (browser, "EditCurrentCatalog_Rename", is_catalog);
		set_action_sensitive (browser, "EditCurrentCatalog_Delete", is_catalog);
		set_action_sensitive (browser, "EditCurrentCatalog_Move", is_catalog && ! catalog_list_is_dir (priv->catalog_list, &iter));
		
		is_search = (is_catalog && (catalog_list_is_search (priv->catalog_list, &iter)));
		set_action_sensitive (browser, "EditCurrentCatalog_EditSearch", is_search);
		set_action_sensitive (browser, "EditCurrentCatalog_RedoSearch", is_search);

		if (is_search) {
			view_catalog = "1";
			view_search = "0";
		} else {
			view_catalog = "0";
			view_search = "1";
		}
	} 

	/* View menu. */

	set_action_sensitive (browser, "View_ShowImage", priv->image_path != NULL);
	set_action_sensitive (browser, "File_ImageProp", priv->image_path != NULL);
	set_action_sensitive (browser, "View_Fullscreen", priv->image_path != NULL);
	set_action_sensitive (browser, "View_ShowPreview", priv->sidebar_visible);
	set_action_sensitive (browser, "View_ShowInfo", ! priv->sidebar_visible);
	set_action_sensitive (browser, "View_PrevImage", image_pos > 0);
	set_action_sensitive (browser, "View_NextImage", (image_pos != -1) && (image_pos < gth_file_view_get_images (priv->file_list->view) - 1));

	/* Tools menu. */

	set_action_sensitive (browser, "Tools_Slideshow", priv->file_list->list != NULL);
	set_action_sensitive (browser, "Tools_IndexImage", sel_not_null);
	set_action_sensitive (browser, "Tools_WebExporter", sel_not_null);
	set_action_sensitive (browser, "Tools_RenameSeries", sel_not_null);
	set_action_sensitive (browser, "Tools_ConvertFormat", sel_not_null);
	set_action_sensitive (browser, "Tools_ChangeDate", sel_not_null);
	set_action_sensitive (browser, "Tools_JPEGRotate", sel_not_null);
	set_action_sensitive (browser, "Wallpaper_Centered", ! image_is_void);
	set_action_sensitive (browser, "Wallpaper_Tiled", ! image_is_void);
	set_action_sensitive (browser, "Wallpaper_Scaled", ! image_is_void);
	set_action_sensitive (browser, "Wallpaper_Stretched", ! image_is_void);

	set_action_sensitive (browser, "Tools_JPEGRotate", sel_not_null);

	window_update_zoom_sensitivity (browser);
}


static void
window_progress (gfloat   percent, 
		 gpointer data)
{
	GthBrowser *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress), 
				       percent);

	if (percent == 0.0) 
		set_action_sensitive (browser, "Go_Stop", 
				       ((priv->activity_ref > 0) 
					|| priv->setting_file_list
					|| priv->changing_directory
					|| priv->file_list->doing_thumbs));
}


static gboolean
load_progress (gpointer data)
{
	GthBrowser *browser = data;
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (browser->priv->progress));
	return TRUE;
}


static void
gth_browser_start_activity_mode (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if (priv->activity_ref++ > 0)
		return;

	gtk_widget_show (priv->progress);

	priv->activity_timeout = g_timeout_add (ACTIVITY_DELAY, 
						  load_progress, 
						  browser);
}


static void
gth_browser_stop_activity_mode (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if (--priv->activity_ref > 0)
		return;

	gtk_widget_hide (priv->progress);

	if (priv->activity_timeout == 0)
		return;

	g_source_remove (priv->activity_timeout);
	priv->activity_timeout = 0;
	window_progress (0.0, browser);
}


/* -- set file list -- */


typedef struct {
	GthBrowser *browser;
	DoneFunc    done_func;
	gpointer    done_func_data;
} WindowSetListData;


static gboolean
set_file_list__final_step_cb (gpointer data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	if (StartInFullscreen) {
		StartInFullscreen = FALSE;
		/*fullscreen_start (fullscreen, browser); FIXME */
	}

	if (StartSlideshow) {
		StartSlideshow = FALSE;
		gth_browser_start_slideshow (browser);
	} 

	if (HideSidebar) {
		HideSidebar = FALSE;
		gth_browser_hide_sidebar (browser);
	}

	if (ImageToDisplay != NULL) {
		int pos = gth_file_list_pos_from_path (priv->file_list, ImageToDisplay);
		if (pos != -1)
			gth_file_list_select_image_by_pos (priv->file_list, pos);
		g_free (ImageToDisplay);
		ImageToDisplay = NULL;

	} else if (ViewFirstImage) {
		ViewFirstImage = FALSE;
		gth_browser_show_first_image (browser, FALSE);
	}

	if (FirstStart)
		FirstStart = FALSE;

	return FALSE;
}


static void
window_set_file_list_continue (gpointer callback_data)
{
	WindowSetListData     *data = callback_data;
	GthBrowser            *browser = data->browser;
	GthBrowserPrivateData *priv = browser->priv;

	gth_browser_stop_activity_mode (browser);
	window_update_statusbar_list_info (browser);
	priv->setting_file_list = FALSE;

	window_update_sensitivity (browser);

	if (data->done_func != NULL)
		(*data->done_func) (data->done_func_data);
	g_free (data);

	g_idle_add (set_file_list__final_step_cb, browser);
}


typedef struct {
	WindowSetListData *wsl_data;
	GList             *list;
	GthBrowser        *browser;
} SetListInterruptedData; 


static void
set_list_interrupted_cb (gpointer callback_data)
{
	SetListInterruptedData *sli_data = callback_data;

	sli_data->browser->priv->setting_file_list = TRUE;
	gth_file_list_set_list (sli_data->browser->priv->file_list, 
				sli_data->list, 
				window_set_file_list_continue, 
				sli_data->wsl_data);

	sli_data->browser->priv->can_set_file_list = TRUE;
	
	g_list_foreach (sli_data->list, (GFunc) g_free, NULL);
	g_list_free (sli_data->list);
	g_free (sli_data);
}


static void
window_set_file_list (GthBrowser *browser, 
		      GList      *list,
		      DoneFunc    done_func,
		      gpointer    done_func_data)
{
	GthBrowserPrivateData *priv = browser->priv;
	WindowSetListData     *data;

	if (! priv->can_set_file_list)
		return;

	priv->can_set_file_list = FALSE;

	if (priv->slideshow)
		gth_browser_stop_slideshow (browser);

	data = g_new (WindowSetListData, 1);
	data->browser = browser;
	data->done_func = done_func;
	data->done_func_data = done_func_data;

	if (priv->setting_file_list) {
		SetListInterruptedData *sli_data;
		GList                  *scan;

		sli_data = g_new (SetListInterruptedData, 1);

		sli_data->wsl_data = data;
		sli_data->browser = browser;

		sli_data->list = NULL;
		for (scan = list; scan; scan = scan->next) {
			char *path = g_strdup ((char*)(scan->data));
			sli_data->list = g_list_prepend (sli_data->list, path);
		}

		gth_file_list_interrupt_set_list (priv->file_list,
						  set_list_interrupted_cb,
						  sli_data);
		return;
	}

	priv->setting_file_list = TRUE;
	gth_browser_start_activity_mode (browser);
	window_update_sensitivity (browser);

	gth_file_list_set_list (priv->file_list, 
				list, 
				window_set_file_list_continue, 
				data);

	priv->can_set_file_list = TRUE;
}





void
gth_browser_update_file_list (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		gth_browser_go_to_directory (browser, priv->dir_list->path);

	else if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) {
		char *catalog_path;

		catalog_path = catalog_list_get_selected_path (priv->catalog_list);
		if (catalog_path == NULL)
			return;

		gth_browser_go_to_catalog (browser, catalog_path);
		g_free (catalog_path);
	}
}


void
gth_browser_update_catalog_list (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *catalog_dir;
	char                  *base_dir;

	if (priv->sidebar_content != GTH_SIDEBAR_CATALOG_LIST) 
		return;

	/* If the catalog still exists, show the directory it belongs to. */

	if ((priv->catalog_path != NULL) 
	    && path_is_file (priv->catalog_path)) {
		GtkTreeIter  iter;
		GtkTreePath *path;

		catalog_dir = remove_level_from_path (priv->catalog_path);
		gth_browser_go_to_catalog_directory (browser, catalog_dir);
		g_free (catalog_dir);

		if (! catalog_list_get_iter_from_path (priv->catalog_list,
						       priv->catalog_path, 
						       &iter))
			return;

		/* Select (without updating the file list) and view 
		 * the catalog. */

		catalog_list_select_iter (priv->catalog_list, &iter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->catalog_list->list_store), &iter);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->catalog_list->list_view),
					      path,
					      NULL,
					      TRUE,
					      0.5,
					      0.0);
		gtk_tree_path_free (path);
		return;
	} 

	/* No catalog selected. */

	if (priv->catalog_path != NULL) {
		g_free (priv->catalog_path);
		priv->catalog_path = NULL;

		/* Update file list. */

		window_set_file_list (browser, NULL, NULL, NULL);
	}

	g_return_if_fail (priv->catalog_list->path != NULL);

	/* If directory exists then update. */

	if (path_is_dir (priv->catalog_list->path)) {
		catalog_list_refresh (priv->catalog_list);
		return;
	}

	/* Else go up one level until a directory exists. */

	base_dir = g_strconcat (g_get_home_dir(),
				"/",
				RC_CATALOG_DIR,
				NULL);
	catalog_dir = g_strdup (priv->catalog_list->path);
	
	while ((strcmp (base_dir, catalog_dir) != 0)
	       && ! path_is_dir (catalog_dir)) {
		char *new_dir;
		
		new_dir = remove_level_from_path (catalog_dir);
		g_free (catalog_dir);
		catalog_dir = new_dir;
	}

	gth_browser_go_to_catalog_directory (browser, catalog_dir);
	
	g_free (catalog_dir);
	g_free (base_dir);
}


/* -- bookmarks & history -- */


static void
activate_action_bookmark (GtkAction *action, 
			  gpointer   data)
{
	GthBrowser  *browser = data;
	const char  *path;
	const char  *no_prefix_path;

	path = g_object_get_data (G_OBJECT (action), "path");

	no_prefix_path = pref_util_remove_prefix (path);
	
	if (pref_util_location_is_catalog (path) 
	    || pref_util_location_is_search (path)) 
		gth_browser_go_to_catalog (browser, no_prefix_path);
	else 
		gth_browser_go_to_directory (browser, no_prefix_path);	
}


static void
add_bookmark_menu_item (GthBrowser     *browser,
			GtkActionGroup *actions,
			uint            merge_id,
			Bookmarks      *bookmarks,
			char           *prefix,
			int             id,
			const char     *base_path,
			const char     *path)
{
	GthBrowserPrivateData *priv = browser->priv;
	GtkAction             *action;
	char                  *name;
	char                  *label;
	char                  *menu_name;
	char                  *e_label;
	char                  *e_tip;
	char                  *utf8_s;
	const char            *stock_id;

	name = g_strdup_printf ("%s%d", prefix, id);

	menu_name = escape_underscore (bookmarks_get_menu_name (bookmarks, path));
	if (menu_name == NULL)
		menu_name = g_strdup (_("(Invalid Name)"));

	label = _g_strdup_with_max_size (menu_name, BOOKMARKS_MENU_MAX_LENGTH);
	utf8_s = g_locale_to_utf8 (label, -1, NULL, NULL, NULL);
	g_free (label);
	if (utf8_s == NULL)
		utf8_s = g_strdup (_("(Invalid Name)"));
	e_label = g_markup_escape_text (utf8_s, -1);
	g_free (utf8_s);

	utf8_s = g_locale_to_utf8 (bookmarks_get_menu_tip (bookmarks, path), -1, NULL, NULL, NULL);
	if (utf8_s == NULL)
		utf8_s = g_strdup (_("(Invalid Name)"));
	e_tip = g_markup_escape_text (utf8_s, -1);
	g_free (utf8_s);

	if (strcmp (menu_name, g_get_home_dir ()) == 0) 
		stock_id = GTK_STOCK_HOME;
	else if (folder_is_film (path))
		stock_id = GTHUMB_STOCK_FILM;
	else if (pref_util_location_is_catalog (path)) 
		stock_id = GTHUMB_STOCK_CATALOG;
	else if (pref_util_location_is_search (path))	
		stock_id = GTHUMB_STOCK_SEARCH;
	else
		stock_id = GTK_STOCK_OPEN;
	g_free (menu_name);

	action = g_object_new (GTK_TYPE_ACTION,
			       "name", name,
			       "label", e_label,
			       "stock_id", stock_id,
			       NULL);
	g_free (e_label);

	if (e_tip != NULL) {
		g_object_set (action, "tooltip", e_tip, NULL);
		g_free (e_tip);
	}

	g_object_set_data_full (G_OBJECT (action), "path", g_strdup (path), (GFreeFunc)g_free);
	g_signal_connect (action, "activate", 
			  G_CALLBACK (activate_action_bookmark), 
			  browser);

	gtk_action_group_add_action (actions, action);
	g_object_unref (action);

	gtk_ui_manager_add_ui (priv->ui, 
			       merge_id, 
			       base_path,
			       name,
			       name,
			       GTK_UI_MANAGER_AUTO, 
			       FALSE);
	
	g_free (name);
}


void
gth_browser_update_bookmark_list (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GList                 *scan, *names;
	int                    i;

	/* Delete bookmarks menu. */

	if (priv->bookmarks_merge_id != 0) {
		gtk_ui_manager_remove_ui (priv->ui, priv->bookmarks_merge_id);
		priv->bookmarks_merge_id = 0;
	}

	gtk_ui_manager_ensure_update (priv->ui);

	if (priv->bookmark_actions != NULL) {
		gtk_ui_manager_remove_action_group (priv->ui, priv->bookmark_actions);
		g_object_unref (priv->bookmark_actions);
		priv->bookmark_actions = NULL;
	}

	/* Load and sort bookmarks */

	bookmarks_load_from_disk (preferences.bookmarks);
	if (preferences.bookmarks->list == NULL)
		return;

	names = g_list_copy (preferences.bookmarks->list);

	/* Update bookmarks menu. */

	if (priv->bookmark_actions == NULL) {
		priv->bookmark_actions = gtk_action_group_new ("BookmarkActions");
		gtk_ui_manager_insert_action_group (priv->ui, priv->bookmark_actions, 0);
	}

	if (priv->bookmarks_merge_id == 0)
		priv->bookmarks_merge_id = gtk_ui_manager_new_merge_id (priv->ui);

	for (i = 0, scan = names; scan; scan = scan->next) {
		add_bookmark_menu_item (browser,
					priv->bookmark_actions,
					priv->bookmarks_merge_id,
					preferences.bookmarks,
					"Bookmark",
					i++,
					"/MenuBar/Bookmarks/BookmarkList",
					scan->data);
	}

	priv->bookmarks_length = i;

	g_list_free (names);

	if (priv->bookmarks_dlg != NULL)
		dlg_edit_bookmarks_update (priv->bookmarks_dlg);
}


static void
window_update_history_list (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GList                 *scan;
	int                    i;

	window_update_go_sensitivity (browser);

	/* Delete bookmarks menu. */

	if (priv->history_merge_id != 0) {
		gtk_ui_manager_remove_ui (priv->ui, priv->history_merge_id);
		priv->history_merge_id = 0;
	}

	gtk_ui_manager_ensure_update (priv->ui);

	if (priv->history_actions != NULL) {
		gtk_ui_manager_remove_action_group (priv->ui, priv->history_actions);
		g_object_unref (priv->history_actions);
		priv->history_actions = NULL;
	}

	/* Update history menu. */

	if (priv->history->list == NULL)
		return;

	if (priv->history_actions == NULL) {
		priv->history_actions = gtk_action_group_new ("HistoryActions");
		gtk_ui_manager_insert_action_group (priv->ui, priv->history_actions, 0);
	}

	if (priv->history_merge_id == 0)
		priv->history_merge_id = gtk_ui_manager_new_merge_id (priv->ui);

	for (i = 0, scan = priv->history_current; scan && (i < eel_gconf_get_integer (PREF_MAX_HISTORY_LENGTH, 15)); scan = scan->next) {
		add_bookmark_menu_item (browser,
					priv->history_actions,
					priv->history_merge_id,
					priv->history,
					"History",
					i,
					HISTORY_LIST_MENU,
					scan->data);
		if (i > 0)
			add_bookmark_menu_item (browser,
						priv->history_actions,
						priv->history_merge_id,
						priv->history,
						"History",
						i,
						HISTORY_LIST_POPUP,
						scan->data);
		i++;
	}
	priv->history_length = i;
}


/**/

static void window_make_current_image_visible (GthBrowser *browser);


static void
view_image_at_pos (GthBrowser *browser, 
		   int           pos)
{
	char *path;

	path = gth_file_list_path_from_pos (browser->priv->file_list, pos);
	if (path == NULL) 
		return;
	gth_browser_load_image (browser, path);
	g_free (path);
}


/* -- activity -- */


static void
image_loader_progress_cb (ImageLoader *loader,
			  float        p, 
			  gpointer     data)
{
	GthBrowser *browser = data;
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (browser->priv->progress), p);
}


static void
image_loader_done_cb (ImageLoader *loader,
		      gpointer     data)
{
	GthBrowser *browser = data;
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (browser->priv->progress), 0.0);
}


static char *
get_command_name_from_sidebar_content (GthBrowser *browser)
{
	switch (browser->priv->sidebar_content) {
	case GTH_SIDEBAR_DIR_LIST:
		return "View_ShowFolders";
	case GTH_SIDEBAR_CATALOG_LIST:
		return "View_ShowCatalogs";
	default:
		return NULL;
	}

	return NULL;
}


static void
set_button_active_no_notify (GthBrowser *browser,
			     GtkWidget  *button,
			     gboolean    active)
{
	if (button == NULL)
		return;
	g_signal_handlers_block_by_data (button, browser);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
	g_signal_handlers_unblock_by_data (button, browser);
}


static GtkWidget *
get_button_from_sidebar_content (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	switch (priv->sidebar_content) {
	case GTH_SIDEBAR_DIR_LIST:
		return priv->show_folders_toolbar_button;
	case GTH_SIDEBAR_CATALOG_LIST:
		return priv->show_catalog_toolbar_button;
	default:
		return NULL;
	}

	return NULL;
}


static void
_window_set_sidebar (GthBrowser *browser,
		     int         sidebar_content)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *cname;
	GError                *error = NULL;

	cname = get_command_name_from_sidebar_content (browser);
	if (cname != NULL) 
		set_action_active_if_different (browser, cname, FALSE);
	set_button_active_no_notify (browser,
				     get_button_from_sidebar_content (browser),
				     FALSE);

	priv->sidebar_content = sidebar_content;

	set_button_active_no_notify (browser,
				     get_button_from_sidebar_content (browser),
				     TRUE);

	cname = get_command_name_from_sidebar_content (browser);
	if ((cname != NULL) && priv->sidebar_visible)
		set_action_active_if_different (browser, cname, TRUE);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 
				       sidebar_content - 1);

	window_update_sensitivity (browser);

	/**/

	if (priv->sidebar_merge_id != 0)
		gtk_ui_manager_remove_ui (priv->ui, priv->sidebar_merge_id);
	gtk_ui_manager_ensure_update (priv->ui);

	if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) { 
		gboolean    is_catalog;
		gboolean    is_search;
		GtkTreeIter iter;
		
		is_catalog = (priv->catalog_path != NULL) && (catalog_list_get_iter_from_path (priv->catalog_list, priv->catalog_path, &iter));
		is_search = (is_catalog && (catalog_list_is_search (priv->catalog_list, &iter)));
		if (is_search)
			priv->sidebar_merge_id = gtk_ui_manager_add_ui_from_string (priv->ui, search_ui_info, -1, &error);
		else
			priv->sidebar_merge_id = gtk_ui_manager_add_ui_from_string (priv->ui, catalog_ui_info, -1, &error);
	} else 
		priv->sidebar_merge_id = gtk_ui_manager_add_ui_from_string (priv->ui, folder_ui_info, -1, &error);

	gtk_ui_manager_ensure_update (priv->ui);
	gtk_widget_queue_resize (GTK_WIDGET (browser));
}


static void
make_image_visible (GthBrowser *browser, 
		    int         pos)
{
	GthBrowserPrivateData *priv = browser->priv;
	GthVisibility          visibility;

	if ((pos < 0) || (pos >= gth_file_view_get_images (priv->file_list->view)))
		return;

	visibility = gth_file_view_image_is_visible (priv->file_list->view, pos);
	if (visibility != GTH_VISIBILITY_FULL) {
		double offset = 0.5;
		
		switch (visibility) {
		case GTH_VISIBILITY_NONE:
			offset = 0.5; 
			break;
		case GTH_VISIBILITY_PARTIAL_TOP:
			offset = 0.0; 
			break;
		case GTH_VISIBILITY_PARTIAL_BOTTOM:
			offset = 1.0; 
			break;
		case GTH_VISIBILITY_PARTIAL:
		case GTH_VISIBILITY_FULL:
			offset = -1.0;
			break;
		}
		if (offset > -1.0)
			gth_file_view_moveto (priv->file_list->view, pos, offset);
	}
}


/* -- window_save_pixbuf -- */


void
save_pixbuf__image_saved_step2 (gpointer data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	int                    pos;

	if (priv->image_path == NULL)
		return;

	priv->image_modified = FALSE;
	priv->saving_modified_image = FALSE;

	pos = gth_file_list_pos_from_path (priv->file_list, 
					   priv->image_path);
	if (pos != -1) {
		view_image_at_pos (browser, pos);
		gth_file_list_select_image_by_pos (priv->file_list, pos);
	}
}


static void
save_pixbuf__image_saved_cb (char     *filename,
			     gpointer  data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	GList                 *file_list;
	int                    pos;

	if (filename == NULL) 
		return;

#ifdef HAVE_LIBEXIF
	if (priv->exif_data != NULL) {
		JPEGData *jdata;

		jdata = jpeg_data_new_from_file (filename);
		if (jdata != NULL) {
			jpeg_data_set_exif_data (jdata, priv->exif_data);
			jpeg_data_save_file (jdata, filename);
			jpeg_data_unref (jdata);
		}
	}
#endif /* HAVE_LIBEXIF */

	/**/

	if ((priv->image_path != NULL)
	    && (strcmp (priv->image_path, filename) == 0))
		priv->image_modified = FALSE;

	file_list = g_list_prepend (NULL, filename);

	pos = gth_file_list_pos_from_path (priv->file_list, filename);
	if (pos == -1) {
		char *destination = remove_level_from_path (filename);

		if ((priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		    && (priv->dir_list->path != NULL) 
		    && (strcmp (priv->dir_list->path, destination) == 0)) {
			g_free (priv->image_path);
			priv->image_path = g_strdup (filename);
			gth_file_list_add_list (priv->file_list, 
						file_list, 
						save_pixbuf__image_saved_step2,
						browser);
		} else
			save_pixbuf__image_saved_step2 (browser);
		g_free (destination);

		all_windows_notify_files_created (file_list);

	} else {
		view_image_at_pos (browser, pos);
		all_windows_notify_files_changed (file_list);
	}

	g_list_free (file_list);
}


static void
gth_browser_save_pixbuf (GthWindow *window,
			 GdkPixbuf *pixbuf)
{
	GthBrowser            *browser = GTH_BROWSER (window);
	GthBrowserPrivateData *priv = browser->priv;
	char                  *current_folder = NULL;

	if (priv->image_path != NULL)
		current_folder = g_strdup (priv->image_path);

	else if (priv->dir_list->path != NULL)
		current_folder = g_strconcat (priv->dir_list->path,
					      "/", 
					      NULL);

	dlg_save_image (GTK_WINDOW (browser), 
			current_folder,
			pixbuf,
			save_pixbuf__image_saved_cb,
			browser);

	g_free (current_folder);
}


static void
ask_whether_to_save__image_saved_cb (char     *filename,
				     gpointer  data)
{
	GthBrowser *browser = data;

	save_pixbuf__image_saved_cb (filename, data);

	if (browser->priv->image_saved_func != NULL)
		(*browser->priv->image_saved_func) (NULL, browser);
}


static void
ask_whether_to_save__response_cb (GtkWidget  *dialog,
				  int         response_id,
				  GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

        gtk_widget_destroy (dialog);
	
        if (response_id == GTK_RESPONSE_YES) {
		dlg_save_image (GTK_WINDOW (browser),
				priv->image_path,
				image_viewer_get_current_pixbuf (IMAGE_VIEWER (priv->viewer)),
				ask_whether_to_save__image_saved_cb,
				browser);
		priv->saving_modified_image = TRUE;

	} else {
		priv->saving_modified_image = FALSE;
		priv->image_modified = FALSE;
		if (priv->image_saved_func != NULL)
			(*priv->image_saved_func) (NULL, browser);
	}
}


static gboolean
ask_whether_to_save (GthBrowser     *browser,
		     ImageSavedFunc  image_saved_func)
{
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *d;

	if (! eel_gconf_get_boolean (PREF_MSG_SAVE_MODIFIED_IMAGE, TRUE)) 
		return FALSE;
		
	d = _gtk_yesno_dialog_with_checkbutton_new (
			    GTK_WINDOW (browser),
			    GTK_DIALOG_MODAL,
			    _("The current image has been modified, do you want to save it?"),
			    _("Do Not Save"),
			    GTK_STOCK_SAVE_AS,
			    _("_Do not display this message again"),
			    PREF_MSG_SAVE_MODIFIED_IMAGE);

	priv->saving_modified_image = TRUE;
	priv->image_saved_func = image_saved_func;
	g_signal_connect (G_OBJECT (d), "response",
			  G_CALLBACK (ask_whether_to_save__response_cb),
			  browser);

	gtk_widget_show (d);

	return TRUE;
}


static void
real_set_void (char     *filename,
	       gpointer  data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	if (!priv->image_error) {
		g_free (priv->image_path);
		priv->image_path = NULL;
		priv->image_mtime = 0;
		priv->image_modified = FALSE;
	}

	image_viewer_set_void (IMAGE_VIEWER (priv->viewer));

	window_update_statusbar_image_info (browser);
 	window_update_image_info (browser);
	window_update_title (browser);
	window_update_infobar (browser);
	window_update_sensitivity (browser);

	if (priv->image_prop_dlg != NULL)
		dlg_image_prop_update (priv->image_prop_dlg);
}


static void
window_image_viewer_set_void (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->image_error = FALSE;
	if (priv->image_modified)
		if (ask_whether_to_save (browser, real_set_void))
			return;
	real_set_void (NULL, browser);
}


static void
window_image_viewer_set_error (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->image_error = TRUE;
	if (priv->image_modified)
		if (ask_whether_to_save (browser, real_set_void))
			return;
	real_set_void (NULL, browser);
}


static void
window_make_current_image_visible (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *path;
	int                    pos;

	if (priv->setting_file_list || priv->changing_directory)
		return;

	path = image_viewer_get_image_filename (IMAGE_VIEWER (priv->viewer));
	if (path == NULL)
		return;

	pos = gth_file_list_pos_from_path (priv->file_list, path);
	g_free (path);

	if (pos == -1) 
		window_image_viewer_set_void (browser);
	else
		make_image_visible (browser, pos);
}



/* -- callbacks -- */


static void
close_window_cb (GtkWidget  *caller, 
		 GdkEvent   *event, 
		 GthBrowser *browser)
{
	gth_window_close (GTH_WINDOW (browser));
}


static gboolean
sel_change_update_cb (gpointer data)
{
	GthBrowser *browser = data;

	g_source_remove (browser->priv->sel_change_timeout);
	browser->priv->sel_change_timeout = 0;

	window_update_sensitivity (browser);
	window_update_statusbar_list_info (browser);

	return FALSE;
}


static int
file_selection_changed_cb (GtkWidget *widget, 
			   gpointer   data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->sel_change_timeout != 0)
		g_source_remove (priv->sel_change_timeout);
	
	priv->sel_change_timeout = g_timeout_add (SEL_CHANGED_DELAY,
						  sel_change_update_cb,
						  browser);

	if (priv->comment_dlg != NULL)
		dlg_comment_update (priv->comment_dlg);
	
	if (priv->categories_dlg != NULL)
		dlg_categories_update (priv->categories_dlg);

	return TRUE;
}


static void
gth_file_list_cursor_changed_cb (GtkWidget *widget,
				 int        pos,
				 gpointer   data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	char                  *focused_image;

	if (priv->setting_file_list || priv->changing_directory) 
		return;

	focused_image = gth_file_list_path_from_pos (priv->file_list, pos);

	if (focused_image == NULL)
		return;

	if ((priv->image_path == NULL) 
	    || (strcmp (focused_image, priv->image_path) != 0))
		view_image_at_pos (browser, pos);

	g_free (focused_image);
}


static int
gth_file_list_button_press_cb (GtkWidget      *widget, 
			       GdkEventButton *event,
			       gpointer        data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	if (event->type == GDK_3BUTTON_PRESS) 
		return FALSE;

	if ((event->button != 1) && (event->button != 3))
		return FALSE;

	if ((event->state & GDK_SHIFT_MASK)
	    || (event->state & GDK_CONTROL_MASK))
		return FALSE;

	if (event->button == 1) {
		int pos;

		pos = gth_file_view_get_image_at (priv->file_list->view, 
						  event->x, 
						  event->y);

		if (pos == -1)
			return FALSE;

		if (event->type == GDK_2BUTTON_PRESS) 
			return FALSE;

		if (event->type == GDK_BUTTON_PRESS) {
			make_image_visible (browser, pos);
			view_image_at_pos (browser, pos);
			return FALSE;
		}

	} else if (event->button == 3) {
		int  pos;

		pos = gth_file_view_get_image_at (priv->file_list->view, 
						  event->x, 
						  event->y);

		if (pos != -1) {
			if (! gth_file_list_is_selected (priv->file_list, pos))
				gth_file_list_select_image_by_pos (priv->file_list, pos);
		} else
			gth_file_list_unselect_all (priv->file_list);

		window_update_sensitivity (browser);

		gtk_menu_popup (GTK_MENU (priv->file_popup_menu),
				NULL,                                   
				NULL,                                   
				NULL,
				NULL,
				3,                               
				event->time);

		return TRUE;
	}

	return FALSE;
}


static gboolean 
hide_sidebar_idle (gpointer data) 
{
	gth_browser_hide_sidebar ((GthBrowser*) data);
	return FALSE;
}


static int 
gth_file_list_item_activated_cb (GtkWidget *widget, 
				 int        idx,
				 gpointer   data)
{
	GthBrowser *browser = data;

	/* use a timeout to avoid that the viewer gets
	 * the button press event. */
	g_timeout_add (HIDE_SIDEBAR_DELAY, hide_sidebar_idle, browser);
	
	return TRUE;
}


static void
dir_list_activated_cb (GtkTreeView       *tree_view,
		       GtkTreePath       *path,
		       GtkTreeViewColumn *column,
		       gpointer           data)
{
	GthBrowser *browser = data;
	char       *new_dir;

	new_dir = dir_list_get_path_from_tree_path (browser->priv->dir_list, path);
	gth_browser_go_to_directory (browser, new_dir);
	g_free (new_dir);
}


/**/


static int
dir_list_button_press_cb (GtkWidget      *widget,
			  GdkEventButton *event,
			  gpointer        data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *treeview = priv->dir_list->list_view;
	GtkListStore          *list_store = priv->dir_list->list_store;
	GtkTreePath           *path;
	GtkTreeIter            iter;

	if (dir_list_tree_path != NULL) {
		gtk_tree_path_free (dir_list_tree_path);
		dir_list_tree_path = NULL;
	}

	if ((event->state & GDK_SHIFT_MASK) 
	    || (event->state & GDK_CONTROL_MASK))
		return FALSE;

	if ((event->button != 1) & (event->button != 3))
		return FALSE;

	if (! gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
					     event->x, event->y,
					     &path, NULL, NULL, NULL))
		return FALSE;
	
	if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store), 
				       &iter, 
				       path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	dir_list_tree_path = gtk_tree_path_copy (path);
	gtk_tree_path_free (path);

	return FALSE;
}


static int
dir_list_button_release_cb (GtkWidget      *widget,
			    GdkEventButton *event,
			    gpointer        data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *treeview = priv->dir_list->list_view;
	GtkListStore          *list_store = priv->dir_list->list_store;
	GtkTreePath           *path;
	GtkTreeIter            iter;

	if ((event->state & GDK_SHIFT_MASK) 
	    || (event->state & GDK_CONTROL_MASK))
		return FALSE;

	if ((event->button != 1) && (event->button != 3))
		return FALSE;

	if (! gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
					     event->x, event->y,
					     &path, NULL, NULL, NULL)) {
		if (event->button != 3)
			return FALSE;
		
		window_update_sensitivity (browser);
		gtk_menu_popup (GTK_MENU (priv->dir_list_popup_menu),
				NULL,
				NULL,
				NULL,
				NULL,
				3,
				event->time);

		return TRUE;
	}
	
	if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store), 
				       &iter, 
				       path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	if ((dir_list_tree_path == NULL)
	    || gtk_tree_path_compare (dir_list_tree_path, path) != 0) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_path_free (path);

	if ((event->button == 1) 
	    && (pref_get_real_click_policy () == GTH_CLICK_POLICY_SINGLE)) {
		char *new_dir;

		new_dir = dir_list_get_path_from_iter (priv->dir_list, 
						       &iter);
		gth_browser_go_to_directory (browser, new_dir);
		g_free (new_dir);

		return FALSE;

	} else if (event->button == 3) {
		GtkTreeSelection *selection;
		char             *utf8_name;
		char             *name;

		utf8_name = dir_list_get_name_from_iter (priv->dir_list, &iter);
		name = g_filename_from_utf8 (utf8_name, -1, NULL, NULL, NULL);
		g_free (utf8_name);

		if (strcmp (name, "..") == 0) {
			g_free (name);
			return TRUE;
		}
		g_free (name);

		/* Update selection. */

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->dir_list->list_view));
		if (selection == NULL)
			return FALSE;

		if (! gtk_tree_selection_iter_is_selected (selection, &iter)) {
			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_iter (selection, &iter);
                }

		/* Popup menu. */

		window_update_sensitivity (browser);
		gtk_menu_popup (GTK_MENU (priv->dir_popup_menu),
				NULL,
				NULL,
				NULL,
				NULL,
				3,
				event->time);
		return TRUE;
	}

	return FALSE;
}


/* directory or catalog list selection changed. */

static void
dir_or_catalog_sel_changed_cb (GtkTreeSelection *selection,
			       gpointer          p)
{
	window_update_sensitivity ((GthBrowser *) p);
}


static void
add_history_item (GthBrowser *browser,
		  const char *path,
		  const char *prefix)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *location;

	bookmarks_remove_from (priv->history, priv->history_current);

	location = g_strconcat (prefix, path, NULL);
	bookmarks_remove_all_instances (priv->history, location);
	bookmarks_add (priv->history, location, FALSE, FALSE);
	g_free (location);

	priv->history_current = priv->history->list;
}


static void
catalog_activate_continue (gpointer data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	window_update_sensitivity (browser);

	/* Add to history list if not present as last entry. */

	if ((priv->go_op == GTH_BROWSER_GO_TO)
	    && ((priv->history_current == NULL) 
		|| ((priv->catalog_path != NULL)
		    && (strcmp (priv->catalog_path, pref_util_remove_prefix (priv->history_current->data)) != 0)))) {
		GtkTreeIter iter;
		gboolean    is_search;

		if (! catalog_list_get_iter_from_path (priv->catalog_list,
						       priv->catalog_path,
						       &iter)) { 
			window_image_viewer_set_void (browser);
			return;
		}
		is_search = catalog_list_is_search (priv->catalog_list, &iter);
		add_history_item (browser,
				  priv->catalog_path,
				  is_search ? SEARCH_PREFIX : CATALOG_PREFIX);
	} else 
		priv->go_op = GTH_BROWSER_GO_TO;

	window_update_history_list (browser);
	window_update_title (browser);
	window_make_current_image_visible (browser);
}


static void
catalog_activate (GthBrowser *browser, 
		  const char *cat_path)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (path_is_dir (cat_path)) {
		gth_browser_go_to_catalog (browser, NULL);
		gth_browser_go_to_catalog_directory (browser, cat_path);

	} else {
		Catalog *catalog = catalog_new ();
		GError  *gerror;

		if (! catalog_load_from_disk (catalog, cat_path, &gerror)) {
			_gtk_error_dialog_from_gerror_run (GTK_WINDOW (browser), &gerror);
			catalog_free (catalog);
			window_image_viewer_set_void (browser);
			return;
		}

		window_set_file_list (browser, 
				      catalog->list,
				      catalog_activate_continue,
				      browser);

		catalog_free (catalog);
		if (priv->catalog_path != cat_path) {
			if (priv->catalog_path)
				g_free (priv->catalog_path);
			priv->catalog_path = g_strdup (cat_path);
		}
	}
}


static void
catalog_list_activated_cb (GtkTreeView       *tree_view,
			   GtkTreePath       *path,
			   GtkTreeViewColumn *column,
			   gpointer           data)
{
	GthBrowser *browser = data;
	char       *cat_path;

	cat_path = catalog_list_get_path_from_tree_path (browser->priv->catalog_list, path);
	if (cat_path == NULL)
		return;
	catalog_activate (browser, cat_path);
	g_free (cat_path);
}


static int
catalog_list_button_press_cb (GtkWidget      *widget, 
			      GdkEventButton *event,
			      gpointer        data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *treeview = priv->catalog_list->list_view;
	GtkListStore          *list_store = priv->catalog_list->list_store;
	GtkTreeIter            iter;
	GtkTreePath           *path;

	if (catalog_list_tree_path != NULL) {
		gtk_tree_path_free (catalog_list_tree_path);
		catalog_list_tree_path = NULL;
	}

	if ((event->state & GDK_SHIFT_MASK) 
	    || (event->state & GDK_CONTROL_MASK))
		return FALSE;

	if ((event->button != 1) & (event->button != 3))
		return FALSE;

	/* Get the path. */

	if (! gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
					     event->x, event->y,
					     &path, NULL, NULL, NULL))
		return FALSE;

	if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store), 
				       &iter, 
				       path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	catalog_list_tree_path = gtk_tree_path_copy (path);
	gtk_tree_path_free (path);

	return FALSE;
}


static int
catalog_list_button_release_cb (GtkWidget      *widget, 
				GdkEventButton *event,
				gpointer        data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *treeview = priv->catalog_list->list_view;
	GtkListStore          *list_store = priv->catalog_list->list_store;
	GtkTreeIter            iter;
	GtkTreePath           *path;

	if ((event->state & GDK_SHIFT_MASK) 
	    || (event->state & GDK_CONTROL_MASK))
		return FALSE;

	if ((event->button != 1) & (event->button != 3))
		return FALSE;

	/* Get the path. */

	if (! gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
					     event->x, event->y,
					     &path, NULL, NULL, NULL)) {
		if (event->button != 3)
			return FALSE;
		
		window_update_sensitivity (browser);
		gtk_menu_popup (GTK_MENU (priv->catalog_list_popup_menu),
				NULL,
				NULL,
				NULL,
				NULL,
				3,
				event->time);

		return TRUE;
	}

	if (! gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store), 
				       &iter, 
				       path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	if (gtk_tree_path_compare (catalog_list_tree_path, path) != 0) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_path_free (path);

	/**/

	if ((event->button == 1) && 
	    (pref_get_real_click_policy () == GTH_CLICK_POLICY_SINGLE)) {
		char *cat_path;

		cat_path = catalog_list_get_path_from_iter (priv->catalog_list, &iter);
		g_return_val_if_fail (cat_path != NULL, FALSE);
		catalog_activate (browser, cat_path);
		g_free (cat_path);

		return FALSE;

	} else if (event->button == 3) {
		GtkTreeSelection *selection;
		char             *utf8_name;
		char             *name;

		utf8_name = catalog_list_get_name_from_iter (priv->catalog_list, &iter);
		name = g_filename_from_utf8 (utf8_name, -1, NULL, NULL, NULL);
		g_free (utf8_name);

		if (strcmp (name, "..") == 0) {
			g_free (name);
			return TRUE;
		}
		g_free (name);

		/* Update selection. */

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->catalog_list->list_view));
		if (selection == NULL)
			return FALSE;

		if (! gtk_tree_selection_iter_is_selected (selection, &iter)) {
			gtk_tree_selection_unselect_all (selection);
			gtk_tree_selection_select_iter (selection, &iter);
                }

		/* Popup menu. */
		
		window_update_sensitivity (browser);
		if (catalog_list_is_dir (priv->catalog_list, &iter))
			gtk_menu_popup (GTK_MENU (priv->library_popup_menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,
					event->time);
		else
			gtk_menu_popup (GTK_MENU (priv->catalog_popup_menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,
					event->time);
		
		return TRUE;
	}

	return FALSE;
}


/* -- location entry stuff -- */


static char *
get_location (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *text;
	char                  *text2;
	char                  *l;

	text = _gtk_entry_get_filename_text (GTK_ENTRY (browser->priv->location_entry));
	text2 = remove_special_dirs_from_path (text);
	g_free (text);

	if (text2 == NULL)
		return NULL;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		l = g_strdup (text2);
	else {
		if (strcmp (text2, "/") == 0) {
			char *base = get_catalog_full_path (NULL);
			l = g_strconcat (base, "/", NULL);
			g_free (base);
		} else {
			if (*text2 == '/')
				l = get_catalog_full_path (text2 + 1);
			else
				l = get_catalog_full_path (text2);
		}
	}
	g_free (text2);

	return l;
}


static void
set_location (GthBrowser *browser,
	      const char *location)
{
	GthBrowserPrivateData *priv = browser->priv;
	const char            *l;
	char                  *abs_location;

	abs_location = remove_special_dirs_from_path (location);
	if (abs_location == NULL)
		return;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		l = abs_location;
	else {
		char *base = get_catalog_full_path (NULL);

		if (strlen (abs_location) == strlen (base))
			l = "/";
		else
			l = abs_location + strlen (base);
		g_free (base);
	}

	if (l) {
		char *utf8_l;
		utf8_l = g_filename_to_utf8 (l, -1, NULL, NULL, NULL);
		gtk_entry_set_text (GTK_ENTRY (priv->location_entry), utf8_l);
		gtk_editable_set_position (GTK_EDITABLE (priv->location_entry), g_utf8_strlen (utf8_l, -1));
		g_free (utf8_l);
	} else
		gtk_entry_set_text (GTK_ENTRY (priv->location_entry), NULL);

	g_free (abs_location);
}


static gboolean
location_is_new (GthBrowser *browser, 
		 const char *text)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		return (priv->dir_list->path != NULL)
			&& strcmp (priv->dir_list->path, text);
	else
		return (priv->catalog_list->path != NULL)
			&& strcmp (priv->catalog_list->path, text);
}


static void
go_to_location (GthBrowser *browser, 
		const char *text)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->focus_location_entry = TRUE;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		gth_browser_go_to_directory (browser, text);
	else {
		gth_browser_go_to_catalog (browser, NULL);
		gth_browser_go_to_catalog_directory (browser, text);
	}
}


static gint
location_entry_key_press_cb (GtkWidget   *widget, 
			     GdkEventKey *event,
			     GthBrowser  *browser)
{
	char *path;
	int   n;

	g_return_val_if_fail (browser != NULL, FALSE);
	
	switch (event->keyval) {
	case GDK_Return:
	case GDK_KP_Enter:
		path = get_location (browser);
		if (path != NULL) {
			go_to_location (browser, path);
			g_free (path);
		}
                return FALSE;

	case GDK_Tab:
		if ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
			return FALSE;
		
		path = get_location (browser);
		n = auto_compl_get_n_alternatives (path);

		if (n > 0) { 
			char *text;
			text = auto_compl_get_common_prefix ();

			if (n == 1) {
				auto_compl_hide_alternatives ();
				if (location_is_new (browser, text))
					go_to_location (browser, text);
				else {
					/* Add a separator at the end. */
					char *new_path;
					int   len = strlen (path);

					if (strcmp (path, text) != 0) {
						/* Reset the right name. */
						set_location (browser, text);
						g_free (path);
						return TRUE;
					}
					
					/* Ending separator, do nothing. */
					if ((len <= 1) 
					    || (path[len - 1] == '/')) {
						g_free (path);
						return TRUE;
					}

					new_path = g_strconcat (path,
								"/",
								NULL);
					set_location (browser, new_path);
					g_free (new_path);

					/* Re-Tab */
					gtk_widget_event (widget, (GdkEvent*)event);
				}
			} else {
				set_location (browser, text);
				auto_compl_show_alternatives (browser, widget);
			}

			if (text)
				g_free (text);
		}
		g_free (path);
		
		return TRUE;
	}
	
	return FALSE;
}


/* -- */


static void
image_loaded_cb (GtkWidget  *widget, 
		 GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->image_mtime = get_file_mtime (priv->image_path);
	priv->image_modified = FALSE;

	window_update_image_info (browser);
	window_update_infobar (browser);
	window_update_title (browser);
	window_update_sensitivity (browser);

	if (priv->image_prop_dlg != NULL)
		dlg_image_prop_update (priv->image_prop_dlg);

#ifdef HAVE_LIBEXIF
	{
		JPEGData *jdata;

		if (priv->exif_data != NULL) {
			exif_data_unref (priv->exif_data);
			priv->exif_data = NULL;
		}
		
		jdata = jpeg_data_new_from_file (priv->image_path);
		if (jdata != NULL) {
			priv->exif_data = jpeg_data_get_exif_data (jdata);
			jpeg_data_unref (jdata);
		}
	}
#endif /* HAVE_LIBEXIF */
}


static void
image_requested_error_cb (GtkWidget  *widget, 
			  GthBrowser *browser)
{
	window_image_viewer_set_error (browser);
}


static void
image_requested_done_cb (GtkWidget  *widget, 
			 GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	ImageLoader           *loader;

	priv->image_error = FALSE;
	loader = gthumb_preloader_get_loader (priv->preloader, priv->image_path);
	if (loader != NULL) 
		image_viewer_load_from_image_loader (IMAGE_VIEWER (priv->viewer), loader);
}


static gint
zoom_changed_cb (GtkWidget  *widget, 
		 GthBrowser *browser)
{
	window_update_statusbar_zoom_info (browser);
	return TRUE;	
}


static gint
size_changed_cb (GtkWidget  *widget, 
		 GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GtkAdjustment         *vadj, *hadj;
	gboolean               hide_vscr, hide_hscr;

	vadj = IMAGE_VIEWER (priv->viewer)->vadj;
	hadj = IMAGE_VIEWER (priv->viewer)->hadj;

	hide_vscr = vadj->upper <= vadj->page_size;
	hide_hscr = hadj->upper <= hadj->page_size;

	if (hide_vscr && hide_hscr) {
		gtk_widget_hide (priv->viewer_vscr); 
		gtk_widget_hide (priv->viewer_hscr); 
		gtk_widget_hide (priv->viewer_event_box);
	} else {
		gtk_widget_show (priv->viewer_vscr); 
		gtk_widget_show (priv->viewer_hscr); 
		gtk_widget_show (priv->viewer_event_box);
	}

	return TRUE;	
}


void
gth_browser_show_image_data (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	gtk_widget_show (priv->preview_widget_data);
	gtk_widget_show (priv->preview_widget_comment);
	gtk_widget_show (priv->preview_widget_data_comment);
	set_button_active_no_notify (browser, priv->preview_button_data, TRUE);

	priv->image_data_visible = TRUE;
	set_action_active_if_different (browser, "View_ShowInfo", TRUE);
}


void
gth_browser_hide_image_data (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	gtk_widget_hide (priv->preview_widget_data_comment);
	set_button_active_no_notify (browser, priv->preview_button_data, FALSE);
	priv->image_data_visible = FALSE;
	set_action_active_if_different (browser, "View_ShowInfo", FALSE);
}


static void
toggle_image_data_visibility (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->sidebar_visible)
		return;

	if (priv->image_data_visible) 
		gth_browser_hide_image_data (browser);
	else
		gth_browser_show_image_data (browser);
}


static void
change_image_preview_content (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (! priv->sidebar_visible) 
		return;

	if (! priv->image_pane_visible) {
		gth_browser_show_image_pane (browser);
		return;
	}

	if (priv->preview_content == GTH_PREVIEW_CONTENT_IMAGE) 
		gth_browser_set_preview_content (browser, GTH_PREVIEW_CONTENT_DATA);
	
	else if (priv->preview_content == GTH_PREVIEW_CONTENT_DATA) 
		gth_browser_set_preview_content (browser, GTH_PREVIEW_CONTENT_COMMENT);
	
	else if (priv->preview_content == GTH_PREVIEW_CONTENT_COMMENT) 
		gth_browser_set_preview_content (browser, GTH_PREVIEW_CONTENT_IMAGE);
}


static void
show_image_preview (GthBrowser *browser)
{
	browser->priv->preview_visible = TRUE;
	gth_browser_show_image_pane (browser);
}


static void
hide_image_preview (GthBrowser *browser)
{
	browser->priv->preview_visible = FALSE;
	gth_browser_hide_image_pane (browser);
}


static void
toggle_image_preview_visibility (GthBrowser *browser)
{
	if (! browser->priv->sidebar_visible) 
		return;

	if (browser->priv->preview_visible) 
		hide_image_preview (browser);
	 else 
		show_image_preview (browser);
}


static void
window_enable_thumbs (GthBrowser *browser,
		      gboolean    enable)
{
	GthBrowserPrivateData *priv = browser->priv;

	gth_file_list_enable_thumbs (priv->file_list, enable);
	set_action_sensitive (browser, "Go_Stop", 
			       ((priv->activity_ref > 0) 
				|| priv->setting_file_list
				|| priv->changing_directory
				|| priv->file_list->doing_thumbs));
}


static gint
key_press_cb (GtkWidget   *widget, 
	      GdkEventKey *event,
	      gpointer     data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	ImageViewer           *viewer = IMAGE_VIEWER (priv->viewer);
	gboolean               sel_not_null;
	gboolean               image_is_void;

	if (GTK_WIDGET_HAS_FOCUS (priv->location_entry)) {
		if (gtk_widget_event (priv->location_entry, (GdkEvent*)event))
			return TRUE;
		else
			return FALSE;
	}

	if (GTK_WIDGET_HAS_FOCUS (priv->preview_button_image)
	    || GTK_WIDGET_HAS_FOCUS (priv->preview_button_data)
	    || GTK_WIDGET_HAS_FOCUS (priv->preview_button_comment))
		if (event->keyval == GDK_space)
			return FALSE;

	if (priv->sidebar_visible
	    && (event->state & GDK_CONTROL_MASK)
	    && ((event->keyval == GDK_1)
		|| (event->keyval == GDK_2)
		|| (event->keyval == GDK_3))) {
		GthPreviewContent content;

		switch (event->keyval) {
		case GDK_1:
			content = GTH_PREVIEW_CONTENT_IMAGE;
			break;
		case GDK_2:
			content = GTH_PREVIEW_CONTENT_DATA;
			break;
		case GDK_3:
		default:
			content = GTH_PREVIEW_CONTENT_COMMENT;
			break;
		}

		if (priv->preview_content == content) 
			toggle_image_preview_visibility (browser);
		 else {
			if (! priv->preview_visible)
				show_image_preview (browser);
			gth_browser_set_preview_content (browser, content);
		}
	}

	if ((event->state & GDK_CONTROL_MASK) || (event->state & GDK_MOD1_MASK))
		return FALSE;

	sel_not_null = gth_file_view_selection_not_null (priv->file_list->view);
	image_is_void = image_viewer_is_void (IMAGE_VIEWER (priv->viewer));

	switch (event->keyval) {
		/* Hide/Show sidebar. */
	case GDK_Return:
	case GDK_KP_Enter:
		if (priv->sidebar_visible) 
			gth_browser_hide_sidebar (browser);
		else
			gth_browser_show_sidebar (browser);
		return TRUE;

		/* Change image pane content. */
	case GDK_i:
		toggle_image_data_visibility (browser);
		return TRUE;

	case GDK_q:
		change_image_preview_content (browser);
		return TRUE;

		/* Hide/Show image pane. */
	case GDK_Q:
		toggle_image_preview_visibility (browser);
		return TRUE;

		/* Full screen view. */
	case GDK_v:
	case GDK_F11:
		/* FIXME
		if (priv->image_path != NULL)
			fullscreen_start (fullscreen, browser);
		*/
		return TRUE;

		/* View/hide thumbnails. */
	case GDK_t:
		eel_gconf_set_boolean (PREF_SHOW_THUMBNAILS, !priv->file_list->enable_thumbs);
		return TRUE;

		/* Zoom in. */
	case GDK_plus:
	case GDK_equal:
	case GDK_KP_Add:
		image_viewer_zoom_in (viewer);
		return TRUE;

		/* Zoom out. */
	case GDK_minus:
	case GDK_KP_Subtract:
		image_viewer_zoom_out (viewer);
		return TRUE;
		
		/* Actual size. */
	case GDK_KP_Divide:
	case GDK_1:
	case GDK_z:
		image_viewer_set_zoom (viewer, 1.0);
		return TRUE;

		/* Set zoom to 2.0. */
	case GDK_2:
		image_viewer_set_zoom (viewer, 2.0);
		return TRUE;

		/* Set zoom to 3.0. */
	case GDK_3:
		image_viewer_set_zoom (viewer, 3.0);
		return TRUE;

		/* Zoom to fit */
	case GDK_x:
		gth_window_activate_action_view_zoom_fit (NULL, browser);
		return TRUE;

		/* Show previous image. */
	case GDK_p:
	case GDK_b:
	case GDK_BackSpace:
		gth_browser_show_prev_image (browser, FALSE);
		return TRUE;

		/* Show next image. */
	case GDK_n:
		gth_browser_show_next_image (browser, FALSE);
		return TRUE;

	case GDK_space:
		if (! GTK_WIDGET_HAS_FOCUS (priv->dir_list->list_view)
		    && ! GTK_WIDGET_HAS_FOCUS (priv->catalog_list->list_view)) {
			gth_browser_show_next_image (browser, FALSE);
			return TRUE;
		}
		break;

		/* Rotate image */
	case GDK_bracketright:
	case GDK_r: 
		gth_window_activate_action_alter_image_rotate90 (NULL, browser);
		return TRUE;
			
		/* Flip image */
	case GDK_l:
		gth_window_activate_action_alter_image_flip (NULL, browser);
		return TRUE;

		/* Mirror image */
	case GDK_m:
		gth_window_activate_action_alter_image_mirror (NULL, browser);
		return TRUE;

		/* Rotate image counter-clockwise */
	case GDK_bracketleft:
		gth_window_activate_action_alter_image_rotate90cc (NULL, browser);
		return TRUE;
			
		/* Delete selection. */
	case GDK_Delete: 
	case GDK_KP_Delete:
		if (! sel_not_null)
			break;
		
		if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
			gth_browser_activate_action_edit_delete_files (NULL, browser);
		else if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST)
			gth_browser_activate_action_edit_remove_from_catalog (NULL, browser);
		return TRUE;

		/* Open images. */
	case GDK_o:
		if (! sel_not_null)
			break;

		gth_window_activate_action_file_open_with (NULL, browser);
		return TRUE;

		/* Go up one level */
	case GDK_u:
		gth_browser_go_up (browser);
		return TRUE;

		/* Go home */
	case GDK_h:
		gth_browser_activate_action_go_home (NULL, browser);
		return TRUE;

		/* Edit comment */
	case GDK_c:
		if (! sel_not_null)
			break;
		gth_window_edit_comment (GTH_WINDOW (browser));
		return TRUE;

		/* Edit categories */
	case GDK_k:
		if (! sel_not_null)
			break;
		gth_window_edit_categories (GTH_WINDOW (browser));
		return TRUE;

	case GDK_Escape:
		if (priv->image_pane_visible)
			gth_browser_hide_image_pane (browser);
		return TRUE;

	default:
		break;
	}

	if ((event->keyval == GDK_F10) 
	    && (event->state & GDK_SHIFT_MASK)) {

		if ((priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST)
		    && GTK_WIDGET_HAS_FOCUS (priv->catalog_list->list_view)) {
			GtkTreeSelection *selection;
			GtkTreeIter       iter;
			char             *name, *utf8_name;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->catalog_list->list_view));
			if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
				return FALSE;

			utf8_name = catalog_list_get_name_from_iter (priv->catalog_list, &iter);
			name = g_filename_from_utf8 (utf8_name, -1, NULL, NULL, NULL);
			g_free (utf8_name);

			if (strcmp (name, "..") == 0) 
				return TRUE;

			if (catalog_list_is_dir (priv->catalog_list, &iter))
				gtk_menu_popup (GTK_MENU (priv->library_popup_menu),
						NULL,
						NULL,
						NULL,
						NULL,
						3,
						event->time);
			else
				gtk_menu_popup (GTK_MENU (priv->catalog_popup_menu),
						NULL,
						NULL,
						NULL,
						NULL,
						3,
						event->time);
		
			return TRUE;
			
		} else if ((priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
			   && GTK_WIDGET_HAS_FOCUS (priv->dir_list->list_view)) {
			GtkTreeSelection *selection;
			GtkTreeIter       iter;
			char             *name, *utf8_name;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->dir_list->list_view));
			if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
				return FALSE;

			utf8_name = dir_list_get_name_from_iter (priv->dir_list, &iter);
			name = g_filename_from_utf8 (utf8_name, -1, NULL, NULL, NULL);
			g_free (utf8_name);

			if (strcmp (name, "..") == 0) 
				return TRUE;

			gtk_menu_popup (GTK_MENU (priv->dir_popup_menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,
					event->time);
			
			return TRUE;
		} else if (GTK_WIDGET_HAS_FOCUS (gth_file_view_get_widget (priv->file_list->view))) {
			window_update_sensitivity (browser);
			
			gtk_menu_popup (GTK_MENU (priv->file_popup_menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,                               
					event->time);
			
			return TRUE;
		}
	}

	return FALSE;
}


static gboolean
image_clicked_cb (GtkWidget  *widget, 
		  GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (! priv->setting_file_list && ! priv->changing_directory)
		gth_browser_show_next_image (browser, FALSE);
	return TRUE;
}


static int
image_button_press_cb (GtkWidget      *widget, 
		       GdkEventButton *event,
		       gpointer        data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	switch (event->button) {
	case 1:
		break;
	case 2:
		break;
	case 3:
		if (priv->fullscreen)
			gtk_menu_popup (GTK_MENU (priv->fullscreen_image_popup_menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,
					event->time);
		else
			gtk_menu_popup (GTK_MENU (priv->image_popup_menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,
					event->time);

		return TRUE;
	}

	return FALSE;
}


static int
image_button_release_cb (GtkWidget      *widget, 
			 GdkEventButton *event,
			 gpointer        data)
{
	GthBrowser *browser = data;

	switch (event->button) {
	case 2:
		gth_browser_show_prev_image (browser, FALSE);
		return TRUE;
	default:
		break;
	}

	return FALSE;
}


static int
image_comment_button_press_cb (GtkWidget      *widget, 
			       GdkEventButton *event,
			       gpointer        data)
{
	GthBrowser *browser = data;

	if ((event->button == 1) && (event->type == GDK_2BUTTON_PRESS)) 
		if (gth_file_view_selection_not_null (browser->priv->file_list->view)) {
			gth_window_edit_comment (GTH_WINDOW (browser)); 
			return TRUE;
		}
	return FALSE;
}


static gboolean
image_focus_changed_cb (GtkWidget     *widget,
			GdkEventFocus *event,
			gpointer       data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	gboolean               viewer_visible;
	gboolean               data_visible;
	gboolean               comment_visible;

	viewer_visible  = ((priv->sidebar_visible 
			    && priv->preview_visible 
			    && (priv->preview_content == GTH_PREVIEW_CONTENT_IMAGE))
			   || ! priv->sidebar_visible);
	data_visible    = (priv->sidebar_visible 
			   && priv->preview_visible 
			   && (priv->preview_content == GTH_PREVIEW_CONTENT_DATA));
	comment_visible = (priv->sidebar_visible 
			   && priv->preview_visible 
			   && (priv->preview_content == GTH_PREVIEW_CONTENT_COMMENT));

	if (viewer_visible)
		gthumb_info_bar_set_focused (GTHUMB_INFO_BAR (priv->info_bar),
					     GTK_WIDGET_HAS_FOCUS (priv->viewer));
	else if (data_visible)
		gthumb_info_bar_set_focused (GTHUMB_INFO_BAR (priv->info_bar),
					     GTK_WIDGET_HAS_FOCUS (gth_exif_data_viewer_get_view (GTH_EXIF_DATA_VIEWER (priv->exif_data_viewer))));
	else if (comment_visible)
		gthumb_info_bar_set_focused (GTHUMB_INFO_BAR (priv->info_bar),
					     GTK_WIDGET_HAS_FOCUS (priv->image_comment));

	return FALSE;
}


/* -- drag & drop -- */


static GList *
get_file_list_from_url_list (char *url_list)
{
	GList *list = NULL;
	int    i;
	char  *url_start, *url_end;

	url_start = url_list;
	while (url_start[0] != '\0') {
		char *escaped;
		char *unescaped;

		if (strncmp (url_start, "file:", 5) == 0) {
			url_start += 5;
			if ((url_start[0] == '/') 
			    && (url_start[1] == '/')) url_start += 2;
		}

		i = 0;
		while ((url_start[i] != '\0')
		       && (url_start[i] != '\r')
		       && (url_start[i] != '\n')) i++;
		url_end = url_start + i;

		escaped = g_strndup (url_start, url_end - url_start);
		unescaped = gnome_vfs_unescape_string_for_display (escaped);
		g_free (escaped);

		list = g_list_prepend (list, unescaped);

		url_start = url_end;
		i = 0;
		while ((url_start[i] != '\0')
		       && ((url_start[i] == '\r')
			   || (url_start[i] == '\n'))) i++;
		url_start += i;
	}
	
	return g_list_reverse (list);
}


static void  
viewer_drag_data_get  (GtkWidget        *widget,
		       GdkDragContext   *context,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       gpointer          data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	char                  *path;

	if (IMAGE_VIEWER (priv->viewer)->is_void) 
		return;

	path = image_viewer_get_image_filename (IMAGE_VIEWER (priv->viewer));

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, 
				path, strlen (path));
	g_free (path);
}


static void  
viewer_drag_data_received  (GtkWidget          *widget,
			    GdkDragContext     *context,
			    int                 x,
			    int                 y,
			    GtkSelectionData   *data,
			    guint               info,
			    guint               time,
			    gpointer            extra_data)
{
	GthBrowser            *browser = extra_data;
	Catalog               *catalog;
	char                  *catalog_path;
	char                  *catalog_name, *catalog_name_utf8;
	GList                 *list;
	GList                 *scan;
	GError                *gerror;
	gboolean               empty = TRUE;

	if (! ((data->length >= 0) && (data->format == 8))) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	gtk_drag_finish (context, TRUE, FALSE, time);

	list = get_file_list_from_url_list ((char *) data->data);

	/* Create a catalog with the Drag&Drop list. */

	catalog = catalog_new ();
	catalog_name_utf8 = g_strconcat (_("Dragged Images"),
					 CATALOG_EXT,
					 NULL);
	catalog_name = g_filename_from_utf8 (catalog_name_utf8, -1, 0, 0, 0);
	catalog_path = get_catalog_full_path (catalog_name);
	g_free (catalog_name);
	g_free (catalog_name_utf8);

	catalog_set_path (catalog, catalog_path);

	for (scan = list; scan; scan = scan->next) {
		char *filename = scan->data;
		if (path_is_file (filename)) {
			catalog_add_item (catalog, filename);
			empty = FALSE;
		}
	}

	if (! empty) {
		if (! catalog_write_to_disk (catalog, &gerror)) 
			_gtk_error_dialog_from_gerror_run (GTK_WINDOW (browser), &gerror);
		else {
			/* View the Drag&Drop catalog. */
			ViewFirstImage = TRUE;
			gth_browser_go_to_catalog (browser, catalog_path);
		}
	}

	catalog_free (catalog);
	path_list_free (list);
	g_free (catalog_path);
}


static gint
viewer_key_press_cb (GtkWidget   *widget, 
		     GdkEventKey *event,
		     gpointer     data)
{
	GthBrowser *browser = data;

	switch (event->keyval) {
	case GDK_Page_Up:
		gth_browser_show_prev_image (browser, FALSE);
		return TRUE;

	case GDK_Page_Down:
		gth_browser_show_next_image (browser, FALSE);
		return TRUE;

	case GDK_Home:
		gth_browser_show_first_image (browser, FALSE);
		return TRUE;

	case GDK_End:
		gth_browser_show_last_image (browser, FALSE);
		return TRUE;

	case GDK_F10:
		if (event->state & GDK_SHIFT_MASK) {
			gtk_menu_popup (GTK_MENU (browser->priv->image_popup_menu),
					NULL,
					NULL,
					NULL,
					NULL,
					3,
					event->time);
			return TRUE;
		}
	}
	
	return FALSE;
}


static gboolean
info_bar_clicked_cb (GtkWidget      *widget,
		     GdkEventButton *event,
		     GthBrowser     *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *widget_to_focus = priv->viewer;

	if (priv->sidebar_visible)
		switch (priv->preview_content) {
		case GTH_PREVIEW_CONTENT_IMAGE:
			widget_to_focus = priv->viewer;
			break;
		case GTH_PREVIEW_CONTENT_DATA:
			widget_to_focus = gth_exif_data_viewer_get_view (GTH_EXIF_DATA_VIEWER (priv->exif_data_viewer));
			break;
		case GTH_PREVIEW_CONTENT_COMMENT:
			widget_to_focus = priv->image_comment;
			break;
		}

	gtk_widget_grab_focus (widget_to_focus);

	return TRUE;
}


static GString*
make_url_list (GList *list, 
	       int    target)
{
	GList      *scan;
	GString    *result;

	if (list == NULL)
		return NULL;

	result = g_string_new (NULL);
	for (scan = list; scan; scan = scan->next) {
		char *url;

		switch (target) {
		case TARGET_PLAIN:
			url = g_locale_from_utf8 (scan->data, -1, NULL, NULL, NULL);
			if (url == NULL) 
				continue;
			g_string_append (result, url);
			g_free (url);
			break;

		case TARGET_PLAIN_UTF8:
			g_string_append (result, scan->data);
			break;
			
		case TARGET_URILIST:
			url = gnome_vfs_get_uri_from_local_path (scan->data);
			if (url == NULL) 
				continue;
			g_string_append (result, url);
			g_free (url);
			break;
		}

		g_string_append (result, "\r\n");
	}
	
	return result;
}


static void
gth_file_list_drag_begin (GtkWidget      *widget,
			  GdkDragContext *context,
			  gpointer        extra_data)
{	
	GthBrowser *browser = extra_data;

	debug (DEBUG_INFO, "Gth::FileList::DragBegin");

	if (gth_file_list_drag_data != NULL)
		path_list_free (gth_file_list_drag_data);
	gth_file_list_drag_data = gth_file_view_get_file_list_selection (browser->priv->file_list->view);
}


static void
gth_file_list_drag_end (GtkWidget      *widget,
			GdkDragContext *context,
			gpointer        extra_data)
{	
	debug (DEBUG_INFO, "Gth::FileList::DragEnd");

	if (gth_file_list_drag_data != NULL)
		path_list_free (gth_file_list_drag_data);
	gth_file_list_drag_data = NULL;
}


static void  
gth_file_list_drag_data_get  (GtkWidget        *widget,
			      GdkDragContext   *context,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint             time,
			      gpointer          data)
{
	GString *url_list;

	debug (DEBUG_INFO, "Gth::FileList::DragDataGet");

	url_list = make_url_list (gth_file_list_drag_data, info);

	if (url_list == NULL) 
		return;

	gtk_selection_data_set (selection_data, 
				selection_data->target,
				8, 
				url_list->str, 
				url_list->len);

	g_string_free (url_list, TRUE);	
}


static void
move_items__continue (GnomeVFSResult result,
		      gpointer       data)
{
	GthBrowser *browser = data;

	if (result != GNOME_VFS_OK) 
		_gtk_error_dialog_run (GTK_WINDOW (browser),
				       "%s %s",
				       _("Could not move the items:"), 
				       gnome_vfs_result_to_string (result));
}


static void
add_image_list_to_catalog (GthBrowser *browser,
			   const char *catalog_path, 
			   GList      *list)
{
	Catalog *catalog;
	GError  *gerror;

	if ((catalog_path == NULL) || ! path_is_file (catalog_path)) 
		return;
	
	catalog = catalog_new ();
	
	if (! catalog_load_from_disk (catalog, catalog_path, &gerror)) 
		_gtk_error_dialog_from_gerror_run (GTK_WINDOW (browser), &gerror);
	
	else {
		GList *scan;
		GList *files_added = NULL;
			
		for (scan = list; scan; scan = scan->next) {
			char *filename = scan->data;
			if (path_is_file (filename)) {
				catalog_add_item (catalog, filename);
				files_added = g_list_prepend (files_added, filename);
			}
		}
		
		if (! catalog_write_to_disk (catalog, &gerror)) 
			_gtk_error_dialog_from_gerror_run (GTK_WINDOW (browser), &gerror);
		else 
			all_windows_notify_cat_files_added (catalog_path, files_added);
		
		g_list_free (files_added);
	}
	
	catalog_free (catalog);
}


static void  
image_list_drag_data_received  (GtkWidget        *widget,
				GdkDragContext   *context,
				int               x,
				int               y,
				GtkSelectionData *data,
				guint             info,
				guint             time,
				gpointer          extra_data)
{
	GthBrowser            *browser = extra_data;
	GthBrowserPrivateData *priv = browser->priv;

	if (! ((data->length >= 0) && (data->format == 8))
	    || (((context->action & GDK_ACTION_COPY) != GDK_ACTION_COPY)
		&& ((context->action & GDK_ACTION_MOVE) != GDK_ACTION_MOVE))) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	gtk_drag_finish (context, TRUE, FALSE, time);

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST) {
		char *dest_dir = priv->dir_list->path;
		if (dest_dir != NULL) {
			GList *list;
			list = get_file_list_from_url_list ((char*) data->data);
			dlg_copy_items (GTH_WINDOW (browser), 
					list,
					dest_dir,
					((context->action & GDK_ACTION_MOVE) == GDK_ACTION_MOVE),
					TRUE,
					FALSE,
					move_items__continue,
					browser);
			path_list_free (list);
		}

	} else if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) {
		GList *list = get_file_list_from_url_list ((char*) data->data);
		add_image_list_to_catalog (browser, priv->catalog_path, list);
		path_list_free (list);
	}
}


static void  
dir_list_drag_data_received  (GtkWidget          *widget,
			      GdkDragContext     *context,
			      int                 x,
			      int                 y,
			      GtkSelectionData   *data,
			      guint               info,
			      guint               time,
			      gpointer            extra_data)
{
	GthBrowser              *browser = extra_data;
	GthBrowserPrivateData   *priv = browser->priv;
	GtkTreePath             *pos_path;
	GtkTreeViewDropPosition  drop_pos;
	int                      pos;
	char                    *dest_dir = NULL;
	const char              *current_dir;

	debug (DEBUG_INFO, "DirList::DragDataReceived");

	if ((data->length < 0) 
	    || (data->format != 8)
	    || (((context->action & GDK_ACTION_COPY) != GDK_ACTION_COPY)
		&& ((context->action & GDK_ACTION_MOVE) != GDK_ACTION_MOVE))) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	gtk_drag_finish (context, TRUE, FALSE, time);

	g_return_if_fail (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST);

	/**/

	if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (priv->dir_list->list_view),
					       x, y,
					       &pos_path,
					       &drop_pos)) {
		pos = gtk_tree_path_get_indices (pos_path)[0];
		gtk_tree_path_free (pos_path);
	} else
		pos = -1;
	
	current_dir = priv->dir_list->path;
	
	if (pos == -1) {
		if (current_dir != NULL)
			dest_dir = g_strdup (current_dir);
	} else
		dest_dir = dir_list_get_path_from_row (priv->dir_list,
						       pos);
	
	/**/

	debug (DEBUG_INFO, "DirList::DragDataReceived: %s", dest_dir);
	
	if (dest_dir != NULL) {
		GList *list;
		list = get_file_list_from_url_list ((char*) data->data);

		dlg_copy_items (GTH_WINDOW (browser), 
				list,
				dest_dir,
				((context->action & GDK_ACTION_MOVE) == GDK_ACTION_MOVE),
				TRUE,
				FALSE,
				move_items__continue,
				browser);
		path_list_free (list);
	}
	
	g_free (dest_dir);
}


static gboolean
auto_load_directory_cb (gpointer data)
{
	GthBrowser              *browser = data;
	GthBrowserPrivateData   *priv = browser->priv;
	GtkTreePath             *pos_path;
	GtkTreeViewDropPosition  drop_pos;
	GtkTreeView             *list_view;
	char                    *new_path;

	list_view = GTK_TREE_VIEW (priv->dir_list->list_view);

	g_source_remove (priv->auto_load_timeout);

	gtk_tree_view_get_drag_dest_row (list_view, &pos_path, &drop_pos);
	new_path = dir_list_get_path_from_tree_path (priv->dir_list, pos_path);
	if (new_path)  {
		gth_browser_go_to_directory (browser, new_path);
		g_free(new_path);
	}

	gtk_tree_path_free (pos_path);

	priv->auto_load_timeout = 0;

	return FALSE;
}


static gboolean
dir_list_drag_motion (GtkWidget          *widget,
		      GdkDragContext     *context,
		      gint                x,
		      gint                y,
		      guint               time,
		      gpointer            extra_data)
{
	GthBrowser              *browser = extra_data;
	GthBrowserPrivateData   *priv = browser->priv;
	GtkTreePath             *pos_path;
	GtkTreeViewDropPosition  drop_pos;
	GtkTreeView             *list_view;

	debug (DEBUG_INFO, "DirList::DragMotion");

	list_view = GTK_TREE_VIEW (priv->dir_list->list_view);

	if (priv->auto_load_timeout != 0) {
		g_source_remove (priv->auto_load_timeout);
		priv->auto_load_timeout = 0;
	}

	if (! gtk_tree_view_get_dest_row_at_pos (list_view,
						 x, y,
						 &pos_path,
						 &drop_pos)) 
		pos_path = gtk_tree_path_new_first ();

	else 
		priv->auto_load_timeout = g_timeout_add (AUTO_LOAD_DELAY, 
							   auto_load_directory_cb, 
							   browser);

	switch (drop_pos) {
	case GTK_TREE_VIEW_DROP_BEFORE:
	case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
		drop_pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
		break;

	case GTK_TREE_VIEW_DROP_AFTER:
	case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
		drop_pos = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
		break;
	}

	gtk_tree_view_set_drag_dest_row (list_view, pos_path, drop_pos);
	gtk_tree_path_free (pos_path);

	return TRUE;
}


static void
dir_list_drag_begin (GtkWidget          *widget,
		     GdkDragContext     *context,
		     gpointer            extra_data)
{	
	GthBrowser            *browser = extra_data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkTreeIter            iter;
	GtkTreeSelection      *selection;

	debug (DEBUG_INFO, "DirList::DragBegin");

	if (dir_list_tree_path != NULL) {
		gtk_tree_path_free (dir_list_tree_path);
		dir_list_tree_path = NULL;
	}

	if (dir_list_drag_data != NULL) {
		g_free (dir_list_drag_data);
		dir_list_drag_data = NULL;
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->dir_list->list_view));
	if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	dir_list_drag_data = dir_list_get_path_from_iter (priv->dir_list, &iter);
}


static void
dir_list_drag_end (GtkWidget          *widget,
		   GdkDragContext     *context,
		   gpointer            extra_data)
{	
	if (dir_list_drag_data != NULL) {
		g_free (dir_list_drag_data);
		dir_list_drag_data = NULL;
	}
}


static void
dir_list_drag_leave (GtkWidget          *widget,
		     GdkDragContext     *context,
		     guint               time,
		     gpointer            extra_data)
{	
	GthBrowser            *browser = extra_data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkTreeView           *list_view;

	debug (DEBUG_INFO, "DirList::DragLeave");

	if (priv->auto_load_timeout != 0) {
		g_source_remove (priv->auto_load_timeout);
		priv->auto_load_timeout = 0;
	}

	list_view = GTK_TREE_VIEW (priv->dir_list->list_view);
	gtk_tree_view_set_drag_dest_row  (list_view, NULL, 0);
}


static void  
dir_list_drag_data_get  (GtkWidget        *widget,
			 GdkDragContext   *context,
			 GtkSelectionData *selection_data,
			 guint             info,
			 guint             time,
			 gpointer          data)
{
        char *target;
	char *uri;

	debug (DEBUG_INFO, "DirList::DragDataGet");

	if (dir_list_drag_data == NULL)
		return;

        target = gdk_atom_name (selection_data->target);
        if (strcmp (target, "text/uri-list") != 0) {
		g_free (target);
		return;
	}
        g_free (target);

	uri = g_strconcat ("file://", dir_list_drag_data, "\n", NULL);
        gtk_selection_data_set (selection_data, 
                                selection_data->target,
                                8, 
                                dir_list_drag_data, 
                                strlen (uri));
	g_free (dir_list_drag_data);
	dir_list_drag_data = NULL;
	g_free (uri);
}


static void  
catalog_list_drag_data_received  (GtkWidget          *widget,
				  GdkDragContext     *context,
				  int                 x,
				  int                 y,
				  GtkSelectionData   *data,
				  guint               info,
				  guint               time,
				  gpointer            extra_data)
{
	GthBrowser              *browser = extra_data;
	GthBrowserPrivateData   *priv = browser->priv;
	GtkTreePath             *pos_path;
	GtkTreeViewDropPosition  drop_pos;
	int                      pos;
	char                    *catalog_path = NULL;

	if (! ((data->length >= 0) && (data->format == 8))) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	gtk_drag_finish (context, TRUE, FALSE, time);

	g_return_if_fail (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST);

	/**/

	if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (priv->catalog_list->list_view),
					       x, y,
					       &pos_path,
					       &drop_pos)) {
		pos = gtk_tree_path_get_indices (pos_path)[0];
		gtk_tree_path_free (pos_path);
	} else
		pos = -1;
	
	if (pos == -1) {
		if (priv->catalog_path != NULL)
			catalog_path = g_strdup (priv->catalog_path);
	} else
		catalog_path = catalog_list_get_path_from_row (priv->catalog_list, pos);
	
	if (catalog_path != NULL) {
		GList *list = get_file_list_from_url_list ((char*) data->data);
		add_image_list_to_catalog (browser, catalog_path, list);
		path_list_free (list);
		g_free (catalog_path);
	}
}


static gboolean
auto_load_library_cb (gpointer data)
{
	GthBrowser              *browser = data;
	GthBrowserPrivateData   *priv = browser->priv;
	GtkTreePath             *pos_path;
	GtkTreeViewDropPosition  drop_pos;
	GtkTreeView             *list_view;
	char                    *new_path;

	list_view = GTK_TREE_VIEW (priv->catalog_list->list_view);

	g_source_remove (priv->auto_load_timeout);
	priv->auto_load_timeout = 0;

	gtk_tree_view_get_drag_dest_row (list_view, &pos_path, &drop_pos);
	new_path = catalog_list_get_path_from_tree_path (priv->catalog_list, pos_path);

	if (new_path)  {
		if (path_is_dir (new_path))
			gth_browser_go_to_catalog_directory (browser, new_path);
		g_free(new_path);
	}

	gtk_tree_path_free (pos_path);

	return FALSE;
}


static gboolean
catalog_list_drag_motion (GtkWidget          *widget,
			  GdkDragContext     *context,
			  gint                x,
			  gint                y,
			  guint               time,
			  gpointer            extra_data)
{
	GthBrowser              *browser = extra_data;
	GthBrowserPrivateData   *priv = browser->priv;
	GtkTreePath             *pos_path;
	GtkTreeViewDropPosition  drop_pos;
	GtkTreeView             *list_view;

	list_view = GTK_TREE_VIEW (priv->catalog_list->list_view);

	if (priv->auto_load_timeout != 0) {
		g_source_remove (priv->auto_load_timeout);
		priv->auto_load_timeout = 0;
	}

	if (! gtk_tree_view_get_dest_row_at_pos (list_view,
						 x, y,
						 &pos_path,
						 &drop_pos)) 
		pos_path = gtk_tree_path_new_first ();
	else 
		priv->auto_load_timeout = g_timeout_add (AUTO_LOAD_DELAY, 
							   auto_load_library_cb, 
							   browser);

	switch (drop_pos) {
	case GTK_TREE_VIEW_DROP_BEFORE:
	case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
		drop_pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
		break;

	case GTK_TREE_VIEW_DROP_AFTER:
	case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
		drop_pos = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
		break;
	}

	gtk_tree_view_set_drag_dest_row  (list_view, pos_path, drop_pos);
	gtk_tree_path_free (pos_path);

	return TRUE;
}


static void
catalog_list_drag_begin (GtkWidget          *widget,
			 GdkDragContext     *context,
			 gpointer            extra_data)
{	
	if (catalog_list_tree_path != NULL) {
		gtk_tree_path_free (catalog_list_tree_path);
		catalog_list_tree_path = NULL;
	}
}


static void
catalog_list_drag_leave (GtkWidget          *widget,
			 GdkDragContext     *context,
			 guint               time,
			 gpointer            extra_data)
{	
	GthBrowser            *browser = extra_data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkTreeView           *list_view;

	if (priv->auto_load_timeout != 0) {
		g_source_remove (priv->auto_load_timeout);
		priv->auto_load_timeout = 0;
	}

	list_view = GTK_TREE_VIEW (priv->catalog_list->list_view);
	gtk_tree_view_set_drag_dest_row  (list_view, NULL, 0);
}


/* -- */


static gboolean
set_busy_cursor_cb (gpointer data)
{
	GthBrowser *browser = data;
	GdkCursor  *cursor;

	if (browser->priv->busy_cursor_timeout != 0) {
		g_source_remove (browser->priv->busy_cursor_timeout);
		browser->priv->busy_cursor_timeout = 0;
	}

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (GTK_WIDGET (browser)->window, cursor);
	gdk_cursor_unref (cursor);

	return FALSE;
}


static void
file_list_busy_cb (GthFileList *file_list,
		   GthBrowser  *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (! GTK_WIDGET_REALIZED (browser))
		return;

	if (priv->busy_cursor_timeout != 0)
		g_source_remove (priv->busy_cursor_timeout);

	priv->busy_cursor_timeout = g_timeout_add (BUSY_CURSOR_DELAY,
						   set_busy_cursor_cb,
						   browser);
}


static void
file_list_idle_cb (GthFileList *file_list,
		   GthBrowser  *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GdkCursor             *cursor;

	if (! GTK_WIDGET_REALIZED (browser))
		return;

	if (priv->busy_cursor_timeout != 0) {
		g_source_remove (priv->busy_cursor_timeout);
		priv->busy_cursor_timeout = 0;
	}

	cursor = gdk_cursor_new (GDK_LEFT_PTR);
	gdk_window_set_cursor (GTK_WIDGET (browser)->window, cursor);
	gdk_cursor_unref (cursor);
}


static gboolean
progress_cancel_cb (GtkButton  *button,
		    GthBrowser *browser)
{
	if (browser->priv->pixop != NULL)
		gth_pixbuf_op_stop (browser->priv->pixop);
	return TRUE;
}


static gboolean
progress_delete_cb (GtkWidget  *caller, 
		    GdkEvent   *event,
		    GthBrowser *browser)
{
	if (browser->priv->pixop != NULL)
		gth_pixbuf_op_stop (browser->priv->pixop);
	return TRUE;
}


static void
window_sync_menu_with_preferences (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *prop = "TranspTypeNone";

	set_action_active (browser, "View_PlayAnimation", TRUE);

	switch (pref_get_zoom_quality ()) {
	case GTH_ZOOM_QUALITY_HIGH: prop = "View_ZoomQualityHigh"; break;
	case GTH_ZOOM_QUALITY_LOW:  prop = "View_ZoomQualityLow"; break;
	}
	set_action_active (browser, prop, TRUE);

	set_action_active (browser, "View_ShowFolders", FALSE);
	set_action_active (browser, "View_ShowCatalogs", FALSE);

	set_action_active (browser, "View_ShowPreview", eel_gconf_get_boolean (PREF_SHOW_PREVIEW, FALSE));
	set_action_active (browser, "View_ShowInfo", eel_gconf_get_boolean (PREF_SHOW_IMAGE_DATA, FALSE));

	/* Sort type item. */

	switch (priv->file_list->sort_method) {
	case GTH_SORT_METHOD_BY_NAME: prop = "SortByName"; break;
	case GTH_SORT_METHOD_BY_PATH: prop = "SortByPath"; break;
	case GTH_SORT_METHOD_BY_SIZE: prop = "SortBySize"; break;
	case GTH_SORT_METHOD_BY_TIME: prop = "SortByTime"; break;
	default: prop = "X"; break;
	}
	set_action_active (browser, prop, TRUE);

	/* 'reversed' item. */

	set_action_active (browser, prop, TRUE);
	set_action_active (browser, "SortReversed", (priv->file_list->sort_type != GTK_SORT_ASCENDING));
}


/* preferences change notification callbacks */


static void
pref_ui_layout_changed (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
	GthBrowser *browser = user_data;
	gth_browser_notify_update_layout (browser);
}


static void
pref_ui_toolbar_style_changed (GConfClient *client,
			       guint        cnxn_id,
			       GConfEntry  *entry,
			       gpointer     user_data)
{
	GthBrowser *browser = user_data;
	gth_browser_notify_update_toolbar_style (browser);
}


static void
gth_browser_set_toolbar_visibility (GthBrowser *browser,
				    gboolean    visible)
{
	g_return_if_fail (browser != NULL);

	set_action_active (browser, "View_Toolbar", visible);
	if (visible)
		gtk_widget_show (browser->priv->toolbar->parent);
	else
		gtk_widget_hide (browser->priv->toolbar->parent);
}


static void
pref_ui_toolbar_visible_changed (GConfClient *client,
				 guint        cnxn_id,
				 GConfEntry  *entry,
				 gpointer     user_data)
{
	GthBrowser *browser = user_data;
	gth_browser_set_toolbar_visibility (browser, gconf_value_get_bool (gconf_entry_get_value (entry)));
}


static void
gth_browser_set_statusbar_visibility  (GthBrowser *browser,
				       gboolean    visible)
{
	g_return_if_fail (browser != NULL);

	set_action_active (browser, "View_Statusbar", visible);
	if (visible) 
		gtk_widget_show (browser->priv->statusbar);
	else
		gtk_widget_hide (browser->priv->statusbar);
}


static void
pref_ui_statusbar_visible_changed (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   gpointer     user_data)
{
	GthBrowser *browser = user_data;
	gth_browser_set_statusbar_visibility (browser, gconf_value_get_bool (gconf_entry_get_value (entry)));
}


static void
pref_show_thumbnails_changed (GConfClient *client,
			      guint        cnxn_id,
			      GConfEntry  *entry,
			      gpointer     user_data)
{
	GthBrowser            *browser = user_data;
	GthBrowserPrivateData *priv = browser->priv;

	priv->file_list->enable_thumbs = eel_gconf_get_boolean (PREF_SHOW_THUMBNAILS, TRUE);
	window_enable_thumbs (browser, priv->file_list->enable_thumbs);
}


static void
pref_show_filenames_changed (GConfClient *client,
			     guint        cnxn_id,
			     GConfEntry  *entry,
			     gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	gth_file_view_set_view_mode (browser->priv->file_list->view, pref_get_view_mode ());
}


static void
pref_show_comments_changed (GConfClient *client,
			    guint        cnxn_id,
			    GConfEntry  *entry,
			    gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	gth_file_view_set_view_mode (browser->priv->file_list->view, pref_get_view_mode ());
}


static void
pref_show_hidden_files_changed (GConfClient *client,
				guint        cnxn_id,
				GConfEntry  *entry,
				gpointer     user_data)
{
	GthBrowser *browser = user_data;
	gth_browser_update_file_list (browser);
}


static void
pref_thumbnail_size_changed (GConfClient *client,
			     guint        cnxn_id,
			     GConfEntry  *entry,
			     gpointer     user_data)
{
	GthBrowser *browser = user_data;
	gth_file_list_set_thumbs_size (browser->priv->file_list, eel_gconf_get_integer (PREF_THUMBNAIL_SIZE, 95));
}


static void
pref_click_policy_changed (GConfClient *client,
			   guint        cnxn_id,
			   GConfEntry  *entry,
			   gpointer     user_data)
{
	GthBrowser *browser = user_data;
	dir_list_update_underline (browser->priv->dir_list);
	catalog_list_update_underline (browser->priv->catalog_list);
}


static void
pref_zoom_quality_changed (GConfClient *client,
			   guint        cnxn_id,
			   GConfEntry  *entry,
			   gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	ImageViewer *image_viewer = IMAGE_VIEWER (browser->priv->viewer);

	if (pref_get_zoom_quality () == GTH_ZOOM_QUALITY_HIGH) {
		set_action_active_if_different (browser, "View_ZoomQualityHigh", 1);
		image_viewer_set_zoom_quality (image_viewer, GTH_ZOOM_QUALITY_HIGH);
	} else {
		set_action_active_if_different (browser, "View_ZoomQualityLow", 1);
		image_viewer_set_zoom_quality (image_viewer, GTH_ZOOM_QUALITY_LOW);
	}

	image_viewer_update_view (image_viewer);
}


static void
pref_zoom_change_changed (GConfClient *client,
			  guint        cnxn_id,
			  GConfEntry  *entry,
			  gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	ImageViewer *image_viewer = IMAGE_VIEWER (browser->priv->viewer);

	image_viewer_set_zoom_change (image_viewer, pref_get_zoom_change ());
	image_viewer_update_view (image_viewer);
}


static void
pref_transp_type_changed (GConfClient *client,
			  guint        cnxn_id,
			  GConfEntry  *entry,
			  gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	ImageViewer *image_viewer = IMAGE_VIEWER (browser->priv->viewer);

	image_viewer_set_transp_type (image_viewer, pref_get_transp_type ());
	image_viewer_update_view (image_viewer);
}


static void
pref_check_type_changed (GConfClient *client,
			 guint        cnxn_id,
			 GConfEntry  *entry,
			 gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	ImageViewer *image_viewer = IMAGE_VIEWER (browser->priv->viewer);

	image_viewer_set_check_type (image_viewer, pref_get_check_type ());
	image_viewer_update_view (image_viewer);
}


static void
pref_check_size_changed (GConfClient *client,
			 guint        cnxn_id,
			 GConfEntry  *entry,
			 gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	ImageViewer *image_viewer = IMAGE_VIEWER (browser->priv->viewer);

	image_viewer_set_check_size (image_viewer, pref_get_check_size ());
	image_viewer_update_view (image_viewer);
}


static void
pref_black_background_changed (GConfClient *client,
			       guint        cnxn_id,
			       GConfEntry  *entry,
			       gpointer     user_data)
{
	GthBrowser  *browser = user_data;
	ImageViewer *image_viewer = IMAGE_VIEWER (browser->priv->viewer);

	image_viewer_set_black_background (image_viewer,
					   eel_gconf_get_boolean (PREF_BLACK_BACKGROUND, FALSE));
}


static GthFileList *
create_new_file_list (GthBrowser *browser)
{
	GthFileList *file_list;
	GtkWidget   *view_widget;

	file_list = gth_file_list_new ();
	view_widget = gth_file_view_get_widget (file_list->view);

	gtk_widget_set_size_request (file_list->root_widget,
				     PANE_MIN_SIZE,
				     PANE_MIN_SIZE);
	gth_file_list_set_progress_func (file_list, window_progress, browser);

	gtk_drag_dest_set (file_list->root_widget,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, G_N_ELEMENTS (target_table),
			   GDK_ACTION_COPY);
	gtk_drag_source_set (file_list->root_widget,
			     GDK_BUTTON1_MASK,
			     target_table, G_N_ELEMENTS (target_table),
			     GDK_ACTION_MOVE | GDK_ACTION_COPY);
	/* FIXME
	gtk_drag_dest_set (file_list->root_widget,
			   GTK_DEST_DEFAULT_ALL,
			   same_app_target_table, G_N_ELEMENTS (same_app_target_table),
			   GDK_ACTION_MOVE);
	
	gtk_drag_source_set (file_list->root_widget,
			     GDK_BUTTON1_MASK,
			     same_app_target_table, G_N_ELEMENTS (same_app_target_table),
			     GDK_ACTION_MOVE);*/
	g_signal_connect (G_OBJECT (file_list->view),
			  "selection_changed",
			  G_CALLBACK (file_selection_changed_cb), 
			  browser);
	g_signal_connect (G_OBJECT (file_list->view),
			  "cursor_changed",
			  G_CALLBACK (gth_file_list_cursor_changed_cb), 
			  browser);
	g_signal_connect (G_OBJECT (file_list->view),
			  "item_activated",
			  G_CALLBACK (gth_file_list_item_activated_cb), 
			  browser);

	/* FIXME
	g_signal_connect_after (G_OBJECT (view_widget),
				"button_press_event",
				G_CALLBACK (gth_file_list_button_press_cb), 
				browser);
	*/

	g_signal_connect (G_OBJECT (view_widget),
			  "button_press_event",
			  G_CALLBACK (gth_file_list_button_press_cb), 
			  browser);

	g_signal_connect (G_OBJECT (file_list->root_widget),
			  "drag_data_received",
			  G_CALLBACK (image_list_drag_data_received), 
			  browser);
	g_signal_connect (G_OBJECT (file_list->root_widget),
			  "drag_begin",
			  G_CALLBACK (gth_file_list_drag_begin), 
			  browser);
	g_signal_connect (G_OBJECT (file_list->root_widget),
			  "drag_end",
			  G_CALLBACK (gth_file_list_drag_end), 
			  browser);
	g_signal_connect (G_OBJECT (file_list->root_widget),
			  "drag_data_get",
			  G_CALLBACK (gth_file_list_drag_data_get), 
			  browser);

	g_signal_connect (G_OBJECT (file_list),
			  "busy",
			  G_CALLBACK (file_list_busy_cb),
			  browser);
	g_signal_connect (G_OBJECT (file_list),
			  "idle",
			  G_CALLBACK (file_list_idle_cb),
			  browser);

	return file_list;
}


static void
pref_view_as_changed (GConfClient *client,
		      guint        cnxn_id,
		      GConfEntry  *entry,
		      gpointer     user_data)
{
	GthBrowser            *browser = user_data;
	GthBrowserPrivateData *priv = browser->priv;
	GthFileList           *file_list;
	GthSortMethod          sort_method;
	GtkSortType            sort_type;
	gboolean               enable_thumbs;

	sort_method = priv->file_list->sort_method;
	sort_type = priv->file_list->sort_type;
	enable_thumbs = priv->file_list->enable_thumbs;

	/**/

	file_list = create_new_file_list (browser);
	gth_file_list_set_sort_method (file_list, sort_method);
	gth_file_list_set_sort_type (file_list, sort_type);
	gth_file_list_enable_thumbs (file_list, enable_thumbs);

	gtk_widget_destroy (priv->file_list->root_widget);
	gtk_box_pack_start (GTK_BOX (priv->file_list_pane), file_list->root_widget, TRUE, TRUE, 0);

	/*
	if (priv->layout_type <= 1) {
		gtk_widget_destroy (GTK_PANED (priv->content_pane)->child2);
		GTK_PANED (priv->content_pane)->child2 = NULL;
		gtk_paned_pack2 (GTK_PANED (priv->content_pane), file_list->root_widget, TRUE, FALSE);
	} else if (priv->layout_type == 2) {
		gtk_widget_destroy (GTK_PANED (priv->main_pane)->child2);
		GTK_PANED (priv->main_pane)->child2 = NULL;
		gtk_paned_pack2 (GTK_PANED (priv->main_pane), file_list->root_widget, TRUE, FALSE);
	} else if (priv->layout_type == 3) {
		gtk_widget_destroy (GTK_PANED (priv->content_pane)->child1);
		GTK_PANED (priv->content_pane)->child1 = NULL;
		gtk_paned_pack1 (GTK_PANED (priv->content_pane), file_list->root_widget, FALSE, FALSE);
	}
	*/

	g_object_unref (priv->file_list);
	priv->file_list = file_list;

	gtk_widget_show_all (priv->file_list->root_widget);
	gth_browser_update_file_list (browser);
}


void
gth_browser_set_preview_content (GthBrowser        *browser,
			    GthPreviewContent  content)
{
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *widget_to_focus = priv->viewer;

	priv->preview_content = content;

	set_button_active_no_notify (browser, priv->preview_button_image, FALSE);
	set_button_active_no_notify (browser, priv->preview_button_data, FALSE);
	set_button_active_no_notify (browser, priv->preview_button_comment, FALSE);

	if (priv->preview_content == GTH_PREVIEW_CONTENT_IMAGE) {
		gtk_widget_hide (priv->preview_widget_data);
		gtk_widget_hide (priv->preview_widget_comment);
		gtk_widget_hide (priv->preview_widget_data_comment);
		gtk_widget_show (priv->preview_widget_image);
		set_button_active_no_notify (browser, priv->preview_button_image, TRUE);

	} else if (priv->preview_content == GTH_PREVIEW_CONTENT_DATA) {
		gtk_widget_hide (priv->preview_widget_image);
		gtk_widget_hide (priv->preview_widget_comment);
		gtk_widget_show (priv->preview_widget_data);
		gtk_widget_show (priv->preview_widget_data_comment);
		set_button_active_no_notify (browser, priv->preview_button_data, TRUE);

	} else if (priv->preview_content == GTH_PREVIEW_CONTENT_COMMENT) {
		gtk_widget_hide (priv->preview_widget_image);
		gtk_widget_hide (priv->preview_widget_data);
		gtk_widget_show (priv->preview_widget_comment);
		gtk_widget_show (priv->preview_widget_data_comment);
		set_button_active_no_notify (browser, priv->preview_button_comment, TRUE);
	}

	switch (priv->preview_content) {
	case GTH_PREVIEW_CONTENT_IMAGE:
		widget_to_focus = priv->viewer;
		break;
	case GTH_PREVIEW_CONTENT_DATA:
		widget_to_focus = gth_exif_data_viewer_get_view (GTH_EXIF_DATA_VIEWER (priv->exif_data_viewer));
		break;
	case GTH_PREVIEW_CONTENT_COMMENT:
		widget_to_focus = priv->image_comment;
		break;
	}

	gtk_widget_grab_focus (widget_to_focus);

	window_update_statusbar_zoom_info (browser);
	window_update_sensitivity (browser);
}


static void
close_preview_image_button_cb (GtkToggleButton *button,
			       GthBrowser      *browser)
{
	if (browser->priv->sidebar_visible) {
		if (browser->priv->image_pane_visible)
			gth_browser_hide_image_pane (browser);
	} else 
		gth_browser_show_sidebar (browser);
}


static void
preview_button_cb (GthBrowser        *browser,
		   GtkToggleButton   *button,
		   GthPreviewContent  content)
{
	if (gtk_toggle_button_get_active (button)) 
		gth_browser_set_preview_content (browser, content);
	else if (browser->priv->preview_content == content) 
		gtk_toggle_button_set_active (button, TRUE);
}


static void
preview_image_button_cb (GtkToggleButton *button,
			 GthBrowser      *browser)
{
	preview_button_cb (browser, button, GTH_PREVIEW_CONTENT_IMAGE);
}


static void
preview_data_button_cb (GtkToggleButton *button,
			GthBrowser      *browser)
{
	if (browser->priv->sidebar_visible) {
		preview_button_cb (browser, button, GTH_PREVIEW_CONTENT_DATA);
	} else {
		if (gtk_toggle_button_get_active (button))
			gth_browser_show_image_data (browser);
		else
			gth_browser_hide_image_data (browser);
	}
}


static void
preview_comment_button_cb (GtkToggleButton *button,
			   GthBrowser    *browser)
{
	preview_button_cb (browser, button, GTH_PREVIEW_CONTENT_COMMENT);
}


static gboolean
initial_location_cb (gpointer data)
{
	GthBrowser *browser = data;
	const char *starting_location = browser->priv->initial_location;

	if (pref_util_location_is_catalog (starting_location)) 
		gth_browser_go_to_catalog (browser, pref_util_get_catalog_location (starting_location));

	else {
		const char *path;

		if (pref_util_location_is_file (starting_location))
			path = pref_util_get_file_location (starting_location);

		else {  /* we suppose it is a directory name without prefix. */
			path = starting_location;
			if (! path_is_dir (path))
				return FALSE;
		}

		gth_browser_go_to_directory (browser, path);
	}

	gtk_widget_grab_focus (gth_file_view_get_widget (browser->priv->file_list->view));

	return FALSE;
}


static void
menu_item_select_cb (GtkMenuItem *proxy,
                     GthBrowser  *browser)
{
        GtkAction *action;
        char      *message;
	
        action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
        g_return_if_fail (action != NULL);
	
        g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
        if (message != NULL) {
		gtk_statusbar_push (GTK_STATUSBAR (browser->priv->statusbar),
				    browser->priv->help_message_cid, message);
		g_free (message);
        }
}


static void
menu_item_deselect_cb (GtkMenuItem *proxy,
		       GthBrowser  *browser)
{
        gtk_statusbar_pop (GTK_STATUSBAR (browser->priv->statusbar),
                           browser->priv->help_message_cid);
}


static void
disconnect_proxy_cb (GtkUIManager *manager,
		     GtkAction    *action,
		     GtkWidget    *proxy,
		     GthBrowser   *browser)
{
        if (GTK_IS_MENU_ITEM (proxy)) {
                g_signal_handlers_disconnect_by_func
                        (proxy, G_CALLBACK (menu_item_select_cb), browser);
                g_signal_handlers_disconnect_by_func
                        (proxy, G_CALLBACK (menu_item_deselect_cb), browser);
        }
}


static void
connect_proxy_cb (GtkUIManager *manager,
		  GtkAction    *action,
		  GtkWidget    *proxy,
                  GthBrowser   *browser)
{
        if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), browser);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), browser);
	}
}


static void
sort_by_radio_action (GtkAction      *action,
		      GtkRadioAction *current,
		      gpointer        data)
{
	GthBrowser *browser = data;
	gth_file_list_set_sort_method (browser->priv->file_list, gtk_radio_action_get_current_value (current));
}


static void
zoom_quality_radio_action (GtkAction      *action,
			   GtkRadioAction *current,
			   gpointer        data)
{
	GthBrowser     *browser = data;
	ImageViewer    *image_viewer = IMAGE_VIEWER (browser->priv->viewer);
	GthZoomQuality  quality;

	quality = gtk_radio_action_get_current_value (current);
	gtk_radio_action_get_current_value (current);
	image_viewer_set_zoom_quality (image_viewer, quality);
	image_viewer_update_view (image_viewer);
	pref_set_zoom_quality (quality);
}


static void
add_go_back_toolbar_item (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;	

	if (priv->go_back_tool_item != NULL) {
		gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar), priv->go_back_tool_item, 4); /*FIXME*/
		return;
	}

	priv->go_back_tool_item = gtk_menu_tool_button_new_from_stock (GTK_STOCK_GO_BACK);
	g_object_ref (priv->go_back_tool_item);
	gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (priv->go_back_tool_item),
				       gtk_ui_manager_get_widget (priv->ui, "/HistoryListPopup"));
	gtk_tool_item_set_homogeneous (priv->go_back_tool_item, FALSE);
	gtk_tool_item_set_tooltip (priv->go_back_tool_item, priv->tooltips, _("Back"), NULL);
	gtk_menu_tool_button_set_arrow_tooltip (GTK_MENU_TOOL_BUTTON (priv->go_back_tool_item), priv->tooltips,	_("Go to the previous visited location"), NULL);

	gtk_action_connect_proxy (gtk_ui_manager_get_action (priv->ui, "/MenuBar/Go/Go_Back"),
				  GTK_WIDGET (priv->go_back_tool_item));

	gtk_widget_show (GTK_WIDGET (priv->go_back_tool_item));
	gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar), priv->go_back_tool_item, 4);  /*FIXME*/

	return;
}


static void
set_mode_specific_ui_info (GthBrowser        *browser, 
			   GthSidebarContent  content,
			   gboolean           first_time)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->toolbar_merge_id != 0)
		gtk_ui_manager_remove_ui (priv->ui, priv->toolbar_merge_id);

	if ((priv->go_back_tool_item != NULL)
	    && (gtk_widget_get_parent ((GtkWidget*) priv->go_back_tool_item) != NULL)) {
		gtk_container_remove (GTK_CONTAINER (priv->toolbar), (GtkWidget*) priv->go_back_tool_item);
	}
	gtk_ui_manager_ensure_update (priv->ui);

	if (content != GTH_SIDEBAR_NO_LIST) {
		if (!first_time)
			gth_browser_set_sidebar_content (browser, content);
		priv->toolbar_merge_id = gtk_ui_manager_add_ui_from_string (priv->ui, browser_ui_info, -1, NULL);
		add_go_back_toolbar_item (browser);

	} else {
		if (!first_time)
			gth_browser_hide_sidebar (browser);
		priv->toolbar_merge_id = gtk_ui_manager_add_ui_from_string (priv->ui, viewer_ui_info, -1, NULL);
	}

	gtk_ui_manager_ensure_update (priv->ui);
}


static void
content_radio_action (GtkAction      *action,
		      GtkRadioAction *current,
		      gpointer        data)
{
	GthBrowser        *browser = data;
	GthSidebarContent  content = gtk_radio_action_get_current_value (current);

	set_mode_specific_ui_info (browser, content, FALSE);
}


static void
gth_browser_show (GtkWidget  *widget)
{
	GthBrowser            *browser = GTH_BROWSER (widget);
	GthBrowserPrivateData *priv = browser->priv;

	GTK_WIDGET_CLASS (parent_class)->show (widget);
	
	if (!browser->priv->first_time_show) 
		return;

	browser->priv->first_time_show = FALSE;

	/* toolbar */

	if (eel_gconf_get_boolean (PREF_UI_TOOLBAR_VISIBLE, TRUE))
		gtk_widget_show (priv->toolbar->parent);
	else
		gtk_widget_hide (priv->toolbar->parent);

	set_action_active (browser, 
			   "View_Toolbar", 
			   eel_gconf_get_boolean (PREF_UI_TOOLBAR_VISIBLE, TRUE));

	/* statusbar */

	if (eel_gconf_get_boolean (PREF_UI_STATUSBAR_VISIBLE, TRUE))
		gtk_widget_show (priv->statusbar);
	else
		gtk_widget_hide (priv->statusbar);

	set_action_active (browser, 
			   "View_Statusbar", 
			   eel_gconf_get_boolean (PREF_UI_STATUSBAR_VISIBLE, TRUE));
}


static void 
gth_browser_finalize (GObject *object)
{
	GthBrowser *browser = GTH_BROWSER (object);

	debug (DEBUG_INFO, "Gth::Browser::Finalize");

	if (browser->priv != NULL) {
		GthBrowserPrivateData *priv = browser->priv;
		int                    i;

		g_object_unref (priv->file_list);
		dir_list_free (priv->dir_list);
		catalog_list_free (priv->catalog_list);

		if (priv->catalog_path) {
			g_free (priv->catalog_path);
			priv->catalog_path = NULL;
		}
		
		if (priv->image_path) {
			g_free (priv->image_path);
			priv->image_path = NULL;
		}
		
#ifdef HAVE_LIBEXIF
		if (priv->exif_data != NULL) {
			exif_data_unref (priv->exif_data);
			priv->exif_data = NULL;
		}
#endif /* HAVE_LIBEXIF */
		
		if (priv->new_image_path) {
			g_free (priv->new_image_path);
			priv->new_image_path = NULL;
		}
		
		if (priv->image_catalog) {
			g_free (priv->image_catalog);	
			priv->image_catalog = NULL;
		}
		
		if (priv->history) {
			bookmarks_free (priv->history);
			priv->history = NULL;
		}
		
		if (priv->preloader) {
			g_object_unref (priv->preloader);
			priv->preloader = NULL;
		}
		
		for (i = 0; i < MONITOR_EVENT_NUM; i++)
			path_list_free (priv->monitor_events[i]);
		
		g_free (browser->priv);
		browser->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gth_browser_init (GthBrowser *browser)
{
	GthBrowserPrivateData *priv;

	priv = browser->priv = g_new0 (GthBrowserPrivateData, 1);
	priv->first_time_show = TRUE;
}


static void
gth_browser_construct (GthBrowser  *browser,
		       const gchar *uri)
{
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget             *paned1;      /* Main paned widget. */
	GtkWidget             *paned2;      /* Secondary paned widget. */
	GtkWidget             *table;
	GtkWidget             *frame;
	GtkWidget             *image_vbox;
	GtkWidget             *dir_list_vbox;
	GtkWidget             *info_frame;
	GtkWidget             *info_hbox;
	GtkWidget             *image_pane_paned1;
	GtkWidget             *image_pane_paned2;
	GtkWidget             *scrolled_win; 
	GtkTreeSelection      *selection;
	int                    i; 
	GtkActionGroup        *actions;
	GtkUIManager          *ui;
	GError                *error = NULL;
	GtkWidget             *toolbar;

	gtk_window_set_default_size (GTK_WINDOW (browser), 
				     eel_gconf_get_integer (PREF_UI_WINDOW_WIDTH, DEF_WIN_WIDTH),
				     eel_gconf_get_integer (PREF_UI_WINDOW_HEIGHT, DEF_WIN_HEIGHT));

	priv->tooltips = gtk_tooltips_new ();

	/* Build the menu and the toolbar. */

	priv->actions = actions = gtk_action_group_new ("Actions");
	gtk_action_group_set_translation_domain (actions, NULL);

	gtk_action_group_add_actions (actions, 
				      gth_window_action_entries, 
				      gth_window_action_entries_size, 
				      browser);
	gtk_action_group_add_toggle_actions (actions, 
					     gth_window_action_toggle_entries, 
					     gth_window_action_toggle_entries_size, 
					     browser);
	gtk_action_group_add_radio_actions (actions, 
					    gth_window_zoom_quality_entries, 
					    gth_window_zoom_quality_entries_size,
					    GTH_ZOOM_QUALITY_HIGH,
					    G_CALLBACK (zoom_quality_radio_action), 
					    browser);

	gtk_action_group_add_actions (actions, 
				      gth_browser_action_entries, 
				      gth_browser_action_entries_size, 
				      browser);
	gtk_action_group_add_toggle_actions (actions, 
					     gth_browser_action_toggle_entries, 
					     gth_browser_action_toggle_entries_size, 
					     browser);
	gtk_action_group_add_radio_actions (actions, 
					    gth_browser_sort_by_entries, 
					    gth_browser_sort_by_entries_size,
					    GTH_SORT_METHOD_BY_NAME,
					    G_CALLBACK (sort_by_radio_action), 
					    browser);
	gtk_action_group_add_radio_actions (actions, 
					    gth_browser_content_entries, 
					    gth_browser_content_entries_size,
					    GTH_SIDEBAR_DIR_LIST,
					    G_CALLBACK (content_radio_action), 
					    browser);
	priv->ui = ui = gtk_ui_manager_new ();

	g_signal_connect (ui, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), browser);
	g_signal_connect (ui, "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb), browser);

	gtk_ui_manager_insert_action_group (ui, actions, 0);
	gtk_window_add_accel_group (GTK_WINDOW (browser), 
				    gtk_ui_manager_get_accel_group (ui));
	
	if (!gtk_ui_manager_add_ui_from_string (ui, main_ui_info, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	gnome_app_add_docked (GNOME_APP (browser),
			      gtk_ui_manager_get_widget (ui, "/MenuBar"),
			      "MenuBar",
			      (BONOBO_DOCK_ITEM_BEH_NEVER_VERTICAL 
			       | BONOBO_DOCK_ITEM_BEH_EXCLUSIVE 
			       | (eel_gconf_get_boolean (PREF_DESKTOP_MENUBAR_DETACHABLE, TRUE) ? BONOBO_DOCK_ITEM_BEH_NORMAL : BONOBO_DOCK_ITEM_BEH_LOCKED)),
			      BONOBO_DOCK_TOP,
			      1, 1, 0);

	priv->toolbar = toolbar = gtk_ui_manager_get_widget (ui, "/ToolBar");
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), TRUE);

	set_action_important (browser, "/ToolBar/View_ShowFolders", TRUE);
	set_action_important (browser, "/ToolBar/View_ShowCatalogs", TRUE);
	set_action_important (browser, "/ToolBar/View_ShowImage", TRUE);
	/* FIXME
	set_action_important (browser, "/ToolBar/ModeCommands/Tools_Slideshow", TRUE);
	set_action_important (browser, "/ToolBar/ModeCommands/View_Fullscreen", TRUE);
	*/

	gnome_app_add_docked (GNOME_APP (browser),
			      toolbar,
			      "ToolBar",
			      (BONOBO_DOCK_ITEM_BEH_NEVER_VERTICAL 
			       | BONOBO_DOCK_ITEM_BEH_EXCLUSIVE 
			       | (eel_gconf_get_boolean (PREF_DESKTOP_TOOLBAR_DETACHABLE, TRUE) ? BONOBO_DOCK_ITEM_BEH_NORMAL : BONOBO_DOCK_ITEM_BEH_LOCKED)),
			      BONOBO_DOCK_TOP,
			      2, 1, 0);

	priv->statusbar = gtk_statusbar_new ();
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (priv->statusbar), TRUE);
	priv->help_message_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "gth_help_message");
	priv->list_info_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "gth_list_info");
	gnome_app_set_statusbar (GNOME_APP (browser), priv->statusbar);

	g_signal_connect (G_OBJECT (browser), 
			  "delete_event",
			  G_CALLBACK (close_window_cb),
			  browser);
	g_signal_connect (G_OBJECT (browser), 
			  "key_press_event",
			  G_CALLBACK (key_press_cb), 
			  browser);

	/* Popup menus */

	priv->file_popup_menu = gtk_ui_manager_get_widget (ui, "/FilePopup");
	priv->image_popup_menu = gtk_ui_manager_get_widget (ui, "/ImagePopup");
	priv->fullscreen_image_popup_menu = gtk_ui_manager_get_widget (ui, "/FullscreenImagePopup");
	priv->catalog_popup_menu = gtk_ui_manager_get_widget (ui, "/CatalogPopup");
	priv->library_popup_menu = gtk_ui_manager_get_widget (ui, "/LibraryPopup");
	priv->dir_popup_menu = gtk_ui_manager_get_widget (ui, "/DirPopup");
	priv->dir_list_popup_menu = gtk_ui_manager_get_widget (ui, "/DirListPopup");
	priv->catalog_list_popup_menu = gtk_ui_manager_get_widget (ui, "/CatalogListPopup");
	priv->history_list_popup_menu = gtk_ui_manager_get_widget (ui, "/HistoryListPopup");

	/* Create the widgets. */

	priv->file_list = create_new_file_list (browser);

	/* Dir list. */

	priv->dir_list = dir_list_new ();
	gtk_drag_dest_set (priv->dir_list->root_widget,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, G_N_ELEMENTS (target_table),
			   GDK_ACTION_MOVE);
	gtk_drag_source_set (priv->dir_list->list_view,
			     GDK_BUTTON1_MASK,
			     target_table, G_N_ELEMENTS (target_table),
			     GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_signal_connect (G_OBJECT (priv->dir_list->root_widget),
			  "drag_data_received",
			  G_CALLBACK (dir_list_drag_data_received), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->list_view), 
			  "drag_begin",
			  G_CALLBACK (dir_list_drag_begin), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->list_view), 
			  "drag_end",
			  G_CALLBACK (dir_list_drag_end), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->root_widget), 
			  "drag_motion",
			  G_CALLBACK (dir_list_drag_motion), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->root_widget), 
			  "drag_leave",
			  G_CALLBACK (dir_list_drag_leave), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->list_view),
			  "drag_data_get",
			  G_CALLBACK (dir_list_drag_data_get), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->list_view), 
			  "button_press_event",
			  G_CALLBACK (dir_list_button_press_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->list_view), 
			  "button_release_event",
			  G_CALLBACK (dir_list_button_release_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->dir_list->list_view),
                          "row_activated",
                          G_CALLBACK (dir_list_activated_cb),
                          browser);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->dir_list->list_view));
	g_signal_connect (G_OBJECT (selection),
                          "changed",
                          G_CALLBACK (dir_or_catalog_sel_changed_cb),
                          browser);

	/* Catalog list. */

	priv->catalog_list = catalog_list_new (pref_get_real_click_policy () == GTH_CLICK_POLICY_SINGLE);
	gtk_drag_dest_set (priv->catalog_list->root_widget,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, G_N_ELEMENTS(target_table),
			   GDK_ACTION_MOVE);
	g_signal_connect (G_OBJECT (priv->catalog_list->root_widget),
			  "drag_data_received",
			  G_CALLBACK (catalog_list_drag_data_received), 
			  browser);
	g_signal_connect (G_OBJECT (priv->catalog_list->list_view), 
			  "drag_begin",
			  G_CALLBACK (catalog_list_drag_begin), 
			  browser);
	g_signal_connect (G_OBJECT (priv->catalog_list->root_widget), 
			  "drag_motion",
			  G_CALLBACK (catalog_list_drag_motion), 
			  browser);
	g_signal_connect (G_OBJECT (priv->catalog_list->root_widget), 
			  "drag_leave",
			  G_CALLBACK (catalog_list_drag_leave), 
			  browser);
	g_signal_connect (G_OBJECT (priv->catalog_list->list_view), 
			  "button_press_event",
			  G_CALLBACK (catalog_list_button_press_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->catalog_list->list_view), 
			  "button_release_event",
			  G_CALLBACK (catalog_list_button_release_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->catalog_list->list_view),
                          "row_activated",
                          G_CALLBACK (catalog_list_activated_cb),
                          browser);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->catalog_list->list_view));
	g_signal_connect (G_OBJECT (selection),
                          "changed",
                          G_CALLBACK (dir_or_catalog_sel_changed_cb),
                          browser);

	/* Location entry. */

	priv->location_entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (priv->location_entry),
			  "key_press_event",
			  G_CALLBACK (location_entry_key_press_cb),
			  browser);

	/* Info bar. */

	priv->info_bar = gthumb_info_bar_new ();
	gthumb_info_bar_set_focused (GTHUMB_INFO_BAR (priv->info_bar), FALSE);
	g_signal_connect (G_OBJECT (priv->info_bar), 
			  "button_press_event",
			  G_CALLBACK (info_bar_clicked_cb), 
			  browser);

	/* Image viewer. */
	
	priv->viewer = image_viewer_new ();
	gtk_widget_set_size_request (priv->viewer, 
				     PANE_MIN_SIZE,
				     PANE_MIN_SIZE);

	/* FIXME 
	gtk_drag_source_set (priv->viewer,
			     GDK_BUTTON2_MASK,
			     target_table, G_N_ELEMENTS(target_table), 
			     GDK_ACTION_MOVE);
	*/

	gtk_drag_dest_set (priv->viewer,
			   GTK_DEST_DEFAULT_ALL,
			   target_table, G_N_ELEMENTS(target_table),
			   GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (priv->viewer), 
			  "image_loaded",
			  G_CALLBACK (image_loaded_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->viewer), 
			  "zoom_changed",
			  G_CALLBACK (zoom_changed_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->viewer), 
			  "size_changed",
			  G_CALLBACK (size_changed_cb), 
			  browser);
	g_signal_connect_after (G_OBJECT (priv->viewer), 
				"button_press_event",
				G_CALLBACK (image_button_press_cb), 
				browser);
	g_signal_connect_after (G_OBJECT (priv->viewer), 
				"button_release_event",
				G_CALLBACK (image_button_release_cb), 
				browser);
	g_signal_connect_after (G_OBJECT (priv->viewer), 
				"clicked",
				G_CALLBACK (image_clicked_cb), 
				browser);
	g_signal_connect (G_OBJECT (priv->viewer), 
			  "focus_in_event",
			  G_CALLBACK (image_focus_changed_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->viewer), 
			  "focus_out_event",
			  G_CALLBACK (image_focus_changed_cb), 
			  browser);

	g_signal_connect (G_OBJECT (priv->viewer),
			  "drag_data_get",
			  G_CALLBACK (viewer_drag_data_get), 
			  browser);

	g_signal_connect (G_OBJECT (priv->viewer), 
			  "drag_data_received",
			  G_CALLBACK (viewer_drag_data_received), 
			  browser);

	g_signal_connect (G_OBJECT (priv->viewer),
			  "key_press_event",
			  G_CALLBACK (viewer_key_press_cb),
			  browser);

	g_signal_connect (G_OBJECT (IMAGE_VIEWER (priv->viewer)->loader), 
			  "image_progress",
			  G_CALLBACK (image_loader_progress_cb), 
			  browser);
	g_signal_connect (G_OBJECT (IMAGE_VIEWER (priv->viewer)->loader), 
			  "image_done",
			  G_CALLBACK (image_loader_done_cb), 
			  browser);
	g_signal_connect (G_OBJECT (IMAGE_VIEWER (priv->viewer)->loader), 
			  "image_error",
			  G_CALLBACK (image_loader_done_cb), 
			  browser);

	priv->viewer_vscr = gtk_vscrollbar_new (IMAGE_VIEWER (priv->viewer)->vadj);
	priv->viewer_hscr = gtk_hscrollbar_new (IMAGE_VIEWER (priv->viewer)->hadj);
	priv->viewer_event_box = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (priv->viewer_event_box), _gtk_image_new_from_xpm_data (nav_button_xpm));

	g_signal_connect (G_OBJECT (priv->viewer_event_box), 
			  "button_press_event",
			  G_CALLBACK (nav_button_clicked_cb), 
			  priv->viewer);

	/* Pack the widgets */

	priv->file_list_pane = gtk_vbox_new (0, FALSE);
	gtk_widget_show_all (priv->file_list->root_widget);
	gtk_box_pack_start (GTK_BOX (priv->file_list_pane), priv->file_list->root_widget, TRUE, TRUE, 0);

	priv->layout_type = eel_gconf_get_integer (PREF_UI_LAYOUT, 2);

	if (priv->layout_type == 1) {
		priv->main_pane = paned1 = gtk_vpaned_new (); 
		priv->content_pane = paned2 = gtk_hpaned_new ();
	} else {
		priv->main_pane = paned1 = gtk_hpaned_new (); 
		priv->content_pane = paned2 = gtk_vpaned_new (); 
	}

	gnome_app_set_contents (GNOME_APP (browser), 
				priv->main_pane);

	if (priv->layout_type == 3)
		gtk_paned_pack2 (GTK_PANED (paned1), paned2, TRUE, FALSE);
	else
		gtk_paned_pack1 (GTK_PANED (paned1), paned2, FALSE, FALSE);

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), 
				  priv->dir_list->root_widget,
				  NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), 
				  priv->catalog_list->root_widget,
				  NULL);

	priv->dir_list_pane = dir_list_vbox = gtk_vbox_new (FALSE, 3);

	gtk_box_pack_start (GTK_BOX (dir_list_vbox), priv->location_entry, 
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (dir_list_vbox), priv->notebook, 
			    TRUE, TRUE, 0);

	if (priv->layout_type == 3) 
		gtk_paned_pack1 (GTK_PANED (paned1), dir_list_vbox, FALSE, FALSE);
	else 
		gtk_paned_pack1 (GTK_PANED (paned2), dir_list_vbox, FALSE, FALSE);

	if (priv->layout_type <= 1) 
		gtk_paned_pack2 (GTK_PANED (paned2), priv->file_list_pane, TRUE, FALSE);
	else if (priv->layout_type == 2)
		gtk_paned_pack2 (GTK_PANED (paned1), priv->file_list_pane, TRUE, FALSE);
	else if (priv->layout_type == 3)
		gtk_paned_pack1 (GTK_PANED (paned2), priv->file_list_pane, FALSE, FALSE);

	/**/

	image_vbox = priv->image_pane = gtk_vbox_new (FALSE, 0);

	if (priv->layout_type <= 1)
		gtk_paned_pack2 (GTK_PANED (paned1), image_vbox, TRUE, FALSE);
	else
		gtk_paned_pack2 (GTK_PANED (paned2), image_vbox, TRUE, FALSE);

	/* image info bar */

	info_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (info_frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (image_vbox), info_frame, FALSE, FALSE, 0);

	info_hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (info_frame), info_hbox);

	gtk_box_pack_start (GTK_BOX (info_hbox), priv->info_bar, TRUE, TRUE, 0);

	/* FIXME */
	{
		GtkWidget *button;
		GtkWidget *image;

		button = gtk_button_new ();
		image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
		gtk_container_add (GTK_CONTAINER (button), image);
		gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
		gtk_box_pack_end (GTK_BOX (info_hbox), button, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (button), 
				  "clicked",
				  G_CALLBACK (close_preview_image_button_cb), 
				  browser);
		gtk_tooltips_set_tip (priv->tooltips,
				      button,
				      _("Close"),
				      NULL);

		image = _gtk_image_new_from_inline (preview_comment_16_rgba);
		priv->preview_button_comment = button = gtk_toggle_button_new ();
		gtk_container_add (GTK_CONTAINER (button), image);
		gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
		gtk_box_pack_end (GTK_BOX (info_hbox), button, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (button), 
				  "toggled",
				  G_CALLBACK (preview_comment_button_cb), 
				  browser);
		gtk_tooltips_set_tip (priv->tooltips,
				      button,
				      _("Image comment"),
				      NULL);

		image = _gtk_image_new_from_inline (preview_data_16_rgba);
		priv->preview_button_data = button = gtk_toggle_button_new ();
		gtk_container_add (GTK_CONTAINER (button), image);
		gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
		gtk_box_pack_end (GTK_BOX (info_hbox), button, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (button), 
				  "toggled",
				  G_CALLBACK (preview_data_button_cb), 
				  browser);
		gtk_tooltips_set_tip (priv->tooltips,
				      button,
				      _("Image data"),
				      NULL);

		image = _gtk_image_new_from_inline (preview_image_16_rgba);
		priv->preview_button_image = button = gtk_toggle_button_new ();
		gtk_container_add (GTK_CONTAINER (button), image);
		gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
		gtk_box_pack_end (GTK_BOX (info_hbox), button, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (button), 
				  "toggled",
				  G_CALLBACK (preview_image_button_cb), 
				  browser);
		gtk_tooltips_set_tip (priv->tooltips,
				      button,
				      _("Image preview"),
				      NULL);
	}

	/* image preview, comment, exif data. */

	image_pane_paned1 = gtk_vpaned_new ();
	gtk_paned_set_position (GTK_PANED (image_pane_paned1), eel_gconf_get_integer (PREF_UI_WINDOW_HEIGHT, DEF_WIN_HEIGHT) / 2);
	gtk_box_pack_start (GTK_BOX (image_vbox), image_pane_paned1, TRUE, TRUE, 0);

	/**/

	priv->viewer_container = frame = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), priv->viewer);

	priv->preview_widget_image = table = gtk_table_new (2, 2, FALSE);
	gtk_paned_pack1 (GTK_PANED (image_pane_paned1), table, FALSE, FALSE);

	gtk_table_attach (GTK_TABLE (table), frame, 0, 1, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->viewer_vscr, 1, 2, 0, 1,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->viewer_hscr, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_table_attach (GTK_TABLE (table), priv->viewer_event_box, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);

	/**/

	priv->preview_widget_data_comment = image_pane_paned2 = gtk_hpaned_new ();
	gtk_paned_set_position (GTK_PANED (image_pane_paned2), eel_gconf_get_integer (PREF_UI_WINDOW_WIDTH, DEF_WIN_WIDTH) / 2);
	gtk_paned_pack2 (GTK_PANED (image_pane_paned1), image_pane_paned2, TRUE, FALSE);

	priv->preview_widget_comment = scrolled_win = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win), GTK_SHADOW_ETCHED_IN);

	priv->image_comment = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->image_comment), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->image_comment), GTK_WRAP_WORD);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (priv->image_comment), TRUE);
	gtk_container_add (GTK_CONTAINER (scrolled_win), priv->image_comment);

	gtk_paned_pack2 (GTK_PANED (image_pane_paned2), scrolled_win, TRUE, FALSE);

	g_signal_connect (G_OBJECT (priv->image_comment), 
			  "focus_in_event",
			  G_CALLBACK (image_focus_changed_cb), 
			  browser);
	g_signal_connect (G_OBJECT (priv->image_comment), 
			  "focus_out_event",
			  G_CALLBACK (image_focus_changed_cb), 
			  browser);

	g_signal_connect (G_OBJECT (priv->image_comment), 
			  "button_press_event",
			  G_CALLBACK (image_comment_button_press_cb), 
			  browser);

	/* exif data */

	priv->preview_widget_data = priv->exif_data_viewer = gth_exif_data_viewer_new (TRUE);
	gtk_paned_pack1 (GTK_PANED (image_pane_paned2), priv->exif_data_viewer, FALSE, FALSE);

	g_signal_connect (G_OBJECT (gth_exif_data_viewer_get_view (GTH_EXIF_DATA_VIEWER (priv->exif_data_viewer))), 
			  "focus_in_event",
			  G_CALLBACK (image_focus_changed_cb), 
			  browser);
	g_signal_connect (G_OBJECT (gth_exif_data_viewer_get_view (GTH_EXIF_DATA_VIEWER (priv->exif_data_viewer))), 
			  "focus_out_event",
			  G_CALLBACK (image_focus_changed_cb), 
			  browser);

	/* Progress bar. */

	priv->progress = gtk_progress_bar_new ();
	gtk_widget_show (priv->progress);
	gtk_box_pack_start (GTK_BOX (priv->statusbar), priv->progress, FALSE, FALSE, 0);

	/* Zoom info */

	priv->zoom_info = gtk_label_new (NULL);
	gtk_widget_show (priv->zoom_info);

	priv->zoom_info_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->zoom_info_frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (priv->zoom_info_frame), priv->zoom_info);
	gtk_box_pack_start (GTK_BOX (priv->statusbar), priv->zoom_info_frame, FALSE, FALSE, 0);
	
	/* Image info */

	priv->image_info = gtk_label_new (NULL);
	gtk_widget_show (priv->image_info);

	priv->image_info_frame = info_frame = gtk_frame_new (NULL);
	gtk_widget_show (priv->image_info_frame);

	gtk_frame_set_shadow_type (GTK_FRAME (info_frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (info_frame), priv->image_info);
	gtk_box_pack_start (GTK_BOX (priv->statusbar), info_frame, FALSE, FALSE, 0);

	/* Progress dialog */

	priv->progress_gui = glade_xml_new (GTHUMB_GLADEDIR "/" GLADE_EXPORTER_FILE, NULL, NULL);
	if (! priv->progress_gui) {
		priv->progress_dialog = NULL;
		priv->progress_progressbar = NULL;
		priv->progress_info = NULL;
	} else {
		GtkWidget *cancel_button;

		priv->progress_dialog = glade_xml_get_widget (priv->progress_gui, "progress_dialog");
		priv->progress_progressbar = glade_xml_get_widget (priv->progress_gui, "progress_progressbar");
		priv->progress_info = glade_xml_get_widget (priv->progress_gui, "progress_info");
		cancel_button = glade_xml_get_widget (priv->progress_gui, "progress_cancel");

		gtk_window_set_transient_for (GTK_WINDOW (priv->progress_dialog), GTK_WINDOW (browser));
		gtk_window_set_modal (GTK_WINDOW (priv->progress_dialog), FALSE);

		g_signal_connect (G_OBJECT (cancel_button), 
				  "clicked",
				  G_CALLBACK (progress_cancel_cb), 
				  browser);
		g_signal_connect (G_OBJECT (priv->progress_dialog), 
				  "delete_event",
				  G_CALLBACK (progress_delete_cb),
				  browser);
	}

	/* Update data. */

	priv->sidebar_content = GTH_SIDEBAR_NO_LIST;
	priv->sidebar_visible = TRUE;
	priv->preview_visible = eel_gconf_get_boolean (PREF_SHOW_PREVIEW, TRUE);
	priv->image_pane_visible = priv->preview_visible;
	priv->image_data_visible = eel_gconf_get_boolean (PREF_SHOW_IMAGE_DATA, FALSE);
	priv->catalog_path = NULL;
	priv->image_path = NULL;
	priv->image_mtime = 0;
	priv->image_catalog = NULL;
	priv->image_modified = FALSE;

	priv->fullscreen = FALSE;
	priv->slideshow = FALSE;
	priv->slideshow_set = NULL;
	priv->slideshow_random_set = NULL;
	priv->slideshow_first = NULL;
	priv->slideshow_current = NULL;
	priv->slideshow_timeout = 0;

	priv->bookmarks_length = 0;
	gth_browser_update_bookmark_list (browser);

	priv->history = bookmarks_new (NULL);
	priv->history_current = NULL;
	priv->history_length = 0;
	priv->go_op = GTH_BROWSER_GO_TO;
	window_update_history_list (browser);

	priv->activity_timeout = 0;
	priv->activity_ref = 0;
	priv->setting_file_list = FALSE;
	priv->can_set_file_list = TRUE;
	priv->changing_directory = FALSE;
	priv->can_change_directory = TRUE;

	priv->monitor_handle = NULL;
	priv->monitor_enabled = FALSE;
	priv->update_changes_timeout = 0;
	for (i = 0; i < MONITOR_EVENT_NUM; i++)
		priv->monitor_events[i] = NULL;

	priv->image_prop_dlg = NULL;

	priv->busy_cursor_timeout = 0;

	priv->view_image_timeout = 0;
	priv->load_dir_timeout = 0;
	priv->auto_load_timeout = 0;

	priv->sel_change_timeout = 0;

	/* preloader */

	priv->preloader = gthumb_preloader_new ();

	g_signal_connect (G_OBJECT (priv->preloader), 
			  "requested_done",
			  G_CALLBACK (image_requested_done_cb), 
			  browser);

	g_signal_connect (G_OBJECT (priv->preloader), 
			  "requested_error",
			  G_CALLBACK (image_requested_error_cb), 
			  browser);

	for (i = 0; i < GCONF_NOTIFICATIONS; i++)
		priv->cnxn_id[i] = 0;

	priv->pixop = NULL;

	/* Sync widgets and visualization options. */

	gtk_widget_realize (GTK_WIDGET (browser));

	/**/

#ifndef HAVE_LIBGPHOTO
	set_action_sensitive (browser, "File_CameraImport", FALSE);
#endif /* HAVE_LIBGPHOTO */

	/**/

	priv->sidebar_width = eel_gconf_get_integer (PREF_UI_SIDEBAR_SIZE, DEF_SIDEBAR_SIZE);
	gtk_paned_set_position (GTK_PANED (paned1), eel_gconf_get_integer (PREF_UI_SIDEBAR_SIZE, DEF_SIDEBAR_SIZE));
	gtk_paned_set_position (GTK_PANED (paned2), eel_gconf_get_integer (PREF_UI_SIDEBAR_CONTENT_SIZE, DEF_SIDEBAR_CONT_SIZE));

	gtk_widget_show_all (priv->main_pane);

	window_sync_menu_with_preferences (browser); 

	if (priv->preview_visible) 
		gth_browser_show_image_pane (browser);
	else 
		gth_browser_hide_image_pane (browser);

	gth_browser_set_preview_content (browser, pref_get_preview_content ());

	gth_browser_notify_update_toolbar_style (browser);
	window_update_statusbar_image_info (browser);
	window_update_statusbar_zoom_info (browser);

	image_viewer_set_zoom_quality (IMAGE_VIEWER (priv->viewer),
				       pref_get_zoom_quality ());
	image_viewer_set_zoom_change  (IMAGE_VIEWER (priv->viewer),
				       pref_get_zoom_change ());
	image_viewer_set_check_type   (IMAGE_VIEWER (priv->viewer),
				       pref_get_check_type ());
	image_viewer_set_check_size   (IMAGE_VIEWER (priv->viewer),
				       pref_get_check_size ());
	image_viewer_set_transp_type  (IMAGE_VIEWER (priv->viewer),
				       pref_get_transp_type ());
	image_viewer_set_black_background (IMAGE_VIEWER (priv->viewer),
					   eel_gconf_get_boolean (PREF_BLACK_BACKGROUND, FALSE));

	/* Add notification callbacks. */

	i = 0;

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_UI_LAYOUT,
					   pref_ui_layout_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_UI_TOOLBAR_STYLE,
					   pref_ui_toolbar_style_changed,
					   browser);
	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   "/desktop/gnome/interface/toolbar_style",
					   pref_ui_toolbar_style_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_UI_TOOLBAR_VISIBLE,
					   pref_ui_toolbar_visible_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_UI_STATUSBAR_VISIBLE,
					   pref_ui_statusbar_visible_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_SHOW_THUMBNAILS,
					   pref_show_thumbnails_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_SHOW_FILENAMES,
					   pref_show_filenames_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_SHOW_COMMENTS,
					   pref_show_comments_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_SHOW_HIDDEN_FILES,
					   pref_show_hidden_files_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_THUMBNAIL_SIZE,
					   pref_thumbnail_size_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_CLICK_POLICY,
					   pref_click_policy_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_ZOOM_QUALITY,
					   pref_zoom_quality_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_ZOOM_CHANGE,
					   pref_zoom_change_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_TRANSP_TYPE,
					   pref_transp_type_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_CHECK_TYPE,
					   pref_check_type_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_CHECK_SIZE,
					   pref_check_size_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_BLACK_BACKGROUND,
					   pref_black_background_changed,
					   browser);

	priv->cnxn_id[i++] = eel_gconf_notification_add (
					   PREF_VIEW_AS,
					   pref_view_as_changed,
					   browser);

	set_mode_specific_ui_info (browser, GTH_SIDEBAR_DIR_LIST, TRUE);

	/*
	priv->toolbar_merge_id = gtk_ui_manager_add_ui_from_string (priv->ui, browser_ui_info, -1, NULL);
	gtk_ui_manager_ensure_update (priv->ui);	
	*/

	/* Initial location. */

	if (uri != NULL)
		priv->initial_location = g_strdup (uri);
	else 
		priv->initial_location = g_strdup (preferences_get_startup_location ());
		
	g_idle_add (initial_location_cb, browser);
}


GtkWidget * 
gth_browser_new (const gchar *uri)
{
	GthBrowser *browser;

	browser = (GthBrowser*) g_object_new (GTH_TYPE_BROWSER, NULL);
	gth_browser_construct (browser, uri);

	return (GtkWidget*) browser;
}


/* -- gth_browserclose -- */


static void
_window_remove_notifications (GthBrowser *browser)
{
	int i;

	for (i = 0; i < GCONF_NOTIFICATIONS; i++)
		if (browser->priv->cnxn_id[i] != 0)
			eel_gconf_notification_remove (browser->priv->cnxn_id[i]);
}


static void
close__step6 (char     *filename,
	      gpointer  data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	ImageViewer           *viewer = IMAGE_VIEWER (priv->viewer);
	gboolean               last_window;
	GdkWindowState         state;
	gboolean               maximized;

	last_window = gth_window_get_n_windows () == 1;

	/* Save visualization options only if the window is not maximized. */
	state = gdk_window_get_state (GTK_WIDGET (browser)->window);
	maximized = (state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
	if (! maximized && GTK_WIDGET_VISIBLE (browser)) {
		int width, height;

		gdk_drawable_get_size (GTK_WIDGET (browser)->window, &width, &height);
		eel_gconf_set_integer (PREF_UI_WINDOW_WIDTH, width);
		eel_gconf_set_integer (PREF_UI_WINDOW_HEIGHT, height);
	}

	if (priv->sidebar_visible) {
		eel_gconf_set_integer (PREF_UI_SIDEBAR_SIZE, gtk_paned_get_position (GTK_PANED (priv->main_pane)));
		eel_gconf_set_integer (PREF_UI_SIDEBAR_CONTENT_SIZE, gtk_paned_get_position (GTK_PANED (priv->content_pane)));
	} else
		eel_gconf_set_integer (PREF_UI_SIDEBAR_SIZE, priv->sidebar_width);

	if (last_window)
		eel_gconf_set_boolean (PREF_SHOW_THUMBNAILS, priv->file_list->enable_thumbs);

	pref_set_arrange_type (priv->file_list->sort_method);
	pref_set_sort_order (priv->file_list->sort_type);

	if (last_window 
	    && eel_gconf_get_boolean (PREF_GO_TO_LAST_LOCATION, TRUE)) {
		char *location = NULL;

		if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST) 
			location = g_strconcat ("file://",
						priv->dir_list->path,
						NULL);
		else if ((priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) 
			 && (priv->catalog_path != NULL))
			location = g_strconcat ("catalog://",
						priv->catalog_path,
						NULL);

		if (location != NULL) {
			eel_gconf_set_path (PREF_STARTUP_LOCATION, location);
			g_free (location);
		}
	}

	if (last_window) {
		pref_set_zoom_quality (image_viewer_get_zoom_quality (viewer));
		pref_set_transp_type (image_viewer_get_transp_type (viewer));
	}
	pref_set_check_type (image_viewer_get_check_type (viewer));
	pref_set_check_size (image_viewer_get_check_size (viewer));

	eel_gconf_set_boolean (PREF_UI_IMAGE_PANE_VISIBLE, priv->image_pane_visible);
	eel_gconf_set_boolean (PREF_SHOW_PREVIEW, priv->preview_visible);
	eel_gconf_set_boolean (PREF_SHOW_IMAGE_DATA, priv->image_data_visible);
	pref_set_preview_content (priv->preview_content);

	/* Destroy the main window. */

	if (priv->progress_timeout != 0) {
		g_source_remove (priv->progress_timeout);
		priv->progress_timeout = 0;
	}

	if (priv->activity_timeout != 0) {
		g_source_remove (priv->activity_timeout);
		priv->activity_timeout = 0;
	}

	if (priv->load_dir_timeout != 0) {
		g_source_remove (priv->load_dir_timeout);
		priv->load_dir_timeout = 0;
	}

	if (priv->sel_change_timeout != 0) {
		g_source_remove (priv->sel_change_timeout);
		priv->sel_change_timeout = 0;
	}

	if (priv->busy_cursor_timeout != 0) {
		g_source_remove (priv->busy_cursor_timeout);
		priv->busy_cursor_timeout = 0;
	}

	if (priv->view_image_timeout != 0) {
 		g_source_remove (priv->view_image_timeout);
		priv->view_image_timeout = 0;
	}

	if (priv->update_changes_timeout != 0) {
		g_source_remove (priv->update_changes_timeout);
		priv->update_changes_timeout = 0;
	}

	if (priv->auto_load_timeout != 0) {
		g_source_remove (priv->auto_load_timeout);
		priv->auto_load_timeout = 0;
	}

	if (priv->pixop != NULL) 
		g_object_unref (priv->pixop);

	if (priv->progress_gui != NULL)
		g_object_unref (priv->progress_gui);

	gtk_widget_destroy (priv->file_popup_menu);
	gtk_widget_destroy (priv->image_popup_menu);
	gtk_widget_destroy (priv->fullscreen_image_popup_menu);
	gtk_widget_destroy (priv->catalog_popup_menu);
	gtk_widget_destroy (priv->library_popup_menu);
	gtk_widget_destroy (priv->dir_popup_menu);
	gtk_widget_destroy (priv->dir_list_popup_menu);
	gtk_widget_destroy (priv->catalog_list_popup_menu);
	gtk_widget_destroy (priv->history_list_popup_menu);

	if (gth_file_list_drag_data != NULL) {
		path_list_free (gth_file_list_drag_data);
		gth_file_list_drag_data = NULL;
	}

	if (dir_list_drag_data != NULL) {
		g_free (dir_list_drag_data);
		dir_list_drag_data = NULL;
	}

	if (priv->slideshow_set != NULL)
		g_list_free (priv->slideshow_set);
	if (priv->slideshow_random_set != NULL)
		g_list_free (priv->slideshow_random_set);

	g_free (priv->initial_location);

	gtk_object_destroy (GTK_OBJECT (priv->tooltips));

	if (last_window) { /* FIXME */
		if (dir_list_tree_path != NULL) {
			gtk_tree_path_free (dir_list_tree_path);
			dir_list_tree_path = NULL;
		}

		if (catalog_list_tree_path != NULL) {
			gtk_tree_path_free (catalog_list_tree_path);
			catalog_list_tree_path = NULL;
		}

	} 

	gtk_widget_destroy (GTK_WIDGET (browser));
}


static void
close__step5 (GthBrowser *browser)
{
	if (browser->priv->image_modified) 
		if (ask_whether_to_save (browser, close__step6))
			return;
	close__step6 (NULL, browser);
}


static void
close__step4 (GthBrowser *browser)
{
	if (browser->priv->slideshow)
		gth_browser_stop_slideshow (browser);

	if (browser->priv->preloader != NULL)
		gthumb_preloader_stop (browser->priv->preloader, 
				       (DoneFunc) close__step5, 
				       browser);
	else
		close__step5 (browser);
}


static void
close__step3 (GthBrowser *browser)
{
	browser->priv->setting_file_list = FALSE;
	if (browser->priv->file_list->doing_thumbs)
		gth_file_list_interrupt_thumbs (browser->priv->file_list, 
						(DoneFunc) close__step4, 
						browser);
	else
		close__step4 (browser);
}


static void
close__step2 (GthBrowser *browser)
{
	browser->priv->changing_directory = FALSE;
	if (browser->priv->setting_file_list) 
		gth_file_list_interrupt_set_list (browser->priv->file_list,
						  (DoneFunc) close__step3,
						  browser);
	else
		close__step3 (browser);
}


static void
gth_browser_close (GthWindow *window)
{
	GthBrowser            *browser = (GthBrowser*) window;
	GthBrowserPrivateData *priv = browser->priv;

	/* Interrupt any activity. */

	if (priv->update_layout_timeout != 0) {
		g_source_remove (priv->update_layout_timeout);
		priv->update_layout_timeout = 0;
	}

	_window_remove_notifications (browser);
	gth_browser_stop_activity_mode (browser);
	gth_browser_remove_monitor (browser);

	if (priv->image_prop_dlg != NULL) {
		dlg_image_prop_close (priv->image_prop_dlg);
		priv->image_prop_dlg = NULL;
	}

	if (priv->comment_dlg != NULL) {
		dlg_comment_close (priv->comment_dlg);
		priv->comment_dlg = NULL;
	}

	if (priv->categories_dlg != NULL) {
		dlg_categories_close (priv->categories_dlg);
		priv->categories_dlg = NULL;
	}

	if (priv->changing_directory) 
		dir_list_interrupt_change_to (priv->dir_list, 
					      (DoneFunc) close__step2,
					      browser);
	else
		close__step2 (browser);
}


void
gth_browser_set_sidebar_content (GthBrowser *browser,
			    int         sidebar_content)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                   old_content = priv->sidebar_content;
	char                  *path;

	_window_set_sidebar (browser, sidebar_content);
	gth_browser_show_sidebar (browser);

	if (old_content == sidebar_content) 
		return;

	switch (sidebar_content) {
	case GTH_SIDEBAR_DIR_LIST: 
		if (priv->dir_list->path == NULL) 
			gth_browser_go_to_directory (browser, g_get_home_dir ());
		else 
			gth_browser_go_to_directory (browser, priv->dir_list->path);
		break;

	case GTH_SIDEBAR_CATALOG_LIST:
		if (priv->catalog_path == NULL)
			path = get_catalog_full_path (NULL);
		else 
			path = remove_level_from_path (priv->catalog_path);
		gth_browser_go_to_catalog_directory (browser, path);
		
		g_free (path);

		debug (DEBUG_INFO, "catalog path: %s", priv->catalog_path);

		if (priv->catalog_path != NULL) {
			GtkTreeIter iter;
			
			if (priv->slideshow)
				gth_browser_stop_slideshow (browser);
			
			if (! catalog_list_get_iter_from_path (priv->catalog_list, priv->catalog_path, &iter)) {
				window_image_viewer_set_void (browser);
				return;
			}
			catalog_list_select_iter (priv->catalog_list, &iter);
			catalog_activate (browser, priv->catalog_path);
		} else {
			window_set_file_list (browser, NULL, NULL, NULL);
			window_image_viewer_set_void (browser);
		}

		window_update_title (browser);
		break;

	default:
		break;
	}
}


GthSidebarContent
gth_browser_get_sidebar_content (GthBrowser *browser)
{
	g_return_val_if_fail (browser != NULL, 0);
	return browser->priv->sidebar_content;
}


void
gth_browser_hide_sidebar (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->image_path == NULL)
		return;

	priv->sidebar_visible = FALSE;
	priv->sidebar_width = gtk_paned_get_position (GTK_PANED (priv->main_pane));

	if (priv->layout_type <= 1)
		gtk_widget_hide (GTK_PANED (priv->main_pane)->child1);

	else if (priv->layout_type == 2) {
		gtk_widget_hide (GTK_PANED (priv->main_pane)->child2);
		gtk_widget_hide (GTK_PANED (priv->content_pane)->child1);

	} else if (priv->layout_type == 3) {
		gtk_widget_hide (GTK_PANED (priv->main_pane)->child1);
		gtk_widget_hide (GTK_PANED (priv->content_pane)->child1);
	}

	if (priv->image_data_visible)
		gth_browser_show_image_data (browser);
	else
		gth_browser_hide_image_data (browser);

	gtk_widget_show (priv->preview_widget_image);
	gtk_widget_grab_focus (priv->viewer);

	/* Sync menu and toolbar. */

	set_action_active_if_different (browser, "View_ShowImage", TRUE);
	set_button_active_no_notify (browser,
				     get_button_from_sidebar_content (browser),
				     FALSE);

	/**/

	if (eel_gconf_get_boolean (PREF_UI_TOOLBAR_VISIBLE, TRUE)) {
		/*
		bonobo_ui_component_set_prop (priv->ui_component, 
					      "/Toolbar",
					      "hidden", "1",
					      NULL);
		bonobo_ui_component_set_prop (priv->ui_component, 
					      "/ImageToolbar",
					      "hidden", "0",
					      NULL);
		*/
	}

	/**/

	gtk_widget_hide (priv->preview_button_image);
	gtk_widget_hide (priv->preview_button_comment);

	/**/

	if (! priv->image_pane_visible) 
		gth_browser_show_image_pane (browser);

	window_update_sensitivity (browser);
	window_update_statusbar_zoom_info (browser);
}


void
gth_browser_show_sidebar (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *cname;

	if (! priv->sidebar_visible) 
		gtk_paned_set_position (GTK_PANED (priv->main_pane), 
					priv->sidebar_width); 

	priv->sidebar_visible = TRUE;

	/**/

	if (priv->layout_type < 2)
		gtk_widget_show (GTK_PANED (priv->main_pane)->child1);

	else if (priv->layout_type == 2) {
		gtk_widget_show (GTK_PANED (priv->main_pane)->child2);
		gtk_widget_show (GTK_PANED (priv->content_pane)->child1);

	} else if (priv->layout_type == 3) {
		gtk_widget_show (GTK_PANED (priv->main_pane)->child1);
		gtk_widget_show (GTK_PANED (priv->content_pane)->child1);
	}

	/**/

	gth_browser_set_preview_content (browser, priv->preview_content);
	
	/* Sync menu and toolbar. */

	cname = get_command_name_from_sidebar_content (browser);
	if (cname != NULL)
		set_action_active_if_different (browser, cname, TRUE);
	set_button_active_no_notify (browser,
				     get_button_from_sidebar_content (browser),
				     TRUE);

	/**/

	if (eel_gconf_get_boolean (PREF_UI_TOOLBAR_VISIBLE, TRUE)) {
		/*
		bonobo_ui_component_set_prop (priv->ui_component, 
					      "/Toolbar",
					      "hidden", "0",
					      NULL);
		bonobo_ui_component_set_prop (priv->ui_component, 
					      "/ImageToolbar",
					      "hidden", "1",
					      NULL);
		*/
	}

	/**/

	gtk_widget_show (priv->preview_button_image);
	gtk_widget_show (priv->preview_button_comment);

	if (priv->preview_visible)
		gth_browser_show_image_pane (browser);
	else
		gth_browser_hide_image_pane (browser);

	/**/

	gtk_widget_grab_focus (gth_file_view_get_widget (priv->file_list->view));
	window_update_sensitivity (browser);
}


void
gth_browser_hide_image_pane (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->image_pane_visible = FALSE;
	gtk_widget_hide (priv->image_pane);

	gtk_widget_grab_focus (gth_file_view_get_widget (priv->file_list->view));

	/**/

	if (! priv->sidebar_visible)
		gth_browser_show_sidebar (browser);
	else {
		priv->preview_visible = FALSE;
		/* Sync menu and toolbar. */
		set_action_active_if_different (browser, 
						"View_ShowPreview", 
						FALSE);
	}

	window_update_statusbar_zoom_info (browser);
	window_update_sensitivity (browser);
}


void
gth_browser_show_image_pane (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->image_pane_visible = TRUE;

	if (priv->sidebar_visible) {
		priv->preview_visible = TRUE;
		/* Sync menu and toolbar. */
		set_action_active_if_different (browser, 
						"View_ShowPreview", 
						TRUE);
	}

	gtk_widget_show (priv->image_pane);
	gtk_widget_grab_focus (priv->viewer);

	window_update_statusbar_zoom_info (browser);
	window_update_sensitivity (browser);
}


/* -- gth_browser_stop_loading -- */


void
stop__step5 (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	set_action_sensitive (browser, "Go_Stop", 
			       (priv->activity_ref > 0) 
			       || priv->setting_file_list
			       || priv->changing_directory
			       || priv->file_list->doing_thumbs);
}


void
stop__step4 (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->slideshow)
		gth_browser_stop_slideshow (browser);

	gthumb_preloader_stop (priv->preloader, 
			       (DoneFunc) stop__step5, 
			       browser);

	set_action_sensitive (browser, "Go_Stop", 
			       (priv->activity_ref > 0) 
			       || priv->setting_file_list
			       || priv->changing_directory
			       || priv->file_list->doing_thumbs);
}


void
stop__step3 (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->setting_file_list = FALSE;
	if (priv->file_list->doing_thumbs)
		gth_file_list_interrupt_thumbs (priv->file_list, 
						(DoneFunc) stop__step4, 
						browser);
	else
		stop__step4 (browser);
}


void
stop__step2 (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	priv->changing_directory = FALSE;
	if (priv->setting_file_list) 
		gth_file_list_interrupt_set_list (priv->file_list,
						  (DoneFunc) stop__step3,
						  browser);
	else
		stop__step3 (browser);
}


void
gth_browser_stop_loading (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	gth_browser_stop_activity_mode (browser);

	if (priv->changing_directory) 
		dir_list_interrupt_change_to (priv->dir_list,
					      (DoneFunc) stop__step2,
					      browser);
	else
		stop__step2 (browser);
}


/* -- gth_browser_refresh -- */


void
gth_browser_refresh (GthBrowser *browser)
{
	browser->priv->refreshing = TRUE;
	gth_browser_update_file_list (browser);
}


/* -- file system monitor -- */


static void notify_files_added (GthBrowser *browser, GList *list);
static void notify_files_deleted (GthBrowser *browser, GList *list);


static gboolean
_proc_monitor_events (gpointer data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	GList                 *dir_created_list, *dir_deleted_list;
	GList                 *file_created_list, *file_deleted_list, *file_changed_list;
	GList                 *scan;

	dir_created_list = priv->monitor_events[MONITOR_EVENT_DIR_CREATED];
	priv->monitor_events[MONITOR_EVENT_DIR_CREATED] = NULL;

	dir_deleted_list = priv->monitor_events[MONITOR_EVENT_DIR_DELETED];
	priv->monitor_events[MONITOR_EVENT_DIR_DELETED] = NULL;

	file_created_list = priv->monitor_events[MONITOR_EVENT_FILE_CREATED];
	priv->monitor_events[MONITOR_EVENT_FILE_CREATED] = NULL;

	file_deleted_list = priv->monitor_events[MONITOR_EVENT_FILE_DELETED];
	priv->monitor_events[MONITOR_EVENT_FILE_DELETED] = NULL;

	file_changed_list = priv->monitor_events[MONITOR_EVENT_FILE_CHANGED];
	priv->monitor_events[MONITOR_EVENT_FILE_CHANGED] = NULL;

	if (priv->update_changes_timeout != 0) 
		g_source_remove (priv->update_changes_timeout);

	/**/

	for (scan = dir_created_list; scan; scan = scan->next) {
		char *path = scan->data;
		const char *name = file_name_from_path (path);

		/* ignore hidden directories. */
		if (name[0] == '.')
			continue;

		dir_list_add_directory (priv->dir_list, path);
	}
	path_list_free (dir_created_list);

	/**/

	for (scan = dir_deleted_list; scan; scan = scan->next) {
		char *path = scan->data;
		dir_list_remove_directory (priv->dir_list, path);
	}
	path_list_free (dir_deleted_list);

	/**/

	if (file_created_list != NULL) {
		notify_files_added (browser, file_created_list);
		path_list_free (file_created_list);
	}

	if (file_deleted_list != NULL) {
		notify_files_deleted (browser, file_deleted_list);
		path_list_free (file_deleted_list);
	}

	if (file_changed_list != NULL) {
		gth_browser_notify_files_changed (browser, file_changed_list);
		path_list_free (file_changed_list);
	}

	/**/

	priv->update_changes_timeout = 0;

	return FALSE;
}


static gboolean
remove_if_present (GList            **monitor_events,
		   MonitorEventType   type,
		   const char        *path)
{
	GList *list, *link;

	list = monitor_events[type];
	link = path_list_find_path (list, path);
	if (link != NULL) {
		monitor_events[type] = g_list_remove_link (list, link);
		path_list_free (link);
		return TRUE;
	}

	return FALSE;
}


static gboolean
add_if_not_present (GList            **monitor_events,
		    MonitorEventType   type,
		    MonitorEventType   add_type,
		    const char        *path)
{
	GList *list, *link;

	list = monitor_events[type];
	link = path_list_find_path (list, path);
	if (link == NULL) {
		monitor_events[add_type] = g_list_append (list, g_strdup (path));
		return TRUE;
	}

	return FALSE;
}


static void
_gth_browser_add_monitor_event (GthBrowser                *browser,
			   GnomeVFSMonitorEventType   event_type,
			   char                      *path,
			   GList                    **monitor_events)
{
	GthBrowserPrivateData *priv = browser->priv;
	MonitorEventType       type;

#ifdef DEBUG /* FIXME */
	{
		char *op;

		if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED)
			op = "CREATED";
		else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED)
			op = "DELETED";
		else 
			op = "CHANGED";

		debug (DEBUG_INFO, "[%s] %s", op, path);
	}
#endif

	if (event_type == GNOME_VFS_MONITOR_EVENT_CREATED) {
		if (path_is_dir (path))
			type = MONITOR_EVENT_DIR_CREATED;
		else
			type = MONITOR_EVENT_FILE_CREATED;

	} else if (event_type == GNOME_VFS_MONITOR_EVENT_DELETED) {
		if (dir_list_get_row_from_path (priv->dir_list, path) != -1)
			type = MONITOR_EVENT_DIR_DELETED;
		else
			type = MONITOR_EVENT_FILE_DELETED;

	} else {
		if (! path_is_dir (path))
			type = MONITOR_EVENT_FILE_CHANGED;
		else 
			return;
	}

	if (type == MONITOR_EVENT_FILE_CREATED) {
		if (remove_if_present (monitor_events, 
				       MONITOR_EVENT_FILE_DELETED, 
				       path))
			type = MONITOR_EVENT_FILE_CHANGED;
		
	} else if (type == MONITOR_EVENT_FILE_DELETED) {
		remove_if_present (monitor_events, 
				   MONITOR_EVENT_FILE_CREATED, 
				   path);
		remove_if_present (monitor_events, 
				   MONITOR_EVENT_FILE_CHANGED, 
				   path);

	} else if (type == MONITOR_EVENT_FILE_CHANGED) {
		remove_if_present (monitor_events, 
				   MONITOR_EVENT_FILE_CHANGED, 
				   path);
		
		if (gth_file_list_pos_from_path (priv->file_list, path) == -1)
			add_if_not_present (monitor_events, 
					    MONITOR_EVENT_FILE_CHANGED, 
					    MONITOR_EVENT_FILE_CREATED, 
					    path);

	} else if (type == MONITOR_EVENT_DIR_CREATED) {
		remove_if_present (monitor_events, 
				   MONITOR_EVENT_DIR_DELETED,
				   path);

	} else if (type == MONITOR_EVENT_DIR_DELETED) 
		remove_if_present (monitor_events, 
				   MONITOR_EVENT_DIR_CREATED, 
				   path);

	monitor_events[type] = g_list_append (monitor_events[type], g_strdup (path));
}


static void
directory_changed (GnomeVFSMonitorHandle    *handle,
		   const char               *monitor_uri,
		   const char               *info_uri,
		   GnomeVFSMonitorEventType  event_type,
		   gpointer                  user_data)
{
	GthBrowser            *browser = user_data; 
	GthBrowserPrivateData *priv = browser->priv;
	char                  *path;

	if (priv->sidebar_content != GTH_SIDEBAR_DIR_LIST)
		return;

	if (priv->update_changes_timeout != 0) 
		return;

	path = gnome_vfs_unescape_string (info_uri + strlen ("file://"), NULL);
	_gth_browser_add_monitor_event (browser, event_type, path, priv->monitor_events);
	g_free (path);

	priv->update_changes_timeout = g_timeout_add (UPDATE_DIR_DELAY,
							_proc_monitor_events,
							browser);
}


/* -- go to directory -- */


void
gth_browser_add_monitor (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GnomeVFSResult         result;
	char                  *uri;

	if (priv->sidebar_content != GTH_SIDEBAR_DIR_LIST)
		return;

	if (priv->dir_list->path == NULL)
		return;

	if (priv->monitor_handle != NULL)
		gnome_vfs_monitor_cancel (priv->monitor_handle);

	uri = g_strconcat ("file://", priv->dir_list->path, NULL);
	result = gnome_vfs_monitor_add (&priv->monitor_handle,
					uri,
					GNOME_VFS_MONITOR_DIRECTORY,
					directory_changed,
					browser);
	g_free (uri);
	priv->monitor_enabled = (result == GNOME_VFS_OK);
}


void
gth_browser_remove_monitor (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->monitor_handle != NULL) {
		gnome_vfs_monitor_cancel (priv->monitor_handle);
		priv->monitor_handle = NULL;
	}
	priv->monitor_enabled = FALSE;
}


static void
set_dir_list_continue (gpointer data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	window_update_title (browser);
	window_update_sensitivity (browser);
	window_make_current_image_visible (browser);

	if (priv->focus_location_entry) {
		gtk_widget_grab_focus (priv->location_entry);
		gtk_editable_set_position (GTK_EDITABLE (priv->location_entry), -1);
		priv->focus_location_entry = FALSE;
	}

	gth_browser_add_monitor (browser);
}


static void
go_to_directory_continue (DirList  *dir_list,
			  gpointer  data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	char                  *path;
	GList                 *file_list;

	gth_browser_stop_activity_mode (browser);

	if (dir_list->result != GNOME_VFS_ERROR_EOF) {
		char *utf8_path;

		utf8_path = g_filename_to_utf8 (dir_list->try_path, -1,
						NULL, NULL, NULL);
		_gtk_error_dialog_run (GTK_WINDOW (browser),
				       _("Cannot load folder \"%s\": %s\n"), 
				       utf8_path, 
				       gnome_vfs_result_to_string (dir_list->result));
		g_free (utf8_path);

		priv->changing_directory = FALSE;
		return;
	}

	path = dir_list->path;

	set_action_sensitive (browser, "Go_Up", strcmp (path, "/") != 0);
	
	/* Add to history list if not present as last entry. */

	if ((priv->go_op == GTH_BROWSER_GO_TO)
	    && ((priv->history_current == NULL) 
		|| (strcmp (path, pref_util_remove_prefix (priv->history_current->data)) != 0))) 
		add_history_item (browser, path, FILE_PREFIX);
	else
		priv->go_op = GTH_BROWSER_GO_TO;
	window_update_history_list (browser);

	set_location (browser, path);

	priv->changing_directory = FALSE;

	/**/

	file_list = dir_list_get_file_list (priv->dir_list);
	window_set_file_list (browser, file_list, 
			      set_dir_list_continue, browser);
	g_list_free (file_list);
}


/* -- real_go_to_directory -- */


typedef struct {
	GthBrowser *browser;
	char       *path;
} GoToData;


void
go_to_directory__step4 (GoToData *gt_data)
{
	GthBrowser            *browser = gt_data->browser;
	GthBrowserPrivateData *priv = browser->priv;
	char                  *dir_path = gt_data->path;
	char                  *path;

	priv->setting_file_list = FALSE;
	priv->changing_directory = TRUE;

	/* Select the directory view. */

	_window_set_sidebar (browser, GTH_SIDEBAR_DIR_LIST); 
	if (! priv->refreshing)
		gth_browser_show_sidebar (browser);
	else
		priv->refreshing = FALSE;

	/**/

	path = remove_ending_separator (dir_path);

	gth_browser_start_activity_mode (browser);
	dir_list_change_to (priv->dir_list, 
			    path, 
			    go_to_directory_continue,
			    browser);
	g_free (path);

	g_free (gt_data->path);
	g_free (gt_data);

	priv->can_change_directory = TRUE;
}


static gboolean
go_to_directory_cb (gpointer data)
{
	GoToData              *gt_data = data;
	GthBrowser            *browser = gt_data->browser;
	GthBrowserPrivateData *priv = browser->priv;

	priv->can_change_directory = FALSE;

	g_source_remove (priv->load_dir_timeout);
	priv->load_dir_timeout = 0;

	if (priv->slideshow)
		gth_browser_stop_slideshow (browser);

	if (priv->changing_directory) {
		gth_browser_stop_activity_mode (browser);
		dir_list_interrupt_change_to (priv->dir_list,
					      (DoneFunc)go_to_directory__step4,
					      gt_data);

	} else if (priv->setting_file_list) 
		gth_file_list_interrupt_set_list (priv->file_list,
						  (DoneFunc) go_to_directory__step4,
						  gt_data);

	else if (priv->file_list->doing_thumbs) 
		gth_file_list_interrupt_thumbs (priv->file_list, 
						(DoneFunc) go_to_directory__step4,
						gt_data);

	else 
		go_to_directory__step4 (gt_data);

	return FALSE;
}


/* used in : goto_dir_set_list_interrupted, gth_browser_go_to_directory. */
static void
real_go_to_directory (GthBrowser *browser,
		      const char *dir_path)
{
	GthBrowserPrivateData *priv = browser->priv;
	GoToData              *gt_data;

	if (! priv->can_change_directory)
		return;

	gt_data = g_new (GoToData, 1);
	gt_data->browser = browser;
	gt_data->path = g_strdup (dir_path);

	if (priv->load_dir_timeout != 0) {
		g_source_remove (priv->load_dir_timeout);
		priv->load_dir_timeout = 0;
	}

	priv->load_dir_timeout = g_timeout_add (LOAD_DIR_DELAY,
						  go_to_directory_cb, 
						  gt_data);
}


typedef struct {
	GthBrowser *browser;
	char       *dir_path;
} GoToDir_SetListInterruptedData;


static void
go_to_dir_set_list_interrupted (gpointer callback_data)
{
	GoToDir_SetListInterruptedData *data = callback_data;

	data->browser->priv->setting_file_list = FALSE;
	gth_browser_stop_activity_mode (data->browser);

	real_go_to_directory (data->browser, data->dir_path);
	g_free (data->dir_path);
	g_free (data);
}

/**/

void
gth_browser_go_to_directory (GthBrowser *browser,
			     const char *dir_path)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if (! priv->can_change_directory)
		return;

	if (priv->setting_file_list && FirstStart)
		return;

	if (priv->setting_file_list && ! priv->can_set_file_list) 
		return;

	/**/

	if (priv->slideshow)
		gth_browser_stop_slideshow (browser);

	if (priv->monitor_handle != NULL) {
		gnome_vfs_monitor_cancel (priv->monitor_handle);
		priv->monitor_handle = NULL;
	}

	if (priv->setting_file_list) 
		gth_browser_stop_activity_mode (browser);

	if (priv->setting_file_list) {
		GoToDir_SetListInterruptedData *sli_data;

		sli_data = g_new (GoToDir_SetListInterruptedData, 1);
		sli_data->browser = browser;
		sli_data->dir_path = g_strdup (dir_path);
		gth_file_list_interrupt_set_list (priv->file_list,
						  go_to_dir_set_list_interrupted,
						  sli_data);
		return;
	}

	real_go_to_directory (browser, dir_path);
}


const char *
gth_browser_get_current_directory (GthBrowser *browser)
{
	return browser->priv->dir_list->path;
}


/* -- */


void
gth_browser_go_to_catalog_directory (GthBrowser *browser,
				     const char *catalog_dir)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *base_dir;
	char                  *catalog_dir2;
	char                  *catalog_dir3;
	char                  *current_path;

	catalog_dir2 = remove_special_dirs_from_path (catalog_dir);
	catalog_dir3 = remove_ending_separator (catalog_dir2);
	g_free (catalog_dir2);

	if (catalog_dir3 == NULL)
		return; /* FIXME: error dialog?  */

	catalog_list_change_to (priv->catalog_list, catalog_dir3);
	set_location (browser, catalog_dir3);
	g_free (catalog_dir3);

	/* Update Go_Up command sensibility */

	current_path = priv->catalog_list->path;
	base_dir = get_catalog_full_path (NULL);
	set_action_sensitive (browser, "Go_Up", 
			       ((current_path != NULL)
				&& strcmp (current_path, base_dir)) != 0);
	g_free (base_dir);
}


/* -- gth_browser_go_to_catalog -- */


void
go_to_catalog__step2 (GoToData *gt_data)
{
	GthBrowser            *browser = gt_data->browser;
	GthBrowserPrivateData *priv = browser->priv;
	char                  *catalog_path = gt_data->path;
	GtkTreeIter            iter;
	char                  *catalog_dir;

	if (priv->slideshow)
		gth_browser_stop_slideshow (browser);

	if (catalog_path == NULL) {
		window_set_file_list (browser, NULL, NULL, NULL);
		if (priv->catalog_path)
			g_free (priv->catalog_path);
		priv->catalog_path = NULL;
		g_free (gt_data->path);
		g_free (gt_data);
		window_update_title (browser);
		window_image_viewer_set_void (browser);
		return;
	}

	if (! path_is_file (catalog_path)) {
		_gtk_error_dialog_run (GTK_WINDOW (browser),
				       _("The specified catalog does not exist."));

		/* gth_browser_go_to_directory (browser, g_get_home_dir ()); FIXME */
		g_free (gt_data->path);
		g_free (gt_data);
		window_update_title (browser);
		window_image_viewer_set_void (browser);
		return;
	}

	if (priv->catalog_path != catalog_path) {
		if (priv->catalog_path)
			g_free (priv->catalog_path);
		priv->catalog_path = g_strdup (catalog_path);
	}

	_window_set_sidebar (browser, GTH_SIDEBAR_CATALOG_LIST); 
	if (! priv->refreshing && ! ViewFirstImage)
		gth_browser_show_sidebar (browser);
	else
		priv->refreshing = FALSE;

	catalog_dir = remove_level_from_path (catalog_path);
	gth_browser_go_to_catalog_directory (browser, catalog_dir);
	g_free (catalog_dir);

	if (! catalog_list_get_iter_from_path (priv->catalog_list,
					       catalog_path,
					       &iter)) {
		g_free (gt_data->path);
		g_free (gt_data);
		window_image_viewer_set_void (browser);
		return;
	}

	catalog_list_select_iter (priv->catalog_list, &iter);
	catalog_activate (browser, catalog_path);

	g_free (gt_data->path);
	g_free (gt_data);
}


void
gth_browser_go_to_catalog (GthBrowser *browser,
			   const char *catalog_path)
{
	GthBrowserPrivateData *priv = browser->priv;
	GoToData              *gt_data;

	g_return_if_fail (browser != NULL);

	if (priv->setting_file_list && FirstStart)
		return;

	gt_data = g_new (GoToData, 1);
	gt_data->browser = browser;
	gt_data->path = g_strdup (catalog_path);
	
	if (priv->file_list->doing_thumbs)
		gth_file_list_interrupt_thumbs (priv->file_list, 
						(DoneFunc) go_to_catalog__step2,
						gt_data);
	else
		go_to_catalog__step2 (gt_data);
}


const char *
gth_browser_get_current_catalog (GthBrowser *browser)
{
	return browser->priv->catalog_path;
}


gboolean
gth_browser_go_up__is_base_dir (GthBrowser *browser,
				const char *dir)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (dir == NULL)
		return FALSE;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		return (strcmp (dir, "/") == 0);
	else {
		char *catalog_base = get_catalog_full_path (NULL);
		gboolean is_base_dir;

		is_base_dir = strcmp (dir, catalog_base) == 0;
		g_free (catalog_base);
		return is_base_dir;
	}
	
	return FALSE;
}


void
gth_browser_go_up (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *current_path;
	char                  *up_dir;

	g_return_if_fail (browser != NULL);

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST) 
		current_path = priv->dir_list->path;
	else 
		current_path = priv->catalog_list->path;

	if (current_path == NULL)
		return;

	if (gth_browser_go_up__is_base_dir (browser, current_path))
		return;

	up_dir = g_strdup (current_path);
	do {
		char *tmp = up_dir;
		up_dir = remove_level_from_path (tmp);
		g_free (tmp);
	} while (! gth_browser_go_up__is_base_dir (browser, up_dir) 
		 && ! path_is_dir (up_dir));

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
		gth_browser_go_to_directory (browser, up_dir);
	else {
		gth_browser_go_to_catalog (browser, NULL);
		gth_browser_go_to_catalog_directory (browser, up_dir);
	}

	g_free (up_dir);
}


static void 
go_to_current_location (GthBrowser *browser,
			WindowGoOp  go_op)
{
	GthBrowserPrivateData *priv = browser->priv;
	char                  *location;

	if (priv->history_current == NULL)
		return;

	priv->go_op = go_op;

	location = priv->history_current->data;
	if (pref_util_location_is_catalog (location))
		gth_browser_go_to_catalog (browser, pref_util_get_catalog_location (location));
	else if (pref_util_location_is_search (location))
		gth_browser_go_to_catalog (browser, pref_util_get_search_location (location));
	else if (pref_util_location_is_file (location)) 
		gth_browser_go_to_directory (browser, pref_util_get_file_location (location));
}


void
gth_browser_go_back (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->history_current == NULL)
		return;

	if (priv->history_current->next == NULL)
		return;

	priv->history_current = priv->history_current->next;
	go_to_current_location (browser, GTH_BROWSER_GO_BACK);
}


void
gth_browser_go_forward (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->history_current == NULL)
		return;

	if (priv->history_current->prev == NULL)
		return;

	priv->history_current = priv->history_current->prev;
	go_to_current_location (browser, GTH_BROWSER_GO_FORWARD);
}


void
gth_browser_delete_history (GthBrowser *browser)
{
	bookmarks_remove_all (browser->priv->history);
	browser->priv->history_current = NULL;
	window_update_history_list (browser);
}


/* -- slideshow -- */


static GList *
get_slideshow_random_image (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	int                    n, i;
	GList                 *item;

	if (priv->slideshow_current != NULL) {
		g_list_free (priv->slideshow_current);
		priv->slideshow_current = NULL;
	}

	n = g_list_length (priv->slideshow_random_set);
	if (n == 0)
		return NULL;
	i = g_random_int_range (0, n - 1);

	item = g_list_nth (priv->slideshow_random_set, i);
	if (item != NULL)
		priv->slideshow_random_set = g_list_remove_link (priv->slideshow_random_set, item);

	priv->slideshow_current = item;

	return item;
}


static GList *
get_next_slideshow_image (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	GList                 *current;
	FileData              *fd;
	gboolean               wrap_around;

	current = priv->slideshow_current;
	if (current == NULL) {
		if (pref_get_slideshow_direction () == GTH_DIRECTION_RANDOM)
			current = get_slideshow_random_image (browser);
		else
			current = priv->slideshow_first;
		fd = current->data;
		if (! fd->error) 
			return current;
	}

	wrap_around = eel_gconf_get_boolean (PREF_SLIDESHOW_WRAP_AROUND, FALSE);
	do {
		if (pref_get_slideshow_direction () == GTH_DIRECTION_FORWARD) {
			current = current->next;
			if (current == NULL)
				current = priv->slideshow_set;
			if ((current == priv->slideshow_first) && !wrap_around) 
				return NULL;

		} else if (pref_get_slideshow_direction () == GTH_DIRECTION_REVERSE) {
			current = current->prev;
			if (current == NULL)
				current = g_list_last (priv->slideshow_set);
			if ((current == priv->slideshow_first) && ! wrap_around)
				return NULL;

		} else if (pref_get_slideshow_direction () == GTH_DIRECTION_RANDOM) {
			current = get_slideshow_random_image (browser);
			if (current == NULL) {
				if (! wrap_around)
					return NULL;
				else {
					priv->slideshow_random_set = g_list_copy (priv->slideshow_set);
					current = get_slideshow_random_image (browser);
				}
			}
		}

		fd = current->data;
	} while (fd->error);

	return current;
}


static gboolean
slideshow_timeout_cb (gpointer data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	gboolean               go_on = FALSE;

	if (priv->slideshow_timeout != 0) {
		g_source_remove (priv->slideshow_timeout);
		priv->slideshow_timeout = 0;
	}

	priv->slideshow_current = get_next_slideshow_image (browser);

	if (priv->slideshow_current != NULL) {
		FileData *fd = priv->slideshow_current->data;
		int       pos = gth_file_list_pos_from_path (priv->file_list, fd->path);
		if (pos != -1) {
			gth_file_view_set_cursor (priv->file_list->view, pos);
			go_on = TRUE;
		}
	}

	if (! go_on) 
		gth_browser_stop_slideshow (browser);
	else
		priv->slideshow_timeout = g_timeout_add (eel_gconf_get_integer (PREF_SLIDESHOW_DELAY, DEF_SLIDESHOW_DELAY) * 1000, slideshow_timeout_cb, browser);

	return FALSE;
}


void
gth_browser_start_slideshow (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	gboolean               only_selected;
	gboolean               go_on = FALSE;

	g_return_if_fail (browser != NULL);

	if (priv->file_list->list == NULL)
		return;

	if (priv->slideshow)
		return;

	priv->slideshow = TRUE;

	if (priv->slideshow_set != NULL) {
		g_list_free (priv->slideshow_set);
		priv->slideshow_set = NULL;
	}
	if (priv->slideshow_random_set != NULL) {
		g_list_free (priv->slideshow_random_set);
		priv->slideshow_random_set = NULL;
	}
	priv->slideshow_first = NULL;
	priv->slideshow_current = NULL;

	only_selected = gth_file_view_selection_not_null (priv->file_list->view) && ! gth_file_view_only_one_is_selected (priv->file_list->view);

	if (only_selected)
		priv->slideshow_set = gth_file_view_get_selection (priv->file_list->view);
	else
		priv->slideshow_set = gth_file_view_get_list (priv->file_list->view);
	priv->slideshow_random_set = g_list_copy (priv->slideshow_set);

	if (pref_get_slideshow_direction () == GTH_DIRECTION_FORWARD)
		priv->slideshow_first = g_list_first (priv->slideshow_set);
	else if (pref_get_slideshow_direction () == GTH_DIRECTION_REVERSE)
		priv->slideshow_first = g_list_last (priv->slideshow_set);
	else if (pref_get_slideshow_direction () == GTH_DIRECTION_RANDOM) 
		priv->slideshow_first = NULL;

	priv->slideshow_current = get_next_slideshow_image (browser);

	if (priv->slideshow_current != NULL) {
		FileData *fd = priv->slideshow_current->data;
		int       pos = gth_file_list_pos_from_path (priv->file_list, fd->path);
		if (pos != -1) {
			/* FIXME
			if (eel_gconf_get_boolean (PREF_SLIDESHOW_FULLSCREEN, TRUE))
				fullscreen_start (fullscreen, browser);
			*/
			gth_file_view_set_cursor (priv->file_list->view, pos);
			go_on = TRUE;
		}
	}

	if (! go_on) 
		gth_browser_stop_slideshow (browser);
	else
		priv->slideshow_timeout = g_timeout_add (eel_gconf_get_integer (PREF_SLIDESHOW_DELAY, DEF_SLIDESHOW_DELAY) * 1000, slideshow_timeout_cb, browser);
}


void
gth_browser_stop_slideshow (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (! priv->slideshow)
		return;

	priv->slideshow = FALSE;
	if (priv->slideshow_timeout != 0) {
		g_source_remove (priv->slideshow_timeout);
		priv->slideshow_timeout = 0;
	}

	if ((pref_get_slideshow_direction () == GTH_DIRECTION_RANDOM)
	    && (priv->slideshow_current != NULL)) {
		g_list_free (priv->slideshow_current);
		priv->slideshow_current = NULL;
	}

	if (priv->slideshow_set != NULL) {
		g_list_free (priv->slideshow_set);
		priv->slideshow_set = NULL;
	}

	if (priv->slideshow_random_set != NULL) {
		g_list_free (priv->slideshow_random_set);
		priv->slideshow_random_set = NULL;
	}

	/* FIXME
	if (eel_gconf_get_boolean (PREF_SLIDESHOW_FULLSCREEN, TRUE) && priv->fullscreen)
		fullscreen_stop (fullscreen);
	*/
}


void
gth_browser_toggle_slideshow (GthBrowser *browser)
{
	if (! browser->priv->slideshow)
		gth_browser_start_slideshow (browser);
	else
		gth_browser_stop_slideshow (browser);
}


/**/


static gboolean
view_focused_image (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	int                    pos;
	char                  *focused;
	gboolean               not_focused;

	pos = gth_file_view_get_cursor (priv->file_list->view);
	if (pos == -1)
		return FALSE;

	focused = gth_file_list_path_from_pos (priv->file_list, pos);
	if (focused == NULL)
		return FALSE;

	not_focused = strcmp (priv->image_path, focused) != 0;
	g_free (focused);

	return not_focused;
}


gboolean
gth_browser_show_next_image (GthBrowser *browser,
			     gboolean    only_selected)
{
	GthBrowserPrivateData *priv = browser->priv;
	gboolean               skip_broken;
	int                    pos;

	g_return_val_if_fail (browser != NULL, FALSE);
	
	if (priv->slideshow 
	    || priv->setting_file_list 
	    || priv->changing_directory) 
		return FALSE;
	
	skip_broken = priv->fullscreen;

	if (priv->image_path == NULL) {
		pos = gth_file_list_next_image (priv->file_list, -1, skip_broken, only_selected);

	} else if (view_focused_image (browser)) {
		pos = gth_file_view_get_cursor (priv->file_list->view);
		if (pos == -1)
			pos = gth_file_list_next_image (priv->file_list, pos, skip_broken, only_selected);

	} else {
		pos = gth_file_list_pos_from_path (priv->file_list, priv->image_path);
		pos = gth_file_list_next_image (priv->file_list, pos, skip_broken, only_selected);
	}

	if (pos != -1) {
		if (! only_selected)
			gth_file_list_select_image_by_pos (priv->file_list, pos); 
		gth_file_view_set_cursor (priv->file_list->view, pos);
	}

	return (pos != -1);
}


gboolean
gth_browser_show_prev_image (GthBrowser *browser,
			     gboolean    only_selected)
{
	GthBrowserPrivateData *priv = browser->priv;
	gboolean               skip_broken;
	int                    pos;

	g_return_val_if_fail (browser != NULL, FALSE);
	
	if (priv->slideshow 
	    || priv->setting_file_list 
	    || priv->changing_directory)
		return FALSE;

	skip_broken = priv->fullscreen;
	
	if (priv->image_path == NULL) {
		pos = gth_file_view_get_images (priv->file_list->view);
		pos = gth_file_list_prev_image (priv->file_list, pos, skip_broken, only_selected);
			
	} else if (view_focused_image (browser)) {
		pos = gth_file_view_get_cursor (priv->file_list->view);
		if (pos == -1) {
			pos = gth_file_view_get_images (priv->file_list->view);
			pos = gth_file_list_prev_image (priv->file_list, pos, skip_broken, only_selected);
		}

	} else {
		pos = gth_file_list_pos_from_path (priv->file_list, priv->image_path);
		pos = gth_file_list_prev_image (priv->file_list, pos, skip_broken, only_selected);
	}

	if (pos != -1) {
		if (! only_selected)
			gth_file_list_select_image_by_pos (priv->file_list, pos); 
		gth_file_view_set_cursor (priv->file_list->view, pos);
	}

	return (pos != -1);
}


gboolean
gth_browser_show_first_image (GthBrowser *browser, 
			      gboolean    only_selected)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (gth_file_view_get_images (priv->file_list->view) == 0)
		return FALSE;

	if (priv->image_path) {
		g_free (priv->image_path);
		priv->image_path = NULL;
	}

	return gth_browser_show_next_image (browser, only_selected);
}


gboolean
gth_browser_show_last_image (GthBrowser *browser, 
			     gboolean    only_selected)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (gth_file_view_get_images (priv->file_list->view) == 0)
		return FALSE;

	if (priv->image_path) {
		g_free (priv->image_path);
		priv->image_path = NULL;
	}

	return gth_browser_show_prev_image (browser, only_selected);
}


void
gth_browser_show_image_prop (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->image_prop_dlg == NULL) 
		priv->image_prop_dlg = dlg_image_prop_new (browser);
	else
		gtk_window_present (GTK_WINDOW (priv->image_prop_dlg));
}


void
gth_browser_set_image_prop_dlg (GthBrowser *browser,
				GtkWidget  *dialog)
{
	browser->priv->image_prop_dlg = dialog;
}


static ImageViewer *
gth_browser_get_image_viewer (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	return IMAGE_VIEWER (browser->priv->viewer);
}


static const char *
gth_browser_get_image_filename (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	return browser->priv->image_path;
}


static gboolean
gth_browser_get_image_modified (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	return browser->priv->image_modified;
}


static void
gth_browser_set_image_modified (GthWindow *window,
				gboolean   modified)
{
	GthBrowser            *browser = GTH_BROWSER (window);
	GthBrowserPrivateData *priv = browser->priv;

	priv->image_modified = modified;
	window_update_infobar (browser);
	window_update_statusbar_image_info (browser);
	window_update_title (browser);

	set_action_sensitive (browser, "File_Revert", ! image_viewer_is_void (IMAGE_VIEWER (priv->viewer)) && priv->image_modified);

	if (modified && priv->image_prop_dlg != NULL)
		dlg_image_prop_update (priv->image_prop_dlg);
}


/* -- load image -- */


static char *
get_image_to_preload (GthBrowser *browser,
		      int           pos,
		      int           priority)
{
	GthBrowserPrivateData *priv = browser->priv;
	FileData              *fdata;
	int                    max_size;

	if (pos < 0)
		return NULL;
	if (pos >= gth_file_view_get_images (priv->file_list->view))
		return NULL;

	fdata = gth_file_view_get_image_data (priv->file_list->view, pos);
	if (fdata == NULL)
		return NULL;

	debug (DEBUG_INFO, "%ld <-> %ld\n", (long int) fdata->size, (long int)PRELOADED_IMAGE_MAX_SIZE);

	if (priority == 1)
		max_size = PRELOADED_IMAGE_MAX_DIM1;
	else
		max_size = PRELOADED_IMAGE_MAX_DIM2;

	if (fdata->size > max_size) {
		debug (DEBUG_INFO, "image %s too large for preloading", gth_file_list_path_from_pos (priv->file_list, pos));
		file_data_unref (fdata);
		return NULL;
	}

#ifdef HAVE_LIBJPEG
	if (image_is_jpeg (fdata->path)) {
		int width = 0, height = 0;

		f_get_jpeg_size (fdata->path, &width, &height);

		debug (DEBUG_INFO, "[%dx%d] <-> %d\n", width, height, max_size);

		if (width * height > max_size) {
			debug (DEBUG_INFO, "image %s too large for preloading", gth_file_list_path_from_pos (priv->file_list, pos));
			file_data_unref (fdata);
			return NULL;
		}
	}
#endif /* HAVE_LIBJPEG */

	file_data_unref (fdata);

	return gth_file_list_path_from_pos (priv->file_list, pos);
}


static gboolean
load_timeout_cb (gpointer data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	char                  *prev1;
	char                  *next1;
	char                  *next2;
	int                    pos;

	if (priv->view_image_timeout != 0) {
		g_source_remove (priv->view_image_timeout);
		priv->view_image_timeout = 0;
	}

	if (priv->image_path == NULL)
		return FALSE;

	pos = gth_file_list_pos_from_path (priv->file_list, priv->image_path);
	g_return_val_if_fail (pos != -1, FALSE);

	prev1 = get_image_to_preload (browser, pos - 1, 1);
	next1 = get_image_to_preload (browser, pos + 1, 1);
	next2 = get_image_to_preload (browser, pos + 2, 2);
	
	gthumb_preloader_start (priv->preloader, 
				priv->image_path, 
				next1, 
				prev1, 
				next2);

	g_free (prev1);
	g_free (next1);
	g_free (next2);

	return FALSE;
}


void
gth_browser_reload_image (GthBrowser *browser)
{
	g_return_if_fail (browser != NULL);
	
	if (browser->priv->image_path != NULL)
		load_timeout_cb (browser);
}


static void
load_image__image_saved_cb (char     *filename,
			    gpointer  data)
{
	GthBrowser            *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	priv->image_modified = FALSE;
	priv->saving_modified_image = FALSE;
	gth_browser_load_image (browser, priv->new_image_path);
}


void
gth_browser_load_image (GthBrowser *browser, 
			const char *filename)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if (priv->image_modified) {
		if (priv->saving_modified_image)
			return;
		g_free (priv->new_image_path);
		priv->new_image_path = g_strdup (filename);
		if (ask_whether_to_save (browser, load_image__image_saved_cb))
			return;
	}

	if (filename == priv->image_path) {
		gth_browser_reload_image (browser);
		return;
	}

	if (! priv->image_modified
	    && (priv->image_path != NULL) 
	    && (filename != NULL)
	    && (strcmp (filename, priv->image_path) == 0)
	    && (priv->image_mtime == get_file_mtime (priv->image_path))) 
		return;

	if (priv->view_image_timeout != 0) {
		g_source_remove (priv->view_image_timeout);
		priv->view_image_timeout = 0;
	}
	
	/* If the image is from a catalog remember the catalog name. */

	if (priv->image_catalog != NULL) {
		g_free (priv->image_catalog);
		priv->image_catalog = NULL;
	}
	if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST)
		priv->image_catalog = g_strdup (priv->catalog_path);

	/**/

	if (filename == NULL) {
		window_image_viewer_set_void (browser);
		return;
	}

	g_free (priv->image_path);
	priv->image_path = g_strdup (filename);

	priv->view_image_timeout = g_timeout_add (VIEW_IMAGE_DELAY,
						    load_timeout_cb, 
						    browser);
}


/* -- changes notification functions -- */


void
notify_files_added__step2 (gpointer data)
{
	GthBrowser *browser = data;

	window_update_statusbar_list_info (browser);
	window_update_infobar (browser);
	window_update_sensitivity (browser);
}


static void
notify_files_added (GthBrowser *browser,
		    GList      *list)
{
	gth_file_list_add_list (browser->priv->file_list, 
				list, 
				notify_files_added__step2,
				browser);
}


void
gth_browser_notify_files_created (GthBrowser *browser,
				  GList      *list)
{
	GthBrowserPrivateData *priv = browser->priv;
	GList *scan;
	GList *created_in_current_dir = NULL;
	char  *current_dir;

	if (priv->sidebar_content != GTH_SIDEBAR_DIR_LIST)
		return;

	current_dir = priv->dir_list->path;
	if (current_dir == NULL)
		return;

	for (scan = list; scan; scan = scan->next) {
		char *path = scan->data;
		char *parent_dir;
		
		parent_dir = remove_level_from_path (path);

		if (strcmp (parent_dir, current_dir) == 0)
			created_in_current_dir = g_list_prepend (created_in_current_dir, path);

		g_free (parent_dir);
	}

	if (created_in_current_dir != NULL) {
		notify_files_added (browser, created_in_current_dir);
		g_list_free (created_in_current_dir);
	}
}


typedef struct {
	GthBrowser *browser;
	GList      *list;
	gboolean    restart_thumbs;
} FilesDeletedData;


static void
notify_files_deleted__step2 (FilesDeletedData *data)
{
	GthBrowser   *browser = data->browser;
	GthBrowserPrivateData *priv = browser->priv;
	GList        *list = data->list;
	GList        *scan;
	char         *filename;
	int           pos, smallest_pos, image_pos;
	gboolean      current_image_deleted = FALSE;
	gboolean      no_image_viewed;

	gth_file_view_freeze (priv->file_list->view);

	pos = -1;
	smallest_pos = -1;
	image_pos = -1;
	if (priv->image_path)
		image_pos = gth_file_list_pos_from_path (priv->file_list, 
							 priv->image_path);
	no_image_viewed = (image_pos == -1);

	for (scan = list; scan; scan = scan->next) {
		filename = scan->data;

		pos = gth_file_list_pos_from_path (priv->file_list, filename);
		if (pos == -1) 
			continue;

		if (image_pos == pos) {
			/* the current image will be deleted. */
			image_pos = -1;
			current_image_deleted = TRUE;

		} else if (image_pos > pos)
			/* a previous image will be deleted, so image_pos 
			 * decrements its value. */
			image_pos--;

		if (scan == list)
			smallest_pos = pos;
		else
			smallest_pos = MIN (smallest_pos, pos);

		gth_file_list_delete_pos (priv->file_list, pos);
	}

	gth_file_view_thaw (priv->file_list->view);

	/* Try to visualize the smallest pos. */
	if (smallest_pos != -1) {
		int images = gth_file_view_get_images (priv->file_list->view);

		pos = smallest_pos;

		if (pos > images - 1)
			pos = images - 1;
		if (pos < 0)
			pos = 0;

		gth_file_view_moveto (priv->file_list->view, pos, 0.5);
	}

	if (! no_image_viewed) {
		if (current_image_deleted) {
			int images = gth_file_view_get_images (priv->file_list->view);

			/* delete the image from the viewer. */
			gth_browser_load_image (browser, NULL);

			if ((images > 0) && (smallest_pos != -1)) {
				pos = smallest_pos;
				
				if (pos > images - 1)
					pos = images - 1;
				if (pos < 0)
					pos = 0;
				
				view_image_at_pos (browser, pos);
				gth_file_list_select_image_by_pos (priv->file_list, pos);
			}
		}
	}

	window_update_statusbar_list_info (browser);
	window_update_sensitivity (browser);

	if (data->restart_thumbs)
		gth_file_list_restart_thumbs (data->browser->priv->file_list, TRUE);

	path_list_free (data->list);
	g_free (data);
}


static void
notify_files_deleted (GthBrowser *browser,
		      GList      *list)
{
	FilesDeletedData *data;

	if (list == NULL)
		return;

	data = g_new (FilesDeletedData, 1);

	data->browser = browser;
	data->list = path_list_dup (list);
	data->restart_thumbs = browser->priv->file_list->doing_thumbs;
	
	gth_file_list_interrupt_thumbs (browser->priv->file_list,
					(DoneFunc) notify_files_deleted__step2,
					data);
}


void
gth_browser_notify_files_deleted (GthBrowser *browser,
			     GList      *list)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if ((priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST)
	    && (priv->catalog_path != NULL)) { /* update the catalog. */
		Catalog *catalog;
		GList   *scan;

		catalog = catalog_new ();
		if (catalog_load_from_disk (catalog, priv->catalog_path, NULL)) {
			for (scan = list; scan; scan = scan->next)
				catalog_remove_item (catalog, (char*) scan->data);
			catalog_write_to_disk (catalog, NULL);
		}
		catalog_free (catalog);
	} 

	notify_files_deleted (browser, list);
}


void
gth_browser_notify_files_changed (GthBrowser *browser,
				  GList      *list)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (! priv->file_list->doing_thumbs)
		gth_file_list_update_thumb_list (priv->file_list, list);

	if (priv->image_path != NULL) {
		int pos = gth_file_list_pos_from_path (priv->file_list,
						       priv->image_path);
		if (pos != -1)
			view_image_at_pos (browser, pos);
	}
}


void
gth_browser_notify_cat_files_added (GthBrowser *browser,
				    const char *catalog_name,
				    GList      *list)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if (priv->sidebar_content != GTH_SIDEBAR_CATALOG_LIST)
		return;
	if (priv->catalog_path == NULL)
		return;
	if (strcmp (priv->catalog_path, catalog_name) != 0)
		return;

	notify_files_added (browser, list);
}


void
gth_browser_notify_cat_files_deleted (GthBrowser *browser,
				      const char *catalog_name,
				      GList      *list)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if (priv->sidebar_content != GTH_SIDEBAR_CATALOG_LIST)
		return;
	if (priv->catalog_path == NULL)
		return;
	if (strcmp (priv->catalog_path, catalog_name) != 0)
		return;

	notify_files_deleted (browser, list);
}

	
void
gth_browser_notify_file_rename (GthBrowser *browser,
				const char *old_name,
				const char *new_name)
{
	GthBrowserPrivateData *priv = browser->priv;
	int pos;

	g_return_if_fail (browser != NULL);
	if ((old_name == NULL) || (new_name == NULL))
		return;

	if ((priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST)
	    && (priv->catalog_path != NULL)) { /* update the catalog. */
		Catalog  *catalog;
		GList    *scan;
		gboolean  changed = FALSE;

		catalog = catalog_new ();
		if (catalog_load_from_disk (catalog, priv->catalog_path, NULL)) {
			for (scan = catalog->list; scan; scan = scan->next) {
				char *entry = scan->data;
				if (strcmp (entry, old_name) == 0) {
					catalog_remove_item (catalog, old_name);
					catalog_add_item (catalog, new_name);
					changed = TRUE;
				}
			}
			if (changed)
				catalog_write_to_disk (catalog, NULL);
		}
		catalog_free (catalog);
	}

	pos = gth_file_list_pos_from_path (priv->file_list, new_name);
	if (pos != -1)
		gth_file_list_delete_pos (priv->file_list, pos);

	pos = gth_file_list_pos_from_path (priv->file_list, old_name);
	if (pos != -1)
		gth_file_list_rename_pos (priv->file_list, pos, new_name);

	if ((priv->image_path != NULL) 
	    && strcmp (old_name, priv->image_path) == 0) 
		gth_browser_load_image (browser, new_name);
}


static gboolean
first_level_sub_directory (GthBrowser *browser,
			   const char *current,
			   const char *old_path)
{
	const char *old_name;
	int         current_l;
	int         old_path_l;

	current_l = strlen (current);
	old_path_l = strlen (old_path);

	if (old_path_l <= current_l + 1)
		return FALSE;

	if (strncmp (current, old_path, current_l) != 0)
		return FALSE;

	old_name = old_path + current_l + 1;

	return (strchr (old_name, '/') == NULL);
}


void
gth_browser_notify_directory_rename (GthBrowser *browser,
				     const char *old_name,
				     const char *new_name)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST) {
		if (strcmp (priv->dir_list->path, old_name) == 0) 
			gth_browser_go_to_directory (browser, new_name);
		else {
			const char *current = priv->dir_list->path;

			/* a sub directory was renamed, refresh. */
			if (first_level_sub_directory (browser, current, old_name)) 
				dir_list_remove_directory (priv->dir_list, 
							   file_name_from_path (old_name));

			if (first_level_sub_directory (browser, current, new_name)) 
				dir_list_add_directory (priv->dir_list, 
							file_name_from_path (new_name));
		}
		
	} else if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) {
		if (strcmp (priv->catalog_list->path, old_name) == 0) 
			gth_browser_go_to_catalog_directory (browser, new_name);
		else {
			const char *current = priv->catalog_list->path;
			if (first_level_sub_directory (browser, current, old_name))  
				gth_browser_update_catalog_list (browser);
		}
	}

	if ((priv->image_path != NULL) 
	    && (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST)
	    && (strncmp (priv->image_path, 
			 old_name,
			 strlen (old_name)) == 0)) {
		char *new_image_name;

		new_image_name = g_strconcat (new_name,
					      priv->image_path + strlen (old_name),
					      NULL);
		gth_browser_notify_file_rename (browser, 
					   priv->image_path,
					   new_image_name);
		g_free (new_image_name);
	}
}


void
gth_browser_notify_directory_delete (GthBrowser *browser,
				     const char *path)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST) {
		if (strcmp (priv->dir_list->path, path) == 0) 
			gth_browser_go_up (browser);
		else {
			const char *current = priv->dir_list->path;
			if (first_level_sub_directory (browser, current, path))
				dir_list_remove_directory (priv->dir_list, 
							   file_name_from_path (path));
		}

	} else if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) {
		if (strcmp (priv->catalog_list->path, path) == 0) 
			gth_browser_go_up (browser);
		else {
			const char *current = priv->catalog_list->path;
			if (path_in_path (current, path))
				/* a sub directory got deleted, refresh. */
				gth_browser_update_catalog_list (browser);
		}
	}

	if ((priv->image_path != NULL) 
	    && (path_in_path (priv->image_path, path))) {
		GList *list;
		
		list = g_list_append (NULL, priv->image_path);
		gth_browser_notify_files_deleted (browser, list);
		g_list_free (list);
	}
}


void
gth_browser_notify_directory_new (GthBrowser *browser,
				  const char *path)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->sidebar_content == GTH_SIDEBAR_DIR_LIST) {
		const char *current = priv->dir_list->path;
		if (first_level_sub_directory (browser, current, path))
			dir_list_add_directory (priv->dir_list, 
						file_name_from_path (path));

	} else if (priv->sidebar_content == GTH_SIDEBAR_CATALOG_LIST) {
		const char *current = priv->catalog_list->path;
		if (path_in_path (current, path))
			/* a sub directory was created, refresh. */
			gth_browser_update_catalog_list (browser);
	}
}


void
gth_browser_notify_catalog_rename (GthBrowser *browser,
				   const char *old_path,
				   const char *new_path)
{
	GthBrowserPrivateData *priv = browser->priv;
	char     *catalog_dir;
	gboolean  viewing_a_catalog;
	gboolean  current_cat_renamed;
	gboolean  renamed_cat_is_in_current_dir;

	if (priv->sidebar_content != GTH_SIDEBAR_CATALOG_LIST) 
		return;

	if (priv->catalog_list->path == NULL)
		return;

	catalog_dir = remove_level_from_path (priv->catalog_list->path);
	viewing_a_catalog = (priv->catalog_path != NULL);
	current_cat_renamed = ((priv->catalog_path != NULL) && (strcmp (priv->catalog_path, old_path) == 0));
	renamed_cat_is_in_current_dir = path_in_path (catalog_dir, new_path);

	if (! renamed_cat_is_in_current_dir) {
		g_free (catalog_dir);
		return;
	}

	if (! viewing_a_catalog) 
		gth_browser_go_to_catalog_directory (browser, priv->catalog_list->path);
	else {
		if (current_cat_renamed)
			gth_browser_go_to_catalog (browser, new_path);
		else {
			GtkTreeIter iter;
			gth_browser_go_to_catalog_directory (browser, priv->catalog_list->path);

			/* reselect the current catalog. */
			if (catalog_list_get_iter_from_path (priv->catalog_list, 
							     priv->catalog_path, &iter))
				catalog_list_select_iter (priv->catalog_list, &iter);
		}
	}

	g_free (catalog_dir);
}


void
gth_browser_notify_catalog_new (GthBrowser *browser,
				const char *path)
{
	GthBrowserPrivateData *priv = browser->priv;
	char     *catalog_dir;
	gboolean  viewing_a_catalog;
	gboolean  created_cat_is_in_current_dir;

	if (priv->sidebar_content != GTH_SIDEBAR_CATALOG_LIST) 
		return;

	if (priv->catalog_list->path == NULL)
		return;

	viewing_a_catalog = (priv->catalog_path != NULL);
	catalog_dir = remove_level_from_path (priv->catalog_list->path);
	created_cat_is_in_current_dir = path_in_path (catalog_dir, path);

	if (! created_cat_is_in_current_dir) {
		g_free (catalog_dir);
		return;
	}

	gth_browser_go_to_catalog_directory (browser, priv->catalog_list->path);

	if (viewing_a_catalog) {
		GtkTreeIter iter;
			
		/* reselect the current catalog. */
		if (catalog_list_get_iter_from_path (priv->catalog_list, 
						     priv->catalog_path, 
						     &iter))
			catalog_list_select_iter (priv->catalog_list, &iter);
	}

	g_free (catalog_dir);
}


void
gth_browser_notify_catalog_delete (GthBrowser *browser,
				   const char *path)
{
	GthBrowserPrivateData *priv = browser->priv;
	char     *catalog_dir;
	gboolean  viewing_a_catalog;
	gboolean  current_cat_deleted;
	gboolean  deleted_cat_is_in_current_dir;

	if (priv->sidebar_content != GTH_SIDEBAR_CATALOG_LIST) 
		return;

	if (priv->catalog_list->path == NULL)
		return;

	catalog_dir = remove_level_from_path (priv->catalog_list->path);
	viewing_a_catalog = (priv->catalog_path != NULL);
	current_cat_deleted = ((priv->catalog_path != NULL) && (strcmp (priv->catalog_path, path) == 0));
	deleted_cat_is_in_current_dir = path_in_path (catalog_dir, path);

	if (! deleted_cat_is_in_current_dir) {
		g_free (catalog_dir);
		return;
	}

	if (! viewing_a_catalog) 
		gth_browser_go_to_catalog_directory (browser, priv->catalog_list->path);
	else {
		if (current_cat_deleted) {
			gth_browser_go_to_catalog (browser, NULL);
			gth_browser_go_to_catalog_directory (browser, priv->catalog_list->path);
		} else {
			GtkTreeIter iter;
			gth_browser_go_to_catalog_directory (browser, priv->catalog_list->path);
			
			/* reselect the current catalog. */
			if (catalog_list_get_iter_from_path (priv->catalog_list, 
							     priv->catalog_path, &iter))
				catalog_list_select_iter (priv->catalog_list, &iter);
		}
	}

	g_free (catalog_dir);
}


void
gth_browser_notify_update_comment (GthBrowser *browser,
				   const char *filename)
{
	GthBrowserPrivateData *priv = browser->priv;
	int pos;

	g_return_if_fail (browser != NULL);

	update_image_comment (browser);

	pos = gth_file_list_pos_from_path (priv->file_list, filename);
	if (pos != -1)
		gth_file_list_update_comment (priv->file_list, pos);
}


void
gth_browser_notify_update_directory (GthBrowser *browser,
				     const char *dir_path)
{
	GthBrowserPrivateData *priv = browser->priv;

	g_return_if_fail (browser != NULL);

	if (priv->monitor_enabled)
		return;
	
	if ((priv->dir_list->path == NULL) 
	    || (strcmp (priv->dir_list->path, dir_path) != 0)) 
		return;

	gth_browser_update_file_list (browser);
}


gboolean
gth_browser_notify_update_layout_cb (gpointer data)
{
	GthBrowser   *browser = data;
	GthBrowserPrivateData *priv = browser->priv;
	GtkWidget    *paned1;      /* Main paned widget. */
	GtkWidget    *paned2;      /* Secondary paned widget. */
	int           paned1_pos;
	int           paned2_pos;
	gboolean      sidebar_visible;

	if (! GTK_IS_WIDGET (priv->main_pane))
		return TRUE;

	if (priv->update_layout_timeout != 0) {
		g_source_remove (priv->update_layout_timeout);
		priv->update_layout_timeout = 0;
	}

	sidebar_visible = priv->sidebar_visible;
	if (! sidebar_visible) 
		gth_browser_show_sidebar (browser);

	priv->layout_type = eel_gconf_get_integer (PREF_UI_LAYOUT, 2);

	paned1_pos = gtk_paned_get_position (GTK_PANED (priv->main_pane));
	paned2_pos = gtk_paned_get_position (GTK_PANED (priv->content_pane));

	if (priv->layout_type == 1) {
		paned1 = gtk_vpaned_new (); 
		paned2 = gtk_hpaned_new ();
	} else {
		paned1 = gtk_hpaned_new (); 
		paned2 = gtk_vpaned_new (); 
	}

	if (priv->layout_type == 3)
		gtk_paned_pack2 (GTK_PANED (paned1), paned2, TRUE, FALSE);
	else
		gtk_paned_pack1 (GTK_PANED (paned1), paned2, FALSE, FALSE);

	if (priv->layout_type == 3)
		gtk_widget_reparent (priv->dir_list_pane, paned1);
	else
		gtk_widget_reparent (priv->dir_list_pane, paned2);

	if (priv->layout_type == 2) 
		gtk_widget_reparent (priv->file_list_pane, paned1);
	else 
		gtk_widget_reparent (priv->file_list_pane, paned2);

	if (priv->layout_type <= 1) 
		gtk_widget_reparent (priv->image_pane, paned1);
	else 
		gtk_widget_reparent (priv->image_pane, paned2);

	gtk_paned_set_position (GTK_PANED (paned1), paned1_pos);
	gtk_paned_set_position (GTK_PANED (paned2), paned2_pos);

	gnome_app_set_contents (GNOME_APP (browser), paned1);

	gtk_widget_show (paned1);
	gtk_widget_show (paned2);

	priv->main_pane = paned1;
	priv->content_pane = paned2;

	if (! sidebar_visible) 
		gth_browser_hide_sidebar (browser);

	return FALSE;
}



void
gth_browser_notify_update_layout (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->update_layout_timeout != 0) {
		g_source_remove (priv->update_layout_timeout);
		priv->update_layout_timeout = 0;
	}

	priv->update_layout_timeout = g_timeout_add (UPDATE_LAYOUT_DELAY, 
						       gth_browser_notify_update_layout_cb, 
						       browser);
}


void
gth_browser_notify_update_toolbar_style (GthBrowser *browser)
{
	GthToolbarStyle toolbar_style;
	GtkToolbarStyle prop = GTK_TOOLBAR_BOTH;

	toolbar_style = pref_get_real_toolbar_style ();

	switch (toolbar_style) {
	case GTH_TOOLBAR_STYLE_TEXT_BELOW:
		prop = GTK_TOOLBAR_BOTH; break;
	case GTH_TOOLBAR_STYLE_TEXT_BESIDE:
		prop = GTK_TOOLBAR_BOTH_HORIZ; break;
	case GTH_TOOLBAR_STYLE_ICONS:
		prop = GTK_TOOLBAR_ICONS; break;
	case GTH_TOOLBAR_STYLE_TEXT:
		prop = GTK_TOOLBAR_TEXT; break;
	default:
		break;
	}

	gtk_toolbar_set_style (GTK_TOOLBAR (browser->priv->toolbar), prop);
}


void
gth_browser_notify_update_icon_theme (GthBrowser *browser)
{
	GthBrowserPrivateData *priv = browser->priv;

	gth_file_view_update_icon_theme (priv->file_list->view);
	dir_list_update_icon_theme (priv->dir_list);

	gth_browser_update_bookmark_list (browser);
	window_update_history_list (browser);
	gth_browser_update_file_list (browser);

	if (priv->bookmarks_dlg != NULL)
		dlg_edit_bookmarks_update (priv->bookmarks_dlg);
}


/* -- image operations -- */


static void
pixbuf_op_done_cb (GthPixbufOp *pixop,
		   gboolean     completed,
		   GthBrowser  *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	ImageViewer *viewer = IMAGE_VIEWER (priv->viewer);

	if (completed) {
		image_viewer_set_pixbuf (viewer, priv->pixop->dest);
		gth_window_set_image_modified (GTH_WINDOW (browser), TRUE);
	}

	g_object_unref (priv->pixop);
	priv->pixop = NULL;

	if (priv->progress_dialog != NULL) 
		gtk_widget_hide (priv->progress_dialog);
}


static void
pixbuf_op_progress_cb (GthPixbufOp *pixop,
		       float        p, 
		       GthBrowser  *browser)
{
	GthBrowserPrivateData *priv = browser->priv;
	if (priv->progress_dialog != NULL) 
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress_progressbar), p);
}


static int
window__display_progress_dialog (gpointer data)
{
	GthBrowser *browser = data;
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->progress_timeout != 0) {
		g_source_remove (priv->progress_timeout);
		priv->progress_timeout = 0;
	}

	if (priv->pixop != NULL)
		gtk_widget_show (priv->progress_dialog);

	return FALSE;
}


static void
gth_browser_exec_pixbuf_op (GthWindow   *window,
			    GthPixbufOp *pixop)
{
	GthBrowser            *browser = GTH_BROWSER (window);
	GthBrowserPrivateData *priv = browser->priv;

	priv->pixop = pixop;
	g_object_ref (priv->pixop);

	gtk_label_set_text (GTK_LABEL (priv->progress_info),
			    _("Wait please..."));

	g_signal_connect (G_OBJECT (pixop),
			  "pixbuf_op_done",
			  G_CALLBACK (pixbuf_op_done_cb),
			  browser);
	g_signal_connect (G_OBJECT (pixop),
			  "pixbuf_op_progress",
			  G_CALLBACK (pixbuf_op_progress_cb),
			  browser);
	
	if (priv->progress_dialog != NULL)
		priv->progress_timeout = g_timeout_add (DISPLAY_PROGRESS_DELAY, window__display_progress_dialog, browser);

	gth_pixbuf_op_start (priv->pixop);
}


static void
gth_browser_set_categories_dlg (GthWindow *window,
				GtkWidget *dialog)
{
	GthBrowser *browser = GTH_BROWSER (window);
	browser->priv->categories_dlg = dialog;
}


static GtkWidget *
gth_browser_get_categories_dlg (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	return browser->priv->categories_dlg;
}


static void
gth_browser_set_comment_dlg (GthWindow *window,
			     GtkWidget *dialog)
{
	GthBrowser *browser = GTH_BROWSER (window);
	browser->priv->comment_dlg = dialog;
}


static GtkWidget *
gth_browser_get_comment_dlg (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	return browser->priv->comment_dlg;
}


static void
gth_browser_reload_current_image (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	gth_browser_reload_image (browser);
}


static void
gth_browser_update_current_image_metadata (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);

	if (browser->priv->image_path == NULL)
		return;
	gth_browser_notify_update_comment (browser, browser->priv->image_path);
}


static GList *
gth_browser_get_file_list_selection (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	return gth_file_list_get_selection (browser->priv->file_list);
}


static GList *
gth_browser_get_file_list_selection_as_fd (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	return gth_file_list_get_selection_as_fd (browser->priv->file_list);
}


static void
gth_browser_set_animation (GthWindow *window,
			   gboolean   play)
{
	GthBrowser  *browser = GTH_BROWSER (window);
	ImageViewer *viewer = IMAGE_VIEWER (browser->priv->viewer);

	set_action_active (browser, "View_PlayAnimation", play);
	set_action_sensitive (browser, "View_StepAnimation", ! play);
	if (play)
		image_viewer_start_animation (viewer);
	else
		image_viewer_stop_animation (viewer);
}


static gboolean       
gth_browser_get_animation (GthWindow *window)
{
	GthBrowser  *browser = GTH_BROWSER (window);
	ImageViewer *viewer = IMAGE_VIEWER (browser->priv->viewer);
	return viewer->play_animation;
}


static void           
gth_browser_step_animation (GthWindow *window)
{
	GthBrowser  *browser = GTH_BROWSER (window);
	ImageViewer *viewer = IMAGE_VIEWER (browser->priv->viewer);
	image_viewer_step_animation (viewer);
}


static void           
gth_browser_delete_image (GthWindow *window)
{
}


static void           
gth_browser_edit_comment (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->comment_dlg == NULL) 
		priv->comment_dlg = dlg_comment_new (GTH_WINDOW (browser));
	else
		gtk_window_present (GTK_WINDOW (priv->comment_dlg));
}


static void           
gth_browser_edit_categories (GthWindow *window)
{
	GthBrowser *browser = GTH_BROWSER (window);
	GthBrowserPrivateData *priv = browser->priv;

	if (priv->categories_dlg == NULL) 
		priv->categories_dlg = dlg_categories_new (GTH_WINDOW (browser));
	else
		gtk_window_present (GTK_WINDOW (priv->categories_dlg));
}


static void           
gth_browser_set_fullscreen (GthWindow *window,
			    gboolean   value)
{
}


static gboolean       
gth_browser_get_fullscreen (GthWindow *window)
{
	return FALSE;
}


static void           
gth_browser_set_slideshow (GthWindow *window,
			   gboolean   value)
{
}


static gboolean       
gth_browser_get_slideshow (GthWindow *window)
{
	return FALSE;
}


static void
gth_browser_class_init (GthBrowserClass *class)
{
	GObjectClass   *gobject_class;
	GtkWidgetClass *widget_class;
	GthWindowClass *window_class;

	parent_class = g_type_class_peek_parent (class);
	gobject_class = (GObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	window_class = (GthWindowClass*) class;

	gobject_class->finalize = gth_browser_finalize;

	widget_class->show = gth_browser_show;

	window_class->close = gth_browser_close;
	window_class->get_image_viewer = gth_browser_get_image_viewer;
	window_class->get_image_filename = gth_browser_get_image_filename;
	window_class->get_image_modified = gth_browser_get_image_modified;
	window_class->set_image_modified = gth_browser_set_image_modified;
	window_class->save_pixbuf = gth_browser_save_pixbuf;
	window_class->exec_pixbuf_op = gth_browser_exec_pixbuf_op;

	window_class->set_categories_dlg = gth_browser_set_categories_dlg;
	window_class->get_categories_dlg = gth_browser_get_categories_dlg;
	window_class->set_comment_dlg = gth_browser_set_comment_dlg;
	window_class->get_comment_dlg = gth_browser_get_comment_dlg;
	window_class->reload_current_image = gth_browser_reload_current_image;
	window_class->update_current_image_metadata = gth_browser_update_current_image_metadata;
	window_class->get_file_list_selection = gth_browser_get_file_list_selection;
	window_class->get_file_list_selection_as_fd = gth_browser_get_file_list_selection_as_fd;

	window_class->set_animation = gth_browser_set_animation;
	window_class->get_animation = gth_browser_get_animation;
	window_class->step_animation = gth_browser_step_animation;
	window_class->delete_image = gth_browser_delete_image;
	window_class->edit_comment = gth_browser_edit_comment;
	window_class->edit_categories = gth_browser_edit_categories;
	window_class->set_fullscreen = gth_browser_set_fullscreen;
	window_class->get_fullscreen = gth_browser_get_fullscreen;
	window_class->set_slideshow = gth_browser_set_slideshow;
	window_class->get_slideshow = gth_browser_get_slideshow;
}


GType
gth_browser_get_type (void)
{
        static GType type = 0;

        if (! type) {
                GTypeInfo type_info = {
			sizeof (GthBrowserClass),
			NULL,
			NULL,
			(GClassInitFunc) gth_browser_class_init,
			NULL,
			NULL,
			sizeof (GthBrowser),
			0,
			(GInstanceInitFunc) gth_browser_init
		};

		type = g_type_register_static (GTH_TYPE_WINDOW,
					       "GthBrowser",
					       &type_info,
					       0);
	}

        return type;
}


void
gth_browser_set_bookmarks_dlg (GthBrowser *browser,
			       GtkWidget  *dialog)
{
	browser->priv->bookmarks_dlg = dialog;
}


GtkWidget *
gth_browser_get_bookmarks_dlg (GthBrowser *browser)
{
	return browser->priv->bookmarks_dlg;
}


GthFileList*
gth_browser_get_file_list (GthBrowser *browser)
{
	return browser->priv->file_list;
}


GthFileView*
gth_browser_get_file_view (GthBrowser *browser)
{
	return browser->priv->file_list->view;
}


DirList*
gth_browser_get_dir_list (GthBrowser *browser)
{
	return browser->priv->dir_list;
}


CatalogList*
gth_browser_get_catalog_list (GthBrowser *browser)
{
	return browser->priv->catalog_list;
}
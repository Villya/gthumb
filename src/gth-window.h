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

#ifndef GTH_WINDOW_H
#define GTH_WINDOW_H

#include <libgnomeui/gnome-app.h>
#include "gth-pixbuf-op.h"
#include "image-viewer.h"


#define GTH_TYPE_WINDOW              (gth_window_get_type ())
#define GTH_WINDOW(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTH_TYPE_WINDOW, GthWindow))
#define GTH_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTH_WINDOW_TYPE, GthWindowClass))
#define GTH_IS_WINDOW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTH_TYPE_WINDOW))
#define GTH_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTH_TYPE_WINDOW))
#define GTH_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GTH_TYPE_WINDOW, GthWindowClass))

typedef struct _GthWindow            GthWindow;
typedef struct _GthWindowClass       GthWindowClass;
typedef struct _GthWindowPrivateData GthWindowPrivateData;

struct _GthWindow
{
	GnomeApp __parent;
	GthWindowPrivateData *priv;
};

struct _GthWindowClass
{
	GnomeAppClass __parent_class;

	/*<virtual functions>*/

	void          (*close)                         (GthWindow *window);
	ImageViewer * (*get_image_viewer)              (GthWindow   *window);
	const char *  (*get_image_filename)            (GthWindow   *window);
	void          (*set_image_modified)            (GthWindow   *window,
							gboolean     value);
	gboolean      (*get_image_modified)            (GthWindow   *window);
	void          (*save_pixbuf)                   (GthWindow   *window,
							GdkPixbuf   *pixbuf);
	void          (*exec_pixbuf_op)                (GthWindow   *window,
							GthPixbufOp *pixop);
	void          (*set_categories_dlg)            (GthWindow   *window,
							GtkWidget   *dialog);
	GtkWidget *   (*get_categories_dlg)            (GthWindow   *window);
	void          (*set_comment_dlg)               (GthWindow   *window,
							GtkWidget   *dialog);
	GtkWidget *   (*get_comment_dlg)               (GthWindow   *window);
	void          (*reload_current_image)          (GthWindow   *window);
	void          (*update_current_image_metadata) (GthWindow   *window);
	GList *       (*get_file_list_selection)       (GthWindow   *window);
	GList *       (*get_file_list_selection_as_fd) (GthWindow   *window);
	void          (*set_animation)                 (GthWindow   *window,
							gboolean     value);
	gboolean      (*get_animation)                 (GthWindow   *window);
	void          (*step_animation)                (GthWindow   *window);
	void          (*delete_image)                  (GthWindow   *window);
	void          (*edit_comment)                  (GthWindow   *window);
	void          (*edit_categories)               (GthWindow   *window);
	void          (*set_fullscreen)                (GthWindow   *window,
							gboolean     value);
	gboolean      (*get_fullscreen)                (GthWindow   *window);
	void          (*set_slideshow)                 (GthWindow   *window,
							gboolean     value);
	gboolean      (*get_slideshow)                 (GthWindow   *window);
};

GType          gth_window_get_type                       (void);
void           gth_window_close                          (GthWindow   *window);
ImageViewer *  gth_window_get_image_viewer               (GthWindow   *window);
const char *   gth_window_get_image_filename             (GthWindow   *window);
void           gth_window_set_image_modified             (GthWindow   *window,
							  gboolean     value);
gboolean       gth_window_get_image_modified             (GthWindow   *window);
void           gth_window_save_pixbuf                    (GthWindow   *window,
							  GdkPixbuf   *pixbuf);
void           gth_window_exec_pixbuf_op                 (GthWindow   *window,
							  GthPixbufOp *pixop);
void           gth_window_set_categories_dlg             (GthWindow   *window,
							  GtkWidget   *dialog);
GtkWidget *    gth_window_get_categories_dlg             (GthWindow   *window);
void           gth_window_set_comment_dlg                (GthWindow   *window,
							  GtkWidget   *dialog);
GtkWidget *    gth_window_get_comment_dlg                (GthWindow   *window);
void           gth_window_reload_current_image           (GthWindow   *window);
void           gth_window_update_current_image_metadata  (GthWindow   *window);
GList *        gth_window_get_file_list_selection        (GthWindow   *window);
GList *        gth_window_get_file_list_selection_as_fd  (GthWindow   *window);

void           gth_window_set_animation                  (GthWindow   *window,
							  gboolean     value);
gboolean       gth_window_get_animation                  (GthWindow   *window);
void           gth_window_step_animation                 (GthWindow   *window);

void           gth_window_delete_image                   (GthWindow   *window);
void           gth_window_edit_comment                   (GthWindow   *window);
void           gth_window_edit_categories                (GthWindow   *window);

void           gth_window_set_fullscreen                 (GthWindow   *window,
							  gboolean     value);
gboolean       gth_window_get_fullscreen                 (GthWindow   *window);

void           gth_window_set_slideshow                  (GthWindow   *window,
							  gboolean     value);
gboolean       gth_window_get_slideshow                  (GthWindow   *window);

/**/

int            gth_window_get_n_windows                  (void);

#endif /* GTH_WINDOW_H */
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2006 Free Software Foundation, Inc.
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gth-filter-bar.h"

enum {
        CHANGED,
        LAST_SIGNAL
};

struct _GthFilterBarPrivate
{
	GtkWidget *text_entry;
};


static GtkHBoxClass *parent_class = NULL;
static guint gth_filter_bar_signals[LAST_SIGNAL] = { 0 };


static void
gth_filter_bar_finalize (GObject *object)
{
	GthFilterBar *filter_bar;

	filter_bar = GTH_FILTER_BAR (object);

	if (filter_bar->priv != NULL) {
		g_free (filter_bar->priv);
		filter_bar->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gth_filter_bar_class_init (GthFilterBarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GObjectClass*) class;

	object_class->finalize = gth_filter_bar_finalize;

	gth_filter_bar_signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GthFilterBarClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1,
			      G_TYPE_STRING);
}


static void
gth_filter_bar_init (GthFilterBar *filter_bar)
{
	filter_bar->priv = g_new0 (GthFilterBarPrivate, 1);
}


static void
gth_filter_bar_construct (GthFilterBar *filter_bar)
{
	gtk_container_set_border_width (GTK_CONTAINER (filter_bar), 6);

	filter_bar->priv->text_entry = gtk_entry_new ();
	gtk_widget_set_size_request (filter_bar->priv->text_entry, 150, -1);
	gtk_widget_show (filter_bar->priv->text_entry);
	gtk_box_pack_end (GTK_BOX (filter_bar), filter_bar->priv->text_entry, FALSE, FALSE, 0);
}


GType
gth_filter_bar_get_type ()
{
        static GType type = 0;

        if (! type) {
                GTypeInfo type_info = {
			sizeof (GthFilterBarClass),
			NULL,
			NULL,
			(GClassInitFunc) gth_filter_bar_class_init,
			NULL,
			NULL,
			sizeof (GthFilterBar),
			0,
			(GInstanceInitFunc) gth_filter_bar_init
		};

		type = g_type_register_static (GTK_TYPE_HBOX,
					       "GthFilterBar",
					       &type_info,
					       0);
	}

        return type;
}


GtkWidget*
gth_filter_bar_new (void)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (g_object_new (GTH_TYPE_FILTER_BAR, NULL));
	gth_filter_bar_construct (GTH_FILTER_BAR (widget));

	return widget;
}
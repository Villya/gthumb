/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2009 The Free Software Foundation, Inc.
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
#include <gtk/gtk.h>
#include <gthumb.h>
#include "gth-script-editor-dialog.h"


#define GET_WIDGET(name) _gtk_builder_get_widget (self->priv->builder, (name))


static gpointer parent_class = NULL;


struct _GthScriptEditorDialogPrivate {
	GtkBuilder *builder;
  	char       *script_id;
  	gboolean    script_visible;
  	gboolean    wait_command;
  	gboolean    shell_script;
  	gboolean    for_each_file;
  	gboolean    help_visible;
};


static void
gth_script_editor_dialog_finalize (GObject *object)
{
	GthScriptEditorDialog *dialog;

	dialog = GTH_SCRIPT_EDITOR_DIALOG (object);

	if (dialog->priv != NULL) {
		g_object_unref (dialog->priv->builder);
		g_free (dialog->priv->script_id);
		g_free (dialog->priv);
		dialog->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gth_script_editor_dialog_class_init (GthScriptEditorDialogClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GObjectClass*) class;

	object_class->finalize = gth_script_editor_dialog_finalize;
}


static void
gth_script_editor_dialog_init (GthScriptEditorDialog *dialog)
{
	dialog->priv = g_new0 (GthScriptEditorDialogPrivate, 1);
	dialog->priv->help_visible = FALSE;
}


GType
gth_script_editor_dialog_get_type (void)
{
        static GType type = 0;

        if (! type) {
                GTypeInfo type_info = {
			sizeof (GthScriptEditorDialogClass),
			NULL,
			NULL,
			(GClassInitFunc) gth_script_editor_dialog_class_init,
			NULL,
			NULL,
			sizeof (GthScriptEditorDialog),
			0,
			(GInstanceInitFunc) gth_script_editor_dialog_init
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "GthScriptEditorDialog",
					       &type_info,
					       0);
	}

        return type;
}


static void
update_sensitivity (GthScriptEditorDialog *self)
{
	/* FIXME */
}


static void
command_entry_icon_press_cb (GtkEntry             *entry,
                             GtkEntryIconPosition  icon_pos,
                             GdkEvent             *event,
                             gpointer              user_data)
{
	GthScriptEditorDialog *self = user_data;

	self->priv->help_visible = ! self->priv->help_visible;

	if (self->priv->help_visible)
		gtk_widget_show (GET_WIDGET("command_help_box"));
	else
		gtk_widget_hide (GET_WIDGET("command_help_box"));
}


static void
gth_script_editor_dialog_construct (GthScriptEditorDialog *self,
				    const char            *title,
			            GtkWindow             *parent)
{
	GtkWidget *content;

	if (title != NULL)
    		gtk_window_set_title (GTK_WINDOW (self), title);
  	if (parent != NULL)
    		gtk_window_set_transient_for (GTK_WINDOW (self), parent);
    	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), 5);
	gtk_container_set_border_width (GTK_CONTAINER (self), 5);

	gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_SAVE, GTK_RESPONSE_OK);
	gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_HELP, GTK_RESPONSE_HELP);

    	self->priv->builder = _gtk_builder_new_from_file ("script-editor.ui", "list_tools");

    	content = _gtk_builder_get_widget (self->priv->builder, "script_editor");
    	gtk_container_set_border_width (GTK_CONTAINER (content), 5);
  	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), content, TRUE, TRUE, 0);

  	g_signal_connect (GET_WIDGET ("command_entry"),
  			  "icon-press",
  			  G_CALLBACK (command_entry_icon_press_cb),
  			  self);

  	/**/

  	gth_script_editor_dialog_set_script (self, NULL);
}


GtkWidget *
gth_script_editor_dialog_new (const char *title,
			      GtkWindow  *parent)
{
	GthScriptEditorDialog *self;

	self = g_object_new (GTH_TYPE_SCRIPT_EDITOR_DIALOG, NULL);
	gth_script_editor_dialog_construct (self, title, parent);

	return (GtkWidget *) self;
}


static void
_gth_script_editor_dialog_set_new_script (GthScriptEditorDialog *self)
{
	gtk_entry_set_text (GTK_ENTRY (GET_WIDGET ("name_entry")), "");
	gtk_entry_set_text (GTK_ENTRY (GET_WIDGET ("command_entry")), "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("shell_script_checkbutton")), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("for_each_file_checkbutton")), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("wait_command_checkbutton")), FALSE);
}


void
gth_script_editor_dialog_set_script (GthScriptEditorDialog *self,
				     GthScript             *script)
{
	g_free (self->priv->script_id);
	self->priv->script_id = NULL;
	self->priv->script_visible = TRUE;

	_gth_script_editor_dialog_set_new_script (self);

	if (script != NULL) {
		self->priv->script_id = g_strdup (gth_script_get_id (script));
		self->priv->script_visible = gth_script_is_visible (script);

		gtk_entry_set_text (GTK_ENTRY (GET_WIDGET ("name_entry")), gth_script_get_display_name (script));
		gtk_entry_set_text (GTK_ENTRY (GET_WIDGET ("command_entry")), gth_script_get_command (script));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("shell_script_checkbutton")), gth_script_is_shell_script (script));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("for_each_file_checkbutton")), gth_script_for_each_file (script));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("wait_command_checkbutton")), gth_script_wait_command (script));
	}

	update_sensitivity (self);
}


GthScript *
gth_script_editor_dialog_get_script (GthScriptEditorDialog  *self,
				     GError                **error)
{
	GthScript *script;

	script = gth_script_new ();
	if (self->priv->script_id != NULL)
		g_object_set (script, "id", self->priv->script_id, NULL);
	g_object_set (script,
		      "display-name", gtk_entry_get_text (GTK_ENTRY (GET_WIDGET ("name_entry"))),
		      "command", gtk_entry_get_text (GTK_ENTRY (GET_WIDGET ("command_entry"))),
		      "visible", self->priv->script_visible,
		      "shell-script", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("shell_script_checkbutton"))),
		      "for-each-file", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("for_each_file_checkbutton"))),
		      "wait-command", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (GET_WIDGET ("wait_command_checkbutton"))),
		      NULL);

	if (g_strcmp0 (gth_script_get_display_name (script), "") == 0) {
		*error = g_error_new (GTH_ERROR, 0, _("No name specified"));
		g_object_unref (script);
		return NULL;
	}

	if (g_strcmp0 (gth_script_get_command (script), "") == 0) {
		*error = g_error_new (GTH_ERROR, 0, _("No command specified"));
		g_object_unref (script);
		return NULL;
	}

	return script;
}

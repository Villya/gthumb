/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2008 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "gth-test-category.h"


typedef struct {
	char      *name;
	GthTestOp  op;
	gboolean   negative;
} GthOpData;


GthOpData category_op_data[] = {
	{ N_("is"), GTH_TEST_OP_EQUAL, FALSE },
	{ N_("is not"), GTH_TEST_OP_EQUAL, TRUE }
};


struct _GthTestCategoryPrivate
{
	char      *category;
	GthTestOp  op;
	gboolean   negative;
	gboolean   has_focus;
	GtkWidget *text_entry;
	GtkWidget *op_combo_box;
};


static gpointer parent_class = NULL;
static DomDomizableIface *dom_domizable_parent_iface = NULL;
static GthDuplicableIface *gth_duplicable_parent_iface = NULL;


static void
gth_test_category_finalize (GObject *object)
{
	GthTestCategory *test;

	test = GTH_TEST_CATEGORY (object);

	if (test->priv != NULL) {
		g_free (test->priv->category);
		g_free (test->priv);
		test->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static gboolean
text_entry_focus_in_event_cb (GtkEntry        *entry,
			      GdkEventFocus   *event,
                              GthTestCategory *test)
{
	test->priv->has_focus = TRUE;
	return FALSE;
}


static gboolean
text_entry_focus_out_event_cb (GtkEntry        *entry,
			       GdkEventFocus   *event,
                               GthTestCategory *test)
{
	test->priv->has_focus = FALSE;
	return FALSE;
}


static void
text_entry_activate_cb (GtkEntry        *entry,
                        GthTestCategory *test)
{
	gth_test_update_from_control (GTH_TEST (test), NULL);
	gth_test_changed (GTH_TEST (test));
}


static void
op_combo_box_changed_cb (GtkComboBox     *combo_box,
                         GthTestCategory *test)
{
	gth_test_update_from_control (GTH_TEST (test), NULL);
	gth_test_changed (GTH_TEST (test));
}


static GtkWidget *
gth_test_category_real_create_control (GthTest *base)
{
	GthTestCategory *test;
	GtkWidget       *control;
	int              i, op_idx;

	test = (GthTestCategory *) base;

	control = gtk_hbox_new (FALSE, 6);

	/* text operation combo box */

	test->priv->op_combo_box = gtk_combo_box_new_text ();
	gtk_widget_show (test->priv->op_combo_box);

	op_idx = 0;
	for (i = 0; i < G_N_ELEMENTS (category_op_data); i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (test->priv->op_combo_box), _(category_op_data[i].name));
		if ((category_op_data[i].op == test->priv->op) && (category_op_data[i].negative == test->priv->negative))
			op_idx = i;
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (test->priv->op_combo_box), op_idx);

	g_signal_connect (G_OBJECT (test->priv->op_combo_box),
			  "changed",
			  G_CALLBACK (op_combo_box_changed_cb),
			  test);

	/* text entry */

	test->priv->text_entry = gtk_entry_new ();
	/*gtk_entry_set_width_chars (GTK_ENTRY (test->priv->text_entry), 6);*/
	if (test->priv->category != NULL)
		gtk_entry_set_text (GTK_ENTRY (test->priv->text_entry), test->priv->category);
	gtk_widget_show (test->priv->text_entry);

	g_signal_connect (G_OBJECT (test->priv->text_entry),
			  "activate",
			  G_CALLBACK (text_entry_activate_cb),
			  test);
	g_signal_connect (G_OBJECT (test->priv->text_entry),
			  "focus-in-event",
			  G_CALLBACK (text_entry_focus_in_event_cb),
			  test);
	g_signal_connect (G_OBJECT (test->priv->text_entry),
			  "focus-out-event",
			  G_CALLBACK (text_entry_focus_out_event_cb),
			  test);

	/**/

	gtk_box_pack_start (GTK_BOX (control), test->priv->op_combo_box, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (control), test->priv->text_entry, FALSE, FALSE, 0);

	return control;
}


static GthMatch
gth_test_category_real_match (GthTest     *test,
			      GthFileData *file)
{
	GthTestCategory *test_category;
	gboolean         result = FALSE;

        test_category = GTH_TEST_CATEGORY (test);

	if (test_category->priv->category != NULL) {
		GthStringList *string_list;
		GList         *list, *scan;
		char          *test_category_casefolded;

		string_list = (GthStringList *) g_file_info_get_attribute_object (file->info, "comment::categories");
		if (string_list != NULL)
			list = gth_string_list_get_list (string_list);
		else
			list = NULL;

		test_category_casefolded = g_utf8_casefold (test_category->priv->category, -1);
		for (scan = list; ! result && scan; scan = scan->next) {
			char *category;

			category = g_utf8_casefold (scan->data, -1);
			if (g_utf8_collate (category, test_category_casefolded) == 0)
				result = TRUE;

			g_free (category);
		}

		g_free (test_category_casefolded);
	}

        if (test_category->priv->negative)
		result = ! result;

	return result ? GTH_MATCH_YES : GTH_MATCH_NO;
}


static DomElement*
gth_test_category_real_create_element (DomDomizable *base,
				       DomDocument  *doc)
{
	GthTestCategory *self;
	DomElement      *element;

	g_return_val_if_fail (DOM_IS_DOCUMENT (doc), NULL);

	self = GTH_TEST_CATEGORY (base);

	element = dom_document_create_element (doc, "test",
					       "id", gth_test_get_id (GTH_TEST (self)),
					       NULL);

	if (! gth_test_is_visible (GTH_TEST (self)))
		dom_element_set_attribute (element, "display", "none");

	dom_element_set_attribute (element, "op", _g_enum_type_get_value (GTH_TYPE_TEST_OP, self->priv->op)->value_nick);
	if (self->priv->negative)
		dom_element_set_attribute (element, "negative", self->priv->negative ? "true" : "false");
	if (self->priv->category != NULL)
		dom_element_set_attribute (element, "value", self->priv->category);

	return element;
}


static void
gth_test_category_set_category (GthTestCategory *self,
				const char      *category)
{
	g_free (self->priv->category);
	self->priv->category = NULL;
	if (category != NULL)
		self->priv->category = g_strdup (category);
}


static void
gth_test_category_real_load_from_element (DomDomizable *base,
					  DomElement   *element)
{
	GthTestCategory *self;
	const char    *value;

	g_return_if_fail (DOM_IS_ELEMENT (element));

	self = GTH_TEST_CATEGORY (base);

	g_object_set (self, "visible", (g_strcmp0 (dom_element_get_attribute (element, "display"), "none") != 0), NULL);

	value = dom_element_get_attribute (element, "op");
	if (value != NULL)
		self->priv->op = _g_enum_type_get_value_by_nick (GTH_TYPE_TEST_OP, value)->value;

	self->priv->negative = g_strcmp0 (dom_element_get_attribute (element, "negative"), "true") == 0;

	gth_test_category_set_category (self, NULL);
	value = dom_element_get_attribute (element, "value");
	if (value != NULL)
		gth_test_category_set_category (self, value);
}


static gboolean
gth_test_category_real_update_from_control (GthTest  *base,
			                    GError  **error)
{
	GthTestCategory *self;
	GthOpData        op_data;

	self = GTH_TEST_CATEGORY (base);

	op_data = category_op_data[gtk_combo_box_get_active (GTK_COMBO_BOX (self->priv->op_combo_box))];
	self->priv->op = op_data.op;
	self->priv->negative = op_data.negative;

	gth_test_category_set_category (self, gtk_entry_get_text (GTK_ENTRY (self->priv->text_entry)));
	if (g_strcmp0 (self->priv->category, "") == 0) {
		if (error != NULL)
			*error = g_error_new (GTH_TEST_ERROR, 0, _("The test definition is incomplete"));
		return FALSE;
	}

	return TRUE;
}


static GObject *
gth_test_category_real_duplicate (GthDuplicable *duplicable)
{
	GthTestCategory *test = GTH_TEST_CATEGORY (duplicable);
	GthTestCategory *new_test;

	new_test = g_object_new (GTH_TYPE_TEST_CATEGORY,
				 "id", gth_test_get_id (GTH_TEST (test)),
				 "display-name", gth_test_get_display_name (GTH_TEST (test)),
				 "visible", gth_test_is_visible (GTH_TEST (test)),
				 NULL);
	new_test->priv->op = test->priv->op;
	new_test->priv->negative = test->priv->negative;
	gth_test_category_set_category (new_test, test->priv->category);

	return (GObject *) new_test;
}


static void
gth_test_category_class_init (GthTestCategoryClass *class)
{
	GObjectClass *object_class;
	GthTestClass *test_class;

	parent_class = g_type_class_peek_parent (class);
	object_class = (GObjectClass*) class;
	test_class = (GthTestClass *) class;

	object_class->finalize = gth_test_category_finalize;
	test_class->create_control = gth_test_category_real_create_control;
	test_class->update_from_control = gth_test_category_real_update_from_control;
	test_class->match = gth_test_category_real_match;
}


static void
gth_test_category_dom_domizable_interface_init (DomDomizableIface * iface)
{
	dom_domizable_parent_iface = g_type_interface_peek_parent (iface);
	iface->create_element = gth_test_category_real_create_element;
	iface->load_from_element = gth_test_category_real_load_from_element;
}


static void
gth_test_category_gth_duplicable_interface_init (GthDuplicableIface *iface)
{
	gth_duplicable_parent_iface = g_type_interface_peek_parent (iface);
	iface->duplicate = gth_test_category_real_duplicate;
}


static void
gth_test_category_init (GthTestCategory *test)
{
	test->priv = g_new0 (GthTestCategoryPrivate, 1);
	g_object_set (test, "attributes", "comment::categories", NULL);
}


GType
gth_test_category_get_type (void)
{
        static GType type = 0;

        if (! type) {
                GTypeInfo type_info = {
			sizeof (GthTestCategoryClass),
			NULL,
			NULL,
			(GClassInitFunc) gth_test_category_class_init,
			NULL,
			NULL,
			sizeof (GthTestCategory),
			0,
			(GInstanceInitFunc) gth_test_category_init
		};
		static const GInterfaceInfo dom_domizable_info = {
			(GInterfaceInitFunc) gth_test_category_dom_domizable_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		static const GInterfaceInfo gth_duplicable_info = {
			(GInterfaceInitFunc) gth_test_category_gth_duplicable_interface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		type = g_type_register_static (GTH_TYPE_TEST,
					       "GthTestCategory",
					       &type_info,
					       0);
		g_type_add_interface_static (type, DOM_TYPE_DOMIZABLE, &dom_domizable_info);
		g_type_add_interface_static (type, GTH_TYPE_DUPLICABLE, &gth_duplicable_info);
	}

        return type;
}

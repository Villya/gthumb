/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2009 Free Software Foundation, Inc.
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

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gthumb.h>
#include <extensions/catalogs/gth-catalog.h>

void ss__gth_browser_construct_cb          (GthBrowser  *browser);
void ss__gth_browser_update_sensitivity_cb (GthBrowser  *browser);
void ss__slideshow_cb                      (GthBrowser  *browser);
void ss__gth_catalog_read_metadata         (GthCatalog  *catalog,
					    GthFileData *file_data);
void ss__gth_catalog_write_metadata        (GthCatalog  *catalog,
					    GthFileData *file_data);
void ss__gth_catalog_read_from_doc         (GthCatalog  *catalog,
					    DomElement  *root);
void ss__gth_catalog_write_to_doc          (GthCatalog  *catalog,
					    DomDocument *doc,
					    DomElement  *root);
void ss__dlg_catalog_properties            (GtkBuilder  *builder,
					    GthFileData *file_data,
					    GthCatalog  *catalog);
void ss__dlg_catalog_properties_save       (GtkBuilder  *builder,
					    GthFileData *file_data,
					    GthCatalog  *catalog);

#endif /* CALLBACKS_H */

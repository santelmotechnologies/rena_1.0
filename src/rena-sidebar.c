/*****************************************************************************/
/* Copyright (C) 2024 Santelmo Technologies <santelmotechnologies@gmail.com> */
/*                                                                           */
/* This program is free software: you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation, either version 3 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>.     */
/*****************************************************************************/

#include "rena-sidebar.h"
#include "rena-hig.h"

struct _RenaSidebar {
	GtkBox     __parent__;

	GtkWidget *container;
	GtkWidget *header;
	GtkWidget *menu_button;
	GtkWidget *close_button;
	GtkWidget *title_box;

	GtkWidget *popover;
};

G_DEFINE_TYPE(RenaSidebar, rena_sidebar, GTK_TYPE_BOX)

enum {
	SIGNAL_CHILDREN_CHANGED,
	LAST_SIGNAL
};
static int signals[LAST_SIGNAL] = { 0 };


/*
 * Public Api.
 */

void
rena_sidebar_attach_plugin (RenaSidebar *sidebar,
                              GtkWidget     *widget,
                              GtkWidget     *title,
                              GtkWidget     *popover)
{
	if (!widget || !title)
		return;

	gtk_notebook_insert_page (GTK_NOTEBOOK(sidebar->container),
	                          widget,
	                          NULL,
	                          0);

	gtk_container_add (GTK_CONTAINER(sidebar->title_box), title);

	if (popover) {
		gtk_popover_set_relative_to (GTK_POPOVER(popover), sidebar->menu_button);
		sidebar->popover = popover;
	}
	gtk_widget_show_all (title);

	g_signal_emit (sidebar, signals[SIGNAL_CHILDREN_CHANGED], 0);
}

void
rena_sidebar_remove_plugin (RenaSidebar *sidebar,
                              GtkWidget     *widget)
{
	GList *list;
	GtkWidget *children;
	gint page;

	page = gtk_notebook_page_num (GTK_NOTEBOOK(sidebar->container), widget);

	if (page >= 0) {
		gtk_notebook_remove_page (GTK_NOTEBOOK(sidebar->container), page);
		gtk_popover_set_relative_to (GTK_POPOVER(sidebar->popover), NULL);

		list = gtk_container_get_children (GTK_CONTAINER(sidebar->title_box));
		if (list) {
			children = list->data;
			gtk_container_remove(GTK_CONTAINER(sidebar->title_box), children);
			g_list_free(list);
		}
	}

	g_signal_emit (sidebar, signals[SIGNAL_CHILDREN_CHANGED], 0);
}

gint
rena_sidebar_get_n_panes (RenaSidebar *sidebar)
{
	return 	gtk_notebook_get_n_pages (GTK_NOTEBOOK(sidebar->container));
}

void
rena_sidebar_style_position (RenaSidebar *sidebar, GtkPositionType position)
{
	GtkWidget *parent;
	parent  = gtk_widget_get_parent (GTK_WIDGET(sidebar->close_button));
	gtk_box_reorder_child (GTK_BOX(parent),
	                       GTK_WIDGET(sidebar->close_button),
	                       (position == GTK_POS_RIGHT) ? 0 : 1);
}


/*
 * Internal Calbacks.
 */

static void
rena_sidebar_close_button_cb (GtkWidget     *widget,
                                RenaSidebar *sidebar)
{
	gtk_widget_hide (GTK_WIDGET(sidebar));
}

static void
rena_sidebar_button_press_cb (GtkWidget      *widget,
                                RenaSidebar  *sidebar)
{
	if(!sidebar->popover)
		return;

	if(!gtk_widget_get_sensitive(gtk_notebook_get_nth_page (GTK_NOTEBOOK(sidebar->container), 0)))
		return;

	gtk_widget_show (GTK_WIDGET(sidebar->popover));
}


/**
 * Construction:
 **/

static GtkWidget *
praga_sidebar_menu_button_new (RenaSidebar *sidebar)
{
	GtkWidget *button, *hbox, *arrow;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);

	button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_set_halign (button, GTK_ALIGN_CENTER);

	arrow = gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_MENU);

	gtk_box_pack_start (GTK_BOX(hbox),
	                    sidebar->title_box,
	                    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(hbox),
	                    arrow,
	                    FALSE, FALSE, 0);

	gtk_widget_set_valign (GTK_WIDGET(sidebar->title_box), GTK_ALIGN_CENTER);

	gtk_container_add (GTK_CONTAINER(button), hbox);

	g_signal_connect(G_OBJECT(button),
	                 "clicked",
	                 G_CALLBACK(rena_sidebar_button_press_cb),
	                 sidebar);

	return button;
}

static GtkWidget *
rena_sidebar_close_button_new(RenaSidebar *sidebar)
{
	GtkWidget *button;
	GIcon *icon = NULL;

 	const gchar *fallback_icons[] = {
		"view-left-close",
		"tab-close",
		"window-close",
		NULL,
	};

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_set_focus_on_click (button, FALSE);
	rena_hig_set_tiny_button (button);
	gtk_widget_set_margin_start (button, 4);
	gtk_widget_set_margin_end (button, 4);
	gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

	icon = g_themed_icon_new_from_names ((gchar **)fallback_icons, -1);
	gtk_button_set_image (GTK_BUTTON (button),
		gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_MENU));
	g_object_unref (icon);

	g_signal_connect(G_OBJECT (button),
	                 "clicked",
	                 G_CALLBACK(rena_sidebar_close_button_cb),
	                 sidebar);

	return button;
}

GtkWidget *
rena_sidebar_header_new(RenaSidebar *sidebar)
{
	GtkWidget *hbox;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	gtk_box_pack_start(GTK_BOX(hbox),
	                   sidebar->menu_button,
	                   TRUE,
	                   TRUE,
	                   0);
	gtk_box_pack_start(GTK_BOX(hbox),
	                   sidebar->close_button,
	                   FALSE,
	                   FALSE,
	                   0);
	return hbox;
}

GtkWidget *
rena_sidebar_container_new(RenaSidebar *sidebar)
{
	GtkWidget *notebook;

	notebook = gtk_notebook_new();

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_popup_disable(GTK_NOTEBOOK(notebook));

	return notebook;
}

static void
rena_sidebar_finalize (GObject *object)
{
	//RenaSidebar *sidebar = RENA_SIDEBAR (object);
	(*G_OBJECT_CLASS (rena_sidebar_parent_class)->finalize) (object);
}

static void
rena_sidebar_init (RenaSidebar *sidebar)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (sidebar),
	                                GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing (GTK_BOX(sidebar), 2);

	sidebar->title_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	sidebar->menu_button = praga_sidebar_menu_button_new (sidebar);
	sidebar->close_button = rena_sidebar_close_button_new (sidebar);

	sidebar->header = rena_sidebar_header_new (sidebar);
	sidebar->container = rena_sidebar_container_new (sidebar);
	sidebar->popover = NULL;

	gtk_box_pack_start (GTK_BOX(sidebar), sidebar->header,
	                    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(sidebar), sidebar->container,
	                    TRUE, TRUE, 0);

	gtk_widget_show_all (GTK_WIDGET(sidebar->header));
	gtk_widget_show_all (GTK_WIDGET(sidebar->container));
}

static void
rena_sidebar_class_init (RenaSidebarClass *klass)
{
	GObjectClass  *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = rena_sidebar_finalize;

	signals[SIGNAL_CHILDREN_CHANGED] =
		g_signal_new ("children-changed",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaSidebarClass, children_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

RenaSidebar *
rena_sidebar_new (void)
{
	return g_object_new (RENA_TYPE_SIDEBAR, NULL);
}

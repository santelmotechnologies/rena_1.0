/*
 * Copyright (C) 2024 Santelmo Technologies <santelmotechnologies@gmail.com> 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include "rena-database-provider.h"
#include "rena-utils.h"
#include "rena.h"

gboolean info_bar_import_music_will_be_useful(RenaApplication *rena)
{
	return rena_application_is_first_run (rena) &&
	         g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
}

static void info_bar_response_cb(GtkInfoBar *info_bar, gint response_id, gpointer user_data)
{
	RenaDatabaseProvider *provider;
	RenaScanner *scanner;

	RenaApplication *rena = user_data;

	const gchar *dir = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC);

	gtk_widget_destroy(GTK_WIDGET(info_bar));

	switch (response_id)
	{
		case GTK_RESPONSE_CANCEL:
			break;
		case GTK_RESPONSE_YES:
			provider = rena_database_provider_get ();
			rena_provider_add_new (provider, dir, "local", _("Local Music"), "drive-harddisk");
			g_object_unref (G_OBJECT (provider));

			scanner = rena_application_get_scanner (rena);
			rena_scanner_scan_library (scanner);
			break;
		default:
			g_warn_if_reached();
	}
}

GtkWidget * create_info_bar_import_music(RenaApplication *rena)
{
	const gchar *dir = g_get_user_special_dir(G_USER_DIRECTORY_MUSIC);

	GtkWidget *info_bar = gtk_info_bar_new();
	GtkWidget *action_area = gtk_info_bar_get_action_area(GTK_INFO_BAR (info_bar));
	GtkWidget *content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(info_bar));

	gtk_orientable_set_orientation(GTK_ORIENTABLE(action_area), GTK_ORIENTATION_HORIZONTAL);

	//GtkInfoBar has undocumented behavior for GTK_RESPONSE_CANCEL
	gtk_info_bar_add_button(GTK_INFO_BAR(info_bar), _("_No"), GTK_RESPONSE_CANCEL);
	gtk_info_bar_add_button(GTK_INFO_BAR(info_bar), _("_Yes"), GTK_RESPONSE_YES);

	gchar *content = g_strdup_printf(_("Would you like to import %s to library?"), dir);

	GtkWidget *label = gtk_label_new(content);
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 0);

	g_signal_connect(info_bar, "response", G_CALLBACK(info_bar_response_cb), rena);

	gtk_widget_show_all(info_bar);

	g_free(content);

	return info_bar;
}

static void info_bar_update_response_cb(GtkInfoBar *info_bar, gint response_id, gpointer user_data)
{
	RenaScanner *scanner;

	RenaApplication *rena = user_data;

	gtk_widget_destroy(GTK_WIDGET(info_bar));

	switch (response_id)
	{
		case GTK_RESPONSE_CANCEL:
			break;
		case GTK_RESPONSE_YES:
			scanner = rena_application_get_scanner (rena);
			rena_scanner_update_library (scanner);
			break;
		default:
			g_warn_if_reached();
	}
}

GtkWidget *create_info_bar_update_music(RenaApplication *rena)
{
	GtkWidget *info_bar = gtk_info_bar_new();
	GtkWidget *action_area = gtk_info_bar_get_action_area(GTK_INFO_BAR (info_bar));
	GtkWidget *content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(info_bar));

	gtk_orientable_set_orientation(GTK_ORIENTABLE(action_area), GTK_ORIENTATION_HORIZONTAL);

	//GtkInfoBar has undocumented behavior for GTK_RESPONSE_CANCEL
	gtk_info_bar_add_button(GTK_INFO_BAR(info_bar), _("_No"), GTK_RESPONSE_CANCEL);
	gtk_info_bar_add_button(GTK_INFO_BAR(info_bar), _("_Yes"), GTK_RESPONSE_YES);

	GtkWidget *label = gtk_label_new(_("Would you like to update your music library?"));
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 0);

	g_signal_connect(info_bar, "response", G_CALLBACK(info_bar_update_response_cb), rena);

	gtk_widget_show_all(info_bar);

	return info_bar;
}


static void
info_bar_restart_response_cb(GtkInfoBar *info_bar, gint response_id, gpointer user_data)
{
	RenaApplication *rena = user_data;

	gtk_widget_destroy(GTK_WIDGET(info_bar));

	switch (response_id)
	{
		case GTK_RESPONSE_CANCEL:
			break;
		case GTK_RESPONSE_YES:
			rena_application_quit(rena);
			break;
		default:
			g_warn_if_reached();
	}
}

GtkWidget *
rena_info_bar_need_restart (RenaApplication *rena)
{
	GtkWidget *info_bar = gtk_info_bar_new();
	GtkWidget *action_area = gtk_info_bar_get_action_area(GTK_INFO_BAR (info_bar));
	GtkWidget *content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(info_bar));

	gtk_orientable_set_orientation(GTK_ORIENTABLE(action_area), GTK_ORIENTATION_HORIZONTAL);

	//GtkInfoBar has undocumented behavior for GTK_RESPONSE_CANCEL
	gtk_info_bar_add_button(GTK_INFO_BAR(info_bar), _("_No"), GTK_RESPONSE_CANCEL);
	gtk_info_bar_add_button(GTK_INFO_BAR(info_bar), _("_Yes"), GTK_RESPONSE_YES);

	GtkWidget *label = gtk_label_new(_("Some changes need restart rena."));
	gtk_box_pack_start(GTK_BOX(content_area), label, FALSE, FALSE, 0);

	g_signal_connect(info_bar, "response", G_CALLBACK(info_bar_restart_response_cb), rena);

	gtk_widget_show_all(info_bar);

	return info_bar;
}


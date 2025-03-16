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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "rena-preferences-dialog.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#ifdef HAVE_LIBPEAS
#include <libpeas-gtk/peas-gtk.h>
#endif

#include "rena-hig.h"
#include "rena-utils.h"
#include "rena-simple-widgets.h"
#include "rena-library-pane.h"
#include "rena-database.h"
#include "rena-database-provider.h"
#include "rena-provider.h"
#include "rena-debug.h"


enum library_columns {
	COLUMN_NAME,
	COLUMN_KIND,
	COLUMN_FRIENDLY,
	COLUMN_ICON_NAME,
	COLUMN_VISIBLE,
	COLUMN_IGNORED,
	COLUMN_MARKUP,
	N_COLUMNS
};

struct _PreferencesTab {
	GtkWidget *widget;
	GtkWidget *vbox;
	GtkWidget *label;
};
typedef struct _PreferencesTab PreferencesTab;

struct _RenaPreferencesDialogClass {
	GtkDialogClass     __parent__;
};
typedef struct _RenaPreferencesDialogClass RenaPreferencesDialogClass;

struct _RenaPreferencesDialog {
	GtkDialog         __parent__;

	RenaPreferences *preferences;

	GtkWidget         *notebook;
	PreferencesTab    *audio_tab;
	PreferencesTab    *desktop_tab;
	PreferencesTab    *services_tab;

#ifndef G_OS_WIN32
	GtkWidget *audio_device_w;
	GtkWidget *audio_sink_combo_w;
	GtkWidget *soft_mixer_w;
#endif
	GtkWidget *ignore_errors_w;
	GtkWidget *system_titlebar_w;
	GtkWidget *small_toolbar_w;
	GtkWidget *album_art_w;
	GtkWidget *album_art_size_w;
	GtkWidget *album_art_pattern_w;

	GtkWidget *library_view_w;
	GtkWidget *sort_by_year_w;

	GtkWidget *instant_filter_w;
	GtkWidget *aproximate_search_w;
	GtkWidget *window_state_combo_w;
	GtkWidget *restore_playlist_w;
	GtkWidget *show_icon_tray_w;
	GtkWidget *close_to_tray_w;
	GtkWidget *add_recursively_w;
};

G_DEFINE_TYPE (RenaPreferencesDialog, rena_preferences_dialog, GTK_TYPE_DIALOG)


/*
 * Utils.
 */

void
rena_gtk_entry_set_text (GtkEntry *entry, const gchar *text)
{
	gtk_entry_set_text (GTK_ENTRY(entry),
		string_is_not_empty(text) ? text : "");
}

/*
 * Dinamic Tabs
 */

static PreferencesTab *
rena_preferences_tab_new (const gchar *label)
{
	PreferencesTab *tab;
	tab = g_slice_new0(PreferencesTab);

	tab->label = gtk_label_new(label);
	tab->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	return tab;
}

static void
rena_preferences_tab_free (PreferencesTab *tab)
{
	g_slice_free (PreferencesTab, tab);
}

static void
rena_preferences_tab_append_setting (PreferencesTab *tab, GtkWidget *widget, gboolean expand)
{
	gtk_box_pack_start (GTK_BOX(tab->vbox), widget, expand, expand, 0);
	gtk_widget_show_all (tab->vbox);
}

static void
rena_preferences_tab_remove_setting (PreferencesTab *tab, GtkWidget *widget)
{
	GList *list = NULL;
	gtk_container_remove (GTK_CONTAINER(tab->vbox), widget);

	list = gtk_container_get_children (GTK_CONTAINER(tab->vbox));

	if (g_list_length(list) == 0)
		gtk_widget_hide (tab->vbox);
	g_list_free(list);
}

static void
rena_preferences_notebook_append_tab (GtkWidget *notebook, PreferencesTab *tab)
{
	GList *list = NULL;

	gtk_notebook_append_page (GTK_NOTEBOOK(notebook),
	                          tab->vbox, tab->label);

	list = gtk_container_get_children (GTK_CONTAINER(tab->vbox));
	if (g_list_length(list) == 0)
		gtk_widget_hide (tab->vbox);
	else
		gtk_widget_show_all (tab->vbox);
	g_list_free (list);
}

void
rena_preferences_append_audio_setting (RenaPreferencesDialog *dialog, GtkWidget *widget, gboolean expand)
{
	rena_preferences_tab_append_setting (dialog->audio_tab, widget, expand);
}

void
rena_preferences_remove_audio_setting (RenaPreferencesDialog *dialog, GtkWidget *widget)
{
	rena_preferences_tab_remove_setting (dialog->audio_tab, widget);
}

void
rena_preferences_append_desktop_setting (RenaPreferencesDialog *dialog, GtkWidget *widget, gboolean expand)
{
	rena_preferences_tab_append_setting (dialog->desktop_tab, widget, expand);
}

void
rena_preferences_remove_desktop_setting (RenaPreferencesDialog *dialog, GtkWidget *widget)
{
	rena_preferences_tab_remove_setting (dialog->desktop_tab, widget);
}

void
rena_preferences_append_services_setting (RenaPreferencesDialog *dialog, GtkWidget *widget, gboolean expand)
{
	rena_preferences_tab_append_setting (dialog->services_tab, widget, expand);
}

void
rena_preferences_remove_services_setting (RenaPreferencesDialog *dialog, GtkWidget *widget)
{
	rena_preferences_tab_remove_setting (dialog->services_tab, widget);
}


const gchar *album_art_pattern_info = N_("Patterns should be of the form:\
<filename>;<filename>;....\nA maximum of six patterns are allowed.\n\
Wildcards are not accepted as of now ( patches welcome :-) ).");

static void
album_art_pattern_helper_response (GtkDialog *dialog,
                                   gint       response,
                                   gpointer   data)
{
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
album_art_pattern_helper (GtkDialog *parent, RenaPreferencesDialog *dialogs)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					"%s",
					album_art_pattern_info);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Album art pattern"));

	g_signal_connect(G_OBJECT(dialog), "response",
			G_CALLBACK(album_art_pattern_helper_response), dialogs);

	gtk_widget_show_all (dialog);
}

static GSList *
rena_preferences_dialog_get_library_list (GtkWidget *library_tree)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *library_list = NULL;
	gchar *u_folder = NULL, *folder = NULL;
	GError *error = NULL;
	gboolean ret;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(library_tree));

	ret = gtk_tree_model_get_iter_first(model, &iter);
	while (ret) {
		gtk_tree_model_get (model, &iter,
		                    COLUMN_NAME, &u_folder,
		                    -1);
		if (u_folder) {
			folder = g_filename_from_utf8 (u_folder, -1, NULL, NULL, &error);
			if (!folder) {
				g_warning ("Unable to get filename from UTF-8 string: %s", u_folder);
				g_error_free(error);
			}
			else {
				library_list = g_slist_append(library_list, folder);
			}
			g_free (u_folder);
		}
		ret = gtk_tree_model_iter_next(model, &iter);
	}
	return library_list;
}

static void
rena_preferences_dialog_set_library_list (GtkWidget *library_tree, GSList *library_list)
{
	RenaProvider *provider;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list;
	gchar *markup = NULL;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(library_tree));
	gtk_list_store_clear (GTK_LIST_STORE(model));

	for (list = library_list; list != NULL; list = list->next)
	{
		provider = RENA_PROVIDER(list->data);

		markup = g_markup_printf_escaped("%s (%s)",
			rena_provider_get_friendly_name(provider),
			rena_provider_get_name(provider));

		gtk_list_store_append (GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
		                    COLUMN_NAME, rena_provider_get_name(provider),
		                    COLUMN_KIND, rena_provider_get_kind(provider),
		                    COLUMN_FRIENDLY, rena_provider_get_friendly_name(provider),
		                    COLUMN_ICON_NAME, rena_provider_get_icon_name(provider),
		                    COLUMN_VISIBLE, rena_provider_get_visible(provider),
		                    COLUMN_IGNORED, rena_provider_get_ignored(provider),
		                    COLUMN_MARKUP, markup,
		                    -1);

		g_free (markup);
	}
}

/*
 * When cancel the preferences dialog should restore all changes
 */
static void
rena_preferences_dialog_restore_changes (RenaPreferencesDialog *dialog)
{
	RenaDatabaseProvider *provider;
	GSList *library_list = NULL;
	const gchar *start_mode = NULL;

	/*
	 * Collection settings.
	 */
	provider = rena_database_provider_get ();
	library_list = rena_database_provider_get_list (provider);
	rena_preferences_dialog_set_library_list(dialog->library_view_w, library_list);
	g_slist_free_full (library_list, g_object_unref);
	g_object_unref (G_OBJECT (provider));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->sort_by_year_w),
		rena_preferences_get_sort_by_year(dialog->preferences));

	/*
	 * Audio settings.
	 */
#ifndef G_OS_WIN32
	const gchar *audio_sink = rena_preferences_get_audio_sink(dialog->preferences);
	if (string_is_not_empty(audio_sink)) {
		if (!g_ascii_strcasecmp(audio_sink, ALSA_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 1);
		else if (!g_ascii_strcasecmp(audio_sink, OSS4_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 2);
		else if (!g_ascii_strcasecmp(audio_sink, OSS_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 3);
		else if (!g_ascii_strcasecmp(audio_sink, PULSE_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 4);
		else
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 0);
	}

	rena_gtk_entry_set_text(GTK_ENTRY(dialog->audio_device_w),
		rena_preferences_get_audio_device(dialog->preferences));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->soft_mixer_w),
		rena_preferences_get_software_mixer (dialog->preferences));
#endif

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->ignore_errors_w),
		rena_preferences_get_ignore_errors(dialog->preferences));

	/*
	 * Apareanse settings
	 */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->system_titlebar_w),
		rena_preferences_get_system_titlebar(dialog->preferences));

	if (rena_preferences_get_toolbar_size(dialog->preferences) == GTK_ICON_SIZE_SMALL_TOOLBAR)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->small_toolbar_w), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->small_toolbar_w), FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(dialog->album_art_w),
		rena_preferences_get_show_album_art(dialog->preferences));

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(dialog->album_art_size_w),
		rena_preferences_get_album_art_size(dialog->preferences));

	rena_gtk_entry_set_text(GTK_ENTRY(dialog->album_art_pattern_w),
		rena_preferences_get_album_art_pattern (dialog->preferences));

	/*
	 * General settings
	 */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->instant_filter_w),
		rena_preferences_get_instant_search(dialog->preferences));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->aproximate_search_w),
		rena_preferences_get_approximate_search(dialog->preferences));

	if (rena_preferences_get_remember_state(dialog->preferences))
		gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 0);
	else {
		start_mode = rena_preferences_get_start_mode(dialog->preferences);
		if(string_is_not_empty(start_mode)) {
			if (!g_ascii_strcasecmp(start_mode, NORMAL_STATE))
				gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 1);
			else if(!g_ascii_strcasecmp(start_mode, FULLSCREEN_STATE))
				gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 2);
			else if(!g_ascii_strcasecmp(start_mode, ICONIFIED_STATE))
				gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 3);
		}
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->restore_playlist_w),
		rena_preferences_get_restore_playlist(dialog->preferences));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->add_recursively_w),
		rena_preferences_get_add_recursively(dialog->preferences));

	/*
	 * Desktop settings
	 */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->show_icon_tray_w),
		rena_preferences_get_show_status_icon(dialog->preferences));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->close_to_tray_w),
		rena_preferences_get_hide_instead_close(dialog->preferences));
}

/*
 * When accepting the preferences dialog must be set changes.
 */
static void
rena_preferences_dialog_accept_changes (RenaPreferencesDialog *dialog)
{
	RenaDatabaseProvider *dbase_provider;
	GSList *list, *library_dir = NULL, *folder_scanned = NULL, *folders_added = NULL, *folders_deleted = NULL;
	gchar *window_state_sink = NULL;
	const gchar *album_art_pattern;
	gchar *prov_base = NULL;
	gboolean show_album_art, instant_search, approximate_search, restore_playlist, add_recursively;
	gboolean test_change = FALSE, pref_setted, pref_toggled, library_locked;
	gboolean system_titlebar, small_toolbar;
	gint album_art_size;
	RenaLibraryStyle style;

	/*
	 * Audio preferences
	 */
#ifndef G_OS_WIN32
	gboolean need_restart = FALSE;
	const gchar *audio_device;
	gchar *audio_sink = NULL;
	gboolean software_mixer;

	audio_sink = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(dialog->audio_sink_combo_w));
	if(audio_sink) {
		if (g_ascii_strcasecmp(audio_sink, rena_preferences_get_audio_sink(dialog->preferences))) {
			rena_preferences_set_audio_sink(dialog->preferences, audio_sink);
			need_restart = TRUE;
		}
		g_free(audio_sink);
	}

	audio_device = gtk_entry_get_text(GTK_ENTRY(dialog->audio_device_w));
	if (audio_device) {
		if (g_ascii_strcasecmp(audio_device, rena_preferences_get_audio_device(dialog->preferences))) {
			rena_preferences_set_audio_device(dialog->preferences, audio_device);
			need_restart = TRUE;
		}
	}

	software_mixer = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->soft_mixer_w));
	if (software_mixer != rena_preferences_get_software_mixer(dialog->preferences)) {
		rena_preferences_set_software_mixer(dialog->preferences, software_mixer);
		need_restart = TRUE;
	}

	if (need_restart)
		rena_preferences_need_restart (dialog->preferences);
#endif

	rena_preferences_set_ignore_errors (dialog->preferences,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->ignore_errors_w)));

	/*
	 * Get scanded folders and compare. If changed show infobar
	 */

	dbase_provider = rena_database_provider_get ();

	folder_scanned = rena_provider_get_list (dbase_provider);
	library_dir = rena_preferences_dialog_get_library_list (dialog->library_view_w);

	library_locked = rena_preferences_get_lock_library (dialog->preferences);
	if (library_locked == FALSE)
	{
		folders_added = rena_string_list_get_added (folder_scanned, library_dir);
		folders_deleted = rena_string_list_get_removed (folder_scanned, library_dir);

		if (folders_added)
		{
			/* Here you can only add local folders. */
			for (list = folders_added; list != NULL; list = list->next)
			{
				prov_base = g_filename_display_basename (list->data);
				rena_provider_add_new (dbase_provider,
				                         list->data,
				                         "local",
				                         prov_base,
				                         "drive-harddisk");
				g_free (prov_base);
			}
			test_change = TRUE;
		}

		if (folders_deleted)
		{
			for (list = folders_deleted; list != NULL; list = list->next)
			{
				rena_provider_remove (dbase_provider, list->data);
			}
			test_change = TRUE;
		}
	}
	g_object_unref (G_OBJECT (dbase_provider));

	if (test_change)
		rena_preferences_local_provider_changed (dialog->preferences);

	if (library_dir)
		g_slist_free_full (library_dir, g_free);
	if (folder_scanned)
		g_slist_free_full (folder_scanned, g_free);
	if (folders_added)
		g_slist_free_full (folders_added, g_free);
	if (folders_deleted)
		g_slist_free_full (folders_deleted, g_free);

	/*
	 * Library view changes
	 */
	style = rena_preferences_get_library_style (dialog->preferences);

	/* Save sort by year preference, and reload view if needed */

	pref_setted = rena_preferences_get_sort_by_year (dialog->preferences);
	pref_toggled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(dialog->sort_by_year_w));

	rena_preferences_set_sort_by_year (dialog->preferences, pref_toggled);

	if ((style != FOLDERS) && (pref_setted != pref_toggled)) {
		dbase_provider = rena_database_provider_get ();
		rena_provider_update_done (dbase_provider);
		g_object_unref (dbase_provider);
	}

	/*
	 * General preferences
	 */
	window_state_sink = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(dialog->window_state_combo_w));

	if (!g_ascii_strcasecmp(window_state_sink, _("Start normal"))) {
		rena_preferences_set_remember_state(dialog->preferences, FALSE);
		rena_preferences_set_start_mode(dialog->preferences, NORMAL_STATE);
	}
	else if (!g_ascii_strcasecmp(window_state_sink, _("Start fullscreen"))){
		rena_preferences_set_remember_state(dialog->preferences, FALSE);
		rena_preferences_set_start_mode(dialog->preferences, FULLSCREEN_STATE);
	}
	else if (!g_ascii_strcasecmp(window_state_sink, _("Start in system tray"))){
		rena_preferences_set_remember_state(dialog->preferences, FALSE);
		rena_preferences_set_start_mode(dialog->preferences, ICONIFIED_STATE);
	}
	else
		rena_preferences_set_remember_state(dialog->preferences, TRUE);

	g_free(window_state_sink);

	instant_search =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->instant_filter_w));
	rena_preferences_set_instant_search(dialog->preferences, instant_search);

	approximate_search =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->aproximate_search_w));
	rena_preferences_set_approximate_search(dialog->preferences, approximate_search);

	restore_playlist =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->restore_playlist_w));
	rena_preferences_set_restore_playlist(dialog->preferences, restore_playlist);

	rena_preferences_set_show_status_icon(dialog->preferences,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->show_icon_tray_w)));

	rena_preferences_set_hide_instead_close(dialog->preferences,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->close_to_tray_w)));

	add_recursively =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->add_recursively_w));
	rena_preferences_set_add_recursively(dialog->preferences, add_recursively);

	show_album_art =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->album_art_w));
	rena_preferences_set_show_album_art(dialog->preferences, show_album_art);

	album_art_size =
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->album_art_size_w));
	rena_preferences_set_album_art_size(dialog->preferences, album_art_size);

	if (show_album_art) {
		album_art_pattern = gtk_entry_get_text(GTK_ENTRY(dialog->album_art_pattern_w));

		if (string_is_not_empty(album_art_pattern)) {
			if (!validate_album_art_pattern(album_art_pattern)) {
				album_art_pattern_helper(GTK_DIALOG(dialog), dialog);
				return;
			}
			/* Proper pattern, store in preferences */
			rena_preferences_set_album_art_pattern (dialog->preferences,
			                                          album_art_pattern);
		}
	}

	system_titlebar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->system_titlebar_w));
	if (system_titlebar != rena_preferences_get_system_titlebar(dialog->preferences)) {
		rena_preferences_set_system_titlebar (dialog->preferences, system_titlebar);
		rena_preferences_set_show_menubar (dialog->preferences, system_titlebar);
	}

	small_toolbar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->small_toolbar_w));
	if (small_toolbar)
		rena_preferences_set_toolbar_size (dialog->preferences, GTK_ICON_SIZE_SMALL_TOOLBAR);
	else
		rena_preferences_set_toolbar_size (dialog->preferences, GTK_ICON_SIZE_LARGE_TOOLBAR);
}

/* Handler for the preferences dialog */

static void
rena_preferences_dialog_response(GtkDialog *dialog_w, gint response_id, RenaPreferencesDialog *dialog)
{
	switch(response_id) {
		case GTK_RESPONSE_OK:
			rena_preferences_dialog_accept_changes (dialog);
			break;
		case GTK_RESPONSE_CANCEL:
		default:
			rena_preferences_dialog_restore_changes (dialog);
			break;
	}
	gtk_widget_hide(GTK_WIDGET(dialog));
}

static gboolean
rena_preferences_dialog_delete (GtkWidget *widget, GdkEvent *event, RenaPreferencesDialog *dialog)
{
	return TRUE;
}

/* Handler for adding a new library */
static void
library_add_cb_response (GtkDialog *add_dialog, gint response, RenaPreferencesDialog *dialog)
{
	gchar *u_folder, *folder, *basename, *markup;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GError *error = NULL;

	switch (response) {
	case GTK_RESPONSE_ACCEPT:
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(dialog->library_view_w));
		folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(add_dialog));

		if (!folder)
			break;

		u_folder = g_filename_to_utf8(folder, -1, NULL, NULL, &error);
		if (!u_folder) {
			g_warning("Unable to get UTF-8 from filename: %s", folder);
			g_error_free(error);
			g_free(folder);
			break;
		}

		basename = g_filename_display_basename (u_folder);
		markup = g_markup_printf_escaped("%s (%s)", basename, u_folder);

		gtk_list_store_append (GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
		                    COLUMN_NAME, u_folder,
		                    COLUMN_KIND, "local",
		                    COLUMN_FRIENDLY, basename,
		                    COLUMN_ICON_NAME, "drive-harddisk",
		                    COLUMN_VISIBLE, TRUE,
		                    COLUMN_IGNORED, FALSE,
		                    COLUMN_MARKUP, markup,
		                    -1);

		g_free(u_folder);
		g_free(folder);
		g_free (basename);
		g_free(markup);

		break;
	default:
		break;
	}
	gtk_widget_destroy(GTK_WIDGET(add_dialog));
}

static void
library_add_cb (GtkButton *button, RenaPreferencesDialog *dialog)
{
	GtkWidget *add_dialog;

	/* Create a folder chooser dialog */

	add_dialog = gtk_file_chooser_dialog_new (_("Select a folder to add to library"),
	                                          GTK_WINDOW(dialog),
	                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
	                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
	                                          _("_Open"), GTK_RESPONSE_ACCEPT,
	                                          NULL);

	gtk_window_set_modal(GTK_WINDOW(add_dialog), TRUE);

	g_signal_connect (G_OBJECT(add_dialog), "response",
	                  G_CALLBACK(library_add_cb_response), dialog);

	gtk_widget_show_all (add_dialog);
}

/* Handler for removing a library */

static void library_remove_cb(GtkButton *button, RenaPreferencesDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->library_view_w));

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}

/* Toggle album art pattern */

static void toggle_album_art(GtkToggleButton *button, RenaPreferencesDialog *dialog)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->album_art_w));

	gtk_widget_set_sensitive(dialog->album_art_pattern_w, is_active);
	gtk_widget_set_sensitive(dialog->album_art_size_w, is_active);
}

/* Some audios toggles handlers */

#ifndef G_OS_WIN32
static void update_audio_device_alsa(RenaPreferencesDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->audio_device_w, TRUE);
	gtk_widget_set_sensitive(dialog->soft_mixer_w, TRUE);
}

static void update_audio_device_oss4(RenaPreferencesDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->audio_device_w, TRUE);
	gtk_widget_set_sensitive(dialog->soft_mixer_w, TRUE);
}

static void update_audio_device_oss(RenaPreferencesDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->audio_device_w, TRUE);
	gtk_widget_set_sensitive(dialog->soft_mixer_w, TRUE);
}

static void update_audio_device_pulse(RenaPreferencesDialog *dialog)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->soft_mixer_w), FALSE);
	gtk_widget_set_sensitive(dialog->audio_device_w, FALSE);
	gtk_widget_set_sensitive(dialog->soft_mixer_w, FALSE);
}

static void update_audio_device_default(RenaPreferencesDialog *dialog)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->soft_mixer_w), FALSE);
	gtk_widget_set_sensitive(dialog->audio_device_w, FALSE);
	gtk_widget_set_sensitive(dialog->soft_mixer_w, FALSE);
}

/* The enumerated audio devices have to be changed here */

static void
change_audio_sink(GtkComboBox *combo, RenaPreferencesDialog *dialog)
{
	gchar *audio_sink;

	audio_sink = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(dialog->audio_sink_combo_w));

	if (!g_ascii_strcasecmp(audio_sink, ALSA_SINK))
		update_audio_device_alsa(dialog);
	else if (!g_ascii_strcasecmp(audio_sink, OSS4_SINK))
		update_audio_device_oss4(dialog);
	else if (!g_ascii_strcasecmp(audio_sink, OSS_SINK))
		update_audio_device_oss(dialog);
	else if (!g_ascii_strcasecmp(audio_sink, PULSE_SINK))
		update_audio_device_pulse(dialog);
	else
		update_audio_device_default(dialog);

	g_free(audio_sink);
}
#endif

static void
rena_preferences_dialog_init_settings(RenaPreferencesDialog *dialog)
{
	RenaDatabaseProvider *provider;
	GSList *library_dir = NULL;
	const gchar *start_mode = rena_preferences_get_start_mode(dialog->preferences);

	/* Audio Options */
#ifndef G_OS_WIN32
	const gchar *audio_sink = rena_preferences_get_audio_sink(dialog->preferences);

	if (string_is_not_empty(audio_sink)) {
		if (!g_ascii_strcasecmp(audio_sink, ALSA_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 1);
		else if (!g_ascii_strcasecmp(audio_sink, OSS4_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 2);
		else if (!g_ascii_strcasecmp(audio_sink, OSS_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 3);
		else if (!g_ascii_strcasecmp(audio_sink, PULSE_SINK))
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 4);
		else
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->audio_sink_combo_w), 0);
	}

	if (string_is_not_empty(audio_sink)) {
		if (!g_ascii_strcasecmp(audio_sink, ALSA_SINK))
			update_audio_device_alsa(dialog);
		else if (!g_ascii_strcasecmp(audio_sink, OSS4_SINK))
			update_audio_device_oss4(dialog);
		else if (!g_ascii_strcasecmp(audio_sink, OSS_SINK))
			update_audio_device_oss(dialog);
		else if (!g_ascii_strcasecmp(audio_sink, PULSE_SINK))
			update_audio_device_pulse(dialog);
		else
			update_audio_device_default(dialog);
	}

	rena_gtk_entry_set_text(GTK_ENTRY(dialog->audio_device_w),
		rena_preferences_get_audio_device(dialog->preferences));

	if (rena_preferences_get_software_mixer(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->soft_mixer_w), TRUE);
#endif

	if (rena_preferences_get_ignore_errors(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->ignore_errors_w), TRUE);

	/* General Options */

	if(rena_preferences_get_remember_state(dialog->preferences))
		gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 0);
	else {
		if(string_is_not_empty(start_mode)) {
			if (!g_ascii_strcasecmp(start_mode, NORMAL_STATE))
				gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 1);
			else if(!g_ascii_strcasecmp(start_mode, FULLSCREEN_STATE))
				gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 2);
			else if(!g_ascii_strcasecmp(start_mode, ICONIFIED_STATE))
				gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->window_state_combo_w), 3);
		}
	}

	if (rena_preferences_get_instant_search(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->instant_filter_w), TRUE);

	if (rena_preferences_get_approximate_search(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->aproximate_search_w), TRUE);

	if (rena_preferences_get_restore_playlist(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->restore_playlist_w), TRUE);

	if (rena_preferences_get_show_status_icon(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->show_icon_tray_w), TRUE);

	if (rena_preferences_get_hide_instead_close(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->close_to_tray_w), TRUE);

	if (rena_preferences_get_add_recursively(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->add_recursively_w), TRUE);

	if (rena_preferences_get_show_album_art(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->album_art_w), TRUE);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON(dialog->album_art_size_w),
	                           rena_preferences_get_album_art_size(dialog->preferences));

	rena_gtk_entry_set_text(GTK_ENTRY(dialog->album_art_pattern_w),
		rena_preferences_get_album_art_pattern(dialog->preferences));

	/* Lbrary Options */

	provider = rena_database_provider_get ();
	library_dir = rena_database_provider_get_list (provider);
	rena_preferences_dialog_set_library_list(dialog->library_view_w, library_dir);
	g_slist_free_full (library_dir, g_object_unref);
	g_object_unref (G_OBJECT (provider));

	if (rena_preferences_get_sort_by_year(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->sort_by_year_w), TRUE);
}

gint library_view_key_press (GtkWidget *win, GdkEventKey *event, RenaPreferencesDialog *dialog)
{
	if (event->state != 0 &&
	    ((event->state & GDK_CONTROL_MASK) ||
	     (event->state & GDK_MOD1_MASK) ||
	     (event->state & GDK_MOD3_MASK) ||
	     (event->state & GDK_MOD4_MASK) ||
	     (event->state & GDK_MOD5_MASK))) {
		return FALSE;
	}
	if (event->keyval == GDK_KEY_Delete) {
		library_remove_cb(NULL, dialog);
		return TRUE;
	}

	return FALSE;
}

#ifndef G_OS_WIN32
static GtkWidget*
pref_create_audio_page (RenaPreferencesDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *audio_device_entry, *audio_device_label, *audio_sink_combo, *sink_label, *soft_mixer;
	guint row = 0;

	table = rena_hig_workarea_table_new();

	rena_hig_workarea_table_add_section_title(table, &row, _("Audio"));

	sink_label = gtk_label_new(_("Audio sink"));

	audio_sink_combo = gtk_combo_box_text_new();
	gtk_widget_set_tooltip_text(GTK_WIDGET(audio_sink_combo),
				    _("Restart Required"));

	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_sink_combo),
				  DEFAULT_SINK);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_sink_combo),
				  ALSA_SINK);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_sink_combo),
				  OSS4_SINK);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_sink_combo),
				  OSS_SINK);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audio_sink_combo),
				  PULSE_SINK);

	rena_hig_workarea_table_add_row (table, &row, sink_label, audio_sink_combo);

	audio_device_label = gtk_label_new(_("Audio Device"));
	gtk_widget_set_halign (GTK_WIDGET(audio_device_label), GTK_ALIGN_START);
	gtk_widget_set_valign (GTK_WIDGET(audio_device_label), GTK_ALIGN_START);

	audio_device_entry = gtk_entry_new();
	gtk_widget_set_tooltip_text(GTK_WIDGET(audio_device_entry), _("Restart Required"));
	gtk_entry_set_activates_default (GTK_ENTRY(audio_device_entry), TRUE);

	rena_hig_workarea_table_add_row (table, &row, audio_device_label, audio_device_entry);

	soft_mixer = gtk_check_button_new_with_label(_("Use software mixer"));
	gtk_widget_set_tooltip_text(GTK_WIDGET(soft_mixer), _("Restart Required"));

	rena_hig_workarea_table_add_wide_control(table, &row, soft_mixer);

	/* Store references */

	dialog->audio_sink_combo_w = audio_sink_combo;
	dialog->audio_device_w = audio_device_entry;
	dialog->soft_mixer_w = soft_mixer;

	/* Setup signal handlers */

	g_signal_connect (G_OBJECT(audio_sink_combo), "changed",
	                  G_CALLBACK(change_audio_sink), dialog);

	return table;
}
#endif

static GtkWidget*
pref_create_library_page (RenaPreferencesDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *library_view, *library_view_scroll, *library_bbox, *library_add, \
		*library_remove, *hbox_library, *sort_by_year, *infobar, *label;
	GtkListStore *library_store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	guint row = 0;

	table = rena_hig_workarea_table_new();

	rena_hig_workarea_table_add_section_title(table, &row, _("Library"));

	infobar = gtk_info_bar_new ();
	gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_INFO);
	label = gtk_label_new (_("Can not change directories while they are analyzing."));
	gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar))), label, FALSE, FALSE, 0);
	rena_hig_workarea_table_add_wide_control(table, &row, infobar);

	/* Local library. */

	hbox_library = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	library_store = gtk_list_store_new (N_COLUMNS,
	                                    G_TYPE_STRING,  // Name.
	                                    G_TYPE_STRING,  // King.
	                                    G_TYPE_STRING,  // Friendly name.
	                                    G_TYPE_STRING,  // Icon name.
	                                    G_TYPE_BOOLEAN, // Visible
	                                    G_TYPE_BOOLEAN, // Ignored
	                                    G_TYPE_STRING); // Markup.

	library_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(library_store));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Folders"));
	gtk_tree_view_column_set_resizable(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
	                                     "active", COLUMN_VISIBLE,
	                                     NULL);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
	                                     "icon-name", COLUMN_ICON_NAME,
	                                     NULL);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
	                                     "markup", COLUMN_MARKUP,
	                                     NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(library_view), column);

	library_view_scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(library_view_scroll),
	                                GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(library_view_scroll),
	                                     GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(library_view_scroll), library_view);

	library_bbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX(library_bbox), GTK_BUTTONBOX_START);

	library_add = gtk_button_new_with_mnemonic (_("_Add"));
	library_remove = gtk_button_new_with_mnemonic (_("_Remove"));

	gtk_box_pack_start (GTK_BOX(library_bbox), library_add,
	                    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(library_bbox), library_remove,
	                    FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX(hbox_library), library_view_scroll,
	                    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(hbox_library), library_bbox,
	                    FALSE, FALSE, 0);

	rena_hig_workarea_table_add_wide_tall_control(table, &row, hbox_library);

	/* Sort by year option. */

	sort_by_year = gtk_check_button_new_with_label(_("Sort albums by release year"));
	rena_hig_workarea_table_add_wide_control(table, &row, sort_by_year);

	/* Store references */

	dialog->library_view_w = library_view;
	dialog->sort_by_year_w = sort_by_year;

	/* Setup signal handlers */

	g_signal_connect (G_OBJECT(library_add), "clicked",
	                  G_CALLBACK(library_add_cb), dialog);
	g_signal_connect (G_OBJECT(library_remove), "clicked",
	                  G_CALLBACK(library_remove_cb), dialog);
	g_signal_connect (G_OBJECT (library_view), "key_press_event",
	                  G_CALLBACK(library_view_key_press), dialog);

	g_object_bind_property (dialog->preferences, "lock-library",
	                        hbox_library, "sensitive",
	                        G_BINDING_INVERT_BOOLEAN);

	g_object_bind_property (dialog->preferences, "lock-library",
	                        infobar, "visible",
	                        G_BINDING_SYNC_CREATE | G_BINDING_DEFAULT);

	return table;
}

static GtkWidget*
pref_create_playback_page (RenaPreferencesDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *ignore_errors_w;
	guint row = 0;

	table = rena_hig_workarea_table_new();

	rena_hig_workarea_table_add_section_title(table, &row, _("Playback"));

	ignore_errors_w = gtk_check_button_new_with_label(_("Ignore errors and continue playback"));

	rena_hig_workarea_table_add_wide_control(table, &row, ignore_errors_w);

	if (rena_preferences_get_ignore_errors (dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ignore_errors_w), TRUE);

	dialog->ignore_errors_w = ignore_errors_w;

	return table;
}

static GtkWidget*
pref_create_appearance_page(RenaPreferencesDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *system_titlebar, *album_art, *small_toolbar;
	GtkWidget *album_art_pattern_label, *album_art_size, *album_art_size_label, *album_art_pattern;
	guint row = 0;

	table = rena_hig_workarea_table_new();

	rena_hig_workarea_table_add_section_title(table, &row, _("Appearance"));

	system_titlebar = gtk_check_button_new_with_label(_("Use system title bar and borders"));
	rena_hig_workarea_table_add_wide_control(table, &row, system_titlebar);

	if (!gdk_screen_is_composited (gdk_screen_get_default()))
		gtk_widget_set_sensitive (system_titlebar, FALSE);

	small_toolbar = gtk_check_button_new_with_label(_("Use small icons on the toolbars"));
	rena_hig_workarea_table_add_wide_control(table, &row, small_toolbar);

	rena_hig_workarea_table_add_section_title(table, &row, _("Controls"));

	album_art = gtk_check_button_new_with_label(_("Show Album art in Panel"));
	rena_hig_workarea_table_add_wide_control(table, &row, album_art);

	album_art_size_label = gtk_label_new(_("Size of Album art"));
	album_art_size = gtk_spin_button_new_with_range (24, 128, 2);

	rena_hig_workarea_table_add_row (table, &row, album_art_size_label, album_art_size);

	album_art_pattern_label = gtk_label_new(_("Album art file pattern"));

	album_art_pattern = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(album_art_pattern), ALBUM_ART_PATTERN_LEN);
	gtk_entry_set_activates_default (GTK_ENTRY(album_art_pattern), TRUE);

	gtk_widget_set_tooltip_text(album_art_pattern, album_art_pattern_info);

	rena_hig_workarea_table_add_row (table, &row, album_art_pattern_label, album_art_pattern);

	/* Store references */

	dialog->system_titlebar_w = system_titlebar;
	dialog->small_toolbar_w = small_toolbar;
	dialog->album_art_w = album_art;
	dialog->album_art_size_w = album_art_size;
	dialog->album_art_pattern_w = album_art_pattern;

	/* Setup signal handlers */


	if (rena_preferences_get_system_titlebar(dialog->preferences))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->system_titlebar_w), TRUE);

	if (rena_preferences_get_toolbar_size(dialog->preferences) == GTK_ICON_SIZE_SMALL_TOOLBAR)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->small_toolbar_w), TRUE);

	g_signal_connect(G_OBJECT(album_art), "toggled",
			 G_CALLBACK(toggle_album_art), dialog);

	return table;
}

static GtkWidget*
pref_create_general_page(RenaPreferencesDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *instant_filter, *aproximate_search, *window_state_combo, *restore_playlist, *add_recursively;
	guint row = 0;

	table = rena_hig_workarea_table_new();

	rena_hig_workarea_table_add_section_title(table, &row, _("Search"));

	instant_filter = gtk_check_button_new_with_label(_("Search while typing"));
	rena_hig_workarea_table_add_wide_control(table, &row, instant_filter);

	aproximate_search = gtk_check_button_new_with_label(_("Search similar words"));
	rena_hig_workarea_table_add_wide_control(table, &row, aproximate_search);

	rena_hig_workarea_table_add_section_title(table, &row, _("When starting rena"));

	window_state_combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(window_state_combo), _("Remember last window state"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(window_state_combo), _("Start normal"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(window_state_combo), _("Start fullscreen"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(window_state_combo), _("Start in system tray"));
	rena_hig_workarea_table_add_wide_control(table, &row, window_state_combo);

	restore_playlist = gtk_check_button_new_with_label(_("Restore last playlist"));
	rena_hig_workarea_table_add_wide_control(table, &row, restore_playlist);

	rena_hig_workarea_table_add_section_title(table, &row, _("When adding folders"));
	add_recursively = gtk_check_button_new_with_label(_("Add files recursively"));

	rena_hig_workarea_table_add_wide_control(table, &row, add_recursively);

	/* Store references */

	dialog->instant_filter_w = instant_filter;
	dialog->aproximate_search_w = aproximate_search;
	dialog->window_state_combo_w = window_state_combo;
	dialog->restore_playlist_w = restore_playlist;
	dialog->add_recursively_w = add_recursively;

	return table;
}

static GtkWidget*
pref_create_desktop_page(RenaPreferencesDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *show_icon_tray, *close_to_tray;
	guint row = 0;

	table = rena_hig_workarea_table_new();

	rena_hig_workarea_table_add_section_title(table, &row, _("Desktop"));

	show_icon_tray = gtk_check_button_new_with_label(_("Show Rena icon in the notification area"));
	rena_hig_workarea_table_add_wide_control(table, &row, show_icon_tray);

	close_to_tray = gtk_check_button_new_with_label(_("Minimize Rena when closing window"));
	rena_hig_workarea_table_add_wide_control(table, &row, close_to_tray);

	/* Store references. */

	dialog->show_icon_tray_w = show_icon_tray;
	dialog->close_to_tray_w = close_to_tray;

	return table;
}

#ifdef HAVE_LIBPEAS
static GtkWidget*
pref_create_plugins_page (RenaPreferencesDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *view, *sw;
	guint row = 0;

	table = rena_hig_workarea_table_new ();

	rena_hig_workarea_table_add_section_title (table, &row, _("Plugins"));

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
	                                     GTK_SHADOW_IN);

	view = peas_gtk_plugin_manager_view_new (peas_engine_get_default ());
	gtk_container_add (GTK_CONTAINER (sw), view);

	rena_hig_workarea_table_add_wide_tall_control (table, &row, sw);

	return table;
}
#endif

void
rena_preferences_dialog_connect_handler (RenaPreferencesDialog *dialog,
                                           GCallback          callback,
                                           gpointer           user_data)
{
	g_signal_connect (G_OBJECT(dialog), "response",
	                  G_CALLBACK(callback), user_data);
}

void
rena_preferences_dialog_disconnect_handler (RenaPreferencesDialog *dialog,
                                              GCallback          callback,
                                              gpointer           user_data)
{
	g_signal_handlers_disconnect_by_func (dialog,
	                                      callback,
	                                      user_data);
}


void
rena_preferences_dialog_show (RenaPreferencesDialog *dialog)
{
	RenaDatabaseProvider *provider;
	GSList *library_list = NULL;

	if (string_is_empty (rena_preferences_get_installed_version (dialog->preferences))) {
		provider = rena_database_provider_get ();
		library_list = rena_database_provider_get_list (provider);
		g_object_unref (G_OBJECT (provider));

		rena_preferences_dialog_set_library_list (dialog->library_view_w, library_list);
		g_slist_free_full (library_list, g_object_unref);
	}
	gtk_notebook_set_current_page (GTK_NOTEBOOK(dialog->notebook), 0);
	gtk_widget_show (GTK_WIDGET(dialog));
}

void
rena_preferences_dialog_set_parent (RenaPreferencesDialog *dialog, GtkWidget *parent)
{
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
}


/* Rena preferences dialog object */

static void
rena_preferences_dialog_dispose (GObject *object)
{
	RenaPreferencesDialog *dialog = RENA_PREFERENCES_DIALOG(object);
	if (dialog->preferences) {
		g_object_unref (dialog->preferences);
		dialog->preferences = NULL;
	}
	(*G_OBJECT_CLASS (rena_preferences_dialog_parent_class)->dispose) (object);
}

static void
rena_preferences_dialog_finalize (GObject *object)
{
	RenaPreferencesDialog *dialog = RENA_PREFERENCES_DIALOG(object);

	rena_preferences_tab_free (dialog->audio_tab);
	rena_preferences_tab_free (dialog->desktop_tab);
	rena_preferences_tab_free (dialog->services_tab);

	(*G_OBJECT_CLASS (rena_preferences_dialog_parent_class)->finalize) (object);
}

static void
rena_preferences_dialog_class_init (RenaPreferencesDialogClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS(klass);
	gobject_class->dispose = rena_preferences_dialog_dispose;
	gobject_class->finalize = rena_preferences_dialog_finalize;
}

static void
rena_preferences_dialog_init (RenaPreferencesDialog *dialog)
{
	RenaHeader *header;
	GtkWidget *pref_notebook;
	GtkWidget *audio_vbox, *playback_vbox, *appearance_vbox, *library_vbox, *general_vbox, *desktop_vbox;
	GtkWidget *label_playback, *label_appearance, *label_library, *label_general;
	#ifdef HAVE_LIBPEAS
	GtkWidget *plugins_vbox;
	GtkWidget *label_plugins;
	#endif

	/* Preferences instance */

	dialog->preferences = rena_preferences_get();

	/* The main preferences dialog */

	gtk_window_set_title (GTK_WINDOW(dialog), _("Preferences"));

	gtk_dialog_add_buttons (GTK_DIALOG(dialog),
	                        _("_Cancel"), GTK_RESPONSE_CANCEL,
	                        _("_Ok"), GTK_RESPONSE_OK,
	                        NULL);

	/* Labels */

	label_appearance = gtk_label_new(_("Appearance"));
	label_playback = gtk_label_new(_("Playback"));
	label_library = gtk_label_new(_("Library"));
	label_general = gtk_label_new(_("General"));
	#ifdef HAVE_LIBPEAS
	label_plugins = gtk_label_new(_("Plugins"));
	#endif

	/* Notebook, pages et al. */

	pref_notebook = gtk_notebook_new();

	gtk_container_set_border_width (GTK_CONTAINER(pref_notebook), 4);

	/* Library */

	library_vbox = pref_create_library_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(pref_notebook), library_vbox, label_library);
	gtk_widget_show_all (library_vbox);

	/* Fose hide infobar */
	rena_preferences_set_lock_library (dialog->preferences, FALSE);

	/* Playback */

	playback_vbox = pref_create_playback_page (dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(pref_notebook), playback_vbox, label_playback);
	gtk_widget_show_all (playback_vbox);

	/* Audio */

	dialog->audio_tab = rena_preferences_tab_new (_("Audio"));
	#ifndef G_OS_WIN32
	audio_vbox = pref_create_audio_page(dialog);
	rena_preferences_tab_append_setting (dialog->audio_tab, audio_vbox, FALSE);
	#endif
	rena_preferences_notebook_append_tab (pref_notebook, dialog->audio_tab);

	appearance_vbox = pref_create_appearance_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(pref_notebook), appearance_vbox, label_appearance);
	gtk_widget_show_all (appearance_vbox);

	general_vbox = pref_create_general_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(pref_notebook), general_vbox, label_general);
	gtk_widget_show_all (general_vbox);

	dialog->desktop_tab = rena_preferences_tab_new (_("Desktop"));
	desktop_vbox = pref_create_desktop_page(dialog);
	rena_preferences_tab_append_setting (dialog->desktop_tab, desktop_vbox, FALSE);

	dialog->services_tab = rena_preferences_tab_new (_("Services"));

	rena_preferences_notebook_append_tab (pref_notebook, dialog->desktop_tab);
	rena_preferences_notebook_append_tab (pref_notebook, dialog->services_tab);

	#ifdef HAVE_LIBPEAS
	plugins_vbox = pref_create_plugins_page(dialog);
	gtk_notebook_append_page(GTK_NOTEBOOK(pref_notebook), plugins_vbox, label_plugins);
	gtk_widget_show_all (plugins_vbox);
	#endif

	/* Add to dialog */

	header = rena_header_new ();
	rena_header_set_title (header, _("Preferences of Rena"));
	rena_header_set_icon_name (header, "rena");

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), GTK_WIDGET(header), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), pref_notebook, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET(header));
	gtk_widget_show (pref_notebook);

	/* Setup signal handlers */

	g_signal_connect (G_OBJECT(dialog), "response",
	                  G_CALLBACK(rena_preferences_dialog_response), dialog);
	g_signal_connect (G_OBJECT(dialog), "delete_event",
	                  G_CALLBACK(rena_preferences_dialog_delete), dialog);

	rena_preferences_dialog_init_settings(dialog);

	toggle_album_art(GTK_TOGGLE_BUTTON(dialog->album_art_w), dialog);

	gtk_dialog_set_default_response(GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	dialog->notebook = pref_notebook;
}

RenaPreferencesDialog *
rena_preferences_dialog_get (void)
{
	static RenaPreferencesDialog *dialog = NULL;

	if (G_UNLIKELY (dialog == NULL))
	{
		CDEBUG(DBG_INFO, "Creating a new RenaPreferencesDialog instance");

		dialog = g_object_new (RENA_TYPE_PREFERENCES_DIALOG, NULL);

		g_object_add_weak_pointer(G_OBJECT (dialog),
		                          (gpointer) &dialog);
	}
	else {
		g_object_ref (G_OBJECT (dialog));
	}

	return dialog;
}


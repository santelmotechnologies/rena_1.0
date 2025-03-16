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

#include "rena-scanner.h"

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include "rena-background-task-bar.h"
#include "rena-background-task-widget.h"
#include "rena-database-provider.h"
#include "rena-file-utils.h"
#include "rena-musicobject-mgmt.h"
#include "rena-playlists-mgmt.h"
#include "rena-simple-async.h"
#include "rena-utils.h"

struct _RenaScanner {
	/* Widgets */
	RenaBackgroundTaskWidget *task_widget;

	/* Cache */
	GHashTable        *tracks_table;
	GSList            *folder_list;
	GSList            *folder_scanned;
	GSList            *playlists;
	gchar             *curr_provider;

	GTimeVal          last_update;
	/* Threads */
	GThread           *no_files_thread;
	GThread           *worker_thread;
	/* Mutex to protect progress */
	GMutex             no_files_mutex;
	GMutex             files_scanned_mutex;
	/* Progress of threads */
	guint              no_files;
	guint              files_scanned;
	/* Cancellation safe */
	GCancellable      *cancellable;
	/* Timeout of update progress, also used as operating flag*/
	guint              update_timeout;
};

/* Update the dialog. */

static gboolean
rena_scanner_update_progress(gpointer user_data)
{
	gdouble fraction = 0.0;
	gint files_scanned = 0;
	gint no_files;
	gchar *data = NULL;

	RenaScanner *scanner = user_data;

	if(g_cancellable_is_cancelled (scanner->cancellable))
		return FALSE;

	g_mutex_lock (&scanner->no_files_mutex);
	no_files = scanner->no_files;
	g_mutex_unlock (&scanner->no_files_mutex);

	g_mutex_lock (&scanner->files_scanned_mutex);
	files_scanned = scanner->files_scanned;
	g_mutex_unlock (&scanner->files_scanned_mutex);

	if(no_files > 0) {
		fraction = (gdouble)files_scanned / (gdouble)no_files;
		rena_background_task_widget_set_job_progress (scanner->task_widget, fraction*100);

		data = g_strdup_printf(_("%i files analyzed of %i detected"), files_scanned, no_files);
		rena_background_task_widget_set_description (scanner->task_widget, data);
		g_free(data);
	}
	else {
		rena_background_task_widget_set_job_progress (scanner->task_widget, 0);
		rena_background_task_widget_set_description (scanner->task_widget, _("Searching files to analyze"));
	}

	return TRUE;
}

/* Thread that counts all files in the library */

static gpointer
rena_scanner_count_no_files_worker(gpointer data)
{
	GSList *list;
	gint no_files = 0;

	RenaScanner *scanner = data;

	for(list = scanner->folder_list ; list != NULL ; list = list->next) {
		if(g_cancellable_is_cancelled (scanner->cancellable))
			break;

		no_files += rena_get_dir_count(list->data, scanner->cancellable);

		g_mutex_lock (&scanner->no_files_mutex);
		scanner->no_files = no_files;
		g_mutex_unlock (&scanner->no_files_mutex);
	}

	return NULL;
}

/* Function that is executed at the end of analyze the files,
 * or if the analysis was canceled.
 * This runs on the main thread. So, can show a dialog.
 * Finally, frees all memory. */

static void
rena_scanner_add_track_db(gpointer key,
                            gpointer value,
                            gpointer user_data)
{
	RenaMusicobject *mobj = value;
	RenaDatabase *database = user_data;

	rena_database_add_new_musicobject (database, mobj);

	rena_process_gtk_events ();
}

static GSList *
rena_scanner_clean_playlist (GSList *list)
{
	gchar *file = NULL;
	GSList *l, *tmp = NULL;

	for (l=list; l != NULL; l = l->next) {
		file = l->data;

		if (g_file_test(file, G_FILE_TEST_EXISTS))
			tmp = g_slist_prepend(tmp, file);
		else
			g_free (file);
	}
	g_slist_free (list);

	return g_slist_reverse(tmp);
}

static void
rena_scanner_import_playlist (RenaDatabase *database,
                                const gchar *playlist_file)
{
	gchar *playlist = NULL;
	gint playlist_id = 0;
	GSList *list = NULL, *i = NULL;

	playlist = get_display_filename(playlist_file, FALSE);

	if (rena_database_find_playlist(database, playlist))
		goto duplicated;

#ifdef HAVE_PLPARSER
	gchar *uri = g_filename_to_uri (playlist_file, NULL, NULL);
	list = rena_totem_pl_parser_parse_from_uri(uri);
	g_free (uri);
#else
	list = rena_pl_parser_parse_from_file_by_extension (playlist_file);
#endif

	list = rena_scanner_clean_playlist (list);
	if (list) {
		playlist_id = rena_database_add_new_playlist (database, playlist);
		for (i = list; i != NULL; i = i->next) {
			rena_database_add_playlist_track (database, playlist_id, i->data);
			g_free(i->data);
		}
		g_slist_free(list);
	}

duplicated:
	g_free(playlist);
}

static void
rena_scanner_finished_dialog_response_cb (GtkDialog *dialog, gint response, gpointer data)
{
	RenaScanner *scanner = data;

	if(!scanner->update_timeout)
		gtk_widget_destroy(GTK_WIDGET(dialog));

}

static gboolean
rena_scanner_finished_dialog_delete (GtkDialog *dialog, GdkEvent  *event, gpointer data)
{
	RenaScanner *scanner = data;

	if(scanner->update_timeout)
		return TRUE;

	return FALSE;
}

static gboolean
rena_scanner_worker_finished (gpointer data)
{
	RenaBackgroundTaskBar *taskbar;
	RenaPreferences *preferences;
	RenaDatabase *database;
	RenaDatabaseProvider *provider;
	GtkWidget *msg_dialog;
	gchar *last_scan_time = NULL;
	GSList *list;

	RenaScanner *scanner = data;

	/* Stop updates */

	g_source_remove(scanner->update_timeout);

	/* Ensure that the other thread has finished */

	g_thread_join (scanner->no_files_thread);

	/* If not cancelled, update database and show a dialog */

	if(!g_cancellable_is_cancelled (scanner->cancellable))
	{
		/* Show the dialog and remove the scanner widget */

		msg_dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(scanner->task_widget))),
		                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		                                    GTK_MESSAGE_INFO,
		                                    GTK_BUTTONS_OK,
		                                    "%s",
		                                    _("Library scan complete"));

		g_signal_connect(G_OBJECT(msg_dialog), "response",
		                 G_CALLBACK(rena_scanner_finished_dialog_response_cb),
		                 scanner);

		g_signal_connect(G_OBJECT(msg_dialog), "delete-event",
		                 G_CALLBACK(rena_scanner_finished_dialog_delete),
		                 scanner);

		gtk_widget_show_all(msg_dialog);

		taskbar = rena_background_task_bar_get ();
		rena_background_task_bar_remove_widget (taskbar, GTK_WIDGET(scanner->task_widget));
		g_object_unref(G_OBJECT(taskbar));

		/* Save new database and update the library view */

		set_watch_cursor(msg_dialog);

		database = rena_database_get();
		provider = rena_database_provider_get ();

		rena_database_begin_transaction (database);

		/* Remove songs of local providers */

		for (list = scanner->folder_list; list != NULL; list = list->next)
			rena_provider_forget_songs (provider, list->data);

		/* Append new songs */

		g_hash_table_foreach (scanner->tracks_table,
		                      rena_scanner_add_track_db,
		                      database);

		/* Set local providers as visible */

		for (list = scanner->folder_list; list != NULL; list = list->next)
			rena_provider_set_visible (provider, list->data, TRUE);

		/* Import playlist detected. */

		for (list = scanner->playlists ; list != NULL; list = list->next)
			rena_scanner_import_playlist(database, list->data);

		rena_database_commit_transaction (database);

		rena_provider_update_done (provider);

		g_object_unref (provider);
		g_object_unref(database);

		remove_watch_cursor(msg_dialog);

		/* Save finished time and folders scanned. */

		g_get_current_time(&scanner->last_update);
		last_scan_time = g_time_val_to_iso8601(&scanner->last_update);
		preferences = rena_preferences_get();
		rena_preferences_set_string(preferences,
			                     GROUP_LIBRARY,
			                     KEY_LIBRARY_LAST_SCANNED,
			                     last_scan_time);
		g_free(last_scan_time);

		rena_preferences_set_lock_library (preferences, FALSE);

		g_object_unref(G_OBJECT(preferences));
	}
	else {
		preferences = rena_preferences_get();
		rena_preferences_set_lock_library (preferences, FALSE);
		g_object_unref(G_OBJECT(preferences));

		taskbar = rena_background_task_bar_get ();
		rena_background_task_bar_remove_widget (taskbar, GTK_WIDGET(scanner->task_widget));
		g_object_unref(G_OBJECT(taskbar));

	}

	/* Reset background task widget */

	rena_background_task_widget_set_job_progress (scanner->task_widget, 0);
	rena_background_task_widget_set_description (scanner->task_widget, _("Searching files to analyze"));

	/* Clean memory */

	g_hash_table_remove_all(scanner->tracks_table);
	free_str_list(scanner->folder_list);
	scanner->folder_list = NULL;
	free_str_list(scanner->folder_scanned);
	scanner->folder_scanned = NULL;

	free_str_list(scanner->playlists);
	scanner->playlists = NULL;

	scanner->no_files = 0;
	scanner->files_scanned = 0;

	g_cancellable_reset (scanner->cancellable);
	scanner->update_timeout = 0;

	return FALSE;
}

/* Function that analyzes all files of library recursively */

static void
rena_scanner_scan_handler(RenaScanner *scanner, const gchar *dir_name)
{
	GDir *dir;
	const gchar *next_file = NULL;
	gchar *ab_file;
	GError *error = NULL;
	RenaMusicobject *mobj = NULL;
	RenaMediaType file_type;

	if(g_cancellable_is_cancelled (scanner->cancellable))
		return;

	dir = g_dir_open(dir_name, 0, &error);
	if (!dir) {
		g_critical("Unable to open library : %s", dir_name);
		return;
	}

	next_file = g_dir_read_name(dir);
	while (next_file) {
		if(g_cancellable_is_cancelled (scanner->cancellable)) {
			g_dir_close(dir);
			return;
		}

		ab_file = g_strconcat(dir_name, G_DIR_SEPARATOR_S, next_file, NULL);
		if (g_file_test(ab_file, G_FILE_TEST_IS_DIR))
			rena_scanner_scan_handler(scanner, ab_file);
		else {
			file_type = rena_file_get_media_type (ab_file);
			switch (file_type) {
				case MEDIA_TYPE_AUDIO:
					mobj = new_musicobject_from_file(ab_file, scanner->curr_provider);
					if (G_LIKELY(mobj))
						 g_hash_table_insert(scanner->tracks_table,
							                 g_strdup(rena_musicobject_get_file(mobj)),
							                 mobj);
					break;
				case MEDIA_TYPE_PLAYLIST:
					scanner->playlists = g_slist_prepend (scanner->playlists, g_strdup(ab_file));
					break;
				case MEDIA_TYPE_IMAGE:
				case MEDIA_TYPE_UNKNOWN:
				default:
					break;
			}

			g_mutex_lock (&scanner->files_scanned_mutex);
			scanner->files_scanned++;
			g_mutex_unlock (&scanner->files_scanned_mutex);
		}

		g_free(ab_file);
		next_file = g_dir_read_name(dir);
	}
	g_dir_close(dir);
}

/* Thread that analyze all files in the library */

static gpointer
rena_scanner_scan_worker(gpointer data)
{
	GSList *list;

	RenaScanner *scanner = data;

	for(list = scanner->folder_list ; list != NULL; list = list->next) {
		if(g_cancellable_is_cancelled (scanner->cancellable))
			break;

		if (scanner->curr_provider)
			g_free (scanner->curr_provider);
		scanner->curr_provider = g_strdup (list->data);

		rena_scanner_scan_handler(scanner, list->data);
	}

	return scanner;
}

/* Function that analyzes all files of library recursively */

static void
rena_scanner_update_handler(RenaScanner *scanner, const gchar *dir_name)
{
	GDir *dir;
	const gchar *next_file = NULL;
	gchar *ab_file = NULL, *s_ab_file = NULL;
	GError *error = NULL;
	struct stat sbuf;
	RenaMusicobject *mobj = NULL;

	if(g_cancellable_is_cancelled (scanner->cancellable))
		return;

	dir = g_dir_open(dir_name, 0, &error);
	if (!dir) {
		g_critical("Unable to open library : %s", dir_name);
		return;
	}

	next_file = g_dir_read_name(dir);
	while (next_file) {
		if(g_cancellable_is_cancelled (scanner->cancellable))
			return;

		ab_file = g_strconcat(dir_name, G_DIR_SEPARATOR_S, next_file, NULL);

		if (g_file_test(ab_file, G_FILE_TEST_IS_DIR)) {
			rena_scanner_update_handler(scanner, ab_file);
		}
		else {
			mobj = g_hash_table_lookup(scanner->tracks_table,
			                           ab_file);
			if(!mobj) {
				mobj = new_musicobject_from_file(ab_file, scanner->curr_provider);
				if (G_LIKELY(mobj))
					 g_hash_table_insert(scanner->tracks_table,
					                     g_strdup(rena_musicobject_get_file(mobj)),
					                     mobj);

			}
			else {
				if ((g_stat(ab_file, &sbuf) == 0) &&
				    (sbuf.st_mtime > scanner->last_update.tv_sec)) {
					mobj = new_musicobject_from_file(ab_file, scanner->curr_provider);
					if (G_LIKELY(mobj)) {
						g_hash_table_replace(scanner->tracks_table,
						                     g_strdup(rena_musicobject_get_file(mobj)),
						                     mobj);
					}
				}
			}

			g_mutex_lock (&scanner->files_scanned_mutex);
			scanner->files_scanned++;
			g_mutex_unlock (&scanner->files_scanned_mutex);

			g_free(s_ab_file);
		}
		g_free(ab_file);
		next_file = g_dir_read_name(dir);
	}
	g_dir_close(dir);
}

/* Thread that analyze all files in the library */

static gpointer
rena_scanner_update_worker(gpointer data)
{
	GSList *list;
	GHashTableIter iter;
	gpointer key, value;
	gchar *file;

	RenaScanner *scanner = data;

	/* Clean removed files */

	g_hash_table_iter_init (&iter, scanner->tracks_table);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		file = key;
		if(g_cancellable_is_cancelled (scanner->cancellable))
			break;
		if(!g_file_test(file, G_FILE_TEST_EXISTS))
			g_hash_table_iter_remove(&iter);
	}

	/* Then update files changed.. */
	for(list = scanner->folder_list ; list != NULL; list = list->next) {
		if(g_cancellable_is_cancelled (scanner->cancellable))
			break;

		if(is_present_str_list(list->data, scanner->folder_scanned)) {
			if (scanner->curr_provider)
				g_free (scanner->curr_provider);
			scanner->curr_provider = g_strdup (list->data);
			rena_scanner_update_handler(scanner, list->data);
		}
	}

	/* Then add news folder in library */
	for(list = scanner->folder_list; list != NULL; list = list->next) {
		if(g_cancellable_is_cancelled (scanner->cancellable))
			break;

		if(!is_present_str_list(list->data, scanner->folder_scanned)) {
			if (scanner->curr_provider)
				g_free (scanner->curr_provider);
			scanner->curr_provider = g_strdup (list->data);
			rena_scanner_scan_handler(scanner, list->data);
		}
	}

	return scanner;
}

void
rena_scanner_update_library(RenaScanner *scanner)
{
	RenaBackgroundTaskBar *taskbar;
	RenaPreferences *preferences;
	RenaDatabase *database;
	RenaDatabaseProvider *provider;
	RenaPreparedStatement *statement;
	RenaMusicobject *mobj = NULL;
	gchar *last_scan_time = NULL;
	const gchar *sql = NULL;
	guint location_id;
	GSList *list;

	if(scanner->update_timeout)
		return;

	preferences = rena_preferences_get();

	rena_preferences_set_lock_library (preferences, TRUE);

	/* Get last time that update the library and folders to analyze */

	last_scan_time = rena_preferences_get_string(preferences,
	                                               GROUP_LIBRARY,
	                                               KEY_LIBRARY_LAST_SCANNED);
	if (last_scan_time) {
		if (!g_time_val_from_iso8601(last_scan_time, &scanner->last_update))
			g_warning("Unable to convert last rescan time");
		g_free(last_scan_time);
	}
	g_object_unref(G_OBJECT(preferences));

	provider = rena_database_provider_get ();
	scanner->folder_list = rena_database_provider_get_list_by_type (provider, "local");
	scanner->folder_scanned = rena_provider_get_handled_list_by_type (provider, "local");
	g_object_unref (provider);

	/* Update the gui */

	scanner->update_timeout =
		g_timeout_add_seconds(1, (GSourceFunc)rena_scanner_update_progress, scanner);

	taskbar = rena_background_task_bar_get ();
	rena_background_task_bar_prepend_widget (taskbar, GTK_WIDGET(scanner->task_widget));
	g_object_unref(G_OBJECT(taskbar));

	/* Append the files from database that no changed. */

	database = rena_database_get();
	for (list = scanner->folder_scanned; list != NULL; list = list->next)
	{
		if (rena_string_list_is_present (scanner->folder_list, list->data))
		{
			sql = "SELECT location FROM TRACK WHERE provider = ?";
			statement = rena_database_create_statement (database, sql);

			rena_prepared_statement_bind_int (statement, 1,
				rena_database_find_provider (database, list->data));

			while (rena_prepared_statement_step (statement)) {
				location_id = rena_prepared_statement_get_int (statement, 0);
				mobj = new_musicobject_from_db(database, location_id);
				if (G_LIKELY(mobj)) {
					g_hash_table_insert(scanner->tracks_table,
					                    g_strdup(rena_musicobject_get_file(mobj)),
					                    mobj);
				}

				rena_process_gtk_events ();
			}
			rena_prepared_statement_free (statement);
		}
	}
	g_object_unref(database);

	/* Launch threads */

	scanner->no_files_thread = g_thread_new("Count no files", rena_scanner_count_no_files_worker, scanner);

	scanner->worker_thread = rena_async_launch_full(rena_scanner_update_worker,
	                                                  rena_scanner_worker_finished,
	                                                  scanner);
}

void
rena_scanner_scan_library(RenaScanner *scanner)
{
	RenaBackgroundTaskBar *taskbar;
	RenaPreferences *preferences;
	RenaDatabaseProvider *provider;
	gchar *last_scan_time = NULL;

	if(scanner->update_timeout)
		return;

	preferences = rena_preferences_get();
	rena_preferences_set_lock_library (preferences, TRUE);

	/* Get last time that update the library and folders to analyze */

	last_scan_time = rena_preferences_get_string(preferences,
	                                               GROUP_LIBRARY,
	                                               KEY_LIBRARY_LAST_SCANNED);
	if (last_scan_time) {
		if (!g_time_val_from_iso8601(last_scan_time, &scanner->last_update))
			g_warning("Unable to convert last rescan time");
		g_free(last_scan_time);
	}
	g_object_unref(G_OBJECT(preferences));

	provider = rena_database_provider_get ();
	scanner->folder_list = rena_database_provider_get_list_by_type (provider, "local");
	scanner->folder_scanned = rena_provider_get_handled_list_by_type (provider, "local");
	g_object_unref (provider);

	/* Update the gui */

	scanner->update_timeout = g_timeout_add_seconds(1, (GSourceFunc)rena_scanner_update_progress, scanner);

	taskbar = rena_background_task_bar_get ();
	rena_background_task_bar_prepend_widget (taskbar, GTK_WIDGET(scanner->task_widget));
	g_object_unref(G_OBJECT(taskbar));

	/* Launch threads */

	scanner->no_files_thread = g_thread_new("Count no files", rena_scanner_count_no_files_worker, scanner);

	scanner->worker_thread = rena_async_launch_full(rena_scanner_scan_worker,
	                                                  rena_scanner_worker_finished,
	                                                  scanner);
}

void
rena_scanner_free(RenaScanner *scanner)
{
	if(scanner->update_timeout) {
		g_cancellable_cancel (scanner->cancellable);
		g_thread_join (scanner->no_files_thread);
		g_thread_join (scanner->worker_thread);
	}

	g_hash_table_destroy(scanner->tracks_table);
	free_str_list(scanner->folder_list);
	free_str_list(scanner->folder_scanned);
	g_mutex_clear (&scanner->no_files_mutex);
	g_mutex_clear (&scanner->files_scanned_mutex);
	g_object_unref(scanner->cancellable);

	g_slice_free (RenaScanner, scanner);
}

RenaScanner *
rena_scanner_new()
{
	RenaScanner *scanner;
	RenaBackgroundTaskWidget *task_widget;

	scanner = g_slice_new0(RenaScanner);

	scanner->cancellable = g_cancellable_new ();
	g_object_ref (G_OBJECT(scanner->cancellable));

	/* Create background task widget */

	task_widget = rena_background_task_widget_new (_("Searching files to analyze"),
	                                                 "drive-harddisk",
	                                                 0,
	                                                 scanner->cancellable);
	g_object_ref (G_OBJECT(task_widget));

	/* Init the rest and save references */

	scanner->task_widget = task_widget;
	scanner->tracks_table = g_hash_table_new_full (g_str_hash,
	                                               g_str_equal,
	                                               g_free,
	                                               g_object_unref);
	scanner->files_scanned = 0;
	g_mutex_init (&scanner->files_scanned_mutex);
	scanner->no_files = 0;
	g_mutex_init (&scanner->no_files_mutex);
	scanner->update_timeout = 0;

	return scanner;
}

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

#include "rena-database.h"
#include "rena-prepared-statement-private.h"
#include "rena-provider.h"

#include "rena-database-provider.h"

struct _RenaDatabaseProviderPrivate
{
	RenaDatabase *database;
};

G_DEFINE_TYPE_WITH_PRIVATE(RenaDatabaseProvider, rena_database_provider, G_TYPE_OBJECT)

enum {
	SIGNAL_WANT_UPDATE,
	SIGNAL_WANT_UPGRADE,
	SIGNAL_WANT_REMOVE,
	SIGNAL_UPDATE_DONE,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

gchar *
rena_database_provider_get_kind_from_id (RenaDatabaseProvider *database_provider,
                                           gint                    kind_id);


/* Provider */

void
rena_provider_add_new (RenaDatabaseProvider *provider,
                         const gchar            *name,
                         const gchar            *type,
                         const gchar            *friendly_name,
                         const gchar            *icon_name)
{
	gint provider_type_id = 0;
	RenaPreparedStatement *statement;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "INSERT INTO PROVIDER (name, type, friendly_name, icon_name, visible, ignore) VALUES (?, ?, ?, ?, ?, ?)";

	if ((provider_type_id = rena_database_find_provider_type (priv->database, type)) == 0)
		provider_type_id = rena_database_add_new_provider_type (priv->database, type);

	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_string (statement, 1, name);
	rena_prepared_statement_bind_int    (statement, 2, provider_type_id);
	rena_prepared_statement_bind_string (statement, 3, friendly_name);
	rena_prepared_statement_bind_string (statement, 4, icon_name);
	rena_prepared_statement_bind_int    (statement, 5, 0);  // No visible by default.
	rena_prepared_statement_bind_int    (statement, 6, 0);  // No ignore by default.
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);
}

void
rena_provider_remove (RenaDatabaseProvider *provider,
                        const gchar            *name)
{
	RenaPreparedStatement *statement;
	gint provider_id = 0;
	const gchar *sql;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	if ((provider_id = rena_database_find_provider (priv->database, name)) == 0)
		return;

	/* Delete all entries from PLAYLIST_TRACKS which match provider */

	sql = "DELETE FROM PLAYLIST_TRACKS WHERE file IN (SELECT name FROM location WHERE id IN (SELECT location FROM TRACK WHERE provider = ?))";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1, provider_id);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);

	/* Delete all tracks of provider */

	sql = "DELETE FROM TRACK WHERE provider = ?";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1, provider_id);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);

	/* Delete the location entries */

	sql = "DELETE FROM LOCATION WHERE id NOT IN (SELECT location FROM TRACK)";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);

	/* Delete Provider */

	sql = "DELETE FROM PROVIDER WHERE id = ?";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1, provider_id);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);

	/* Now flush unused artists, albums, genres, years */

	rena_database_flush_stale_entries (priv->database);
}

gboolean
rena_provider_exist (RenaDatabaseProvider *provider,
                       const gchar            *name)
{
	gint provider_id = 0;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	provider_id = rena_database_find_provider (priv->database, name);

	return (provider_id != 0);
}

void
rena_provider_forget_songs (RenaDatabaseProvider *provider,
                              const gchar            *name)
{
	RenaPreparedStatement *statement;
	gint provider_id = 0;
	const gchar *sql;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	if ((provider_id = rena_database_find_provider (priv->database, name)) == 0)
		return;

	/* Delete all tracks of provider */

	sql = "DELETE FROM TRACK WHERE provider = ?";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1, provider_id);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);

	/* Delete the location entries */

	sql = "DELETE FROM LOCATION WHERE id NOT IN (SELECT location FROM TRACK)";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);

	/* Delete all entries from PLAYLIST_TRACKS which match given dir */

	sql = "DELETE FROM PLAYLIST_TRACKS WHERE file NOT IN (SELECT name FROM LOCATION)";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);

	/* Now flush unused artists, albums, genres, years */

	rena_database_flush_stale_entries (priv->database);
}

GSList *
rena_provider_get_list (RenaDatabaseProvider *provider)
{
	RenaPreparedStatement *statement;
	GSList *list = NULL;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "SELECT name FROM PROVIDER";

	statement = rena_database_create_statement (priv->database, sql);
	while (rena_prepared_statement_step (statement)) {
		const gchar *name = rena_prepared_statement_get_string (statement, 0);
		list = g_slist_append (list, g_strdup(name));
	}

	rena_prepared_statement_free (statement);

	return list;
}

GSList *
rena_provider_get_visible_list (RenaDatabaseProvider *provider, gboolean visibles)
{
	RenaPreparedStatement *statement;
	GSList *list = NULL;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "SELECT name FROM PROVIDER WHERE visible = ?";

	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1,
	                                    visibles ? 1 : 0);
	while (rena_prepared_statement_step (statement)) {
		const gchar *name = rena_prepared_statement_get_string (statement, 0);
		list = g_slist_append (list, g_strdup(name));
	}

	rena_prepared_statement_free (statement);

	return list;
}

GSList *
rena_provider_get_handled_list (RenaDatabaseProvider *provider)
{
	RenaPreparedStatement *statement;
	GSList *list = NULL;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "SELECT name FROM PROVIDER WHERE id IN (SELECT provider FROM TRACK)";

	statement = rena_database_create_statement (priv->database, sql);
	while (rena_prepared_statement_step (statement)) {
		const gchar *name = rena_prepared_statement_get_string (statement, 0);
		list = g_slist_append (list, g_strdup(name));
	}

	rena_prepared_statement_free (statement);

	return list;
}

GSList *
rena_database_provider_get_list_by_type (RenaDatabaseProvider *provider,
                                           const gchar            *provider_type)
{
	RenaPreparedStatement *statement;
	GSList *list = NULL;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "SELECT name FROM PROVIDER WHERE type = ? AND ignore = ?";

	statement = rena_database_create_statement (priv->database, sql);

	rena_prepared_statement_bind_int (statement, 1,
		rena_database_find_provider_type (priv->database, provider_type));
	rena_prepared_statement_bind_int (statement, 2, 0);

	while (rena_prepared_statement_step (statement)) {
		const gchar *name = rena_prepared_statement_get_string (statement, 0);
		list = g_slist_append (list, g_strdup(name));
	}
	rena_prepared_statement_free (statement);

	return list;
}

GSList *
rena_provider_get_handled_list_by_type (RenaDatabaseProvider *provider,
                                          const gchar            *provider_type)
{
	RenaPreparedStatement *statement;
	GSList *list = NULL;

	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "SELECT name FROM PROVIDER WHERE id IN (SELECT provider FROM TRACK) AND type = ? AND ignore = ?";

	statement = rena_database_create_statement (priv->database, sql);

	rena_prepared_statement_bind_int (statement, 1,
		rena_database_find_provider_type (priv->database, provider_type));
	rena_prepared_statement_bind_int (statement, 2, 0);

	while (rena_prepared_statement_step (statement)) {
		const gchar *name = rena_prepared_statement_get_string (statement, 0);
		list = g_slist_append (list, g_strdup(name));
	}

	rena_prepared_statement_free (statement);

	return list;
}

GSList *
rena_database_provider_get_list (RenaDatabaseProvider *database_provider)
{
	RenaPreparedStatement *statement;
	RenaProvider *provider;
	GSList *list = NULL;

	RenaDatabaseProviderPrivate *priv = database_provider->priv;

	const gchar *sql = "SELECT name, type, friendly_name, icon_name, visible, ignore FROM PROVIDER";

	statement = rena_database_create_statement (priv->database, sql);
	while (rena_prepared_statement_step (statement)) {
		const gchar *name = rena_prepared_statement_get_string (statement, 0);
		gchar *kind = rena_database_provider_get_kind_from_id (database_provider,
			rena_prepared_statement_get_int (statement, 1));
		const gchar *friendly_name = rena_prepared_statement_get_string (statement, 2);
		const gchar *icon_name = rena_prepared_statement_get_string (statement, 3);
		gint visible = rena_prepared_statement_get_int (statement, 4);
		gint ignore = rena_prepared_statement_get_int (statement, 5);

		provider = rena_provider_new (name, kind, friendly_name, icon_name,
		                                visible,
		                                ignore);
		list = g_slist_append (list, provider);
		g_free(kind);
	}
	rena_prepared_statement_free (statement);

	return list;
}


void
rena_provider_set_visible (RenaDatabaseProvider *provider,
                             const gchar            *name,
                             gboolean                visible)
{
	RenaPreparedStatement *statement;
	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "UPDATE PROVIDER SET visible = ? WHERE name = ?";

	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1, visible ? 1 : 0);
	rena_prepared_statement_bind_string (statement, 2, name);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);
}

void
rena_provider_set_ignore (RenaDatabaseProvider *provider,
                            const gchar            *name,
                            gboolean                ignore)
{
	RenaPreparedStatement *statement;
	RenaDatabaseProviderPrivate *priv = provider->priv;

	const gchar *sql = "UPDATE PROVIDER SET ignore = ? WHERE name = ?";

	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1, ignore ? 1 : 0);
	rena_prepared_statement_bind_string (statement, 2, name);
	rena_prepared_statement_step (statement);
	rena_prepared_statement_free (statement);
}

gchar *
rena_database_provider_get_friendly_name (RenaDatabaseProvider *provider, const gchar *name)
{
	RenaPreparedStatement *statement;
	RenaDatabaseProviderPrivate *priv = provider->priv;
	gchar *friendly_name = NULL;

	const gchar *sql = "SELECT friendly_name FROM PROVIDER WHERE name = ?";

	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_string (statement, 1, name);
	rena_prepared_statement_step (statement);
	friendly_name = g_strdup(rena_prepared_statement_get_string (statement, 0));
	rena_prepared_statement_free (statement);

	return friendly_name;
}

gchar *
rena_database_provider_get_icon_name (RenaDatabaseProvider *provider, const gchar *name)
{
	RenaPreparedStatement *statement;
	RenaDatabaseProviderPrivate *priv = provider->priv;
	gchar *icon_name = NULL;

	const gchar *sql = "SELECT icon_name FROM PROVIDER WHERE name = ?";

	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_string (statement, 1, name);
	rena_prepared_statement_step (statement);
	icon_name = g_strdup(rena_prepared_statement_get_string (statement, 0));
	rena_prepared_statement_free (statement);

	return icon_name;
}

gchar *
rena_database_provider_get_kind_from_id (RenaDatabaseProvider *database_provider,
                                           gint                    kind_id)
{
	RenaPreparedStatement *statement;
	gchar *kind = NULL;

	RenaDatabaseProviderPrivate *priv = database_provider->priv;

	const gchar *sql = "SELECT name FROM PROVIDER_TYPE WHERE id = ?";
	statement = rena_database_create_statement (priv->database, sql);
	rena_prepared_statement_bind_int (statement, 1, kind_id);

	rena_prepared_statement_step (statement);
	kind = g_strdup(rena_prepared_statement_get_string (statement, 0));

	rena_prepared_statement_free (statement);

	return kind;
}

/*
 * Signals.
 */

void
rena_provider_want_upgrade (RenaDatabaseProvider *provider, gint provider_id)
{
	g_return_if_fail(RENA_IS_DATABASE_PROVIDER(provider));

	g_signal_emit (provider, signals[SIGNAL_WANT_UPDATE], 0, provider_id);
}

void
rena_provider_want_update (RenaDatabaseProvider *provider, gint provider_id)
{
	g_return_if_fail(RENA_IS_DATABASE_PROVIDER(provider));

	g_signal_emit (provider, signals[SIGNAL_WANT_UPDATE], 0, provider_id);
}

void
rena_provider_want_remove (RenaDatabaseProvider *provider, gint provider_id)
{
	g_return_if_fail(RENA_IS_DATABASE_PROVIDER(provider));

	g_signal_emit (provider, signals[SIGNAL_WANT_UPDATE], 0, provider_id);
}

void
rena_provider_update_done (RenaDatabaseProvider *provider)
{
	g_return_if_fail(RENA_IS_DATABASE_PROVIDER(provider));

	g_signal_emit (provider, signals[SIGNAL_UPDATE_DONE], 0);
}


/*
 * RenaDatabaseProvider implementation.
 */

static void
rena_database_provider_dispose (GObject *object)
{
	RenaDatabaseProvider *provider = RENA_DATABASE_PROVIDER(object);
	RenaDatabaseProviderPrivate *priv = provider->priv;

	if (priv->database) {
		g_object_unref (priv->database);
		priv->database = NULL;
	}

	G_OBJECT_CLASS(rena_database_provider_parent_class)->dispose(object);
}

static void
rena_database_provider_class_init (RenaDatabaseProviderClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = rena_database_provider_dispose;

	signals[SIGNAL_WANT_UPGRADE] =
		g_signal_new ("want-upgrade",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaDatabaseProviderClass, want_upgrade),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT);

	signals[SIGNAL_WANT_UPDATE] =
		g_signal_new ("want-update",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaDatabaseProviderClass, want_update),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT);

	signals[SIGNAL_WANT_REMOVE] =
		g_signal_new ("want-remove",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaDatabaseProviderClass, want_remove),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT);

	signals[SIGNAL_UPDATE_DONE] =
		g_signal_new ("update-done",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaDatabaseProviderClass, update_done),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

static void
rena_database_provider_init (RenaDatabaseProvider *provider)
{
	provider->priv = G_TYPE_INSTANCE_GET_PRIVATE(provider,
	                                             RENA_TYPE_DATABASE_PROVIDER,
	                                             RenaDatabaseProviderPrivate);

	RenaDatabaseProviderPrivate *priv = provider->priv;

	/* Database instance */

	priv->database = rena_database_get ();
}

/**
 * rena_database_provider_get:
 *
 * Queries the global #RenaDatabaseProvider instance, which is
 * shared by all modules. The function automatically takes a
 * reference for the caller, so you'll need to call g_object_unref()
 * when you're done with it.
 *
 * Return value: the global #RenaDatabaseProvider instance.
 **/
RenaDatabaseProvider *
rena_database_provider_get (void)
{
   static RenaDatabaseProvider *provider = NULL;

   if (G_UNLIKELY (provider == NULL)) {
      provider = g_object_new(RENA_TYPE_DATABASE_PROVIDER, NULL);
      g_object_add_weak_pointer(G_OBJECT (provider),
                                (gpointer) &provider);
   }
   else {
      g_object_ref (G_OBJECT (provider));
   }

   return provider;
}

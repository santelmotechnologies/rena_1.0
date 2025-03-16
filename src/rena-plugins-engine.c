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

#include "rena-plugins-engine.h"

#include <libpeas/peas.h>

#include "rena-utils.h"
#include "rena-debug.h"
#include "rena.h"

struct _RenaPluginsEngine {
	GObject           _parent;

	GObject           *object;

	PeasEngine        *peas_engine;
	PeasExtensionSet  *peas_exten_set;

	gboolean           starting;
	gboolean           shutdown;
};

G_DEFINE_TYPE(RenaPluginsEngine, rena_plugins_engine, G_TYPE_OBJECT)

static void
on_extension_added (PeasExtensionSet  *set,
                    PeasPluginInfo    *info,
                    PeasExtension     *exten,
                    gpointer           data)
{
	peas_activatable_activate (PEAS_ACTIVATABLE (exten));
}

static void
on_extension_removed (PeasExtensionSet  *set,
                      PeasPluginInfo    *info,
                      PeasExtension     *exten,
                      gpointer           data)
{
	peas_activatable_deactivate (PEAS_ACTIVATABLE (exten));
}

gboolean
rena_plugins_engine_is_starting (RenaPluginsEngine *engine)
{
	return engine->starting;
}

gboolean
rena_plugins_engine_is_shutdown (RenaPluginsEngine *engine)
{
	return engine->shutdown;
}

void
rena_plugins_engine_shutdown (RenaPluginsEngine *engine)
{
	RenaPreferences *preferences;
	gchar **loaded_plugins = NULL;

	CDEBUG(DBG_PLUGIN,"Plugins engine shutdown");

	engine->shutdown = TRUE;

	loaded_plugins = peas_engine_get_loaded_plugins (engine->peas_engine);
	if (loaded_plugins) {
		preferences = rena_application_get_preferences (RENA_APPLICATION(engine->object));
		rena_preferences_set_string_list (preferences,
				                            "PLUGINS",
				                            "Activated",
				                            (const gchar * const*)loaded_plugins,
		                                     g_strv_length(loaded_plugins));

		g_strfreev(loaded_plugins);
	}
	peas_engine_set_loaded_plugins (engine->peas_engine, NULL);
}

void
rena_plugins_engine_startup (RenaPluginsEngine *engine)
{
	RenaPreferences *preferences;
	gchar **loaded_plugins = NULL;
	const gchar *default_plugins[] = {"notify", "mpris2", "song-info", NULL};

	CDEBUG(DBG_PLUGIN,"Plugins engine startup");

	preferences = rena_application_get_preferences (RENA_APPLICATION(engine->object));

	if (string_is_not_empty (rena_preferences_get_installed_version (preferences))) {
		loaded_plugins = rena_preferences_get_string_list (preferences,
		                                                     "PLUGINS",
		                                                     "Activated",
		                                                     NULL);

		if (loaded_plugins) {
			peas_engine_set_loaded_plugins (engine->peas_engine, (const gchar **) loaded_plugins);
			g_strfreev(loaded_plugins);
		}
	}
	else {
		peas_engine_set_loaded_plugins (engine->peas_engine, (const gchar **) default_plugins);
	}

	engine->starting = FALSE;
}

/*
 * RenaPluginsEngine
 */
static void
rena_plugins_engine_dispose (GObject *object)
{
	RenaPluginsEngine *engine = RENA_PLUGINS_ENGINE(object);

	CDEBUG(DBG_PLUGIN,"Dispose plugins engine");

	if (engine->peas_exten_set) {
		g_object_unref (engine->peas_exten_set);
		engine->peas_exten_set = NULL;
	}
	if (engine->peas_engine) {
		peas_engine_garbage_collect (engine->peas_engine);

		g_object_unref (engine->peas_engine);
		engine->peas_engine = NULL;
	}
	if (engine->object) {
		g_object_unref (engine->object);
		engine->object = NULL;
	}

	G_OBJECT_CLASS(rena_plugins_engine_parent_class)->dispose(object);
}

static void
rena_plugins_engine_class_init (RenaPluginsEngineClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->dispose = rena_plugins_engine_dispose;
}

static void
rena_plugins_engine_init (RenaPluginsEngine *engine)
{
	engine->peas_engine = peas_engine_get_default ();
	engine->starting = TRUE;
	engine->shutdown = FALSE;
}

RenaPluginsEngine *
rena_plugins_engine_new (GObject *object)
{
	RenaPluginsEngine *engine;

	CDEBUG(DBG_PLUGIN,"Create new plugins engine");

	engine = g_object_new (RENA_TYPE_PLUGINS_ENGINE, NULL);

	engine->object = g_object_ref(object);

	peas_engine_add_search_path (engine->peas_engine, LIBPLUGINDIR, USRPLUGINDIR);
	//peas_engine_add_search_path (engine->peas_engine, "/home/matias/Desarrollo/rena/plugins/", "/home/matias/Desarrollo/rena/plugins/");
	engine->peas_exten_set = peas_extension_set_new (engine->peas_engine,
	                                                 PEAS_TYPE_ACTIVATABLE,
	                                                 "object", object,
	                                                 NULL);

	g_signal_connect (engine->peas_exten_set, "extension-added",
	                  G_CALLBACK (on_extension_added), engine);
	g_signal_connect (engine->peas_exten_set, "extension-removed",
	                  G_CALLBACK (on_extension_removed), engine);

	return engine;
}

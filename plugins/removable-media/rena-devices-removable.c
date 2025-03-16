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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif
#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include <gudev/gudev.h>

#include <libpeas/peas.h>
#include <libpeas-gtk/peas-gtk.h>

#include "plugins/rena-plugin-macros.h"

#include "src/rena-device-client.h"
#include "src/rena-database-provider.h"
#include "src/rena-playback.h"
#include "src/rena-utils.h"
#include "src/rena.h"

#define RENA_TYPE_REMOVABLE_PLUGIN         (rena_removable_plugin_get_type ())
#define RENA_REMOVABLE_PLUGIN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), RENA_TYPE_REMOVABLE_PLUGIN, RenaRemovablePlugin))
#define RENA_REMOVABLE_PLUGIN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), RENA_TYPE_REMOVABLE_PLUGIN, RenaRemovablePlugin))
#define RENA_IS_REMOVABLE_PLUGIN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), RENA_TYPE_REMOVABLE_PLUGIN))
#define RENA_IS_REMOVABLE_PLUGIN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), RENA_TYPE_REMOVABLE_PLUGIN))
#define RENA_REMOVABLE_PLUGIN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), RENA_TYPE_REMOVABLE_PLUGIN, RenaRemovablePluginClass))

typedef struct _RenaRemovablePluginPrivate RenaRemovablePluginPrivate;

struct _RenaRemovablePluginPrivate {
	RenaApplication  *rena;

	RenaDeviceClient *device_client;

	/* Gudev devie */
	guint64             bus_hooked;
	guint64             device_hooked;
	GUdevDevice        *u_device;

	/* Gio Volume */
	GVolume            *volume;

	/* Mount point. */
	gchar              *mount_path;
};

RENA_PLUGIN_REGISTER (RENA_TYPE_REMOVABLE_PLUGIN,
                        RenaRemovablePlugin,
                        rena_removable_plugin)

static void
rena_removable_clear_hook_device (RenaRemovablePlugin *plugin)
{
	RenaRemovablePluginPrivate *priv = plugin->priv;

	priv->bus_hooked = 0;
	priv->device_hooked = 0;

	if (priv->u_device) {
		g_object_unref (priv->u_device);
		priv->u_device = NULL;
	}
	if (priv->volume) {
		g_object_unref (priv->volume);
		priv->volume = NULL;
	}
	if (priv->mount_path) {
		g_free(priv->mount_path);
		priv->mount_path = NULL;
	}
}

static void
rena_block_device_add_to_library (RenaRemovablePlugin *plugin, GMount *mount)
{
	RenaDatabaseProvider *provider;
	RenaScanner *scanner;
	GSList *provider_list = NULL;
	GFile       *mount_point;
	gchar       *mount_path, *name;

	RenaRemovablePluginPrivate *priv = plugin->priv;

	mount_point = g_mount_get_root (mount);
	mount_path = g_file_get_path (mount_point);

	provider = rena_database_provider_get ();
	provider_list = rena_provider_get_list (provider);

	if (rena_string_list_is_not_present (provider_list, mount_path))
	{
		name = g_mount_get_name (mount);

		rena_provider_add_new (provider,
		                         mount_path,
		                         "local",
		                         name,
		                         "media-removable");

		scanner = rena_application_get_scanner (priv->rena);
		rena_scanner_update_library (scanner);

		g_free (name);
	}
	else
	{
		/* Show old backup */
		rena_provider_set_visible (provider, mount_path, TRUE);
		rena_provider_set_ignore (provider, mount_path, FALSE);
		rena_provider_update_done (provider);
	}
	g_slist_free_full (provider_list, g_free);

	priv->mount_path = g_strdup(mount_path);

	g_object_unref (provider);
	g_object_unref (mount_point);
	g_free (mount_path);
}

static void
rena_removable_drop_device_from_library (RenaRemovablePlugin *plugin)
{
	RenaDatabaseProvider *provider;
	GSList *provider_list = NULL;

	RenaRemovablePluginPrivate *priv = plugin->priv;

	provider = rena_database_provider_get ();
	provider_list = rena_provider_get_list (provider);

	if (rena_string_list_is_present (provider_list, priv->mount_path))
	{
		/* Hide the provider but leave it as backup */
		rena_provider_set_visible (provider, priv->mount_path, FALSE);
		rena_provider_set_ignore (provider, priv->mount_path, TRUE);
		rena_provider_update_done (provider);
	}

	g_slist_free_full (provider_list, g_free);
	g_object_unref (provider);
}


/*
 * Some functions to mount block removable.
 */

/* Decode the ID_FS_LABEL_ENC of block device.
 * Extentions copy of Thunar-volman code.
 * http://git.xfce.org/xfce/thunar-volman/tree/thunar-volman/tvm-gio-extensions.c */

static GVolume *
tvm_g_volume_monitor_get_volume_for_kind (GVolumeMonitor *monitor,
                                          const gchar    *kind,
                                          const gchar    *identifier)
{
	GVolume *volume = NULL;
	GList   *volumes;
	GList   *lp;
	gchar   *value;

	g_return_val_if_fail (G_IS_VOLUME_MONITOR (monitor), NULL);
	g_return_val_if_fail (kind != NULL && *kind != '\0', NULL);
	g_return_val_if_fail (identifier != NULL && *identifier != '\0', NULL);

	volumes = g_volume_monitor_get_volumes (monitor);

	for (lp = volumes; volume == NULL && lp != NULL; lp = lp->next) {
		value = g_volume_get_identifier (lp->data, kind);
		if (value == NULL)
			continue;
		if (g_strcmp0 (value, identifier) == 0)
			volume = g_object_ref (lp->data);
		g_free (value);
	}
	g_list_foreach (volumes, (GFunc)g_object_unref, NULL);
	g_list_free (volumes);

	return volume;
}

static void
rena_block_device_mount_finish (GVolume *volume, GAsyncResult *result, RenaRemovablePlugin *plugin)
{
	GtkWidget *dialog;
	GMount    *mount;
	GError    *error = NULL;
	gchar     *name = NULL, *primary = NULL;

	g_return_if_fail (G_IS_VOLUME (volume));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));

	RenaRemovablePluginPrivate *priv = plugin->priv;

	/* finish mounting the volume */
	if (!g_volume_mount_finish (volume, result, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED &&
		    error->code != G_IO_ERROR_ALREADY_MOUNTED) {
			name = g_volume_get_name (G_VOLUME (volume));
			primary = g_strdup_printf (_("Unable to accesss to “%s” device"), name);
			g_free (name);

			dialog = rena_gudev_dialog_new (rena_application_get_window (priv->rena),
			                                  _("Removable Device"), "media-removable",
			                                  primary, error->message,
			                                  NULL, RENA_DEVICE_RESPONSE_NONE);
			g_signal_connect (dialog, "response",
			                  G_CALLBACK (gtk_widget_destroy), NULL);

			gtk_widget_show_all (dialog);

			g_free (primary);
		}
		g_error_free (error);
	}

	/* get the moint point of the volume */
	mount = g_volume_get_mount (volume);
	if (mount != NULL) {
		rena_block_device_add_to_library (plugin, mount);
		g_object_unref (mount);
	}
}

static void
rena_block_device_mount_device (RenaRemovablePlugin *plugin)
{
	GMountOperation *mount_operation;

	RenaRemovablePluginPrivate *priv = plugin->priv;

	/* try to mount the volume asynchronously */
	mount_operation = gtk_mount_operation_new (NULL);
	g_volume_mount (priv->volume, G_MOUNT_MOUNT_NONE, mount_operation,
	                NULL, (GAsyncReadyCallback) rena_block_device_mount_finish,
	                plugin);
	g_object_unref (mount_operation);
}

static void
rena_block_device_detected_response (GtkWidget *dialog,
                                       gint       response,
                                       gpointer   user_data)
{
	RenaRemovablePlugin *plugin = user_data;

	switch (response)
	{
		case RENA_DEVICE_RESPONSE_BROWSE:
			rena_block_device_mount_device (plugin);
			break;
		case RENA_DEVICE_RESPONSE_NONE:
			rena_removable_clear_hook_device (plugin);
		default:
			break;
	}
	gtk_widget_destroy (dialog);
}

static gboolean
rena_block_device_detected (gpointer data)
{
	GtkWidget      *dialog;
	GVolumeMonitor *monitor;
	GVolume        *volume;
	gchar *name = NULL, *secondary = NULL;

	RenaRemovablePlugin *plugin = data;
	RenaRemovablePluginPrivate *priv = plugin->priv;

	/* determine the GVolume corresponding to the udev removable */
	monitor = g_volume_monitor_get ();
	volume = tvm_g_volume_monitor_get_volume_for_kind (monitor,
	                                                   G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE,
	                                                   g_udev_device_get_device_file (priv->u_device));
	g_object_unref (monitor);

	/* check if we have a volume */
	priv->volume = volume;
	if (volume == NULL || !g_volume_can_mount (volume)) {
		rena_removable_clear_hook_device (plugin);
		return FALSE;
	}

	name = g_volume_get_name (G_VOLUME (volume));
	secondary = g_strdup_printf (_("Do you want to manage the “%s” device with Rena?"), name);
	g_free (name);

	dialog = rena_gudev_dialog_new (rena_application_get_window (priv->rena),
	                                  _("Removable Device"), "media-removable",
	                                  _("An removable device was detected"), secondary,
	                                  _("Manage device"), RENA_DEVICE_RESPONSE_BROWSE);

	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (rena_block_device_detected_response), plugin);

	gtk_widget_show_all (dialog);

	g_free (secondary);

	return FALSE;
}

static void
rena_removable_plugin_device_added (RenaDeviceClient *device_client,
                                      RenaDeviceType    device_type,
                                      GUdevDevice        *u_device,
                                      gpointer            user_data)
{
	RenaRemovablePlugin *plugin = user_data;
	RenaRemovablePluginPrivate *priv = plugin->priv;

	if (device_type != RENA_DEVICE_MOUNTABLE)
		return;

	priv->bus_hooked = g_udev_device_get_property_as_uint64 (u_device, "BUSNUM");
	priv->device_hooked = g_udev_device_get_property_as_uint64 (u_device, "DEVNUM");
	priv->u_device = g_object_ref (u_device);
	priv->volume = NULL;

	/*
	 * HACK: We're listening udev. Then wait 2 seconds, to ensure that GVolume also detects the device.
	 */
	g_timeout_add_seconds(2, rena_block_device_detected, plugin);
}

void
rena_removable_plugin_device_removed (RenaDeviceClient *device_client,
                                        RenaDeviceType    device_type,
                                        GUdevDevice        *u_device,
                                        gpointer            user_data)
{
	guint64 busnum = 0;
	guint64 devnum = 0;

	RenaRemovablePlugin *plugin = user_data;
	RenaRemovablePluginPrivate *priv = plugin->priv;

	if (!priv->u_device || !priv->mount_path)
		return;

	if (device_type != RENA_DEVICE_MOUNTABLE)
		return;

	busnum = g_udev_device_get_property_as_uint64(u_device, "BUSNUM");
	devnum = g_udev_device_get_property_as_uint64(u_device, "DEVNUM");

	if (busnum == priv->bus_hooked && devnum == priv->device_hooked) {
		rena_removable_drop_device_from_library (plugin);
		rena_removable_clear_hook_device (plugin);
	}
}

static void
rena_plugin_activate (PeasActivatable *activatable)
{
	RenaRemovablePlugin *plugin = RENA_REMOVABLE_PLUGIN (activatable);
	RenaRemovablePluginPrivate *priv = plugin->priv;

	CDEBUG(DBG_PLUGIN, "Removable plugin %s", G_STRFUNC);

	priv->rena = g_object_get_data (G_OBJECT (plugin), "object");

	priv->device_client = rena_device_client_get();
	g_signal_connect (G_OBJECT(priv->device_client), "device-added",
	                  G_CALLBACK(rena_removable_plugin_device_added), plugin);
	g_signal_connect (G_OBJECT(priv->device_client), "device-removed",
	                  G_CALLBACK(rena_removable_plugin_device_removed), plugin);
}

static void
rena_plugin_deactivate (PeasActivatable *activatable)
{
	RenaDatabaseProvider *provider;

	RenaRemovablePlugin *plugin = RENA_REMOVABLE_PLUGIN (activatable);
	RenaRemovablePluginPrivate *priv = plugin->priv;

	CDEBUG(DBG_PLUGIN, "Removable plugin %s", G_STRFUNC);

	/* Remove provider if user disable the plugin or hide it */

	provider = rena_database_provider_get ();
	if (!rena_plugins_engine_is_shutdown(rena_application_get_plugins_engine(priv->rena)))
	{
		if (priv->mount_path)
		{
			rena_provider_remove (provider,
			                        priv->mount_path);
			rena_provider_update_done (provider);
		}
	}
	else
	{
		if (priv->mount_path)
		{
			rena_provider_set_visible (provider, priv->mount_path, FALSE);
			rena_provider_set_ignore (provider, priv->mount_path, TRUE);
		}
	}
	g_object_unref (provider);

	/* Clean memory */

	rena_removable_clear_hook_device (plugin);

	/* Disconnect signals */

	g_signal_handlers_disconnect_by_func (priv->device_client,
	                                      rena_removable_plugin_device_added,
	                                      plugin);
	g_signal_handlers_disconnect_by_func (priv->device_client,
	                                      rena_removable_plugin_device_removed,
	                                      plugin);

	g_object_unref (G_OBJECT(priv->device_client));

	priv->rena = NULL;
}

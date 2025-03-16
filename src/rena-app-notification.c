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

#include "config.h"

#include "rena-app-notification.h"
#include "rena-app-notification-container.h"


struct _RenaAppNotification {
	GtkFrame   parent_instance;

	GtkWidget *grid;

	GtkWidget *head;
	GtkWidget *body;
	GtkWidget *close_button;

	gchar     *head_msg;
	gchar     *body_msg;

	guint      timeout_id;
};

struct _RenaAppNotificationClass {
	GtkFrameClass parent_class;
};

enum {
	PROP_0,
	PROP_HEAD,
	PROP_BODY
};

G_DEFINE_TYPE (RenaAppNotification, rena_app_notification, GTK_TYPE_GRID);

static void
rena_app_notification_constructed (GObject *object)
{
	RenaAppNotification *self = RENA_APP_NOTIFICATION (object);

	G_OBJECT_CLASS (rena_app_notification_parent_class)->constructed (object);

	if (self->head_msg) {
		gtk_label_set_text (GTK_LABEL (self->head), self->head_msg);
		gtk_widget_set_visible(GTK_WIDGET(self->head), TRUE);
	}

	if (self->body_msg) {
		gtk_label_set_text (GTK_LABEL (self->body), self->body_msg);
		gtk_widget_set_visible(GTK_WIDGET(self->body), TRUE);
	}
}

static void
rena_app_notification_finalize (GObject *object)
{
	RenaAppNotification *self = RENA_APP_NOTIFICATION (object);

	g_free (self->head_msg);
	g_free (self->body_msg);

	G_OBJECT_CLASS (rena_app_notification_parent_class)->finalize (object);
}

static void
rena_app_notification_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	RenaAppNotification *self = RENA_APP_NOTIFICATION (object);

	switch (prop_id) {
		case PROP_HEAD:
			self->head_msg = g_value_dup_string (value);
			break;
		case PROP_BODY:
			self->body_msg = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
		}
}

static void
rena_app_notification_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	RenaAppNotification *self = RENA_APP_NOTIFICATION (object);

	switch (prop_id) {
		case PROP_HEAD:
			g_value_set_string (value, self->head_msg);
			break;
		case PROP_BODY:
			g_value_set_string (value, self->body_msg);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
rena_app_notification_close_button_clicked_cb (GtkButton             *button,
                                                 RenaAppNotification *self)
{
	RenaAppNotificationContainer *container;

	if (self->timeout_id != 0) {
		g_source_remove (self->timeout_id);
		self->timeout_id = 0;
	}

	gtk_widget_destroy (GTK_WIDGET (self));

	container = rena_app_notification_container_get_default ();
	if (rena_app_notification_container_get_num_children (container) == 0) {
		gtk_widget_hide (GTK_WIDGET (container));
		gtk_revealer_set_reveal_child (GTK_REVEALER (container), FALSE);
	}
}

static gboolean
rena_app_notification_timeout_call (gpointer data)
{
	RenaAppNotificationContainer *container;

	RenaAppNotification *self = RENA_APP_NOTIFICATION(data);

	gtk_widget_destroy(GTK_WIDGET(self));

	container = rena_app_notification_container_get_default ();
	if (rena_app_notification_container_get_num_children (container) == 0) {
		gtk_widget_hide (GTK_WIDGET (container));
		gtk_revealer_set_reveal_child (GTK_REVEALER (container), FALSE);
	}

	return G_SOURCE_REMOVE;
}

static void
rena_app_notification_init (RenaAppNotification *self)
{
	GtkWidget *image;
	GtkStyleContext *context;

	self->grid = gtk_grid_new ();
	context = gtk_widget_get_style_context (self->grid);
	gtk_style_context_add_class (context, "app-notification");
	gtk_container_add (GTK_CONTAINER (self), self->grid);

	self->head = gtk_label_new (NULL);
	gtk_widget_set_halign (self->head, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (self->head, TRUE);
	gtk_widget_set_vexpand (self->head, TRUE);
	gtk_grid_attach (GTK_GRID (self->grid), self->head, 0, 0, 1, 1);

	self->body = gtk_label_new (NULL);
	gtk_widget_set_halign (self->body, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand (self->body, TRUE);
	gtk_widget_set_vexpand (self->body, TRUE);
	gtk_grid_attach (GTK_GRID (self->grid), self->body, 0, 1, 1, 1);

	self->close_button = gtk_button_new ();
	g_object_set (self->close_button,
	              "relief", GTK_RELIEF_NONE,
	              "focus-on-click", FALSE,
	              NULL);
	gtk_grid_attach (GTK_GRID (self->grid), self->close_button, 1, 0, 1, 2);

	image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (self->close_button), image);

	g_signal_connect (self->close_button,
	                  "clicked",
	                  G_CALLBACK (rena_app_notification_close_button_clicked_cb),
	                  self);

	gtk_widget_show(GTK_WIDGET(self));
	gtk_widget_show(GTK_WIDGET (self->grid));
	gtk_widget_show_all(GTK_WIDGET (self->close_button));
}

static void
rena_app_notification_class_init (RenaAppNotificationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructed = rena_app_notification_constructed;
	object_class->finalize = rena_app_notification_finalize;
	object_class->set_property = rena_app_notification_set_property;
	object_class->get_property = rena_app_notification_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_HEAD,
	                                 g_param_spec_string ("head",
	                                                      "Head",
	                                                      "The notification head",
	                                                      "",
	                                                      G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
	                                 PROP_BODY,
	                                 g_param_spec_string ("body",
	                                                      "Body",
	                                                      "The notification body",
	                                                      "",
	                                                      G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

RenaAppNotification *
rena_app_notification_new (const char *head,
                             const char *body)
{
	return g_object_new (RENA_TYPE_APP_NOTIFICATION,
	                     "column-spacing", 12,
	                     "orientation", GTK_ORIENTATION_HORIZONTAL,
	                     "head", head,
	                     "body", body,
	                      NULL);
}

void
rena_app_notification_show (RenaAppNotification *self)
{
	g_assert (RENA_IS_APP_NOTIFICATION (self));

	rena_app_notification_container_add_notification (rena_app_notification_container_get_default (),
	                                                    GTK_WIDGET (self));
}

void
rena_app_notification_set_timeout (RenaAppNotification *self,
                                     guint                  timeout)
{
	g_assert (RENA_IS_APP_NOTIFICATION (self));

	self->timeout_id = g_timeout_add_seconds(timeout, rena_app_notification_timeout_call, self);
}


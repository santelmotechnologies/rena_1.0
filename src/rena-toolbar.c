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

#include "rena-toolbar.h"

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gdk/gdkkeysyms.h>

#include "rena-background-task-bar.h"
#include "rena-simple-widgets.h"
#include "rena-hig.h"
#include "rena-utils.h"

static void rena_toolbar_set_remaning_mode (RenaToolbar *toolbar, gboolean remaning_mode);
gboolean    rena_toolbar_get_remaning_mode (RenaToolbar *toolbar);

struct _RenaToolbar {
	GtkHeaderBar   __parent__;

	RenaAlbumArt *albumart;

	GtkWidget      *track_progress_bar;

	RenaToolbarButton *prev_button;
	RenaToolbarButton *play_button;
	RenaToolbarButton *stop_button;
	RenaToolbarButton *next_button;
	RenaToolbarButton *unfull_button;
	
	GtkWidget      *progress_button;
	GtkWidget      *vol_button;

	GtkWidget      *extra_button_box;
	GtkWidget      *favorites_button;

	GtkWidget      *track_length_label;
	GtkWidget      *track_time_label;
	GtkWidget      *now_playing_label;
	GtkWidget      *extention_box;

	gboolean       remaning_mode;
};

enum {
	PROP_0,
	PROP_VOLUME,
	PROP_REMANING_MODE,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST] = { 0 };

enum
{
	PREV_ACTIVATED,
	PLAY_ACTIVATED,
	STOP_ACTIVATED,
	NEXT_ACTIVATED,
	ALBUM_ART_ACTIVATED,
	TRACK_INFO_ACTIVATED,
	TRACK_PROGRESS_ACTIVATED,
	TRACK_FAVORITE_TOGGLE,
	UNFULL_ACTIVATED,
	TRACK_TIME_ACTIVATED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(RenaToolbar, rena_toolbar, GTK_TYPE_HEADER_BAR)

void
rena_toolbar_update_progress (RenaToolbar *toolbar, gint length, gint progress)
{
	gdouble fraction = 0;
	gchar *tot_length = NULL, *cur_pos = NULL, *str_length = NULL, *str_cur_pos = NULL;

	cur_pos = convert_length_str(progress);
	str_cur_pos = g_markup_printf_escaped ("<small>%s</small>", cur_pos);

	if (length == 0 || !rena_toolbar_get_remaning_mode (toolbar)) {
		tot_length = convert_length_str(length);
		str_length = g_markup_printf_escaped ("<small>%s</small>", tot_length);
	}
	else {
		tot_length = convert_length_str(length - progress);
		str_length = g_markup_printf_escaped ("<small>- %s</small>", tot_length);
	}

	gtk_label_set_markup (GTK_LABEL(toolbar->track_time_label), str_cur_pos);
	gtk_label_set_markup (GTK_LABEL(toolbar->track_length_label), str_length);

	gtk_tooltip_trigger_tooltip_query(gtk_widget_get_display (toolbar->track_length_label));

	if(length) {
		fraction = (gdouble) progress / (gdouble)length;
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(toolbar->track_progress_bar), fraction);
	}

	g_free(cur_pos);
	g_free(str_cur_pos);

	g_free(tot_length);
	g_free(str_length);
}

void
rena_toolbar_set_title (RenaToolbar *toolbar, RenaMusicobject *mobj)
{
	gchar *str = NULL, *str_title = NULL;

	const gchar *file = rena_musicobject_get_file (mobj);
	const gchar *title = rena_musicobject_get_title (mobj);
	const gchar *artist = rena_musicobject_get_artist (mobj);
	const gchar *album = rena_musicobject_get_album (mobj);

	if(string_is_not_empty(title))
		str_title = g_strdup(title);
	else
		str_title = get_display_filename(file, FALSE);

	if(string_is_not_empty(artist) && string_is_not_empty(album))
		str = g_markup_printf_escaped (_("%s <small><span weight=\"light\">by</span></small> %s <small><span weight=\"light\">in</span></small> %s"),
		                               str_title,
		                               artist,
		                               album);
	else if(string_is_not_empty(artist))
		str = g_markup_printf_escaped (_("%s <small><span weight=\"light\">by</span></small> %s"),
		                                str_title,
		                                artist);
	else if(string_is_not_empty(album))
		str = g_markup_printf_escaped (_("%s <small><span weight=\"light\">in</span></small> %s"),
		                                str_title,
		                                album);
	else
		str = g_markup_printf_escaped("%s", str_title);

	gtk_label_set_markup(GTK_LABEL(toolbar->now_playing_label), str);

	g_free(str_title);
	g_free(str);
}

static void
rena_toolbar_unset_song_info(RenaToolbar *toolbar)
{
	gtk_label_set_markup(GTK_LABEL(toolbar->now_playing_label), _("<b>Not playing</b>"));
	gtk_label_set_markup(GTK_LABEL(toolbar->track_length_label),  "<small>--:--</small>");
	gtk_label_set_markup(GTK_LABEL(toolbar->track_time_label),    "<small>00:00</small>");

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(toolbar->track_progress_bar), 0);
	rena_toolbar_set_favorite_icon (toolbar, FALSE);

	rena_album_art_set_path(toolbar->albumart, NULL);
}

static void
rena_toolbar_timer_label_event_change_mode (GtkWidget      *widget,
                                              GdkEventButton *event,
                                              RenaToolbar  *toolbar)
{
	rena_toolbar_set_remaning_mode (toolbar,
		!rena_toolbar_get_remaning_mode (toolbar));
}

void
rena_toolbar_set_image_album_art (RenaToolbar *toolbar, const gchar *uri)
{
	rena_album_art_set_path (toolbar->albumart, uri);
}

void
rena_toolbar_set_favorite_icon (RenaToolbar *toolbar, gboolean love)
{
	GtkWidget *image;
	GIcon *icon = NULL;

 	const gchar *love_icons[] = {
		"favorite",
		"starred",
		"starred-symbolic",
		NULL,
	};
 	const gchar *unlove_icons[] = {
		"not-favorite",
		"not-starred",
		"non-starred",
		"not-starred-symbolic",
		"non-starred-symbolic",
		NULL,
	};

	gtk_widget_set_tooltip_text (GTK_WIDGET(toolbar->favorites_button),
	                             love ? _("Remove from Favorites") : _("Add to Favorites"));
	icon = g_themed_icon_new_from_names (love ? (gchar **)love_icons : (gchar **)unlove_icons, -1);
	image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
	gtk_image_set_pixel_size (GTK_IMAGE(image), 12);
	gtk_button_set_image (GTK_BUTTON (toolbar->favorites_button), image);
}

/* Grab focus on current playlist when press Up or Down and move between controls with Left or Right */

/*static gboolean
panel_button_key_press (GtkWidget *win, GdkEventKey *event, RenaApplication *rena)
{
	gboolean ret = FALSE;

	if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Down ||
	    event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_Page_Down) {
		ret = rena_playlist_propagate_event(rena->cplaylist, event);
	}

	return ret;
}*/

/*
 * Emit signals..
 */

static gboolean
play_button_handler(GtkButton *button, RenaToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[PLAY_ACTIVATED], 0);

	return TRUE;
}

static gboolean
stop_button_handler(GtkButton *button, RenaToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[STOP_ACTIVATED], 0);

	return TRUE;
}

static gboolean
prev_button_handler(GtkButton *button, RenaToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[PREV_ACTIVATED], 0);

	return TRUE;
}

static gboolean
next_button_handler(GtkButton *button, RenaToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[NEXT_ACTIVATED], 0);

	return TRUE;
}

static gboolean
unfull_button_handler (GtkButton *button, RenaToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[UNFULL_ACTIVATED], 0);

	return TRUE;
}

static gboolean
rena_toolbar_album_art_activated (GtkWidget      *event_box,
                                    GdkEventButton *event,
                                    RenaToolbar  *toolbar)
{
	if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
		if (!gtk_header_bar_get_show_close_button(GTK_HEADER_BAR(toolbar)))
			g_signal_emit (toolbar, signals[ALBUM_ART_ACTIVATED], 0);

	return FALSE;
}

static gboolean
rena_toolbar_song_label_event_edit (GtkWidget      *event_box,
                                      GdkEventButton *event,
                                      RenaToolbar  *toolbar)
{
	if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
		if (!gtk_header_bar_get_show_close_button(GTK_HEADER_BAR(toolbar)))
			g_signal_emit (toolbar, signals[TRACK_INFO_ACTIVATED], 0);

	return FALSE;
}

static void
rena_toolbar_progress_bar_event_seek (GtkWidget *widget,
                                        GdkEventButton *event,
                                        RenaToolbar *toolbar)
{
	GtkAllocation allocation;
	gdouble fraction = 0;

	if (event->button != 1)
		return;

	gtk_widget_get_allocation(widget, &allocation);

	fraction = (gdouble) event->x / allocation.width;

	g_signal_emit (toolbar, signals[TRACK_PROGRESS_ACTIVATED], 0, fraction);
}

/*
 * Callbacks that response to gstreamer signals.
 */

void
rena_toolbar_update_buffering_cb (RenaBackend *backend, gint percent, gpointer user_data)
{
	RenaToolbar *toolbar = user_data;

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(toolbar->track_progress_bar), (gdouble)percent/100);
}

void
rena_toolbar_update_playback_progress(RenaBackend *backend, gpointer user_data)
{
	gint length = 0, newsec = 0;
	RenaMusicobject *mobj = NULL;

	RenaToolbar *toolbar = user_data;

	newsec = GST_TIME_AS_SECONDS(rena_backend_get_current_position(backend));

	if (newsec > 0) {
		mobj = rena_backend_get_musicobject (backend);
		length = rena_musicobject_get_length (mobj);

		if (length > 0) {
			rena_toolbar_update_progress (toolbar, length, newsec);
		}
		else {
			gint nlength = GST_TIME_AS_SECONDS(rena_backend_get_current_length(backend));
			rena_musicobject_set_length (mobj, nlength);
		}
	}
}

void
rena_toolbar_playback_state_cb (RenaBackend *backend, GParamSpec *pspec, gpointer user_data)
{
	RenaToolbar *toolbar = user_data;
	RenaBackendState state = rena_backend_get_state (backend);

	gboolean playing = (state != ST_STOPPED);

	gtk_widget_set_sensitive (GTK_WIDGET(toolbar->prev_button), playing);

	rena_toolbar_button_set_icon_name (toolbar->play_button,
	                                     (state == ST_PLAYING) ? "media-playback-pause" : "media-playback-start");
	gtk_widget_set_sensitive (GTK_WIDGET(toolbar->stop_button), playing);
	gtk_widget_set_sensitive (GTK_WIDGET(toolbar->next_button), playing);

	if (playing == FALSE)
		rena_toolbar_unset_song_info(toolbar);
}

void
rena_toolbar_show_ramaning_time_cb (RenaToolbar *toolbar, GParamSpec *pspec, gpointer user_data)
{
	RenaBackend *backend = user_data;
	rena_toolbar_update_playback_progress (backend, toolbar);
}

/*
 * Show the unfullscreen button according to the state of the window.
 */

gboolean
rena_toolbar_window_state_event (GtkWidget *widget, GdkEventWindowState *event, RenaToolbar *toolbar)
{
	if (event->type == GDK_WINDOW_STATE && (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)) {
		gtk_widget_set_visible(GTK_WIDGET(toolbar->unfull_button), (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0);
	}

	return FALSE;
}

/*
 * Public api.
 */

void
rena_toolbar_set_style (RenaToolbar *toolbar, gboolean system_titlebar)
{
	GtkStyleContext *context;
	context = gtk_widget_get_style_context (GTK_WIDGET(toolbar));

	if (system_titlebar) {
		gtk_style_context_remove_class (context, "header-bar");
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOOLBAR);
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	}
	else {
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_TOOLBAR);
		gtk_style_context_remove_class (context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
		gtk_style_context_add_class (context, "header-bar");
	}

	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(toolbar), !system_titlebar);
}

void
rena_toolbar_add_extention_widget(RenaToolbar *toolbar, GtkWidget *widget)
{
	GList *list;
	GtkWidget *children;

	list = gtk_container_get_children (GTK_CONTAINER(toolbar->extention_box));
	if(list) {
		children = list->data;
		gtk_container_remove(GTK_CONTAINER(toolbar->extention_box), children);
		gtk_widget_destroy(GTK_WIDGET(children));
		g_list_free(list);
	}
	gtk_container_add(GTK_CONTAINER(toolbar->extention_box), widget);
}

void
rena_toolbar_add_extra_button (RenaToolbar *toolbar, GtkWidget *widget)
{
	gtk_container_add(GTK_CONTAINER(toolbar->extra_button_box), widget);
}

void
rena_toolbar_remove_extra_button (RenaToolbar *toolbar, GtkWidget *widget)
{
	gtk_container_remove(GTK_CONTAINER(toolbar->extra_button_box), widget);
}


const gchar*
rena_toolbar_get_progress_text(RenaToolbar *toolbar)
{
	return gtk_label_get_text (GTK_LABEL(toolbar->track_time_label));
}

const gchar*
rena_toolbar_get_length_text(RenaToolbar *toolbar)
{
	return gtk_label_get_text (GTK_LABEL(toolbar->track_length_label));
}

RenaAlbumArt *
rena_toolbar_get_album_art(RenaToolbar *toolbar)
{
	return toolbar->albumart;
}

/*
 * Rena toolbar creation and destruction.
 */

static void
rena_toolbar_favorites_clicked (GtkButton *button, RenaToolbar *toolbar)
{
	g_signal_emit (toolbar, signals[TRACK_FAVORITE_TOGGLE], 0);
}

GtkWidget *
rena_toolbar_get_song_box (RenaToolbar *toolbar)
{
	RenaPreferences *preferences;
	RenaAlbumArt *albumart;
	GtkWidget *hbox, *vbox, *top_hbox, *botton_hbox;
	GtkWidget *album_art_frame,*title, *title_event_box, *favorites_button, *extention_box;
	GtkWidget *progress_bar, *progress_bar_event_box, *time_label, *length_label, *length_event_box;

	const GBindingFlags binding_flags =
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL;

	preferences = rena_preferences_get();

	/*
	 * Main box that allow expand.
	 * Main box: [Album][Song info]
	 */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand (GTK_WIDGET(hbox), TRUE);

	album_art_frame = gtk_event_box_new ();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(album_art_frame), FALSE);
	g_signal_connect(G_OBJECT (album_art_frame), "button_press_event",
	                 G_CALLBACK (rena_toolbar_album_art_activated), toolbar);
	gtk_box_pack_start (GTK_BOX(hbox), album_art_frame, FALSE, FALSE, 2);

	albumart = rena_album_art_new ();
	g_object_bind_property (preferences, "album-art-size",
	                        albumart, "size", binding_flags);
	g_object_bind_property (preferences, "show-album-art",
	                        albumart, "visible", binding_flags);
	gtk_container_add(GTK_CONTAINER(album_art_frame), GTK_WIDGET(albumart));
	toolbar->albumart = albumart;

	/*
	 * Song info vbox
	 */

 	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
 	gtk_widget_set_halign (GTK_WIDGET(vbox), GTK_ALIGN_FILL);
	gtk_widget_set_valign (GTK_WIDGET(vbox), GTK_ALIGN_CENTER);

 	/*
 	 * Top box: [Title][extentions]
 	 */
	top_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), top_hbox, TRUE, TRUE, 0);

	/* The title widget. */

	title_event_box = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(title_event_box), FALSE);

	g_signal_connect (G_OBJECT(title_event_box), "button-press-event",
	                  G_CALLBACK(rena_toolbar_song_label_event_edit), toolbar);

	title = gtk_label_new(NULL);
	gtk_label_set_ellipsize (GTK_LABEL(title), PANGO_ELLIPSIZE_END);
	gtk_label_set_markup(GTK_LABEL(title),_("<b>Not playing</b>"));
	gtk_widget_set_halign (GTK_WIDGET(title), GTK_ALIGN_START);
	gtk_widget_set_valign (GTK_WIDGET(title), GTK_ALIGN_CENTER);

	gtk_container_add (GTK_CONTAINER(title_event_box), title);

	/* Favorites button */

	favorites_button = gtk_button_new ();
	gtk_button_set_relief(GTK_BUTTON(favorites_button), GTK_RELIEF_NONE);
	rena_hig_set_tiny_button (favorites_button);
	g_signal_connect (G_OBJECT(favorites_button), "clicked",
	                  G_CALLBACK(rena_toolbar_favorites_clicked), toolbar);

	/* The extentions box. */
	
	extention_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* Pack top widgets:  */

	gtk_box_pack_start (GTK_BOX(top_hbox),
	                    GTK_WIDGET(title_event_box),
	                    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(top_hbox),
	                    GTK_WIDGET(favorites_button),
	                    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(top_hbox),
	                    GTK_WIDGET(extention_box),
	                    FALSE, FALSE, 0);

	/*
	 * Botton box: [Time][ProgressBar][Length]
	 */
	botton_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), botton_hbox, FALSE, FALSE, 0);

	/* Time progress widget. */

	time_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(time_label),"<small>00:00</small>");

	/* Progress bar widget. */

	progress_bar_event_box = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(progress_bar_event_box), FALSE);

	g_signal_connect (G_OBJECT(progress_bar_event_box), "button-press-event",
	                  G_CALLBACK(rena_toolbar_progress_bar_event_seek), toolbar);

	progress_bar = gtk_progress_bar_new ();
	gtk_widget_set_valign (GTK_WIDGET(progress_bar), GTK_ALIGN_CENTER);

	gtk_container_add (GTK_CONTAINER(progress_bar_event_box),
	                   GTK_WIDGET(progress_bar));

	/* Length and remaining time widget. */

	length_event_box = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(length_event_box), FALSE);
	g_signal_connect (G_OBJECT(length_event_box), "button-press-event",
	                  G_CALLBACK(rena_toolbar_timer_label_event_change_mode), toolbar);

	length_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(length_label),"<small>--:--</small>");

	gtk_container_add(GTK_CONTAINER(length_event_box), length_label);

	/* Pack widgets. */

	gtk_box_pack_start (GTK_BOX(botton_hbox),
	                    GTK_WIDGET(time_label),
	                    FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX(botton_hbox),
	                    GTK_WIDGET(progress_bar_event_box),
	                    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX(botton_hbox),
	                    GTK_WIDGET(length_event_box),
	                    FALSE, FALSE, 3);

	/* Save references. */

	toolbar->track_progress_bar = progress_bar;
	toolbar->now_playing_label  = title;
	toolbar->track_time_label   = time_label;
	toolbar->track_length_label = length_label;
	toolbar->favorites_button   = favorites_button;
	toolbar->extention_box      = extention_box;

	rena_toolbar_set_favorite_icon (toolbar, FALSE);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 2);

	gtk_widget_show_all(GTK_WIDGET(vbox));
	gtk_widget_show(GTK_WIDGET(album_art_frame));
	gtk_widget_show(GTK_WIDGET(hbox));

	g_object_unref(preferences);

	return GTK_WIDGET(hbox);
}

static void
vol_button_value_changed (GtkVolumeButton *button, gdouble value, RenaToolbar *toolbar)
{
	g_object_notify_by_pspec (G_OBJECT (toolbar), properties[PROP_VOLUME]);
}

static void
rena_toolbar_set_volume (RenaToolbar *toolbar, gdouble volume)
{
	gtk_scale_button_set_value (GTK_SCALE_BUTTON(toolbar->vol_button), volume);
}

gdouble
rena_toolbar_get_volume (RenaToolbar *toolbar)
{
	return gtk_scale_button_get_value (GTK_SCALE_BUTTON(toolbar->vol_button));
}

static void
rena_toolbar_set_remaning_mode (RenaToolbar *toolbar, gboolean remaning_mode)
{
	toolbar->remaning_mode = remaning_mode;

	g_object_notify_by_pspec(G_OBJECT(toolbar), properties[PROP_REMANING_MODE]);
}

gboolean
rena_toolbar_get_remaning_mode (RenaToolbar *toolbar)
{
	return toolbar->remaning_mode;
}

static void
rena_toolbar_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	RenaToolbar *toolbar = RENA_TOOLBAR (object);

	switch (property_id)
	{
		case PROP_VOLUME:
			rena_toolbar_set_volume (toolbar, g_value_get_double (value));
			break;
		case PROP_REMANING_MODE:
			rena_toolbar_set_remaning_mode (toolbar, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
rena_toolbar_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	RenaToolbar *toolbar = RENA_TOOLBAR (object);

	switch (property_id)
	{
		case PROP_VOLUME:
			g_value_set_double (value, rena_toolbar_get_volume (toolbar));
			break;
		case PROP_REMANING_MODE:
			g_value_set_boolean (value, rena_toolbar_get_remaning_mode (toolbar));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
rena_toolbar_finalize (GObject *object)
{
	(*G_OBJECT_CLASS (rena_toolbar_parent_class)->finalize) (object);
}

static void
rena_toolbar_dispose (GObject *object)
{
	(*G_OBJECT_CLASS (rena_toolbar_parent_class)->dispose) (object);
}

static void
rena_toolbar_class_init (RenaToolbarClass *klass)
{
	GObjectClass  *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->set_property = rena_toolbar_set_property;
	gobject_class->get_property = rena_toolbar_get_property;
	gobject_class->finalize = rena_toolbar_finalize;
	gobject_class->dispose = rena_toolbar_dispose;

	/*
	 * Properties:
	 */
	properties[PROP_VOLUME] = g_param_spec_double ("volume", "Volume", "Volume showed on toolbar",
	                                               0.0, 1.0, 0.5,
	                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_REMANING_MODE] = g_param_spec_boolean ("timer-remaining-mode", "TimerRemainingMode", "Show Remaining Time",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (gobject_class, PROP_LAST, properties);

	/*
	 * Signals:
	 */
	signals[PREV_ACTIVATED] =
		g_signal_new ("prev",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, prev),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[PLAY_ACTIVATED] =
		g_signal_new ("play",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, play),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[STOP_ACTIVATED] =
		g_signal_new ("stop",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, stop),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[NEXT_ACTIVATED] =
		g_signal_new ("next",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, next),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[ALBUM_ART_ACTIVATED] =
		g_signal_new ("album-art-activated",
		             G_TYPE_FROM_CLASS (gobject_class),
		             G_SIGNAL_RUN_LAST,
		             G_STRUCT_OFFSET (RenaToolbarClass, album_art_activated),
		             NULL, NULL,
		             g_cclosure_marshal_VOID__VOID,
		             G_TYPE_NONE, 0);
	signals[TRACK_INFO_ACTIVATED] =
		g_signal_new ("track-info-activated",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, track_info_activated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[TRACK_PROGRESS_ACTIVATED] =
		g_signal_new ("track-progress-activated",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, track_progress_activated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__DOUBLE,
		              G_TYPE_NONE, 1, G_TYPE_DOUBLE);
	signals[TRACK_FAVORITE_TOGGLE] =
		g_signal_new ("favorite-toggle",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, favorite_toggle),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[UNFULL_ACTIVATED] =
		g_signal_new ("unfull-activated",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaToolbarClass, unfull),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

static void
rena_toolbar_init (RenaToolbar *toolbar)
{
	RenaPreferences *preferences;
	RenaToolbarButton *prev_button, *play_button, *stop_button, *next_button;
	RenaToolbarButton *unfull_button;
	RenaToggleButton *shuffle_button, *repeat_button;
	GtkWidget *vol_button, *progress_button;

	const GBindingFlags binding_flags =
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL;

	preferences = rena_preferences_get();

	/* Setup Left control buttons */

	prev_button = rena_toolbar_button_new ("media-skip-backward");
	gtk_widget_set_tooltip_text(GTK_WIDGET(prev_button), _("Previous Track"));
	toolbar->prev_button = prev_button;

	play_button = rena_toolbar_button_new ("media-playback-start");
	gtk_widget_set_tooltip_text(GTK_WIDGET(play_button), _("Play / Pause Track"));
	toolbar->play_button = play_button;

	stop_button = rena_toolbar_button_new ("media-playback-stop");
	gtk_widget_set_tooltip_text(GTK_WIDGET(stop_button), _("Stop playback"));
	toolbar->stop_button = stop_button;

	next_button = rena_toolbar_button_new ("media-skip-forward");
	gtk_widget_set_tooltip_text(GTK_WIDGET(next_button), _("Next Track"));
	toolbar->next_button = next_button;

	gtk_header_bar_pack_start(GTK_HEADER_BAR(toolbar), GTK_WIDGET(prev_button));
	gtk_header_bar_pack_start(GTK_HEADER_BAR(toolbar), GTK_WIDGET(play_button));
	gtk_header_bar_pack_start(GTK_HEADER_BAR(toolbar), GTK_WIDGET(stop_button));
	gtk_header_bar_pack_start(GTK_HEADER_BAR(toolbar), GTK_WIDGET(next_button));

	/* Setup Right control buttons */

	unfull_button = rena_toolbar_button_new ("view-restore");
	gtk_widget_set_tooltip_text(GTK_WIDGET(unfull_button), _("Leave Fullscreen"));
	toolbar->unfull_button = unfull_button;

	progress_button = GTK_WIDGET(rena_background_task_bar_get ());
	toolbar->progress_button = progress_button;

	shuffle_button = rena_toggle_button_new ("media-playlist-shuffle");
	gtk_widget_set_tooltip_text(GTK_WIDGET(shuffle_button), _("Play songs in a random order"));

	repeat_button = rena_toggle_button_new ("media-playlist-repeat");
	gtk_widget_set_tooltip_text(GTK_WIDGET(repeat_button), _("Repeat playback list at the end"));

	vol_button = gtk_volume_button_new();
	g_object_set(vol_button, "use-symbolic", FALSE, NULL);
	gtk_button_set_relief(GTK_BUTTON(vol_button), GTK_RELIEF_NONE);
	g_object_set(G_OBJECT(vol_button), "size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
	gtk_widget_set_valign (GTK_WIDGET(vol_button), GTK_ALIGN_CENTER);
	toolbar->vol_button = vol_button;

	toolbar->extra_button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_valign (GTK_WIDGET(toolbar->extra_button_box), GTK_ALIGN_CENTER);

	gtk_header_bar_pack_end(GTK_HEADER_BAR(toolbar), GTK_WIDGET(toolbar->extra_button_box));
	gtk_header_bar_pack_end(GTK_HEADER_BAR(toolbar), GTK_WIDGET(vol_button));
	gtk_header_bar_pack_end(GTK_HEADER_BAR(toolbar), GTK_WIDGET(repeat_button));
	gtk_header_bar_pack_end(GTK_HEADER_BAR(toolbar), GTK_WIDGET(shuffle_button));
	gtk_header_bar_pack_end(GTK_HEADER_BAR(toolbar), GTK_WIDGET(unfull_button));
	gtk_header_bar_pack_end(GTK_HEADER_BAR(toolbar), GTK_WIDGET(progress_button));

	/* Connect signals */

	g_signal_connect(G_OBJECT(prev_button), "clicked",
	                 G_CALLBACK(prev_button_handler), toolbar);
	g_signal_connect(G_OBJECT(play_button), "clicked",
	                 G_CALLBACK(play_button_handler), toolbar);
	g_signal_connect(G_OBJECT(stop_button), "clicked",
	                 G_CALLBACK(stop_button_handler), toolbar);
	g_signal_connect(G_OBJECT(next_button), "clicked",
	                 G_CALLBACK(next_button_handler), toolbar);
	g_signal_connect(G_OBJECT(unfull_button), "clicked",
	                 G_CALLBACK(unfull_button_handler), toolbar);

	/*g_signal_connect(G_OBJECT (prev_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (play_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (stop_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (next_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (next_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (unfull_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (shuffle_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (repeat_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);
	g_signal_connect(G_OBJECT (vol_button), "key-press-event",
	                 G_CALLBACK(panel_button_key_press), toolbar);*/

	g_signal_connect (G_OBJECT (vol_button), "value-changed",
	                  G_CALLBACK (vol_button_value_changed), toolbar);

	g_object_bind_property(preferences, "shuffle", shuffle_button, "active", binding_flags);
	g_object_bind_property(preferences, "repeat", repeat_button, "active", binding_flags);

	/* Fix styling */

	rena_toolbar_set_style(toolbar,
		rena_preferences_get_system_titlebar (preferences));

	g_object_bind_property(preferences, "toolbar-size", prev_button, "icon-size", binding_flags);
	g_object_bind_property(preferences, "toolbar-size", play_button, "icon-size", binding_flags);
	g_object_bind_property(preferences, "toolbar-size", stop_button, "icon-size", binding_flags);
	g_object_bind_property(preferences, "toolbar-size", next_button, "icon-size", binding_flags);
	g_object_bind_property(preferences, "toolbar-size", unfull_button, "icon-size", binding_flags);
	g_object_bind_property(preferences, "toolbar-size", shuffle_button, "icon-size", binding_flags);
	g_object_bind_property(preferences, "toolbar-size", repeat_button, "icon-size", binding_flags);
	g_object_bind_property(preferences, "toolbar-size", vol_button, "size", binding_flags);

	gtk_widget_show(GTK_WIDGET(prev_button));
	gtk_widget_show(GTK_WIDGET(play_button));
	gtk_widget_show(GTK_WIDGET(stop_button));
	gtk_widget_show(GTK_WIDGET(next_button));
	gtk_widget_show(GTK_WIDGET(shuffle_button));
	gtk_widget_show(GTK_WIDGET(repeat_button));
	gtk_widget_show(GTK_WIDGET(vol_button));
	gtk_widget_show(GTK_WIDGET(toolbar->extra_button_box));

	gtk_widget_hide(GTK_WIDGET(toolbar->unfull_button));
	gtk_widget_hide(GTK_WIDGET(toolbar->progress_button));

	gtk_widget_show(GTK_WIDGET(toolbar));

	g_object_unref(preferences);
}

RenaToolbar *
rena_toolbar_new (void)
{
	return g_object_new (RENA_TYPE_TOOLBAR, NULL);
}


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

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include "rena-song-info-ui.h"

#include <glyr/glyr.h>

#include "rena-song-info-pane.h"

#include "src/rena-database.h"
#include "src/rena-musicobject-mgmt.h"
#include "src/rena-hig.h"
#include "src/rena-utils.h"

struct _RenaSonginfoPane {
	GtkScrolledWindow  parent;

	/* Title */
	GtkWidget         *title;

	/* Text widget */
	GtkWidget         *text_view;

	/* List widget */
	GtkWidget         *list_view;
	GtkWidget         *append_button;

	/* Info that show thde pane */
	GLYR_GET_TYPE      info_type;

	/* Sidebar widgets */
	GtkWidget          *pane_title;
	GtkBuilder         *builder;
	GSimpleActionGroup *actions;
};

G_DEFINE_TYPE(RenaSonginfoPane, rena_songinfo_pane, GTK_TYPE_SCROLLED_WINDOW)

enum {
	SIGNAL_TYPE_CHANGED,
	SIGNAL_APPEND,
	SIGNAL_APPEND_ALL,
	SIGNAL_QUEUE,
	LAST_SIGNAL
};
static int signals[LAST_SIGNAL] = { 0 };

/*
 * Menus definitions
 *
 **/
static void
rena_songinfo_pane_show_artist_info_action (GSimpleAction *action,
                                              GVariant      *parameter,
                                              gpointer       user_data);

static void
rena_songinfo_pane_show_lyrics_action      (GSimpleAction *action,
                                              GVariant      *parameter,
                                              gpointer       user_data);

static void
rena_songinfo_pane_show_similar_action     (GSimpleAction *action,
                                              GVariant      *parameter,
                                              gpointer       user_data);

static const GActionEntry song_info_aentries[] = {
	{ "artist",  rena_songinfo_pane_show_artist_info_action, NULL, NULL, NULL },
	{ "lyrics",  rena_songinfo_pane_show_lyrics_action,      NULL, NULL, NULL },
	{ "similar", rena_songinfo_pane_show_similar_action,     NULL, NULL, NULL }
};


/*
 * Public Api
 */

GtkWidget *
rena_songinfo_pane_row_new (RenaMusicobject *mobj)
{
	RenaDatabase *cdbase;
	RenaMusicobject *db_mobj;
	GtkWidget *row, *box, *icon, *label;
	const gchar *title = NULL, *artist = NULL;
	gchar *song_name = NULL;

	row = gtk_list_box_row_new ();
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add(GTK_CONTAINER(row), box);

	title = rena_musicobject_get_title (mobj);
	artist = rena_musicobject_get_artist (mobj);

	cdbase = rena_database_get ();
	db_mobj = rena_database_get_artist_and_title_song (cdbase, artist, title);
	if (db_mobj) {
		g_object_set_data_full (G_OBJECT(row), "SONG", db_mobj, g_object_unref);
		icon = gtk_image_new_from_icon_name ("media-playback-start-symbolic",
		                                     GTK_ICON_SIZE_MENU);
	}
	else {
		g_object_set_data_full (G_OBJECT(row), "SONG", mobj, g_object_unref);
		icon = gtk_image_new_from_icon_name ("edit-find-symbolic",
		                                     GTK_ICON_SIZE_MENU);
	}

	song_name = g_strdup_printf ("%s - %s", title, artist);

	label = gtk_label_new (song_name);
	gtk_label_set_ellipsize (GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
	gtk_widget_set_halign (label, GTK_ALIGN_START);

	gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

	gtk_widget_show_all (row);

	g_object_unref(cdbase);
	g_free (song_name);

	return row;
}

void
rena_songinfo_pane_set_title (RenaSonginfoPane *pane,
                                const gchar        *title)
{
	gtk_label_set_text (GTK_LABEL(pane->title), title);
	gtk_widget_show (GTK_WIDGET(pane->title));
}

void
rena_songinfo_pane_set_text (RenaSonginfoPane *pane,
                               const gchar        *text,
                               const gchar        *provider)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (pane->text_view));

	gtk_text_buffer_set_text (buffer, "", -1);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER(buffer), &iter);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER(buffer), &iter, text, -1);

	if (string_is_not_empty(provider)) {
		if (string_is_not_empty(text))
			gtk_text_buffer_insert (GTK_TEXT_BUFFER(buffer), &iter, "\n\n", -1);
		gtk_text_buffer_insert (GTK_TEXT_BUFFER(buffer), &iter, _("Thanks to "), -1);
		gtk_text_buffer_insert_with_tags_by_name (GTK_TEXT_BUFFER(buffer), &iter, provider, -1, "style_bold", "style_italic", NULL);
	}
}

void
rena_songinfo_pane_append_song_row (RenaSonginfoPane *pane,
                                      GtkWidget          *row)
{
	gtk_list_box_insert (GTK_LIST_BOX(pane->list_view), row, 0);
	gtk_widget_show (GTK_WIDGET(pane->list_view));
	gtk_widget_show (GTK_WIDGET(pane->append_button));
}

void
rena_songinfo_pane_clear_text (RenaSonginfoPane *pane)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (pane->text_view));
	gtk_text_buffer_set_text (buffer, "", -1);

	gtk_widget_hide (GTK_WIDGET(pane->title));
}

void
rena_songinfo_pane_clear_list (RenaSonginfoPane *pane)
{
	GList *list, *l;
	GtkWidget *children;

	list = gtk_container_get_children (GTK_CONTAINER(pane->list_view));
	l = list;
	while (l != NULL) {
		children = l->data;
		gtk_container_remove(GTK_CONTAINER(pane->list_view), children);
		l = g_list_next(l);
	}
	g_list_free(list);

	gtk_widget_hide (GTK_WIDGET(pane->list_view));
	gtk_widget_hide (GTK_WIDGET(pane->append_button));
}

GList *
rena_songinfo_get_mobj_list (RenaSonginfoPane *pane)
{
	RenaMusicobject *mobj;
	GList *mlist = NULL, *list, *l;
	GtkWidget *row;
	const gchar *provider = NULL;

	list = gtk_container_get_children (GTK_CONTAINER(pane->list_view));
	l = list;
	while (l != NULL) {
		row = l->data;
		mobj = g_object_get_data (G_OBJECT(row), "SONG");
		provider = rena_musicobject_get_provider (mobj);
		if (string_is_not_empty(provider))
			mlist = g_list_append (mlist, mobj);
		l = g_list_next(l);
	}
	g_list_free (list);

	return mlist;
}

GtkWidget *
rena_songinfo_pane_get_pane_title (RenaSonginfoPane *pane)
{
	return pane->pane_title;
}

GtkWidget *
rena_songinfo_pane_get_popover (RenaSonginfoPane *pane)
{
	GMenuModel *model;
	GtkWidget *popover;

	model = G_MENU_MODEL(gtk_builder_get_object (pane->builder, "song-info-menu"));
	popover = gtk_popover_new_from_model (pane->pane_title, model);
	gtk_widget_insert_action_group (popover, "info", G_ACTION_GROUP(pane->actions));

	return popover;
}

GLYR_GET_TYPE
rena_songinfo_pane_get_default_view (RenaSonginfoPane *pane)
{
	return pane->info_type;
}

void
rena_songinfo_pane_set_default_view (RenaSonginfoPane *pane, GLYR_GET_TYPE view_type)
{
	switch(view_type) {
		case GLYR_GET_ARTIST_BIO:
			rena_songinfo_pane_show_artist_info_action (NULL, NULL, pane);
			break;
		case GLYR_GET_SIMILAR_SONGS:
			rena_songinfo_pane_show_similar_action (NULL, NULL, pane);
			break;
		case GLYR_GET_LYRICS:
		default:
			rena_songinfo_pane_show_lyrics_action (NULL, NULL, pane);
			break;
	}
}

/*
 * Private
 */

static void
rena_song_info_row_activated (GtkListBox         *box,
                                GtkListBoxRow      *row,
                                RenaSonginfoPane *pane)
{
	RenaMusicobject *mobj = NULL;

	mobj = g_object_get_data (G_OBJECT(row), "SONG");
	if (mobj == NULL)
		return;

	g_signal_emit (pane, signals[SIGNAL_APPEND], 0, mobj);
}

static gboolean
rena_song_info_row_key_press (GtkWidget          *widget,
                                GdkEventKey        *event,
                                RenaSonginfoPane *pane)
{
	GtkListBoxRow *row;
	RenaMusicobject *mobj = NULL;
	if (event->keyval != GDK_KEY_q && event->keyval != GDK_KEY_Q)
		return FALSE;

	row = gtk_list_box_get_selected_row (GTK_LIST_BOX (pane->list_view));
	mobj = g_object_get_data (G_OBJECT(row), "SONG");
	if (mobj == NULL)
		return FALSE;

	g_signal_emit (pane, signals[SIGNAL_QUEUE], 0, mobj);

	return TRUE;
}

static void
rena_song_info_append_songs (GtkButton          *button,
                               RenaSonginfoPane *pane)
{
	g_signal_emit (pane, signals[SIGNAL_APPEND_ALL], 0);
}

static void
song_list_header_func (GtkListBoxRow *row,
                       GtkListBoxRow *before,
                       gpointer       user_data)
{
	GtkWidget *header;
	header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_list_box_row_set_header (row, header);
}

static gint
song_list_sort_func (GtkListBoxRow *a,
                     GtkListBoxRow *b,
                     gpointer user_data)
{
	RenaMusicobject *mobja = NULL, *mobjb = NULL;
	const gchar *providera = NULL, *providerb = NULL;

	mobja = g_object_get_data (G_OBJECT(a), "SONG");
	mobjb = g_object_get_data (G_OBJECT(b), "SONG");

	providera = rena_musicobject_get_provider (mobja);
	providerb = rena_musicobject_get_provider (mobjb);

	if (string_is_empty(providera) && string_is_not_empty(providerb))
		return 1;

	if (string_is_not_empty(providera) && string_is_empty(providerb))
		return -1;

	if (string_is_empty(providera) && string_is_empty(providerb))
		return -1;

	if (string_is_not_empty(providera) && string_is_not_empty(providerb))
		return 1;

	return 0;
}

/* Menus */

static void
rena_songinfo_pane_show_artist_info_action (GSimpleAction *action,
                                              GVariant      *parameter,
                                              gpointer       user_data)
{
	RenaSonginfoPane *pane = RENA_SONGINFO_PANE(user_data);

	gtk_label_set_text (GTK_LABEL(pane->pane_title), _("Artist info"));
	pane->info_type = GLYR_GET_ARTIST_BIO;

	g_signal_emit (pane, signals[SIGNAL_TYPE_CHANGED], 0);
}

static void
rena_songinfo_pane_show_lyrics_action (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
	RenaSonginfoPane *pane = RENA_SONGINFO_PANE(user_data);

	gtk_label_set_text (GTK_LABEL(pane->pane_title), _("Lyrics"));
	pane->info_type = GLYR_GET_LYRICS;

	g_signal_emit (pane, signals[SIGNAL_TYPE_CHANGED], 0);
}

static void
rena_songinfo_pane_show_similar_action (GSimpleAction *action,
                                          GVariant      *parameter,
                                          gpointer       user_data)
{
	RenaSonginfoPane *pane = RENA_SONGINFO_PANE(user_data);

	gtk_label_set_text (GTK_LABEL(pane->pane_title), _("Similar songs"));
	pane->info_type = GLYR_GET_SIMILAR_SONGS;

	g_signal_emit (pane, signals[SIGNAL_TYPE_CHANGED], 0);
}

/* Construction */

static void
rena_songinfo_pane_construct_builder (RenaSonginfoPane *pane)
{
	GError *error = NULL;

	pane->builder = gtk_builder_new ();
	gtk_builder_add_from_string (pane->builder, rena_song_info_ui, -1, &error);
	if (error) {
		g_print ("GtkBuilder error: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	pane->actions =  g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP(pane->actions),
	                                 song_info_aentries,
	                                 G_N_ELEMENTS(song_info_aentries),
	                                 (gpointer)pane);
}

static void
rena_songinfo_pane_finalize (GObject *object)
{
	RenaSonginfoPane *pane = RENA_SONGINFO_PANE (object);

	g_object_unref (pane->builder);

	(*G_OBJECT_CLASS (rena_songinfo_pane_parent_class)->finalize) (object);
}

static void
rena_songinfo_pane_init (RenaSonginfoPane *pane)
{
	GtkWidget *box, *lbox, *label, *append_button, *icon, *view, *list;
	GtkTextBuffer *buffer;
	PangoAttrList *attrs;
	GtkStyleContext *context;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	context = gtk_widget_get_style_context (box);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

	label = gtk_label_new (_("Lyrics"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	g_object_set (label, "xalign", 0.0, NULL);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
	gtk_label_set_attributes (GTK_LABEL (label), attrs);
	pango_attr_list_unref (attrs);

	lbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	context = gtk_widget_get_style_context (lbox);
	gtk_style_context_add_class (context, "linked");

	append_button = gtk_button_new ();
	rena_hig_set_tiny_button (append_button);
	gtk_widget_set_tooltip_text (append_button, _("_Add to current playlist"));
	gtk_widget_set_valign (append_button, GTK_ALIGN_CENTER);
	icon = gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_MENU);
	gtk_image_set_pixel_size (GTK_IMAGE(icon), 12);
	gtk_button_set_image(GTK_BUTTON(append_button), icon);

	view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
	gtk_text_view_set_accepts_tab (GTK_TEXT_VIEW (view), FALSE);
	g_object_set (view, "left-margin", 4, "right-margin", 4, NULL);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(buffer), "style_bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(buffer), "style_large", "scale", PANGO_SCALE_X_LARGE, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(buffer), "style_italic", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(buffer), "margin_top", "pixels-above-lines", 2, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pane),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(pane),
	                                     GTK_SHADOW_IN);

	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW(pane), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW(pane), NULL);

	list = gtk_list_box_new ();
	gtk_list_box_set_header_func (GTK_LIST_BOX (list),
	                              song_list_header_func, list, NULL);
	gtk_list_box_set_sort_func (GTK_LIST_BOX (list),
	                            song_list_sort_func, list, NULL);

	gtk_box_pack_start (GTK_BOX(lbox), GTK_WIDGET(label), FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX(lbox), GTK_WIDGET(append_button), FALSE, FALSE, 4);

	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(lbox), FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(list), FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX(box), GTK_WIDGET(view), FALSE, FALSE, 2);
	gtk_container_add (GTK_CONTAINER (pane), box);

	gtk_widget_show_all (GTK_WIDGET(pane));

	pane->pane_title = gtk_label_new (_("Lyrics"));
	gtk_widget_set_halign (GTK_WIDGET(pane->pane_title), GTK_ALIGN_START);
	gtk_widget_set_valign (GTK_WIDGET(pane->pane_title), GTK_ALIGN_CENTER);

	g_signal_connect (list, "row-activated",
	                  G_CALLBACK(rena_song_info_row_activated), pane);
	g_signal_connect (list, "key-press-event",
	                  G_CALLBACK(rena_song_info_row_key_press), pane);
	g_signal_connect (append_button, "clicked",
	                  G_CALLBACK(rena_song_info_append_songs), pane);

	rena_songinfo_pane_construct_builder (pane);

	pane->title = label;
	pane->text_view = view;
	pane->list_view = list;
	pane->append_button = append_button;
	pane->info_type = GLYR_GET_LYRICS;
}

static void
rena_songinfo_pane_class_init (RenaSonginfoPaneClass *klass)
{
	GObjectClass  *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = rena_songinfo_pane_finalize;

	signals[SIGNAL_TYPE_CHANGED] =
		g_signal_new ("type-changed",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaSonginfoPaneClass, type_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[SIGNAL_APPEND] =
		g_signal_new ("append",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaSonginfoPaneClass, append),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[SIGNAL_APPEND_ALL] =
		g_signal_new ("append-all",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaSonginfoPaneClass, append_all),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[SIGNAL_QUEUE] =
		g_signal_new ("queue",
		              G_TYPE_FROM_CLASS (gobject_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaSonginfoPaneClass, queue),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);
}

RenaSonginfoPane *
rena_songinfo_pane_new (void)
{
	return g_object_new (RENA_TYPE_SONGINFO_PANE, NULL);
}

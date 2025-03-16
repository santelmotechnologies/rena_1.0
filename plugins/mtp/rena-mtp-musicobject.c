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

#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>

#include "src/rena-utils.h"
#include "src/rena-file-utils.h"
#include "src/rena-music-enum.h"
#include "src/rena-debug.h"

#include "rena-mtp-musicobject.h"


static gboolean
is_valid_mime(const gchar *mime, const gchar **mlist)
{
	gint i = 0;
	while (mlist[i]) {
		if (g_content_type_equals(mime, mlist[i]))
			return TRUE;
		i++;
	}
	return FALSE;
}

static guint
mtp_track_get_year (const gchar *date)
{
	gchar *year_string = NULL;
	guint year = 0;

	year_string = g_strndup (date, 4);
	if (year_string) {
		year = atoi (year_string);
		g_free (year_string);
	}

	// Minimal validate since some phones not implement it well.
	return year > 1900 ? year : 0;
}

LIBMTP_track_t *
mtp_track_new_from_rena_musicobject (LIBMTP_mtpdevice_t *mtp_device, RenaMusicobject *mobj)
{
	LIBMTP_track_t *tr;
	LIBMTP_filetype_t filetype;
	gchar *filename;
	const gchar *mime_type;
	struct stat sbuf;

	mime_type = rena_musicobject_get_mime_type (mobj);

	if (is_valid_mime(mime_type, mime_flac))
		filetype = LIBMTP_FILETYPE_FLAC;
	else if (is_valid_mime(mime_type, mime_mpeg))
		filetype = LIBMTP_FILETYPE_MP3;
	else if (is_valid_mime(mime_type, mime_ogg))
		filetype = LIBMTP_FILETYPE_OGG;
	else if (is_valid_mime(mime_type, mime_wav))
		filetype = LIBMTP_FILETYPE_WAV;
	else if (is_valid_mime(mime_type, mime_asf))
		filetype = LIBMTP_FILETYPE_WMA;
	else if (is_valid_mime(mime_type, mime_mp4))
		filetype = LIBMTP_FILETYPE_MP4;
	else
		filetype = LIBMTP_FILETYPE_UNKNOWN;

	if (filetype == LIBMTP_FILETYPE_UNKNOWN)
		return NULL;

	filename = g_strdup(rena_musicobject_get_file(mobj));
	if (g_stat(filename, &sbuf) == -1) {
		g_free(filename);
		return NULL;
	}

	tr = LIBMTP_new_track_t();

	/* Minimun data. */

	tr->filesize = (uint64_t) sbuf.st_size;
	tr->filename = get_display_name(mobj);
	tr->filetype = filetype;

	/* Metadata. */

	tr->title = g_strdup(rena_musicobject_get_title(mobj));
	tr->artist = g_strdup(rena_musicobject_get_artist(mobj));
	tr->album = g_strdup(rena_musicobject_get_album(mobj));
	tr->duration = (1000 * rena_musicobject_get_length(mobj));
	tr->genre = g_strdup(rena_musicobject_get_genre (mobj));
	tr->date = g_strdup_printf("%d", rena_musicobject_get_year (mobj));

	/* Storage data. */

	tr->parent_id = mtp_device->default_music_folder;
	tr->storage_id = 0;

	g_free(filename);

	return tr;
}

RenaMusicobject *
rena_musicobject_new_from_mtp_track (LIBMTP_track_t *track)
{
	RenaMusicEnum *enum_map = NULL;
	RenaMusicobject *mobj = NULL;
	gchar *mime_type = NULL, *uri = NULL;

	/* First chech mime_type */

	if (track->filetype == LIBMTP_FILETYPE_FLAC)
		mime_type = g_strdup ("audio/x-flac");
	else if (track->filetype == LIBMTP_FILETYPE_MP3)
		mime_type = g_strdup ("audio/mpeg");
	else if (track->filetype == LIBMTP_FILETYPE_OGG)
		mime_type = g_strdup ("audio/ogg");
	else if (track->filetype == LIBMTP_FILETYPE_WAV)
		mime_type = g_strdup ("audio/x-wav");
	else if (track->filetype == LIBMTP_FILETYPE_WMA)
		mime_type = g_strdup ("audio/x-ms-wma");
	else if (track->filetype == LIBMTP_FILETYPE_MP4)
		mime_type = g_strdup ("audio/x-m4a");
	else
		return NULL;

	uri = g_strdup_printf ("mtp://%i-%s", track->item_id, track->filename);

	CDEBUG(DBG_MOBJ, "Creating new musicobject to MTP: %s", uri);

	enum_map = rena_music_enum_get();
	mobj = g_object_new (RENA_TYPE_MUSICOBJECT,
	                     "file", uri,
	                     "mime-type", mime_type,
	                     "source", rena_music_enum_map_get(enum_map, "MTP"),
	                     NULL);
	g_object_unref (enum_map);

	if (track->title)
		rena_musicobject_set_title (mobj, track->title);
	if (track->artist)
		rena_musicobject_set_artist (mobj, track->artist);
	if (track->album)
		rena_musicobject_set_album (mobj, track->album);
	if (track->date)
		rena_musicobject_set_year (mobj, mtp_track_get_year (track->date));
	if (track->genre)
		rena_musicobject_set_genre (mobj, track->genre);
	if (track->duration)
		rena_musicobject_set_length (mobj, track->duration/1000);
	if (track->tracknumber)
		rena_musicobject_set_track_no (mobj, track->tracknumber);
	if (track->samplerate)
		rena_musicobject_set_samplerate (mobj, track->samplerate);
	if (track->nochannels)
		rena_musicobject_set_channels (mobj, track->nochannels);

	g_free (uri);
	g_free (mime_type);

	return mobj;
}

gint
rena_mtp_plugin_get_track_id (RenaMusicobject *mobj)
{
	const gchar *track_id, *file;

	file = rena_musicobject_get_file (mobj);
	track_id = file + strlen ("mtp://");

	return atoi(track_id);
}

gchar *
rena_mtp_plugin_get_temp_filename (RenaMusicobject *mobj)
{
	const gchar *track_id, *file;

	file = rena_musicobject_get_file (mobj);
	track_id = file + strlen ("mtp://");

	return g_strdup_printf ("/tmp/%s", track_id);
}

gboolean
rena_musicobject_is_mtp_file (RenaMusicobject *mobj)
{
	RenaMusicEnum *enum_map = NULL;
	RenaMusicSource file_source = FILE_NONE;

	enum_map = rena_music_enum_get ();
	file_source = rena_music_enum_map_get(enum_map, "MTP");
	g_object_unref (enum_map);

	return (file_source == rena_musicobject_get_source (mobj));
}

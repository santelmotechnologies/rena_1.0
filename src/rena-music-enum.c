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

#include "rena-music-enum.h"
#include "rena-musicobject.h"

struct _RenaMusicEnumItem {
    gchar *name;
    gint   code;
};
typedef struct _RenaMusicEnumItem RenaMusicEnumItem;

struct _RenaMusicEnum {
	GObject             _parent;
	RenaMusicEnumItem map[MAX_ENUM_SIZE];
	gint                size;
};

enum {
	SIGNAL_ENUM_REMOVED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(RenaMusicEnum, rena_music_enum, G_TYPE_OBJECT)

static void
rena_music_enum_finalize (GObject *object)
{
	RenaMusicEnum *enum_map = RENA_MUSIC_ENUM(object);

	gint i = 0;
	for (i = 0; i <= enum_map->size; i++) {
		if (enum_map->map[i].name == NULL)
			continue;
		g_free (enum_map->map[i].name);
	}

	G_OBJECT_CLASS(rena_music_enum_parent_class)->finalize(object);
}

static void
rena_music_enum_class_init (RenaMusicEnumClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = rena_music_enum_finalize;

	signals[SIGNAL_ENUM_REMOVED] =
		g_signal_new ("enum-removed",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (RenaMusicEnumClass, enum_removed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
rena_music_enum_init (RenaMusicEnum *enum_map)
{
	gint i = 0, code = 0;

	/* TODO: Add props to this!. */
	gint min_enum = FILE_USER_0;
	gint max_enum = FILE_USER_L;

	/* Set size */
	enum_map->size = max_enum - min_enum;
	if (enum_map->size > MAX_ENUM_SIZE)
		enum_map->size = MAX_ENUM_SIZE;

	for (i = 0, code = min_enum; i <= enum_map->size; i++, code++) {
		enum_map->map[i].name = NULL;
		if (i <= enum_map->size)
			enum_map->map[i].code = code;
		else
			enum_map->map[i].code = -1;
	}
}

RenaMusicEnum *
rena_music_enum_get (void)
{
	static RenaMusicEnum *enum_map = NULL;

	if (G_UNLIKELY (enum_map == NULL)) {
		enum_map = g_object_new (RENA_TYPE_MUSIC_ENUM, NULL);
		g_object_add_weak_pointer (G_OBJECT (enum_map),
		                          (gpointer) &enum_map);
	}
	else {
		g_object_ref (G_OBJECT(enum_map));
	}

	return enum_map;
}

const gchar *
rena_music_enum_map_get_name (RenaMusicEnum *enum_map, gint enum_code)
{
	return enum_map->map[enum_code].name;
}

gint
rena_music_enum_map_get (RenaMusicEnum *enum_map, const gchar *name)
{
	gint i = 0;

	if (g_ascii_strcasecmp(name, "local") == 0)
		return FILE_LOCAL;

	/* First check if exist */
	for (i = 0; i <= enum_map->size; i++) {
		if (enum_map->map[i].name == NULL)
			continue;
		if (g_ascii_strcasecmp(name, enum_map->map[i].name) == 0)
			return enum_map->map[i].code;
	}
	/* Add a new enum */
	for (i = 0; i <= enum_map->size; i++) {
		if (enum_map->map[i].name == NULL) {
			enum_map->map[i].name = g_strdup(name);
			return enum_map->map[i].code;
		}
	}
	return -1;
}

gint
rena_music_enum_map_remove (RenaMusicEnum *enum_map, const gchar *name)
{
	gint i = 0;

	for (i = 0; i <= enum_map->size; i++) {
		if (enum_map->map[i].name == NULL)
			continue;

		if (g_ascii_strcasecmp (name, enum_map->map[i].name) == 0) {
			g_free (enum_map->map[i].name);
			enum_map->map[i].name = NULL;

			g_signal_emit (enum_map, signals[SIGNAL_ENUM_REMOVED], 0, enum_map->map[i].code);

			return enum_map->map[i].code;
		}
	}
	return -1;
}

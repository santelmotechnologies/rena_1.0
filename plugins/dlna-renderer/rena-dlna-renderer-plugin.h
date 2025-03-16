/*************************************************************************/
/* Copyright (C) 2014 matias <mati86dl@gmail.com>                        */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/*************************************************************************/

#ifndef __PRAGHA_DLNA_RENDERER_PLUGIN_H__
#define __PRAGHA_DLNA_RENDERER_PLUGIN_H__

#include <gtk/gtk.h>
#include <libpeas/peas.h>

#include "src/pragha.h"

G_BEGIN_DECLS

#define PRAGHA_TYPE_DLNA_RENDERER_PLUGIN         (pragha_dlna_renderer_plugin_get_type ())
#define PRAGHA_DLNA_RENDERER_PLUGIN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PRAGHA_TYPE_DLNA_RENDERER_PLUGIN, PraghaDlnaRendererPlugin))
#define PRAGHA_DLNA_RENDERER_PLUGIN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PRAGHA_TYPE_DLNA_RENDERER_PLUGIN, PraghaDlnaRendererPlugin))
#define PRAGHA_IS_DLNA_RENDERER_PLUGIN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PRAGHA_TYPE_DLNA_RENDERER_PLUGIN))
#define PRAGHA_IS_DLNA_RENDERER_PLUGIN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PRAGHA_TYPE_DLNA_RENDERER_PLUGIN))
#define PRAGHA_DLNA_RENDERER_PLUGIN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PRAGHA_TYPE_DLNA_RENDERER_PLUGIN, PraghaDlnaRendererPluginClass))

GType   pragha_dlna_renderer_plugin_get_type     (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __PRAGHA_DLNA_RENDERER_PLUGIN_H__ */

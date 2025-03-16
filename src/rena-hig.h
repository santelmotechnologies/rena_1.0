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

#ifndef RENA_HIG_H
#define RENA_HIG_H

#include <gtk/gtk.h>

void       gtk_label_set_attribute_bold                    (GtkLabel *label);

void       rena_hig_set_tiny_button                      (GtkWidget *button);

GtkWidget *rena_hig_workarea_table_add_section_title     (GtkWidget *table, guint *row, const char *section_title);
void       rena_hig_workarea_table_add_wide_control      (GtkWidget *table, guint *row, GtkWidget *widget);
void       rena_hig_workarea_table_add_wide_tall_control (GtkWidget *table, guint *row, GtkWidget *widget);
void       rena_hig_workarea_table_add_row               (GtkWidget *table, guint *row, GtkWidget *label, GtkWidget *control);
GtkWidget *rena_hig_workarea_table_new                   ();

#endif /* RENA_HIG_H */

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

#ifndef RENA_SCANNER_H
#define RENA_SCANNER_H

typedef struct _RenaScanner RenaScanner;

void
rena_scanner_update_library(RenaScanner *scanner);

void
rena_scanner_scan_library(RenaScanner *scanner);

void
rena_scanner_free(RenaScanner *scanner);

RenaScanner *
rena_scanner_new();

#endif /* RENA_SCANNER_H */

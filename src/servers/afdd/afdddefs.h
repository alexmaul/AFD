/*
 *  afdddefs - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __afdddefs_h
#define __afdddefs_h

#include "afdd_common_defs.h"

#define DEFAULT_AFD_PORT_NO   "4444"
#define AFDD_SHUTDOWN_MESSAGE "500 AFDD shutdown."

/* Function prototypes. */
extern void check_changes(FILE *),
            display_file(FILE *),
            handle_request(int, int, int, char *),
            show_dir_list(FILE *),
            show_host_list(FILE *),
            show_host_stat(FILE *),
            show_job_list(FILE *),
            show_summary_stat(FILE *);
extern int  get_display_data(char *, int, char *, int, int, int, int);

#endif /* __afdddefs_h */

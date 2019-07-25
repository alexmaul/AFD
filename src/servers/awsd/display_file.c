/*
 *  display_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2009 Deutscher Wetterdienst (DWD),
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

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   display_file - writes the contents of a file to a socket
 **
 ** SYNOPSIS
 **   void display_file(FILE *p_data)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.06.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                     /* strerror()                    */
#include <stdlib.h>                     /* malloc(), free()              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>                     /* read(), write(), close()      */
#include <errno.h>
#include "magic.h"
#include "awsddefs.h"

/* External global variables. */
extern char *p_work_dir;

/*############################ display_file() ###########################*/
void display_file(FILE *p_data)
{
   int fd, from_fd, i;
   size_t hunk, left;
   char *buffer;
   struct stat stat_buf;
   magic_t mookie;
   const char *mime;

   /* Open source file. */
   if ((from_fd = open(p_work_dir, O_RDONLY)) < 0)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to open() %s : %s (%s %d)\r\n", p_work_dir, strerror(errno));
      print_http_state(p_data, HTTP_STATUS_500);
      return;
   }

   if (fstat(from_fd, &stat_buf) != 0)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to fstat() %s : %s (%s %d)\r\n", p_work_dir, strerror(errno));
      print_http_state(p_data, HTTP_STATUS_500);
      (void) close(from_fd);
      return;
   }

   left = hunk = stat_buf.st_size;

   if (hunk > HUNK_MAX)
   {
      hunk = HUNK_MAX;
   }

   if ((buffer = malloc(hunk)) == NULL)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to malloc() memory : %s (%s %d)\r\n", strerror(errno));
      print_http_state(p_data, HTTP_STATUS_500);
      (void) close(from_fd);
      return;
   }

   i = strlen(p_work_dir) - 1;
   if (p_work_dir[i - 2] == '.' && p_work_dir[i - 1] == 'j' && p_work_dir[i] == 's')
   {
      mime = HTTP_CONTENT_TYPE_JS;
   }
   else if (p_work_dir[i - 4] == '.' && p_work_dir[i - 3] == 'j' && p_work_dir[i - 2] == 's' && p_work_dir[i - 1] == 'o'
            && p_work_dir[i] == 'n')
   {
      mime = HTTP_CONTENT_TYPE_JSON;
   }
   else
   {
      /* determine MIME-type from MAGIC */
      if ((mookie = magic_open(MAGIC_SYMLINK | MAGIC_MIME_TYPE)) == NULL)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to do magic %s : %s (%s %d)\r\n", p_work_dir,
                  strerror(errno));
         mime = HTTP_CONTENT_TYPE_BINARY;
      }
      else if (magic_load(mookie, NULL))
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to do magic %s : %s (%s %d)\r\n", p_work_dir,
                  magic_error(mookie));
         mime = HTTP_CONTENT_TYPE_BINARY;
      }
      else if ((mime = magic_file(mookie, p_work_dir)) == NULL)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to do magic %s : %s (%s %d)\r\n", p_work_dir,
                  magic_error(mookie));
         mime = HTTP_CONTENT_TYPE_BINARY;
      }
   }
   print_http_state(p_data, HTTP_STATUS_200);
   print_http_content_length(p_data, left);
   print_http_content_type(p_data, mime);
   fprintf(stderr, "MIME: %s\n", mime); //XXX
   if (mookie != NULL)
   {
      magic_close(mookie);
   }
   fprintf(p_data, "\r\n\r\n");
   (void) fflush(p_data);
   fd = fileno(p_data);

   while (left > 0)
   {
      if (read(from_fd, buffer, hunk) != hunk)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__, "Failed to read() %s : %s (%s %d)\r\n", p_work_dir,
                  strerror(errno));
         print_http_state(p_data, HTTP_STATUS_500);
         free(buffer);
         (void) close(from_fd);
         return;
      }

      if (write(fd, buffer, hunk) != hunk)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__, "write() error : %s (%s %d)\r\n", strerror(errno));
         print_http_state(p_data, HTTP_STATUS_500);
         free(buffer);
         (void) close(from_fd);
         return;
      }
      left -= hunk;
      if (left < hunk)
      {
         hunk = left;
      }
   }

   free(buffer);
   if (close(from_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, _("Failed to close() %s : %s"), p_work_dir, strerror(errno));
   }

   return;
}

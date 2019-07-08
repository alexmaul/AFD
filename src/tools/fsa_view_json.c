/*
 *  fsa_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2014 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   fsa_json - shows all information in the FSA about a specific
 **              host in JSON format.
 **
 ** SYNOPSIS
 **   fsa_json [--version] [-w working directory] [-a|-i] hostname|position
 **
 ** DESCRIPTION
 **   This program shows all information about a specific host in the
 **   FSA in JSON format.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.02.1996 H.Kiehl Created
 **   05.01.1997 H.Kiehl Added support for burst mode.
 **   21.08.1997 H.Kiehl Show real hostname as well.
 **   12.10.1997 H.Kiehl Show bursting and mailing.
 **   05.12.2000 H.Kiehl If available show host toggle string.
 **   04.08.2001 H.Kiehl Show more details of special_flag and added
 **                      active|passive mode and idle time to protocol.
 **   16.02.2006 H.Kiehl Added SFTP, ignore_bin, socket send and
 **                      socket receive buffer.
 **   27.03.2006 H.Kiehl Option with long view with full filenames.
 **   18.10.2013 H.Kiehl Beautified output so it shows the table aligned
 **                      properly.
 **   --.07.2015 A.Maul  Re-write for JSON output.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr, stdout    */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include <math.h>
#include "version.h"

#define CTRL_ALL_VIEW    1
#define INFO_VIEW     2

#define GET_MAX_DIGIT(value)                          \
        {                                             \
           if ((value) > 999999999)                   \
           {                                          \
              nr_of_digits = (int)log10((value)) + 1; \
              if (nr_of_digits > max_digits)          \
              {                                       \
                 max_digits = nr_of_digits;           \
              }                                       \
           }                                          \
        }

/* Local functions. */
static void usage(void);

int sys_log_fd = STDERR_FILENO, /* Not used!    */
fsa_id, fsa_fd = -1, no_of_hosts = 0;
#ifdef HAVE_MMAP
off_t fsa_size;
#endif
char *p_work_dir;
struct filetransfer_status *fsa;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fsa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int main(int argc, char *argv[])
{
   int i, j, last = 0, position = -1, view_type = CTRL_ALL_VIEW;
   char hostname[MAX_HOSTNAME_LENGTH + 1], *ptr, work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   if (get_arg(&argc, argv, "-a", NULL, 0) == SUCCESS)
   {
      view_type = CTRL_ALL_VIEW;
   }
   if (get_arg(&argc, argv, "-i", NULL, 0) == SUCCESS)
   {
      view_type = INFO_VIEW;
   }

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, stdout, 0) > 0)
   {
      (void) fprintf(stderr, "The compiled binary does not match stored database.\n");
      (void) fprintf(stderr, "Initialize database with the command : afd -i\n");
      exit(INCORRECT);
   }

   if (argc == 2)
   {
      if (isdigit((int) (argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
         last = position + 1;
      }
      else
      {
         t_hostname(argv[1], hostname);
      }
   }
   else
      if (argc == 1)
      {
         position = -2;
      }
      else
      {
         usage();
         exit(INCORRECT);
      }

   if ((j = fsa_attach_passive(NO, "fsa_view")) != SUCCESS)
   {
      if (j == INCORRECT_VERSION)
      {
         (void) fprintf(stderr,
                  _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                  __FILE__, __LINE__);
      }
      else
      {
         if (j < 0)
         {
            (void) fprintf(stderr, _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
            __FILE__, __LINE__);
         }
         else
         {
            (void) fprintf(stderr, _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"), strerror(j), __FILE__,
            __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if (position == -1)
   {
      if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
      {
         (void) fprintf(stderr, _("WARNING : Could not find host `%s´ in FSA. (%s %d)\n"), hostname, __FILE__,
         __LINE__);
         exit(INCORRECT);
      }
      last = position + 1;
   }
   else
      if (position == -2)
      {
         last = no_of_hosts;
         position = 0;
      }
      else
         if (position >= no_of_hosts)
         {
            (void) fprintf(stderr, _("WARNING : There are only %d hosts in the FSA. (%s %d)\n"), no_of_hosts, __FILE__,
            __LINE__);
            exit(INCORRECT);
         }

   ptr = (char *) fsa;
   ptr -= AFD_WORD_OFFSET;
   (void) fprintf(stdout, "[");
   for (j = position; j < last; j++)
   {
      if (j > 0)
      {
         (void) fprintf(stdout, ",");
      }
      (void) fprintf(stdout, "{\"ord\":%d,", j);
      (void) fprintf(stdout, "\"alias\":\"%s\",", fsa[j].host_alias);
      (void) fprintf(stdout, "\"real1\":\"%s\",", fsa[j].real_hostname[0]);
      (void) fprintf(stdout, "\"real2\":\"%s\",", fsa[j].real_hostname[1]);
      (void) fprintf(stdout, "\"display\":\"%s\",", fsa[j].host_dsp_name);
      (void) fprintf(stdout, "\"direction\":\"");
      if (fsa[j].protocol & SEND_FLAG)
      {
         (void) fprintf(stdout, "S");
      }
      if (fsa[j].protocol & RETRIEVE_FLAG)
      {
         (void) fprintf(stdout, "R");
      }
      (void) fprintf(stdout, "\",");
      if (fsa[j].debug == NORMAL_MODE)
      {
         (void) fprintf(stdout, "\"debug_mode\":\"none\",");
      }
      else
         if (fsa[j].debug == DEBUG_MODE)
         {
            (void) fprintf(stdout, "\"debug_mode\":\"debug\",");
         }
         else
            if (fsa[j].debug == TRACE_MODE)
            {
               (void) fprintf(stdout, "\"debug_mode\":\"trace\",");
            }
            else
               if (fsa[j].debug == FULL_TRACE_MODE)
               {
                  (void) fprintf(stdout, "\"debug_mode\":\"full_trace\",");
               }
               else
               {
                  (void) fprintf(stdout, "\"debug_mode\":\"none\",");
               }
      (void) fprintf(stdout, "\"host_status\":[", fsa[j].host_status);
      if (fsa[j].host_status & PAUSE_QUEUE_STAT)
      {
         (void) fprintf(stdout, "\"PAUSE_QUEUE\",");
      }
      if (fsa[j].host_status & AUTO_PAUSE_QUEUE_STAT)
      {
         (void) fprintf(stdout, "\"AUTO_PAUSE_QUEUE\",");
      }
#ifdef WITH_ERROR_QUEUE
      if (fsa[j].host_status & ERROR_QUEUE_SET)
      {
         (void) fprintf(stdout, "\"ERROR_QUEUE_SET\",");
      }
#endif
      if (fsa[j].host_status & STOP_TRANSFER_STAT)
      {
         (void) fprintf(stdout, "\"STOP_TRANSFER\",");
      }
      if (fsa[j].host_status & HOST_CONFIG_HOST_DISABLED)
      {
         (void) fprintf(stdout, "\"HOST_CONFIG_HOST_DISABLED\",");
      }
      if ((fsa[j].special_flag & HOST_IN_DIR_CONFIG) == 0)
      {
         (void) fprintf(stdout, "\"HOST_NOT_IN_DIR_CONFIG\",");
      }
      if (fsa[j].host_status & DANGER_PAUSE_QUEUE_STAT)
      {
         (void) fprintf(stdout, "\"DANGER_PAUSE_QUEUE_STAT\",");
      }
      if (fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED)
      {
         (void) fprintf(stdout, "HOST_ERROR_ACKNOWLEDGED\",");
      }
      if (fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED_T)
      {
         (void) fprintf(stdout, "\"HOST_ERROR_ACKNOWLEDGED_T\",");
      }
      if (fsa[j].host_status & HOST_ERROR_OFFLINE)
      {
         (void) fprintf(stdout, "HOST_ERROR_OFFLINE\",");
      }
      if (fsa[j].host_status & HOST_ERROR_OFFLINE_T)
      {
         (void) fprintf(stdout, "\"HOST_ERROR_OFFLINE_T\",");
      }
      if (fsa[j].host_status & HOST_ERROR_OFFLINE_STATIC)
      {
         (void) fprintf(stdout, "\"HOST_ERROR_OFFLINE_STATIC\",");
      }
      if (fsa[j].host_status & DO_NOT_DELETE_DATA)
      {
         (void) fprintf(stdout, "\"DO_NOT_DELETE_DATA\",");
      }
      if (fsa[j].host_status & HOST_ACTION_SUCCESS)
      {
         (void) fprintf(stdout, "\"HOST_ACTION_SUCCESS\",");
      }
      if (fsa[j].active_transfers > 0)
      {
         (void) fprintf(stdout, "\"TRANSFER_ACTIVE\",");
      }
      else
      {
         (void) fprintf(stdout, "\"NORMAL_STATUS\",");
      }
      if (fsa[j].special_flag & HOST_DISABLED)
      {
         (void) fprintf(stdout, "\"HOST_DISABLED\",");
      }
      if (fsa[j].special_flag & HOST_IN_DIR_CONFIG)
      {
         (void) fprintf(stdout, "\"HOST_IN_DIR_CONFIG\",");
      }
      (void) fprintf(stdout, "0],"); /* ende host_status */
      (void) fprintf(stdout, "\"error_count\":%d,", fsa[j].error_counter);
      (void) fprintf(stdout, "\"file_count\":%d,", fsa[j].total_file_counter);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "\"file_size\":%ld,",
#else
      (void) fprintf(stdout, "\"file_size\":%lld,",
#endif
               (pri_off_t) fsa[j].total_file_size);
      (void) fprintf(stdout, "\"transfers\":%d,", fsa[j].active_transfers);

      (void) fprintf(stdout, "\"jobs\":[");
      for (i = 0; i < fsa[j].allowed_transfers; i++)
      {
         if (i > 0)
         {
            (void) fprintf(stdout, ",");
         }
         (void) fprintf(stdout, "{\"job_num\":%d,", i);
         switch (fsa[j].job_status[i].connect_status)
         {
         case CONNECTING:
            if (fsa[j].protocol & LOC_FLAG)
            {
               if ((fsa[j].protocol & FTP_FLAG) || (fsa[j].protocol & SFTP_FLAG) || (fsa[j].protocol & HTTP_FLAG) ||
#ifdef _WITH_MAP_SUPPORT
                        (fsa[j].protocol & MAP_FLAG) ||
#endif
#ifdef _WITH_SCP_SUPPORT
                        (fsa[j].protocol & SCP_FLAG) ||
#endif
#ifdef _WITH_WMO_SUPPORT
                        (fsa[j].protocol & WMO_FLAG) ||
#endif
                        (fsa[j].protocol & SMTP_FLAG))
               {
                  (void) fprintf(stdout, "\"connect_status\":\"CONNECTING\",");
               }
               else
               {
                  (void) fprintf(stdout, "\"connect_status\":\"CONNECTING\",");
               }
            }
            else
            {
               (void) fprintf(stdout, "\"connect_status\":\"CONNECTING\",");
            }
            break;

         case DISCONNECT:
            (void) fprintf(stdout, "\"connect_status\":\"DISCONNECT\",");
            break;

         case NOT_WORKING:
            (void) fprintf(stdout, "\"connect_status\":\"NOT working\",");
            break;

         case FTP_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"FTP active\",");
            break;

         case FTP_BURST2_TRANSFER_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"FTP burst active\",");
            break;

         case SFTP_ACTIVE:
#ifdef _WITH_MAP_SUPPORT
            /* or MAP_ACTIVE */
            (void)fprintf(stdout, "\"connect_status\":\"SFTP/MAP\",");
#else
            (void) fprintf(stdout, "\"connect_status\":\"SFTP active\",");
#endif
            break;

         case SFTP_BURST_TRANSFER_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"SFTP burst active\",");
            break;

         case LOC_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"LOC active\",");
            break;

         case HTTP_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"HTTP active\",");
            break;

         case HTTP_RETRIEVE_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"HTTP retrieve active\",");
            break;

         case SMTP_BURST_TRANSFER_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"SMTP burst active\",");
            break;

         case SMTP_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"SMTP active\",");
            break;

#ifdef _WITH_SCP_SUPPORT
         case SCP_BURST_TRANSFER_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"SCP burst active\",");
            break;

         case SCP_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"SCP active\",");
            break;
#endif
#ifdef _WITH_WMO_SUPPORT
            case WMO_BURST_TRANSFER_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"WMO burst active\",");
            break;

            case WMO_ACTIVE:
            (void) fprintf(stdout, "\"connect_status\":\"WMO active\",");
            break;
#endif

         case CLOSING_CONNECTION:
            (void) fprintf(stdout, "\"connect_status\":\"Closing connection\",");
            break;

         default:
            (void) fprintf(stdout, "\"connect_status\":\"Unknown status\",");
            break;
         }
         (void) fprintf(stdout, "\"number_of_files\":%d,", fsa[j].job_status[i].no_of_files);
         (void) fprintf(stdout, "\"number_of_files_done\":%d}", fsa[j].job_status[i].no_of_files_done);
      }
      (void) fprintf(stdout, "]}\n");
   }
   (void) fprintf(stdout, "]\n");
   exit(SUCCESS);
}

/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void usage(void)
{
   (void) fprintf(stderr, _("SYNTAX  : fsa_view [--version] [-w working directory] [-l|-s] hostname|position\n"));
   (void) fprintf(stderr, _("          Options:\n"));
   (void) fprintf(stderr, _("             -l         Long view.\n"));
   (void) fprintf(stderr, _("             -s         Short view.\n"));
   return;
}

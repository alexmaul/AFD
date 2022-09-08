/*
 *  resend_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   resend_files_cmd - resends files from the AFD archive
 **
 ** SYNOPSIS
 **   resend_files_cmd [-w <working directory>] archived-file [archived-file ...]
 **
 ** DESCRIPTION
 **   resend_files_cmd will resend all files given as parameter.
 **   Use only files that have been archived!
 **
 **   The archived file shall be given as full relative path from
 **   "afd_work_dir/archive".
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.05.1997 H.Kiehl Created
 **   10.02.1998 H.Kiehl Adapted to new message form.
 **   24.09.2004 H.Kiehl Added split job counter.
 **   16.01.2005 H.Kiehl Adapted to new message names.
 **   08.09.2022 A.Maul  Re-write the show_olog function and add main(),
 **                      to use resend from command-line instead from
 **                      show_olog dialog.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>         /* strerror(), strcmp(), strcpy(), strcat()  */
#include <stdlib.h>         /* calloc(), free(), malloc()                */
#include <unistd.h>         /* rmdir(), W_OK, F_OK                       */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>          /* utime()                                   */
#include <dirent.h>         /* opendir(), readdir(), closedir()          */
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* Status definitions for resending files. */
#define FILE_PENDING             10
#define NOT_ARCHIVED             11
#define NOT_FOUND                12
#define NOT_IN_ARCHIVE           13
#define SEND_LIMIT_REACHED       14
/* NOTE: DONE is defined in afddefs.h as 3. */

#define DEFAULT_PRIORITY 		 9

/* #define WITH_RESEND_DEBUG 1 */

/* External global variables. */
extern int              log_date_length,
                        max_hostname_length;
char                    *p_work_dir;
extern struct item_list *il;

/* Global variables. */
int                     counter_fd,
                        fsa_fd = -1,
                        fsa_id,
                        no_of_hosts;
#ifdef HAVE_MMAP
off_t                   fsa_size;
#endif
struct filetransfer_status *fsa;
int                     sys_log_fd = STDERR_FILENO;
const char              *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int              max_copied_files,
                        overwrite;
static char             archive_dir[MAX_PATH_LENGTH],
                        *p_file_name,
                        *p_archive_name,
                        *p_dest_dir_end = NULL,
#ifdef MULTI_FS_SUPPORT
                        *p_orig_msg_name,
#endif
                        *p_msg_name,
                        dest_dir[MAX_PATH_LENGTH];

struct resend_list
{
   unsigned int 		job_id;
   char 				*archive_name;
   char 				status; /* DONE - file has been resend. */
};

/* Local function prototypes. */
static int              send_new_message(char*, time_t, unsigned int,
                                         unsigned int, unsigned int, char,
                                         int, off_t),
                        get_file(char*, char*, off_t*);
static void             resend_files(int, char**),
                        get_afd_config_value(void),
                        usage(char*);

/*############################# resend_files() ##########################*/
void
resend_files(int no_selected, char **select_list)
{
#ifdef MULTI_FS_SUPPORT
   int                added_fs_id;
#endif
   int                i,
                      k,
                      total_no_of_items,
                      length = 0,
                      to_do = 0,    /* Number still to be done. */
                      no_done = 0,  /* Number done.             */
                      not_found = 0,
                      not_archived = 0,
                      not_in_archive = 0,
                      select_done = 0,
                      *select_done_list,
                      *unique_number;
   unsigned int       current_job_id = 0,
                      last_job_id = 0,
                      split_job_counter;
   time_t             creation_time = 0;
   off_t              file_size = 0,
                      total_file_size;
   static int         user_limit = 0;
   char               user_message[256];
   char               *jobid_buf_start = NULL,
                      *jobid_buffer = NULL,
                      *foo;
   struct resend_list *rl;

   overwrite = 0;
   dest_dir[0] = '\0';
   if ((rl = calloc(no_selected, sizeof(struct resend_list))) == NULL)
   {
      (void) fprintf(stderr, "calloc() error : %s (%s %d)", strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Open counter file, so we can create new unique name. */
   if ((counter_fd = open_counter_file(COUNTER_FILE, &unique_number)) < 0)
   {
      (void) fprintf(stderr, "Failed to open counter file. (%s %d)",
      __FILE__, __LINE__);
      free((void*) rl);
      return;
   }

   /* See how many files we may copy in one go. */
   get_afd_config_value();

   /* Prepare the archive directory name. */
   p_archive_name = archive_dir + sprintf(archive_dir, "%s%s/",
                                          p_work_dir, AFD_ARCHIVE_DIR);
   p_msg_name = dest_dir + sprintf(dest_dir, "%s%s%s/",
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR);
#ifdef MULTI_FS_SUPPORT
   p_orig_msg_name = p_msg_name;
#endif

   /* Get the fsa_id and no of host of the FSA. */
   if (fsa_attach("resend_files") != SUCCESS)
   {
      (void) fprintf(stderr, "Failed to attach to FSA. (%s %d)",
      __FILE__, __LINE__);
      return;
   }

   /*
    * Get the job ID, archive name and file name from cmd-line param.
    */
   for (i = 0; i < no_selected; i++)
   {
      sprintf(p_archive_name, "%s", select_list[i]);
      // search end of archive dir, backwards from end.
      foo = archive_dir + strlen(archive_dir);
      while (*foo != '/')
      {
         foo--;
      }
      p_file_name = foo + 1;
      while (*foo != '_')
      {
         foo--;
      }
      foo++;
      jobid_buffer = strndup(foo, 8);
      for (int ape = 0; ape < 3; ape++)
      {
         while (*(p_file_name++) != '_')
            ;
      }
      rl[i].job_id = (unsigned int) strtoul(jobid_buffer, NULL, 16);
      rl[i].archive_name = select_list[i];
      rl[i].status = FILE_PENDING;
      to_do++;
   }

   /*
    * Now we have the job ID of every file that is to be resend.
    * Plus we have removed those that have not been archived or
    * could not be found.
    * Lets lookup the archive directory for each job ID and then
    * collect all files that are to be resend for this ID. When
    * all files have been collected we send a message for this
    * job ID and then deselect all selected items that have just
    * been resend.
    */
   while (to_do > 0)
   {
      total_file_size = 0;
      for (i = 0; i < no_selected; i++)
      {
         if (rl[i].status == FILE_PENDING)
         {
            current_job_id = rl[i].job_id;
            break;
         }
      }

#ifdef MULTI_FS_SUPPORT
      added_fs_id = NO;
      p_msg_name = p_orig_msg_name;
#endif

      for (k = i; k < no_selected; k++)
      {
         if ((rl[k].status == FILE_PENDING) &&
             (current_job_id == rl[k].job_id))
         {
            (void)strcpy(p_archive_name, rl[k].archive_name);
            /* stattdessen mit stat() auf existenz prüfen?
             
            if (get_archive_data(rl[k].pos, rl[k].file_no) < 0)
            {
               rl[k].status = NOT_IN_ARCHIVE;
               not_in_archive++;
            }
            else
            {
            */
#ifdef MULTI_FS_SUPPORT
            if (added_fs_id == NO)
            {
               int m = 0;

               /* Copy the filesystem ID to dest_dir. */
               while ((*(p_archive_name + m) != '/') &&
                      (*(p_archive_name + m) != '\0') &&
                      (m < MAX_INT_HEX_LENGTH))
               {
                  *(p_orig_msg_name + m) = *(p_archive_name + m);
                  m++;
               }
               if ((m == MAX_INT_HEX_LENGTH) ||
                   (*(p_archive_name + m) == '\0'))
               {
                  (void)fprintf(stderr,
                             "Failed to locate filesystem ID in `%s' : (%s %d)",
                             p_archive_name, __FILE__, __LINE__);
                  return;
               }
               *(p_orig_msg_name + m) = '/';
               p_msg_name = p_dest_dir_end = p_orig_msg_name + m + 1;
               added_fs_id = YES;
            }
#endif
            if ((select_done % max_copied_files) == 0)
            {
               /* Copy a message so FD can pick up the job. */
               if (select_done != 0)
               {
                  int m;

                  if (send_new_message(p_msg_name, creation_time,
                                       (unsigned int)*unique_number,
                                       split_job_counter,
                                       current_job_id, DEFAULT_PRIORITY,
                                       max_copied_files,
                                       total_file_size) < 0)
                  {
                     (void) fprintf(stderr, "Failed to create message : (%s %d)",
                     __FILE__, __LINE__);
                     close_counter_file(counter_fd, &unique_number);
                     return;
                  }

                  select_done = 0;
                  total_file_size = 0;
               }

               /* Create a new directory. */
               creation_time = time(NULL);
               *p_msg_name = '\0';
               split_job_counter = 0;
               if (create_name(dest_dir, strlen(dest_dir), DEFAULT_PRIORITY, creation_time, current_job_id,
                        &split_job_counter, unique_number, p_msg_name,
                        MAX_PATH_LENGTH - (p_msg_name - dest_dir), counter_fd) < 0)
               {
                  (void) fprintf(stderr, "Failed to create a unique directory : (%s %d)",
                  __FILE__, __LINE__);
                  free((void*) rl);
                  close_counter_file(counter_fd, &unique_number);
                  return;
               }
               p_dest_dir_end = p_msg_name;
               while (*p_dest_dir_end != '\0')
               {
                  p_dest_dir_end++;
               }
               *(p_dest_dir_end++) = '/';
               *p_dest_dir_end = '\0';
            }
            if (get_file(dest_dir, p_dest_dir_end, &file_size) < 0)
            {
               rl[k].status = NOT_IN_ARCHIVE;
               not_in_archive++;
            }
            else
            {
               rl[k].status = DONE;
               no_done++;
               select_done++;
               total_file_size += file_size;
               last_job_id = current_job_id;
            }
            to_do--;
         }
      } /* for (k = i; k < no_selected; k++) */

      /* Copy a message so FD can pick up the job. */
      if ((select_done > 0) && ((select_done % max_copied_files) != 0))
      {
         int m;

         if (send_new_message(p_msg_name, creation_time,
         					  (unsigned int)*unique_number,
         					  split_job_counter, last_job_id, DEFAULT_PRIORITY,
                              select_done, total_file_size) < 0)
         {
            (void) fprintf(stderr, "Failed to create message : (%s %d)",
            __FILE__, __LINE__);
            close_counter_file(counter_fd, &unique_number);
            return;
         }
         select_done = 0;
         total_file_size = 0;
      }

   } /* while (to_do > 0) */

   if ((no_done == 0) && (dest_dir[0] != '\0') && (p_dest_dir_end != NULL))
   {
      /* Remove the directory we created in the files dir. */
      *p_dest_dir_end = '\0';
   }

   /* Show user a summary of what was done. */
   length = 0;
   if (no_done > 0)
   {
      if ((no_done - overwrite) == 1)
      {
         length = sprintf(user_message, "1 file resend");
      }
      else
      {
         length = sprintf(user_message, "%d files resend",
                          no_done - overwrite);
      }
   }
   if (not_archived > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not archived",
                           not_archived);
      }
      else
      {
         length = sprintf(user_message, "%d not archived", not_archived);
      }
   }
   if (not_in_archive > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not in archive",
                           not_in_archive);
      }
      else
      {
         length = sprintf(user_message, "%d not in archive", not_in_archive);
      }
   }
   if (overwrite > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d overwrites",
                           overwrite);
      }
      else
      {
         length = sprintf(user_message, "%d overwrites", overwrite);
      }
   }
   if (not_found > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not found", not_found);
      }
      else
      {
         length = sprintf(user_message, "%d not found", not_found);
      }
   }
   (void) fprintf(stderr, user_message);

   free((void*) rl);

   close_counter_file(counter_fd, &unique_number);

   if (fsa_detach(NO) < 0)
   {
      (void) fprintf(stderr, "Failed to detach from FSA. (%s %d)",
      __FILE__, __LINE__);
   }

   return;
}

/*++++++++++++++++++++++++++ send_new_message() +++++++++++++++++++++++++*/
/*                           ------------------                          */
/* Description: Sends a message via fifo to the FD.                      */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
send_new_message(char         *p_msg_name,
                 time_t       creation_time,
                 unsigned int unique_number,
                 unsigned int split_job_counter,
                 unsigned int job_id,
                 char         priority,
                 int          files_to_send,
                 off_t        file_size_to_send)
{
   unsigned short dir_no;
   int            fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  readfd,
#endif
                  ret;
   char           msg_fifo[MAX_PATH_LENGTH],
                  *ptr;

   ptr = p_msg_name;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '/')
   {
      (void) fprintf(stderr, "Unable to find directory number in `%s' (%s %d)", p_msg_name, __FILE__, __LINE__);
      return (INCORRECT);
   }
   dir_no = (unsigned short)strtoul(ptr + 1, NULL, 16);

   (void)strcpy(msg_fifo, p_work_dir);
   (void)strcat(msg_fifo, FIFO_DIR);
   (void)strcat(msg_fifo, MSG_FIFO);

   /* Open and create message file. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(msg_fifo, &readfd, &fd) == -1)
#else
   if ((fd = open(msg_fifo, O_RDWR)) == -1)
#endif
   {
      (void) fprintf(stderr, "Could not open %s : %s (%s %d)", msg_fifo, strerror(errno), __FILE__, __LINE__);
      ret = INCORRECT;
   }
   else
   {
      char fifo_buffer[MAX_BIN_MSG_LENGTH];

      /* Fill fifo buffer with data. */
      *(time_t *)(fifo_buffer) = creation_time;
#ifdef MULTI_FS_SUPPORT
# if SIZEOF_TIME_T == 4
      *(unsigned int *)(fifo_buffer + sizeof(time_t)) = (dev_t)strtoul(p_archive_name, NULL, 16);;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
      *(off_t *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                 sizeof(unsigned int) + sizeof(unsigned int) +
                 sizeof(unsigned int)) = file_size_to_send;
# else
      *(off_t *)(fifo_buffer + sizeof(time_t)) = file_size_to_send;
      *(off_t *)(fifo_buffer + sizeof(time_t) + sizeof(off_t)) = (dev_t)strtoul(p_archive_name, NULL, 16);;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(dev_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(dev_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(dev_t) + sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
# endif
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int) + sizeof(unsigned int) +
                        sizeof(unsigned int) + sizeof(off_t)) = unique_number;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                          sizeof(unsigned int) + sizeof(unsigned int) +
                          sizeof(unsigned int) + sizeof(off_t) +
                          sizeof(unsigned int)) = dir_no;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                sizeof(unsigned int) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(off_t) + sizeof(unsigned int) +
                sizeof(unsigned short)) = priority;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                sizeof(unsigned int) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(off_t) + sizeof(unsigned int) +
                sizeof(unsigned short) + sizeof(char)) = SHOW_OLOG_NO;
#else
# if SIZEOF_TIME_T == 4
      *(unsigned int *)(fifo_buffer + sizeof(time_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
      *(off_t *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                 sizeof(unsigned int) +
                 sizeof(unsigned int)) = file_size_to_send;
# else
      *(off_t *)(fifo_buffer + sizeof(time_t)) = file_size_to_send;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) +
                        sizeof(off_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
# endif
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                        sizeof(unsigned int) + sizeof(unsigned int) +
                        sizeof(off_t)) = unique_number;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                          sizeof(unsigned int) + sizeof(unsigned int) +
                          sizeof(off_t) + sizeof(unsigned int)) = dir_no;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) +
                sizeof(unsigned int) + sizeof(unsigned short)) = priority;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) +
                sizeof(unsigned int) + sizeof(unsigned short) +
                sizeof(char)) = SHOW_OLOG_NO;
#endif

      /* Send the message. */
      if (write(fd, fifo_buffer, MAX_BIN_MSG_LENGTH) != MAX_BIN_MSG_LENGTH)
      {
         (void) fprintf(stderr, "Could not write to %s : %s (%s %d)", msg_fifo, strerror(errno), __FILE__, __LINE__);
         ret = INCORRECT;
      }
      else
      {
         ret = SUCCESS;
      }

#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(readfd) == -1)
      {
         (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)",
                    msg_fifo, strerror(errno), __FILE__, __LINE__);
      }
#endif
      if (close(fd) == -1)
      {
         (void) fprintf(stderr, "Failed to close() %s : %s (%s %d)", msg_fifo, strerror(errno), __FILE__, __LINE__);
      }
   }

   return(ret);
}


/*++++++++++++++++++++++++++++++ get_file() +++++++++++++++++++++++++++++*/
/*                               ----------                              */
/* Description: Will try to link a file from the archive directory to    */
/*              new file directory. If it fails to link them and errno   */
/*              is EXDEV (file systems diver) or EEXIST (file exists),   */
/*              it will try to copy the file (ie overwrite it in case    */
/*              errno is EEXIST).                                        */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_file(char *dest_dir, char *p_dest_dir_end, off_t *file_size)
{
   (void)strcpy(p_dest_dir_end, p_file_name);
   if (eaccess(archive_dir, W_OK) == 0)
   {
      if (link(archive_dir, dest_dir) < 0)
      {
         switch (errno)
         {
            case EEXIST : /* File already exists. Overwrite it. */
                          overwrite++;
                          /* NOTE: Falling through! */
            case EXDEV  : /* File systems differ. */
                          if (copy_file(archive_dir, dest_dir, NULL) < 0)
                          {
                             (void)fprintf(stdout,
                                           "Failed to copy %s to %s (%s %d)\n",
                                           archive_dir, dest_dir,
                                           __FILE__, __LINE__);
                             return(INCORRECT);
                          }
                          break;
            default:      /* All other errors go here. */
                          (void)fprintf(stdout,
                                        "Failed to link() %s to %s : %s (%s %d)\n",
                                        archive_dir, dest_dir, strerror(errno),
                                        __FILE__, __LINE__);
                          return(INCORRECT);
         }
      }
      else /* We must update the file time or else when age limit is */
           /* set the files will be deleted by process sf_xxx before */
           /* being send.                                            */
      {
         struct stat stat_buf;

         if (utime(dest_dir, NULL) == -1)
         {
            /*
             * Do NOT use xrec() here to report any errors. If you try
             * to do this with al lot of files your screen will be filled
             * with lots of error messages.
             */
            (void)fprintf(stdout, "Failed to set utime() of %s : %s (%s %d)\n",
                          dest_dir, strerror(errno), __FILE__, __LINE__);
         }

         if (stat(dest_dir, &stat_buf) == -1)
         {
            (void)fprintf(stdout, "Failed to stat() `%s' : %s (%s %d)\n",
                          dest_dir, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
         else
         {
            *file_size = stat_buf.st_size;
         }
      }
   }
   else
   {
      if (eaccess(archive_dir, R_OK) == 0)
      {
         /*
          * If we do not have write permission to the file we must copy
          * the file so the date of the file is that time when we copied it.
          */
         int from_fd;

         if ((from_fd = open(archive_dir, O_RDONLY)) == -1)
         {
            /* File is not in archive dir. */
            (void)fprintf(stdout, "Failed to open() `%s' : %s (%s %d)\n",
                          archive_dir, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
         else
         {
            struct stat stat_buf;

            if (fstat(from_fd, &stat_buf) == -1)
            {
               (void)fprintf(stderr, "Failed to fstat() %s : %s (%s %d)\n",
                             archive_dir, strerror(errno), __FILE__, __LINE__);
               (void)close(from_fd);
               return(INCORRECT);
            }
            else
            {
               int to_fd;

               (void)unlink(dest_dir);
               if ((to_fd = open(dest_dir, O_WRONLY | O_CREAT | O_TRUNC,
                                 stat_buf.st_mode)) == -1)
               {
                  (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                                dest_dir, strerror(errno), __FILE__, __LINE__);
                  (void)close(from_fd);
                  return(INCORRECT);
               }
               else
               {
                  if (stat_buf.st_size > 0)
                  {
                     int  bytes_buffered;
                     char *buffer;

                     if ((buffer = malloc(stat_buf.st_blksize)) == NULL)
                     {
                        (void)fprintf(stderr,
                                      "malloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        (void)close(from_fd); (void)close(to_fd);
                        return(INCORRECT);
                     }

                     do
                     {
                        if ((bytes_buffered = read(from_fd, buffer,
                                                   stat_buf.st_blksize)) == -1)
                        {
                           (void)fprintf(stderr,
                                         "Failed to read() from %s : %s (%s %d)\n",
                                         archive_dir, strerror(errno),
                                         __FILE__, __LINE__);
                           free(buffer);
                           (void)close(from_fd);
                           (void)close(to_fd);
                           return(INCORRECT);
                        }
                        if (bytes_buffered > 0)
                        {
                           if (write(to_fd, buffer, bytes_buffered) != bytes_buffered)
                           {
                              (void)fprintf(stderr,
                                            "Failed to write() to %s : %s (%s %d)\n",
                                            dest_dir, strerror(errno),
                                            __FILE__, __LINE__);
                              free(buffer);
                              (void)close(from_fd);
                              (void)close(to_fd);
                              return(INCORRECT);
                           }
                        }
                     } while (bytes_buffered == stat_buf.st_blksize);
                     free(buffer);
                  }
                  (void)close(to_fd);
                  *file_size = stat_buf.st_size;
               }
            }
            (void)close(from_fd);
         }
      }
      else
      {
         if (link(archive_dir, dest_dir) < 0)
         {
            (void)fprintf(stdout, "Failed to link() %s to %s : %s (%s %d)\n",
                          archive_dir, dest_dir, strerror(errno),
                          __FILE__, __LINE__);
            return(INCORRECT);
         }
         else
         {
            struct stat stat_buf;

            /*
             * Since we do not have write permission we cannot update
             * the access and modification time. So if age limit is set,
             * it can happen that the files are deleted immediatly by
             * sf_xxx.
             */
            if (stat(dest_dir, &stat_buf) == -1)
            {
               (void)fprintf(stdout, "Failed to stat() `%s' : %s (%s %d)\n",
                             dest_dir, strerror(errno), __FILE__, __LINE__);
               return(INCORRECT);
            }
            else
            {
               *file_size = stat_buf.st_size;
            }
         }
      }
   }

   return(SUCCESS);
}

/*++++++++++++++++++++++++ get_afd_config_value() +++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char                 *buffer,
                        config_file[MAX_PATH_LENGTH];

   (void) sprintf(config_file, "%s%s%s", p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0)
            && (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, MAX_COPIED_FILES_DEF, value, MAX_INT_LENGTH) != NULL)
      {
         max_copied_files = atoi(value);
         if ((max_copied_files < 1) || (max_copied_files > 10240))
         {
            max_copied_files = MAX_COPIED_FILES;
         }
      }
      else
      {
         max_copied_files = MAX_COPIED_FILES;
      }
      free(buffer);
   }
   else
   {
      max_copied_files = MAX_COPIED_FILES;
   }

   return;
}

/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ reset_fsa() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  arg_i = 1,
                        no_arc = 0;
   char                 work_dir[MAX_PATH_LENGTH];

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if (argc > arg_i)
   {
      if ((argv[1][0] == '-') && (argv[1][1] == 'w'))
      {
         arg_i += 2;
      }
      if (argc > arg_i)
      {
         resend_files(argc - arg_i, argv + arg_i);
      }
   }
   else
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   exit(SUCCESS);
}

/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void) fprintf(stderr, _("SYNTAX  : %s [-w working directory] archived-file [archived-file ...]\n"), progname);
   return;
}

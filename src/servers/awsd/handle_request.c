/*
 *  handle_request.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_request - handles a TCP request
 **
 ** SYNOPSIS
 **   void handle_request(int  cmd_sd,
 **                       int  pos,
 **                       int  trusted_ip_pos,
 **                       char *remote_ip_str)
 **
 ** DESCRIPTION
 **   Handles all request from remote user in a loop. If user is inactive
 **   for AWSD_CMD_TIMEOUT seconds, the connection will be closed.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1999 H.Kiehl Created
 **   06.04.2005 H.Kiehl Open FSA here and not in awsd.c.
 **   23.11.2008 H.Kiehl Added danger_no_of_jobs.
 **   22.03.2014 H.Kiehl Added typesize data information (TD).
 **   31.03.2017 H.Kiehl Do not count the data from group identifiers.
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* memset()                                */
#include <stdlib.h>           /* atoi(), strtoul()                       */
#include <time.h>             /* time()                                  */
#include <ctype.h>            /* toupper()                               */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>           /* close()                                 */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include "awsddefs.h"
#include "version.h"
#include "logdefs.h"

/* #define DEBUG_LOG_CMD */

/* Global variables. */
int cmd_sd, fra_fd = -1, fra_id, fsa_fd = -1, fsa_id, host_config_counter, in_log_child = NO, no_of_dirs, no_of_hosts;
#ifdef HAVE_MMAP
off_t fra_size,
fsa_size;
#endif
char *line_buffer = NULL, log_dir[MAX_PATH_LENGTH], *log_buffer = NULL, *p_log_dir;
unsigned char **old_error_history;
FILE *p_data = NULL;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;

/* External global variables. */
extern int *ip_log_defs, log_defs;
extern long danger_no_of_jobs;
extern char afd_name[], hostname[], *p_work_dir, *p_work_dir_end;
extern struct logdata ld[];
extern struct afd_status *p_afd_status;

/* Local global variables. */
static int report_changes = NO;

/*  Local function prototypes. */
static void report_shutdown(void);

struct http_request* parse_request(struct http_request *, int);
void free_http_request(struct http_request *);

/*########################### handle_request() ##########################*/
void handle_request(int sock_sd, int pos, int trusted_ip_pos, char *remote_ip_str)
{
   register int i, j;
   int status;
   long log_interval = 0;
   char buf[1024];
   fd_set rset;
   struct timeval timeout;
   struct http_request *request = NULL;

   cmd_sd = sock_sd;
   if ((p_data = fdopen(cmd_sd, "r+")) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("fdopen() control error : %s"), strerror(errno));
      exit(INCORRECT);
   }

   if (fsa_attach_passive(NO, AWSD) != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FSA."));
      exit(INCORRECT);
   }
   host_config_counter = (int) *(unsigned char *) ((char *) fsa - AFD_WORD_OFFSET + SIZEOF_INT);
   RT_ARRAY(old_error_history, no_of_hosts, ERROR_HISTORY_LENGTH, unsigned char);
   for (i = 0; i < no_of_hosts; i++)
   {
      if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
      {
         (void) memset(old_error_history[i], 0, ERROR_HISTORY_LENGTH);
      }
      else
      {
         (void) memcpy(old_error_history[i], fsa[i].error_history, ERROR_HISTORY_LENGTH);
      }
   }
   if (fra_attach_passive() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FRA."));
      exit(INCORRECT);
   }

   status = 0;
   while (p_afd_status->amg_jobs & WRITTING_JID_STRUCT)
   {
      (void) my_usleep(100000L);
      status++;
      if ((status > 1) && ((status % 100) == 0))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                  _("Timeout arrived for waiting for AMG to finish writting to JID structure."));
      }
   }

   if (atexit(report_shutdown) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("Could not register exit handler : %s"), strerror(errno));
   }

   init_get_display_data();

   /*
    * Handle the request and close connection as a web-browser would expect.
    */
   FD_ZERO(&rset);
   FD_SET(cmd_sd, &rset);
   timeout.tv_sec = AWSD_CMD_TIMEOUT;
   timeout.tv_usec = 0;

   /* Wait for message x seconds and then continue. */
   status = select(cmd_sd + 1, &rset, NULL, NULL, &timeout);

   fprintf(stderr, "request %p\n\n", request); //XXX
   if (FD_ISSET(cmd_sd, &rset))
   {
      request = parse_request(NULL, cmd_sd);
   }
   fprintf(stderr, "request %p\n\n", request); //XXX

   if (request)
   {
      fprintf(stderr, "request_method %d\n\nurl: '%s'\n\n", request->http_method, request->url); //XXX

      if (request->http_method != HTTP_METHOD_GET && request->http_method != HTTP_METHOD_POST)
      {
         print_http_state(p_data, HTTP_STATUS_400);
      }
      else
      {
         /*
          @app.route("/")
          @app.route("/fsa/json", methods=["GET"])
          @app.route("/alias/info/<host>", methods=["POST"])
          @app.route("/alias/<action>", methods=["POST"])
          @app.route("/alias/<action>/<host>", methods=["GET"])
          @app.route("/afd/<command>", methods=["GET"])
          @app.route("/afd/<command>/<host>", methods=["GET"])
          @app.route("/afd/<command>/<action>", methods=["POST"])
          @app.route("/log/<typ>", methods=["POST"])
          @app.route("/view/<mode>/<path:arcfile>", methods=["GET"])
          */
         if (strcmp(request->url, "/") == 0)
         {
            (void) snprintf(p_work_dir_end,
            MAX_PATH_LENGTH - (p_work_dir_end - p_work_dir), "/%s/%s", AWSD_DIR, DEFAULT_AWSD_PAGE);
            display_file(p_data);
            *p_work_dir_end = '\0';
         }
         else if (strcmp(request->path_list[0], "static") == 0 && request->path_list[1] != NULL)
         {
            for (i = 0; request->path_list[i] != NULL; i++)
               ;
            i--;

            (void) snprintf(p_work_dir_end,
            MAX_PATH_LENGTH - (p_work_dir_end - p_work_dir), "/%s/%s", AWSD_DIR, request->path_list[i]);

            fprintf(stderr, "->'%s'\n", p_work_dir); //XXX

            display_file(p_data);

            fprintf(stderr, "display done\n"); //XXX

            *p_work_dir_end = '\0';
         }
         else if (request->http_method == HTTP_METHOD_GET && strcmp(request->path_list[0], "fsa") == 0)
         {
            /*  */
         }
         else if (strcmp(request->path_list[0], "alias") == 0)
         {

            if (request->http_method == HTTP_METHOD_POST)
            {
               if (strcmp(request->path_list[1], "info") == 0)
               {

               }
               else
               {
                  if (strcmp(request->path_list[1], "start") == 0)
                  {

                  }
                  else if (strcmp(request->path_list[1], "stop") == 0)
                  {

                  }
                  else if (strcmp(request->path_list[1], "able") == 0)
                  {

                  }
                  else if (strcmp(request->path_list[1], "debug") == 0)
                  {

                  }
                  else if (strcmp(request->path_list[1], "trace") == 0)
                  {

                  }
                  else if (strcmp(request->path_list[1], "fulltrace") == 0)
                  {

                  }
                  else if (strcmp(request->path_list[1], "switch") == 0)
                  {

                  }
                  else if (strcmp(request->path_list[1], "retry") == 0)
                  {

                  }
               }
            }
            else if (request->http_method == HTTP_METHOD_GET)
            {
               if (strcmp(request->path_list[0], "info") == 0)
               {

               }
               else if (strcmp(request->path_list[0], "config") == 0)
               {

               }
            }
         }
         else if (request->http_method == HTTP_METHOD_GET && strcmp(request->path_list[0], "afd") == 0)
         {

         }
         else if (request->http_method == HTTP_METHOD_POST && strcmp(request->path_list[0], "afd") == 0)
         {

         }
         else if (request->http_method == HTTP_METHOD_POST && strcmp(request->path_list[0], "log") == 0)
         {

         }
         else if (request->http_method == HTTP_METHOD_GET && strcmp(request->path_list[0], "view") == 0)
         {

         }
         else
         {
            print_http_state(p_data, HTTP_STATUS_404);
            (void) fflush(p_data);
         }
      }
   } /* if (nbytes > 0) */

   (void) fflush(p_data);
   if (fclose(p_data) == EOF)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__, _("fclose() error : %s"), strerror(errno));
   }
   p_data = NULL;

   free_http_request(request);

   close_get_display_data();

   exit(SUCCESS);
}

void free_http_request(struct http_request *request)
/*
 * Release all memory allocated by parse_request().
 */
{
   free(request->path_list);
   free(request->header_list);
   free(request->param_list);
   free(request->content);
   free(request);
   request = NULL;
}

struct http_request* parse_request(struct http_request *prev_request, int sd)
/*
 * Reads all data from socket-descriptor sd and parses, if it's HTTP.
 *
 * If prev_request is not NULL, this structure is ammended, otherwise a new one is allocated.
 */
{
   char path_token[10][100];
   char buf_param[100][2][100];
   int head_idx, i;
   char *pbuf, *pa, *pb;
   char buf[4096];
   int nbytes;
   struct http_request *request = NULL;

   fprintf(stderr, "parse_request\n"); //XXX

   if (prev_request == NULL)
   {
      if ((request = (struct http_request*) malloc(sizeof(struct http_request))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, _("malloc() error"));
         return NULL;
      }
      request->url[0] = '\0';
      request->path_list = (char**) calloc(10, sizeof(char*));
      request->header_list = (struct http_key_value*) malloc(sizeof(struct http_key_value) * 10);
      request->param_list = (struct http_key_value*) malloc(sizeof(struct http_key_value) * 10);
   }
   else
   {
      request = prev_request;
   }
   if ((nbytes = read(sd, buf, 4096)) <= 0)
   {
      if (nbytes == 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__, _("Remote hangup"));
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, _("read() error : %s"), strerror(errno));
      }
      return NULL;
   }
   fprintf(stderr, "%s\n\n", buf); //XXX
   if (strncmp(buf, "GET /", 5) == 0)
   {
      request->http_method = HTTP_METHOD_GET;
   }
   else if (strncmp(buf, "POST /", 6) == 0)
   {
      request->http_method = HTTP_METHOD_POST;
   }
   pbuf = strchr(buf, '/');
   if (pbuf[0] == '/' && pbuf[1] == 0x20)
   {
      strcpy(request->url, "/static/afd-gui.html");
   }
   else
   {
      pa = pbuf;
      pb = strchr(pa, 0x20);
      strncpy(request->url, pa, pb - pa);
      request->url[pb - pa] = '\0';
   }
   /* tokenize the url */
   i = 0;
   pa = strdup(request->url);
   pb = strtok(pa, "/");
   request->path_list[i++] = strdup(pb);
   while ((pb = strtok(NULL, "/")) != NULL && i < 3)
   {
      request->path_list[i++] = strdup(pb);
   }
   free(pa);

   pbuf = strchr(pbuf, '\n') + 1;
   head_idx = 0;
   while (pbuf < buf + nbytes)
   {
      pa = pbuf;
      pbuf = strchr(pbuf, ':');
      request->header_list[head_idx].key = strndup(pa, pbuf - pa);
      while (*(++pbuf) <= ' ')
         ;
      pb = pbuf;
      while (*(++pbuf) >= ' ')
         ;
      request->header_list[head_idx].value = strndup(pb, pbuf - pb);
      request->header_list[head_idx].type = HTTP_PARAM_TYPE_STR;

      if (pbuf[0] == '\r' && pbuf[1] == '\n' && (pbuf >= buf + nbytes || pbuf[2] == '\r' && pbuf[3] == '\n'))
      {
         /* empty line divides header from content */
         break;
      }

      fprintf(stderr, "header #%d : '%s' = '%s'\n", head_idx, request->header_list[head_idx].key,
               (char*) request->header_list[head_idx].value); //XXX

      if (strcmp(request->header_list[head_idx].key, "Content-Length") == 0)
      {
         request->content_length = atoi(request->header_list[head_idx].value);
      }

      head_idx++;
      while (*(++pbuf) <= '@')
         ;
   }
   if (request->http_method == HTTP_METHOD_POST && pbuf < buf + nbytes)
   {
      request->content = malloc(request->content_length);
      /* copy content */
      strncpy(request->content, pbuf, buf + nbytes - pbuf);
   }
   else
   {
      request->content = NULL;
      request->content_length = 0;
   }
   fprintf(stderr, "request parsed\n"); //XXX
   fprintf(stderr, "url           : '%s'\n", request->url); //XXX
   fprintf(stderr, "content-length: %d\n", request->content_length); //XXX

   return request;
}

/*++++++++++++++++++++++++ report_shutdown() ++++++++++++++++++++++++++++*/
static void report_shutdown(void)
{
   if (in_log_child == NO)
   {
      if (p_data != NULL)
      {
         if (report_changes == YES)
         {
            show_summary_stat(p_data);
            check_changes(p_data);
         }
         (void) fprintf(p_data, "%s\r\n", AWSD_SHUTDOWN_MESSAGE);
         (void) fflush(p_data);

         if (fclose(p_data) == EOF)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__, _("fclose() error : %s"), strerror(errno));
         }
         p_data = NULL;
      }
   }

   return;
}

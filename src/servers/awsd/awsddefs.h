/*
 *  wsddefs - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2015 Deutscher Wetterdienst (DWD),
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

#ifndef __awsddefs_h
#define __awsddefs_h

#include "../log/logdefs.h" /* MAX_LOG_NAME_LENGTH */

#define NOT_SET                  -1
#define DEFAULT_FILE_NO          0
#define HUNK_MAX                 4096
#define MAX_LOG_DATA_BUFFER      131072 /* 128KB */
/********************************************************************/
/* NOTE: MAX_LOG_DATA_BUFFER _MUST_ be divisible by MAX_LINE_LENGTH */
/*       and there may NOT be a rest!                               */
/********************************************************************/
#define MAX_LOG_COMMAND_LENGTH   (2 + 1 + MAX_INT_LENGTH + 1 + \
                                  MAX_INT_LENGTH + 1 + MAX_INT_LENGTH + 1)
#define DEFAULT_AWSD_PORT_NO     "4334"
#define DEFAULT_AWSD_PAGE        "afd-gui.html"
#define DEFAULT_AWSD_LOG_DEFS    0
#define DEFAULT_FILE_NO          0
#define EVERYTHING               -1
#define AWSD_CMD_TIMEOUT         900
#define AWSD_LOG_CHECK_INTERVAL  2
#define MAX_AWSD_CONNECTIONS     5
#define MAX_AWSD_CONNECTIONS_DEF "MAX_AWSD_CONNECTIONS"
#define AFD_SHUTTING_DOWN        124
#define LOG_WRITE_INTERVAL       30 /* Interval at which we must write */
/* some log data before afd_mon    */
/* thinks that the connection is   */
/* dead and disconnects.           */

#define DEFAULT_CHECK_INTERVAL   3 /* Default interval in seconds to */
/* check if certain values have   */
/* changed in the FSA.            */

#define HELP_CMD              "HELP\r\n"
#define QUIT_CMD              "QUIT\r\n"
#define TRACEI_CMD            "TRACEI"
#define TRACEI_CMD_LENGTH     (sizeof(TRACEI_CMD) - 1)
#define TRACEI_CMDL           "TRACEI\r\n"

#define AWSD_SHUTDOWN_MESSAGE "500 AWSD shutdown."

/* Shortcut pragmas for http-header.
 * a: filehandle, b: value (str|int)
 */
#define print_http_state(a, b) (fprintf(a, "HTTP/1.1 %s\r\n", b))
#define print_http_content_length(a, b) (fprintf(a, "Content-length: %d\r\n", b))
#define print_http_content_type(a, b) (fprintf(a, "Content-type: %s\r\n", b))

#define HTTP_METHOD_GET     1
#define HTTP_METHOD_POST    2

#define HTTP_CONTENT_TYPE_TEXT      "text/plain"
#define HTTP_CONTENT_TYPE_HTML      "text/html"
#define HTTP_CONTENT_TYPE_JSON      "application/json"

#define HTTP_STATUS_200    "200 OK"
#define HTTP_STATUS_202    "202 Accepted"
#define HTTP_STATUS_204    "204 No Content"
#define HTTP_STATUS_304    "304 Not Modified"
#define HTTP_STATUS_308    "308 Permanent Redirect"
#define HTTP_STATUS_400    "400 Bad Request"
#define HTTP_STATUS_401    "401 Unauthorized"
#define HTTP_STATUS_403    "403 Forbidden"
#define HTTP_STATUS_404    "404 Not Found"
#define HTTP_STATUS_405    "405 Method Not Allowed"
#define HTTP_STATUS_408    "408 Request Timeout"
#define HTTP_STATUS_500    "500 Internal Server Error"
#define HTTP_STATUS_501    "501 Not Implemented"
#define HTTP_STATUS_503    "503 Service Unavailable"

/* Definitions for the different logs in the logdata array. */
#define SYS_LOG_POS           0
#define EVE_LOG_POS           1
#define REC_LOG_POS           2
#define TRA_LOG_POS           3
#define TDB_LOG_POS           4
#ifdef _INPUT_LOG
# define INP_LOG_POS          5
# ifdef _DISTRIBUTION_LOG
#  define DIS_LOG_POS         6
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        7
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       8
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      9
#     define DUM_LOG_POS      10
#     define NO_OF_LOGS       11
#    else
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       7
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   endif
#  endif
# else
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        6
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       7
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       6
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  endif
# endif
#else
# ifdef _DISTRIBUTION_LOG
#  define DIS_LOG_POS         5
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        6
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       7
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      8
#     define DUM_LOG_POS      9
#     define NO_OF_LOGS       10
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       6
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  endif
# else
#  ifdef _PRODUCTION_LOG
#   define PRO_LOG_POS        5
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       6
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      7
#     define DUM_LOG_POS      8
#     define NO_OF_LOGS       9
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   endif
#  else
#   ifdef _OUTPUT_LOG
#    define OUT_LOG_POS       5
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      6
#     define DUM_LOG_POS      7
#     define NO_OF_LOGS       8
#    else
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    endif
#   else
#    ifdef _DELETE_LOG
#     define DEL_LOG_POS      5
#     define DUM_LOG_POS      6
#     define NO_OF_LOGS       7
#    else
#     define DUM_LOG_POS      5
#     define NO_OF_LOGS       6
#    endif
#   endif
#  endif
# endif
#endif

/* Structure to hold all relevant log managemant data. */
#define FIRST_POS_SET         1
struct logdata
{
   char log_name[MAX_LOG_NAME_LENGTH + 1];
   char log_data_cmd[3];
   char log_inode_cmd[3];
   FILE *fp;
   ino_t current_log_inode;
   off_t offset;
   int current_log_no;
   int log_name_length;
   unsigned int log_flag;
   unsigned int options;
   unsigned int packet_no;
   unsigned int flag;
};

/* Structure to hold open file descriptors. */
#define AWSD_ILOG_NO             0
#define AWSD_OLOG_NO             1
#define AWSD_SLOG_NO             2
#define AWSD_TLOG_NO             3
#define AWSD_TDLOG_NO            4
#define MAX_AWSD_LOG_FILES       5
struct fd_cache
{
   ino_t st_ino;
   int fd;
};

/* Structure holding a parsed http request.
 * The void* and struct* are lists with the last element set to NULL.
 */
struct http_request
{
   unsigned int http_method;
   char url[1024];
   char **path_list;
   struct http_key_value *param_list;
   struct http_key_value *header_list;
   char *content;
   unsigned int content_length;
   unsigned int content_type;
};

#define HTTP_PARAM_TYPE_STR     1
#define HTTP_PARAM_TYPE_INT     2
#define HTTP_PARAM_TYPE_DOUBLE  3

struct http_key_value
{
   char *key;
   void *value;
   unsigned int type;
};

/* Function prototypes. */
extern int get_display_data(char *, int, char *, int, int, int, int);
extern long check_logs(time_t);
extern void check_changes(FILE *), close_get_display_data(void), display_file(FILE *), handle_request(int, int, int,
         char *), init_get_display_data(void), show_dir_list(FILE *), show_host_list(FILE *), show_host_stat(FILE *),
         show_job_list(FILE *), show_summary_stat(FILE *);

#endif /* __awsddefs_h */

#include <stdio.h>
#include <string.h>
#include "magic.h"

int main(int argc, char *argv[])
{
   magic_t mookie;
   const char *mime;
   char fn[20] = "play.c";

   mookie = magic_open(MAGIC_SYMLINK | MAGIC_MIME_TYPE);
   magic_load(mookie, NULL);
   mime = magic_file(mookie, fn);
   printf("MIME: %s\n", mime);
   magic_close(mookie);

   return 0;
}

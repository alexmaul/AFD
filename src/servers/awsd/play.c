#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
   char a[]="/alias/info/<host>";
   char b[]="/aa/";
   char *p;

   printf("a: %p %s\n",a,a);
   p=strtok(a, "/");
   printf("%p %s\n",p,p);
   p=strtok(NULL, "/");
   printf("%p %s\n",p,p);
   p=strtok(NULL, "/");
   printf("%p %s\n",p,p);

   printf("b: %p %s\n",b,b);
   p=strtok(b, "/");
   printf("%p %s\n",p,p);
   p=strtok(NULL, "/");
   printf("%p %s\n",p,p);

   return 0;
}

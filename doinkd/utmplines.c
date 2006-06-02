/* Written By Mike Crider on September 18, 1994
 * Prints out the number of lines in the utmp file.
 * Use a number greater than the output number for the
 * MAXUSERS definition in doinkd.h.
 *
 * Cleaned for distribution on October 3, 1995
 */

/* Choose a system type */
/* #define SYSV */
/* #define BSD */

#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include "doinkd.h"

#if !defined(SYSV) && !defined(BSD)
#  define SYSV
#endif

/* Maybe this should always be included? */
#ifdef SYSV
#  include <fcntl.h>
#endif SYSV

int main()
{
#ifdef HAVE_UTMPX
   int fd, numlines;
   struct utmpx uent;

   fd = open(UTMP_FILE,O_RDONLY,0);

   numlines = 0;
   while (read(fd,&uent,sizeof(struct utmpx)) > 0)
      numlines++;
   
#else
   int fd, numlines;
   struct utmp uent;

   fd = open(UTMP_FILE,O_RDONLY,0);

   numlines = 0;
   while (read(fd,&uent,sizeof(struct utmp)) > 0)
      numlines++;
   
#endif

   close(fd);

   printf("Number of lines in utmp file: %d\n",numlines);

   return 0;
}

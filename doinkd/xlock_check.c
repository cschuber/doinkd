/* This module checks for an "xlock" console locking programming
 * running on the machine by the person logged in on console.
 * If one is found, it returns the time that it was started.
 */
 
#include <time.h>       /* For time_t, time() */
#include <string.h>     /* For my string copies and compares */
#include <pwd.h>        /* For struct passwd and getpwnam */
#include <fcntl.h>      /* For O_RDONLY */

#ifdef HAVE_PROC_FS
#  include <sys/ioctl.h>  /* For _IOR define for OSF */
#  include <dirent.h>     /* For opendir, readdir, and such */
#  include <sys/procfs.h> /* For prpsinfo_t */
#endif  /* HAVE_PROC_FS */

#ifdef PROC_SEARCH_1
#  include <kvm.h>
#  include <sys/param.h>
#  include <sys/proc.h>
#  include <sys/user.h>
#endif /* PROC_SEARCH_1 */

#define user user_i     /* Don't want user struct from doinkd */
#include "doinkd.h"
#undef user             /* need user from <sys/user.h> instead */

extern void   logfile();

#ifndef DEBUG
#  define DEBUG 0
#endif
#if (DEBUG > 0)
#   define debug(level,string)  if (DEBUG > level) logfile string
#else
#   define debug(level,string)  /* Erase debug code, since DEBUG is off */
#endif

#if defined(HAVE_PROC_FS) || defined(PROC_SEARCH_1)
int sameuser(username,uid)
   char    *username;
   uid_t   uid;
{
   struct passwd *pw;

   pw = getpwnam(username);
   if (pw == NULL)
   {
      debug(3,("Error looking up uid for user %s in xlock_check.",username));
      return 0;
   }
   if (pw->pw_uid == uid)
   {
      return 1;
   }
   return 0;
}
#endif /* defined(HAVE_PROC_FS) || defined(PROC_SEARCH_1) */

#if defined(BSD_OS2)
/* Return the uid of the given username */
int getuid(username)
   char    *username;
{
   struct passwd *pw;

   pw = getpwnam(username);
   if (pw == NULL)
   {
      debug(3,("Error looking up uid for user %s in xlock_check.",username));
      return -1;
   }

   return pw->pw_uid;
}
#endif /* BSD_OS2 */
   
time_t xlock_check(username)
   char    *username;
{
#ifdef HAVE_PROC_FS
   time_t starttime;
   time_t tempus;
#ifdef UNIXWARE
   psinfo_t procinfo;
#else
   prpsinfo_t procinfo;
#endif
   int     procfd;
   DIR    *dirp;
   struct dirent *dp;
#ifdef UNIXWARE
   static char procfile[] = "/proc/XXXXX/psinfo";
#else
   static char procfile[] = "/proc/XXXXX";
#endif

   starttime = -1;

   (void) time(&tempus);

   dirp = opendir ("/proc");
   if (dirp == NULL)
   {
      debug(1,("Error.  Cannot open /proc"));
      return NULL;
   }

   while ((dp = readdir (dirp)) != NULL)
   {
      /* Skip "." and ".." (some NFS filesystems' directories lack them). */
      if (dp->d_name != NULL && strcmp (".", dp->d_name) != 0 && strcmp ("..", dp->d_name) != 0)
      {

#ifdef UNIXWARE
	strncpy (procfile + 6, dp->d_name, 6);
	strncat (procfile, "/psinfo", 7);
    debug(3,("psinfo file is %s.", procfile));
#else
	 strncpy (procfile + 6, dp->d_name, 5);
#endif

	 if ((procfd = open (procfile, O_RDONLY)) >= 0)
	 {

#ifdef UNIXWARE
		read(procfd,&procinfo,sizeof(procinfo));
#else
	    ioctl (procfd, PIOCPSINFO, &procinfo);
#endif
	    close (procfd);

	    /* Only collect non-zombies with controlling tty */
#ifdef UNIXWARE
	    if (procinfo.pr_ttydev != NODEV && procinfo.pr_nlwp != 0)
#else
	    if (procinfo.pr_ttydev != NODEV && !procinfo.pr_zomb)
#endif
	    {
	       if (procinfo.pr_fname == NULL)
	       {
		  debug(3,("procinfo.pr_fname is null"));
	       }
	       else if (strcmp(procinfo.pr_fname,XLOCK_NAME) == 0)
	       {
                  if (sameuser(username,procinfo.pr_uid))
                  {
                     starttime = tempus-procinfo.pr_start.tv_sec;
                     debug(2,("Program %s	started by %s at %d.", XLOCK_NAME, username,
                            (int)starttime));
                     break;     /* Just find the first xlock program */
                  }
                  else
                  {
                     debug(3,("xlock running by different user."));
                  }
	       }
	    }
	 }
      }
   }
   closedir (dirp);

   return starttime;

#endif /* HAVE_PROC_FS */ /******************************************/

#ifdef PROC_SEARCH_1
   kvm_t  *kd;
   struct proc *PCB;
   struct user *u;
   time_t tempus, starttime;
#if defined(BSD_OS2)
   int i, numprocs;
#endif

   starttime = -1;

   /* Open up kvm so that we can search the processes */
   if ((kd = kvm_open (NULL, NULL, NULL, O_RDONLY, "")) == NULL)
   {
      logfile ("Error: Cannot open /dev/kmem.");
      return -1;
   }

   (void) time(&tempus);

#if defined(BSD_OS2)
   /* This finds processes with a real user id of the person on console.
    * Should it use the effective user id instead?  I can't think of any
    * reason why it should, but.....
    */
#ifndef KINFO_PROC_RUID
#define KINFO_PROC_RUID 6
#endif
   PCB = kvm_getprocs(kd, KINFO_PROC_RUID, getuid(username), &numprocs);

   for (i = 0; i < numprocs; i++)

#else /* !BSD_OS2 */

   if (kvm_setproc (kd) == -1)
   {
      logfile ("Error:  kvm_setproc failed.");
      return -1;
   }
   while ((PCB = kvm_nextproc (kd)) != NULL)

#endif
   {
#if !defined(BSDI) && !defined (BSD_OS2)
      /* Grab the user structure */
      u = kvm_getu(kd, PCB);

      if (u == NULL)
      {
         debug(2,("kvm_getu in xlock_check returned NULL."));
         break;
      }

      if (strcmp(u->u_comm,XLOCK_NAME) == 0)
      {
         if (sameuser(username,PCB->p_uid))
         {
            starttime = tempus - u->u_start.tv_sec;
#elif defined (BSD_OS2)
      if (strcmp(PCB[i].p_comm,XLOCK_NAME) == 0)
      {
         if (TRUE) /* We know it is the correct user */
         {
            starttime = 1;
#else /* BSDI */
      if (strcmp(PCB->p_comm,XLOCK_NAME) == 0)
      {
         if (sameuser(username,PCB->p_cred->p_ruid))
         {
            starttime = 1;
#endif /* BSDI */
            debug(2,("Program %19s       started by %s at %d.", XLOCK_NAME, username,
                   (int)starttime));
            break;     /* Just find the first xlock program */
         }
         else
         {
            debug(3,("xlock running by different user."));
         }
      }
   }

   kvm_close (kd);      /* Close off the kvm stuff */

   return starttime;
#endif /* PROC_SEARCH_1 */
}

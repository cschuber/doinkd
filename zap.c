/*
**	Actually does the killing of terminal jobs 
*/

#include <sys/types.h>

#ifdef __hpux
#  include <sys/ptyio.h>
#  include <termios.h>          /* for ioctl kill VQUIT and termios struct */
#  define seteuid(x)  setresuid(-1,x,-1) /* No seteuid in HP-UX */
#endif /* __hpux */

#include <sys/file.h>
#include <pwd.h>                /* For struct passwd and getpwnam */
#include <errno.h>              /* For errno */
#include <sys/time.h>           /* For time() */
#include <sys/resource.h>       /* For rlimit stuff */
#include <fcntl.h>              /* For open() */

#ifndef UTMPPID
#  include <termios.h>          /* for ioctl kill VQUIT and termios struct */
#endif

#include "doinkd.h"

#ifdef SYSV
#  include <termios.h>
#endif /* SYSV */

#include <signal.h>             /* For "kill" and SIGKILL */

static	char	message[] = "\n\n\007Logged out by the system.\n";

extern	char	*ctime();

time_t		time();

void close_descriptors();
void pidkill();

#ifndef UTMPPID
#  ifdef PS_HACK
     void ps_hack_kill();
#  else /* !PS_HACK */
     int thangup();
#  endif
#endif

#ifndef DEBUG
#  define DEBUG 0
#endif

#ifndef DEBUG
#  define DEBUG 0
#endif
#if (DEBUG > 0)
#   define debug(level,string)  if (DEBUG > level) logfile string
#else
#   define debug(level,string)  /* Erase debug code, since DEBUG is off */
#endif

/**************************************************************************
 *	Disconnect the person logged on to tty "dev".                     *
 *      On SYSV systems, just slaughter the shell.                        *
 *      On BSD systems, force the line to hangup.                         *
 *      On some other systems, such as Ultrix, find the shell pid from    *
 *              the tty parent group and slaughter the shell that way.    *
 **************************************************************************/
void zap(him, type, do_msg)
   struct user *him;
   char *type;
   int   do_msg;
{
   int td;
   time_t  tempus;
   struct passwd *pw;

   /* close all the child's descriptors (from the fork() in warn.c) */

   close_descriptors();

#ifdef MAILMESSAGE
   if (!do_msg)
   {
      char cmd[256];

      sprintf(cmd,"%s -s 'Logged out by the system.' %s < %s",MAILPATH,
              him->uid, MAILMESSAGEFILE);
      system(cmd);
   }
#endif /* MAILMESSAGE */

   /* now tell him it's over, and disconnect */

   if (do_msg)
   {
      td = open (him->line, O_RDWR, 0600);
      (void) tcflow(td, TCOON);
   }

   tempus = time ((time_t *) NULL);

   /* Become the user for the killing and warning */
   pw = getpwnam(him->uid);
   if (pw == NULL)
      logfile("Error getting user information for %s in zap.  Will attempt kill\n\twithout uid changing.",
              him->uid);
   else
      seteuid(pw->pw_uid);

   if (td >= 0 && do_msg)
   {
#if (DEBUG < 1)
      (void) write (td, message, sizeof (message));
      (void) fsync (td);

#ifndef __linux__  /* discard, as libbsd does this */
      (void) ioctl (td, TCIOFLUSH, (char *) 0);
#endif /* __linux__ */

      (void) close (td);
#endif /* DEBUG < 1 */
   }
   else if (do_msg)
   {
      seteuid(0);
      logfile
	 (
	    "%19.19s : couldn't open %s for %s.",
	    ctime (&tempus),
	    him->line,
	    him->uid
	 );
   }

#if (DEBUG < 1)

#if defined(UTMPPID)
   pidkill(him->pid);

#elif defined(PS_HACK) /* !UTMPPID */

   ps_hack_kill(him);

#else /* !UTMPPID && !PS_HACK */

   td = open (him->line, O_RDWR, 0600);
   if (td < 0)
   {
      seteuid(0);
      logfile
         (
            "%19.19s : Open failed on tty %s. Errno = %d",
            ctime (&tempus),
            him->line,
            errno
         );
      /* Neither kill method from here can work, so quit */
      return;
   }


#ifndef BAD_PGRP
   if ((him->pid = tcgetpgrp(td)) != -1)
   {
      /* Make sure to kill all foreground processes, ending with the
       * shell process.
       */
      while ((him->pid = tcgetpgrp(td)) != -1)
         pidkill(him->pid);
   }
   else
#endif /* !BAD_PGRP */
   {
      if (thangup(td,him,tempus) < 0)
         return;        /* Failed */
   }

   (void) close(td);
#endif /* !UTMPPID && !PS_HACK */

   seteuid(0);
   logfile
      (
         "%19.19s : %s on %s because %s",
         ctime (&tempus),
         him->uid,
         him->line,
         type
      );

#else /* DEBUG < 1*/
   debug(1,("Just pretended to kill user %s on %s because of %s with pid %d.",
         him->uid, him->line, type, him->pid));
#endif /* DEBUG < 1*/

}

/**************************************************************************
 * Close all of the descriptors for this process, since we recently       *
 * forked it in warn.c.                                                   *
 **************************************************************************/
void close_descriptors()
{
   int     nfds, fd;
   struct rlimit rl;

#ifdef RLIMIT_NOFILE
   if (getrlimit (RLIMIT_NOFILE, &rl) < 0)
      logfile ("Error calling system function: getrlimit");
   nfds = rl.rlim_max;
#else /* RLIMIT_NOFILE */
   nfds = getdtablesize ();
#endif /* RLIMIT_NOFILE */

   /* Close all fds. */
#ifdef BSD_OS2
   for (fd = 2; fd < nfds; ++fd)   /* Don't close fd 0 or 1 on BSDI BSD/OS 2 */
                                   /* This works around some weird bug */
#else
   for (fd = 0; fd < nfds; ++fd)
#endif
   {
      (void) close (fd);
   }
}

/**************************************************************************
 * Kill a particular process.  This is used for UTMPPID systems,          *
 * and some other systems that have good tcgetgprp() functions that       *
 * work properly to find the controlling shell of the terminal.           *
 * Currently, this procedure simply runs through several kill signals,    *
 * starting with a kinder SIGHUP and ending in a SIGKILL.  Sometimes this *
 * should call the harsher ones only if necessary.                        *
 *                                                                        *
 * A problem here is that I have seen some shells, such as tcsh, which    *
 * hangup but do not kill their forground processes.  If the user is in   *
 * opening, then, their login shell disappears while they remain safe in  *
 * openwin (on Solaris 2.x).                                              *
 **************************************************************************/
void pidkill(pid)
   int pid;
{
#ifdef RUDEKILL
   /* Just slaughter */
   (void) kill ((pid_t) pid, SIGKILL);
#else
   /* Kill nicely */
   (void) kill ((pid_t) pid, SIGHUP);
   (void) kill ((pid_t) pid, SIGTERM);
   (void) kill ((pid_t) pid, SIGKILL);
#endif
}

/**************************************************************************
 * ps_hack_kill() uses a call to 'ps' to find the pid of the process on   *
 * the specified tty and kills those processes.                           *
 *   Original ps hack by Scott Gifford <swgsh@genesee.freenet.org>        *
 **************************************************************************/
#ifdef PS_HACK
void ps_hack_kill(him)
   struct user *him;
{
   FILE   *f;
   char    pscmd[50], psLine[100], *s;
   int     killMe;

   /* We don't have the pid that we need to kill, but let's fake it. */

   /* Instead of doing it the right way, fork off a ps.
    * Pull off the last two chars of the "/dev/tty??" string 
    * and pass them along to ps as the terminal.
    */
   /*sprintf (pscmd, "/bin/ps -t%s", &him->line[8]);*/
   /* last 6 for UW */
   sprintf (pscmd, "/bin/ps -t%s", &him->line[5]);
   f = popen (pscmd, "r");

   /* Pitch the first line (the headers line) */
   fgets (psLine, 100, f);

   /* Just to be safe, kill off everything on that terminal. */
   while (fgets (psLine, 100, f))
   {
      sscanf (psLine, "%d", &killMe);
      pidkill (killMe);
   }
   pclose (f);
}
#endif /* PS_HACK */

/**************************************************************************
 * thangup() is used on systems on which we can not find the pid of the   *
 * of the master shell.  Since we can't just call kill() on it, we will   *
 * try to get the current attributes for the terminal, and then change    *
 * them to set the baud rate to 0.  This *should* tell the shell to       *
 * hangup, and hence the user will be kicked off.  Some operating systems *
 * prefer not to pay attention to it.                                     *
 **************************************************************************/
#if !defined(UTMPPID) && !defined(PS_HACK)
int thangup(td, him, tempus)
   int td;
   struct user *him;
   time_t tempus;
{
   struct termios zap_ioctl;
   if (tcgetattr(td,&zap_ioctl) < 0)
   {
      logfile
         (
	    "%19.19s : zap failed on tty %s for %s, due to failure of tcgetattr().",
	    ctime (&tempus),
	    him->line,
	    him->uid
         );
      (void) close(td);
      return -1;
   }

   /* Set the speed of the terminal to B0 to tell it to hangup */
   cfsetospeed(&zap_ioctl,B0);

   if (tcsetattr(td,TCSANOW,&zap_ioctl) < 0)
   {
      logfile
         (
	    "%19.19s : zap failed on tty %s for %s.",
	    ctime (&tempus),
	    him->line,
	    him->uid
         );
      return -1;
   }
   return 0;
}
#endif /* !UTMPPID && !PS_HACK*/

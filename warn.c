/*
**	Warn users of impending logout, and zap them if they
**	have already been warned.
*/

#include <unistd.h>     /* For fork(), alarm(), and sleep() */
#include <setjmp.h>
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>   /* For WEXITSTATUS() define */
#include <string.h>     /* For strlen(), strcpy(), etc. */
#include <errno.h>

#ifdef __hpux
#  include <sys/ptyio.h>
#endif /* __hpux */

#include "doinkd.h"

#ifdef SYSV
#  include <fcntl.h>
#endif /* SYSV */

jmp_buf	env_buf;

extern	void	finish(),
		wakeup(),
		zap();

extern	char	*ctime();
time_t		time();

extern struct linetime *errorlines;

char *strtime();

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

#define SAY_MORE TRUE

/* Just for ease of consistency to pass the "Error in Warn" message from
 * child to parent with this exit code:
 */
#define OPENERROR 5

/**************************************************************************
 * Warn the specified user for the particular TYPE of problem:            *
 *      IS_IDLE         being idle                                        *
 *      IS_MULT         having multiple logins                            *
 *      IS_LIMIT        being logged in too long                          *
 *      IS_REFU         being logged in at all when they are not allowed  *
 *      IS_XLOCK        having xlock locking the console for too long     *
 *                                                                        *
 * If the user has not been warned, do so now, mark them as warned, and   *
 * return.  If they have been warned, then zap them now.                  *
 **************************************************************************/
void warn(i, type, do_msg)
   int i;
   int type;
   int do_msg;
{
   register struct user *him;
   int     opened = 0, status;
   FILE   *termf;
   time_t  tempus;

#if (DEBUG > 1) || defined(DISABLE_WARNS)
   do_msg = FALSE;          /* No warnings when debugging */
#endif

   if (type == IS_IDLE || type == IS_XLOCK)
      him = &users[i];
   else		/* type & ( IS_MULT | IS_LIMIT | IS_REFU | IS_CIDLE | IS_MAXU ) */
      him = pusers[i];

   /* Define BE_NICE to still give warn time, even when they won't be warned,
    * which currently only applies to the do_msg = false situation of a user
    * on console and not in X-Windows.
    */
#if !defined(BE_NICE)
   if (!do_msg)
      him->warned |= type;
#endif /* BE_NICE */


   switch (fork ())
   {
      case SYSERROR:
         logfile ("Cannot fork in warn: %s",strerror(errno));
         finish ();
         break;

      case 0:     /* Child */
         break;

      default:    /* Parent */
         if (type == IS_MAXU && !(him->warned & IS_MAXU))
            him->warned |= IS_MAXU;

         if (type == IS_MULT && !(him->warned & IS_MULT))
            him->warned |= IS_MULT;

         if (type == IS_IDLE && !(him->warned & IS_IDLE))
            him->warned |= IS_IDLE;

         if (type == IS_LIMIT && !(him->warned & IS_LIMIT))
            him->warned |= IS_LIMIT;

         wait (&status);  /* Parent waits for child to exit */
         if (WEXITSTATUS(status) == OPENERROR)
         {
            /* It died to to "Error in Warn--Cannot open", so we
             * need to add this user to the list as a bad line,
             * so that it doesn't get logged again.
             */
            addlinetime(him,&errorlines);
         }
         return;          /* Then returns back to doinkd.c */
   }

   /* Set up the signal catching for alarm() */
   (void) signal (SIGALRM, wakeup);

   /*
    *  Normal zero return means we just continue.
    *  1 is returned on timeout by SIGALRM, see wakeup
    *  free FILE without write
    */

   if (setjmp (env_buf) == 1)
   {
      if (opened)
      {
         /* Prevent the close from flushing unwritten data, since that is
          * probably the block that caused us to get here in the first
          * place.  This probably isn't very portable, but I can't find
          * any other method that works (barring changing all the output
          * here to use a simple file descriptor instead of a FILE *).
          * Linux is the only system I have seen that doesn't have the
          * _ptr and _base.  Let me know if there are others, or if you
          * have a system independant way to fix it!
          */
#if defined(__linux__)
         termf->_IO_read_ptr = termf->_IO_read_base;
         termf->_IO_write_ptr = termf->_IO_write_base;
#elif defined(BSDI) || defined(BSD_OS2)
         termf->_p = (termf->_ub)._base;
#elif defined(UNIXWARE)
	 termf->__ptr = termf->__base;
#else
	 termf->_ptr = termf->_base;
#endif
         (void) fclose (termf);
      }

      exit (0);
   }

   /* Turn on an alarm so if we get stuck writing to the terminal, then
    * it will wake us up after 5 seconds.
    */
   (void) alarm (5);

   if (do_msg)
   {
      /* Ensure that the line really exists before writing to it
       * (in case something is weird, as is too possible with XDM).
       */
      if ((termf = fopen (him->line, "r")) == (FILE *) NULL)
         do_msg = FALSE;        /* Doesn't exist, no warnings, no messages */
      else
         fclose(termf);         /* Just fine.  Go ahead and do messages */

      if ((termf = fopen (him->line, "w")) == (FILE *) NULL)
      {
         /* An error!  Check to see if we have already logged it. */
         if (inlinetime(him,&errorlines))
         {
            /* We did, so just quit. */
            exit (0);
         }
         else
         {
            /* Not yet logged.  Do so, and return an exit status to tell
             * the main parent to add it to the list.
             */
            tempus = time ((time_t *) NULL);
            logfile("%19.19s: Error in warn--Cannot open %s for %s.",ctime(&tempus), him->line, him->uid);
            exit (OPENERROR);
         }
      }
      opened = 1;
   }


   /* start the terminal if stopped */
   if (do_msg)
   {
      tcflow(fileno(termf), TCOON);
   }

   tempus = time ((time_t *) NULL);

   switch (type)
   {
   case IS_MAXU:
      if (him->warned & IS_MAXU)
      {
         zap (him, "maxuser", do_msg);
         break;
      }

      if (do_msg)
      {
         (void) fprintf
            (
               termf,
               "%s%19.19s%s%d%s%s%s%s%s%s",
               "\007\r\n\r\n\r\n\r\n",
               ctime (&tempus),
               "\r\nYou are only allowed ", him->maxuser," simultaneous logins\r\n",
               "with your current account.  Please wait for other users in\r\n",
               "your group to log-off and try again.\r\n",
               "\r\nThis session will end in ",
               strtime (sleeptime, !SAY_MORE), ".\r\n\r\n\007"
            );
      }

      break;

   case IS_MULT:
      if (him->warned & IS_MULT)
      {
         zap (him, "multiple", do_msg);
         break;
      }

      if (do_msg)
      {
         (void) fprintf
            (
               termf,
               "%s%19.19s%s%s%s %d%s %s%s",
               "\007\r\n\r\n\r\n\r\n",
               ctime (&tempus),
               "\r\nThis user id is logged on too many times.  Please end\r\n",
               "some logins to reduce your total number of logins to no\r\n",
               "more than ", him->next, ".  Please do so in the next",
               strtime (sleeptime, !SAY_MORE),
               "\r\nor you will be logged out by the system.\r\n\r\n\007"
            );
      }

      break;

   case IS_XLOCK:
      /* No warns for xlock! */
      zap (him, "xlock", do_msg);
      break;

   case IS_CIDLE:
      /* We have already determined guilt here, so just kill it */
      zap (him, "console-idle", do_msg);
      break;

   case IS_IDLE:
      if (him->warned & IS_IDLE)
      {
         zap (him, "idle", do_msg);
         break;
      }

      if (do_msg)
      {
         (void) fprintf
            (
               termf,
               "%s%19.19s%s %d %s %s %s",
               "\007\r\n\r\n\r\n\r\n",
               ctime (&tempus),
               "\r\nThis terminal has been idle",
               him->idle / 60,
               "minutes. If it remains idle\r\nfor",
               strtime (warntime, SAY_MORE),
               "it will be logged out by the system.\r\n\r\n\007"
            );
      }

      break;

   case IS_LIMIT:
      if (him->warned & IS_LIMIT)
      {
         zap (him, "session", do_msg);
         break;
      }

      if (do_msg)
      {
         (void) fprintf
            (
               termf,
               "%s%19.19s%s %d %s %s %s",
               "\007\r\n\r\n\r\n\r\n",
               ctime (&tempus),
               "\r\nThis terminal has been in use",
               him->session / 60,
               "minutes.\r\nIn",
               strtime (warntime, !SAY_MORE),
               "it will be logged out by the system.\r\n\r\n\007"
            );
      }

      break;

   case IS_REFU:
      if (do_msg)
      {
         (void) fprintf
            (
               termf,
               "%s%19.19s%s %s",
               "\007\r\n\r\n\r\n\r\n",
               ctime (&tempus),
               "\r\nYour terminal session is about to be",
               "terminated by the system.\r\n\r\n\007"
            );
      }

      /* Shut off the alarm so I can sleep as long as I want. */
      (void) alarm (0);
      (void) sleep ((unsigned) 5);

      /* Need that alarm back now.  (I'd hate to lock up!) */
      (void) alarm (5);
      zap (him, "refused", do_msg);
      break;
   }

   if (do_msg)
      (void) fclose (termf);

   opened = 0;
   (void) alarm (0);
   exit (0);			/* child exits here */
}


/**************************************************************************
 * signal handler for SIGALRM                                             *
 *    This will (hopefully) only get called if we got stuck somewhere,    *
 *    such as having the tty block on us when trying to write to it.      *
 *    Should that happen, this will get us out--we will give up trying    *
 *    to kill the user, but we will still be alive!                       *
 **************************************************************************/
void wakeup()
{
   (void)longjmp(env_buf, 1);
}

/**************************************************************************
 * This procedure returns a string with the number of minutes/seconds     *
 * in it.  If the number 34 is given, it will return "34 seconds".  If    *
 * 120 is given, it will return "2 minutes".                              *
 **************************************************************************/
char *strtime(seconds,domore)
   int seconds;
   int domore;
{
   static char buffer[32];
   char *morestr;

   if (domore)
      morestr = "more ";
   else
      morestr = "";

   if ((seconds % 60) == 0)
      (void) sprintf (buffer,"%d %s%s",seconds / 60,morestr,"minutes");
   else
      (void) sprintf (buffer,"%d %s%s",seconds,morestr,"seconds");
   
   return buffer;
}

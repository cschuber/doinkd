/*
 *	Main doinkd routine contols everything.
 */

#include <sys/types.h>

#include <signal.h>
#include <sys/file.h>

#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>           /* for time structs and time() */
#include <sys/resource.h>       /* For getrlimit() and RLIMIT_NOFILE */
#include <string.h>             /* For strlen(), strcpy(), etc */

#ifdef SYSV
#  include <termios.h>
#  include <fcntl.h>
#endif /* SYSV */

#include "doinkd.h"
#include "version.h"

/* SystemV type systems use utmp entry types.  If the utmp.h has
 * not defined this for us, do so now, assuming that those systems
 * consistently use 7 for an active user process.
 */
#if defined(SYSV) && !defined(USER_PROCESS)
#  define USER_PROCESS 7
#endif

/* If this system does not have utmpx, then the time should just
 * be in ut_time.
 */
#ifndef HAVE_UTMPX
#define ut_xtime ut_time
#endif

#ifndef DEBUG
#  define DEBUG 0
#endif

#if (DEBUG > 0)
#   define debug(level,string)  if (DEBUG > level) logfile string
#else
#   define debug(level,string)  /* Erase debug code, since DEBUG is off */
#endif

struct usertime
{
   char uid[NAMELEN + 1];       /* Who is being refused */
   time_t start;                /* The start of the refuse period */
   struct usertime *next;	/* Pointer to the next entry */
};

struct	user	users[MAXUSERS];
struct	user	*pusers[MAXUSERS];
struct	usertime *sess_users = NULL;
struct	linetime *errorlines = NULL;

struct	qelem	*exmpt;	 	/* lists for exemptions */
struct	qelem	*refuse;	/* lists for refusals */
struct	qelem	*rules;		/* lists for timeouts */
struct	qelem	*session;	/* lists for session limits */
/* LEC */
struct	qelem	*maxuser;	/* lists for max user */
/* LEC */
char    console_user[NAMELEN];  /* name of the person on console */

int     ioidle = FALSE; /* False to use input from user only as idle time */
                        /* True to use input & output as idle time */

#ifdef XDM_HACK
int     zapconsole = FALSE;     /* If true, kill all tty's owned by console_user */
#endif

int	m_threshold;	/* number of users before multiple limits */
int	s_threshold;	/* number of users before session limits */
int	session_default;/* The default session limit time */
int	sess_refuse_len;/* The refuse time after a session logout */
int	sleeptime;	/* max time to sleep between checks */
int	warntime;	/* time to allow for idle warning time */
int	conswins_idle;	/* time to allow for idle time for terminals opened
			 * by the user on console on this machine. */
int	conswins_sess;	/* max idle time for wins of console user */
int	conswins_mult;	/* max idle time for wins of console user */
int	mult_per_user;	/* number of multiple logins allowed per user */

void	chk_idle(),
	chk_multiple(),
	chk_maxuser(),
	chk_refuse(),
	add_session_refuse(),
	chk_session_refuse(),
	console_check(),
        console_zap (),
	finish(),
	core_time(),
	getgroups_func(),
	logfile();

int	comp(),
	chk_session(),
	num_uids();

extern	void	wakeup();

extern	void	freelist(),
		setlimits(),
		warn();

extern	char	*ctime();
extern	time_t	time();

extern  int     xlock_check();    /* From xlock_check.c */

/* Configuration Stuffs */
char    *config_file;
int     update_configuration();
time_t  conf_oldstamp;

void daemonize();

int main(argc,argv)
   int argc;
   char *argv[];
{
   int     userptr;
   int     utmptr;
   struct user *user;
   int     fl_idle = 1;
   int     fl_multiple = 1;
   /* LEC */
   int     fl_maxuser = 1;
   /* LEC */
   int     fl_refuse = 1;
   int     fl_session = 1;
   int     new;
   int     res;
   int     utmpfd;
   int     isConsole;
   char    pathbuf[20];
   char    tmpname[NAMELEN + 1];
   time_t  tempus;
   int     nextcheck;   /* Number of seconds to next check status */
   FILE   *logfd;
#ifdef HAVE_UTMPX
   struct utmpx utmpbuf;
#else
   struct utmp utmpbuf;
#endif
   struct stat statbuf;
   struct passwd *pswd;
   int     i;

   strcpy(console_user,"");	/* default to user "" on console */
   conswins_idle = -2;          /* default the actions to 'normal' logouts */
   conswins_sess = -2;          /* default the actions to 'normal' sessions */
   conswins_mult = -2;          /* default the actions to 'normal' multiple logins */
   session_default = -1;	/* default to no session limit */
   sess_refuse_len = 0;         /* No refuse time after a session logout */

   config_file = CONFIG;

   for (i = 1; i < argc; i++)
   {
      if (argv[i] == NULL)
         continue;
      if (argv[i][0] == '-' && argv[i][2] == '\0')
      {
         switch (argv[i][1])
         {
         case 'm':		/* no multiple login checks */
            fl_multiple = 0;
            break;

         case 'i':		/* no idle timeouts */
            fl_idle = 0;
            break;

         case 'r':		/* no refusals */
            fl_refuse = 0;
            break;

         case 's':		/* no session limits */
            fl_session = 0;
            break;

         case 'f':		/* Set the configuration file name */
            if (argv[i+1] != NULL)
            {
               config_file = (char *) malloc(strlen(argv[i+1])+1);
               strcpy(config_file,argv[i+1]);
               fprintf(stdout,"Using configuration file '%s'.\n",config_file);
               i++;
            }
            else
               fprintf(stderr,"doinkd: flag -f specified without naming config file\n");
            break;

         case 'V':              /* Print your version info! */
            fprintf(stdout,"%s\n",IDLED_VERSION);
            exit(0);

         default:
            (void) fprintf (stderr, "doinkd: bad flag -%c\n", argv[i][1]);
            break;
         }
      }
      else
         (void) fprintf (stderr, "doinkd: bad flag -%s\n", &argv[i][1]);
   }

   /* do nothing!! */

   if (!fl_idle && !fl_multiple && !fl_refuse && !fl_session)
   {
      fprintf(stderr,"doinkd: nothing to do.  exiting.\n");
      exit (0);
   }

   /* Trap signals! */
   (void) signal (SIGHUP, SIG_IGN);
   (void) signal (SIGINT, SIG_IGN);
   (void) signal (SIGQUIT, SIG_IGN);

   /* Trap some error ones */
   (void) signal (SIGILL, core_time);
   (void) signal (SIGBUS, core_time);
   (void) signal (SIGSEGV, core_time);

#if defined(SIGTTOU) && defined(SIGTSTP)
   (void) signal (SIGTTOU, SIG_IGN);
   (void) signal (SIGTSTP, SIG_IGN);
#endif	/* SIGS defined */

   (void) signal (SIGTERM, finish);
   (void) signal (SIGALRM, finish);

   /* a very old stamp for the configuration file */

   conf_oldstamp = 1;

   /* set up new header nodes for each of the lists */

   exmpt = (struct qelem *) malloc (sizeof (struct qelem));
   refuse = (struct qelem *) malloc (sizeof (struct qelem));
   rules = (struct qelem *) malloc (sizeof (struct qelem));
   session = (struct qelem *) malloc (sizeof (struct qelem));
   /* LEC */
   maxuser = (struct qelem *) malloc (sizeof (struct qelem));
   maxuser->q_forw = maxuser->q_back = maxuser;
   maxuser->q_item = NULL;
   /* LEC */

   exmpt->q_forw = exmpt->q_back = exmpt;
   refuse->q_forw = refuse->q_back = refuse;
   rules->q_forw = rules->q_back = rules;
   session->q_forw = session->q_back = session;
   exmpt->q_item = refuse->q_item = rules->q_item = session->q_item = NULL;

   if ((logfd = fopen (LOGFILE, "a")) != (FILE *) NULL)
   {
      tempus = time ((time_t *) NULL);
      (void) fprintf (logfd, "%24.24s  %s started.\n", ctime (&tempus),IDLED_VERSION);
      (void) fclose (logfd);
   }
   else
   {
      (void) fprintf (stderr, "doinkd: Cannot open log file: %s\n", LOGFILE);
      exit (1);
   }

   /* lose our controlling terminal */
   /* No more printing to stdout, or stderr allowed after this point! */
   daemonize();


   /* now sit in an infinite loop and work */

   for (;;)
   {
      new = update_configuration();

      (void) time (&tempus);

      if ((utmpfd = open (UTMP_FILE, O_RDONLY, 0)) == SYSERROR)
      {
	 logfile ("%19.19s:  Cannot open %s.",ctime(&tempus),UTMP_FILE);
	 exit (1);
      }

      /* Set our nextcheck time to the max (sleeptime), though it may
       * be lowered in the coming for loop so that an idle tty gets
       * only the alloted time to be idle before being warned.
       */
      nextcheck = tempus + sleeptime;

      /* If we aren't doing the hack to get XDM to work, then set this
       * to false so that we won't have to worry about it anywhere else.
       */
#ifndef XDM_HACK
      isConsole = FALSE;
#endif

      /* Set zapconsole to false, so if it is set to true in this
       * check, then the console user will be killed below.
       */
#ifdef XDM_HACK
      zapconsole = FALSE;
#endif

      /*
       * look through the utmp file, compare each entry to the users
       * array to see if an entry has changed.  If it has, build a new
       * record for that person, if it hasn't, see if it is time to
       * examine him again.
       */

#ifdef HAVE_UTMPX
      for (utmptr = 0, userptr = 0; (res = read (utmpfd, (char *) &utmpbuf, sizeof (struct utmpx))) > 0;)
#else
      for (utmptr = 0, userptr = 0; (res = read (utmpfd, (char *) &utmpbuf, sizeof (struct utmp))) > 0;)
#endif
      {
         if (utmptr >= MAXUSERS)
         {
            logfile ("Error:  too many users.  Recompile with higher 'MAXUSERS' setting.");
            break;
         }

#ifdef HAVE_UTMPX
	 if (res != sizeof (struct utmpx))
#else
	 if (res != sizeof (struct utmp))
#endif
	 {
	    logfile ("Error reading utmp file, continuing.");
            break;
	 }

	 (void) time (&tempus);

#ifdef XDM_HACK
         isConsole = FALSE;
         if (strcmp(utmpbuf.ut_line,XDM_DEV) == 0)
         {
            /* This is the console.  Is there a real name attached? */
            if (strlen(utmpbuf.ut_name) > 0)
               isConsole = TRUE;                /* Yes, use it */
            else
               strcpy(console_user,"");         /* No, clear the console user */
         }
#endif

#ifdef SYSV
         if (utmpbuf.ut_type == USER_PROCESS || isConsole)
#else /* SYSV */
	 if (utmpbuf.ut_name[0] != NULL || isConsole)
#endif /* SYSV */
	 {
	    user = &users[utmptr];
	    (void) strncpy (tmpname, utmpbuf.ut_name, NAMELEN);
	    tmpname[NAMELEN] = 0;

	    if (!strcmp (user->uid, tmpname) && user->time_on == utmpbuf.ut_xtime)
	    {
	       if (new)
		  setlimits (utmptr);

               /* If we are handling idle stuff, check to see if it is time
                * to see if this tty is idle, giving ourself five seconds of
                * leeway time.
                */
	       if (fl_idle && tempus > user->next-5)
		  chk_idle (utmptr);

               if (fl_idle && (user->next < nextcheck) && (user->next > tempus))
               {
                  debug(2,("Shortening this pause to %d seconds due to %s.",user->next-tempus,user->line));
                  nextcheck = user->next;
               }
	    }
	    else
	    {
               /* Don't include entries if the name is blank */
               if ((int)strlen(tmpname) <= 0)
                  continue;

#ifdef UTMPPID
               user->pid = utmpbuf.ut_pid;
#endif /* UTMPPID */
#ifdef UTMPHOST
               debug(4,("utmp host is %s",utmpbuf.ut_host));
               if ((int)strlen(utmpbuf.ut_host) <= 0)
                  (void) strncpy (user->host,"localhost",HOSTLEN);
               else
                  (void) strncpy (user->host,utmpbuf.ut_host,HOSTLEN);
               user->host[HOSTLEN] = '\0';      /* Make sure that it is null-terminated */
#endif /* UTMPHOST */
	       (void) strcpy (pathbuf, DEV);
	       (void) strcat (pathbuf, utmpbuf.ut_line);
	       (void) strcpy (user->line, pathbuf);
#ifdef XDM_HACK
               if (isConsole)
                  (void) strcpy(user->line,CONSOLE_NAME);
#endif

	       (void) stat (pathbuf, &statbuf);
	       (void) strcpy (user->uid, tmpname);
	       pswd = getpwnam (user->uid);
               if (pswd == NULL)
                  logfile ("Error:  could not get info on supposed user %s.",user->uid);
               else
                  getgroups_func (pswd->pw_name, user->groups, pswd->pw_gid);
	       user->time_on = utmpbuf.ut_xtime;
	       setlimits (utmptr);
	       user->next = tempus;
               chk_session_refuse(user);

               /* This is a new user, so remove this line from the errorlines
                * list if it is in there.
                */
               checklinetimeremove(user,&errorlines);

               /* If we are handling idle stuff, check to see if it has been
                * idle too long.
                */
               if (fl_idle)
                  chk_idle (utmptr);
 
               /* If this is the line for CONSOLE_NAME (ie. "/dev/console")
                * then set console_user to this person.
                */
               if (strcmp(CONSOLE_NAME,user->line) == 0)
               {
                  strcpy(console_user,user->uid);
               }

               /* Ensure that the sleep time is not greater than the smallest
                * allowed idle time
                */
               if (user->idle < sleeptime && user->idle > 0)
               {
                  sleeptime = user->idle;
                  if ((tempus + sleeptime) < nextcheck)
                     nextcheck = tempus+sleeptime;
                  debug(2,("Shortening sleeptime to %d seconds due to %s.",sleeptime,user->line));
               }
	    }

	    debug(1,
	       ("debug: %s %s %s %d %d %s %x %x",user->uid,user->line,user->host,
		  user->session,user->idle,
		  user->refuse == true ? "true" : "false",
		  user->exempt,user->warned));

	    pusers[userptr++] = user;
	 }
         else
         {
            /* This is not a current user tty! */

            /* If this line is the console, then make our console_user
             * string empty, since no one is on it!
             */
            if (strcmp(CONSOLE_NAME,utmpbuf.ut_line) == 0)
            {
               strcpy(console_user,"");
            }
         }

	 utmptr++;
      }
      debug(5,("Checked The entire UTMP file!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"));

      (void) close (utmpfd);

#ifdef XDM_HACK
      /* If the console user has been marked for killing, then do it. */
      if (zapconsole)
         console_zap (userptr);
#endif

      if (fl_refuse)
	 chk_refuse (userptr);

      if (fl_session)
      {
	 tempus = chk_session (userptr);
         if ((tempus > 0) && (tempus < nextcheck))
            nextcheck = tempus;
      }

      if (fl_multiple)
	 chk_multiple (userptr);

     /* LEC */
     if (fl_maxuser)
        chk_maxuser (user, userptr);
    /* LEC */

      (void) time (&tempus);
      (void) sleep ((unsigned) (nextcheck - tempus));
   }
}

/**************************************************************************
 * If the configuration file has changed, then read in the new settings.  *
 * If it is gone, then log that fact, and keep going.                     *
 * Returns:  0 == No changes.                                             *
 *           1 == Changes.  Set "new" in calling procedure to true.       *
 **************************************************************************/
int update_configuration()
{
   FILE   *conffd;
   struct stat statbuf;
   time_t  tempus;

   (void) time(&tempus);

   if (stat (config_file, &statbuf) == SYSERROR)
   {
      logfile ("%19.19s:  Cannot stat conf file.",ctime(&tempus));
      if (conf_oldstamp == 1)
      {
         logfile ("Have never read in conf settings.  Must die.");
         exit (1);
      }
      else
      {
         logfile ("Ignoring, and maintaining old settings.");
         return 0;
      }
   }

   if (statbuf.st_mtime > conf_oldstamp)
   {
      logfile ("%19.19s   Reading in configuration file.", ctime(&tempus));

      if ((conffd = freopen (config_file, "r", stdin)) == (FILE *) NULL)
      {
         logfile ("%19.19s:  Cannot open configuration file.", ctime(&tempus));
         if (conf_oldstamp == 1)
         {
            logfile ("Have never read in conf settings.  Must die.");
            exit (1);
         }
         else
         {
            logfile ("Ignoring, and maintaining old settings.");
            return 0;
         }
      }

      /* get rid of the old rules and exempt lists */

      freelist (exmpt);
      freelist (refuse);
      freelist (rules);
      freelist (session);
      /* LEC */
      freelist (maxuser);
      /* LEC */
      m_threshold = 0;
      s_threshold = 0;

      /* Default to proportional multiple logins allowed */
      mult_per_user = -1;

      /* default the actions to 'normal' logouts */
      conswins_idle = -2;
      conswins_sess = -2;
      conswins_mult = -2;

      /* default to no session limit, and no refuse time after
       * a session warn/logout.
       */
      session_default = -1;
      sess_refuse_len = 0;

      /* Default to input required from user to not be idle */
      ioidle = FALSE;

      /* now read the conf file and set up the rules */

      (void) yyparse ();
      (void) yyrestart (conffd);              /* jn */
      (void) fclose (conffd);

      /* Update the stamp time */
      conf_oldstamp = statbuf.st_mtime;

      return 1;
   }
   else
      return 0;
}

/**************************************************************************
 * Add this user to the list of session refusals.                         *
 **************************************************************************/
void add_session_refuse(user)
   struct user *user;
{
   struct usertime *tmpsu;
   time_t tempus;

   /* Don't bother to insert into the list if it will have no effect */
   if (sess_refuse_len <= 0)
      return;

   (void) time (&tempus);

   /* Check to see if the user is already in the list.  If the user
    * is in the list, then update the time (so it will be longer before
    * login is allowed again).
    */
   tmpsu = sess_users;
   while (tmpsu != NULL)
   {
      if (strcmp(tmpsu->uid,user->uid) == 0)
      {
         debug(3,("updated %s to session refuse, start time %d",user->uid,tempus));
	 tmpsu->start = tempus;
	 return;				/* Done it, so leave */
      }
      tmpsu = tmpsu->next;
   }

   /* The user was not in the list, so add a new entry to the start
    * of the list.
    */
   tmpsu = (struct usertime *) malloc(sizeof(struct usertime));
   strcpy(tmpsu->uid,user->uid);
   tmpsu->start = tempus;
   tmpsu->next = sess_users;
   sess_users = tmpsu;
   debug(3,("added %s to session refuse, start time %d",user->uid,tempus));
}

/* LEC */

void chk_maxuser(user, n_users)
   struct user *user;
   int n_users;
{
   register int who;
   int i,i2;
   int grpcnt = 0;
   int tgrpcnt = 0;
   
   if (n_users && user->mgroup != 0) {
     for (i = 0; i < n_users; i++) {
	   if ( (*pusers[i]).uid == user->uid)
          who = i;
       tgrpcnt = 0;
       for (i2 = 0; i2 < 1000; i2++) {
  	     if ( (*pusers[i]).groups[i2] == user->mgroup) {
            tgrpcnt = 1;
         }
       }
       grpcnt += tgrpcnt;
     }
	if (!((*pusers[who]).exempt & IS_MAXU) && (grpcnt > user->maxuser)) {
           debug(3,("expiring %s for %d in maxusers",user->uid,grpcnt));
           user->maxed = true;
           warn (who, IS_MAXU, DO_MSG);
     }
   }
}
/* LEC */

/**************************************************************************
 * check the state of refusals caused from a user overlapping session     *
 * limits.  Remove any expired refusals first, then see if a refusal      *
 * applies to this user.                                                  *
 **************************************************************************/
void chk_session_refuse(user)
   struct user *user;
{
   struct usertime **tmpsu, *delsu, *tsu;
   time_t tempus;

   (void) time (&tempus);

   /* Check for expired session limits */
   tmpsu = &sess_users;
   while ((*tmpsu) != NULL)
   {
      if ((*tmpsu)->start + sess_refuse_len < tempus)
      {
         debug(3,("expiring for %s in session refuse",user->uid));
         delsu = *tmpsu;                        /* store the address to free */
         *tmpsu = (*tmpsu)->next;
         free(delsu);
      }
      else
         tmpsu = &((*tmpsu)->next);
   }

   /* Check to see if this user is in the list */
   tsu = sess_users;
   while (tsu != NULL)
   {
      if (strcmp(tsu->uid,user->uid) == 0)
      {
         debug(3,("refusing for %s in session refuse",user->uid));
         user->refuse = true;
         return;
      }
      tsu = tsu->next;
   }
}

/**************************************************************************
 * check for refusal, tell user, then zap him                             *
 **************************************************************************/
void chk_refuse(n_users)
   int n_users;
{
   register int who;

   for (who = 0; who < n_users; who++)
      if (!(pusers[who]->exempt & IS_REFU) && pusers[who]->refuse == true)
	 warn (who, IS_REFU, DO_MSG);
}


/**************************************************************************
 * check session limits, warn users who have exceeded session limits      *
 **************************************************************************/
int chk_session(n_users)
   int n_users;
{
   int who;
   int timeon, mintime, nexttime, allowed_time, save;
   time_t  tempus;

   mintime = 0;         /* Default is to not change the nextcheck time */

   if (s_threshold == 0 || n_users < s_threshold)
      return 0;

   (void) time (&tempus);

   for (who = 0; who < n_users; who++)
   {
      /* Set default allowed time.  Might be different for the console user */
      allowed_time = pusers[who]->session;

      /* If a session time has not been set for this user, set it to the
       * default session limit, if any.
       */
      if (allowed_time <= 0)
         allowed_time = session_default * 60;

      /* If the user is not exempt from sessions and has a limit set */
      /* AND the user is over the session limit */
      if (!(pusers[who]->exempt & IS_LIMIT) && (allowed_time > 0))
      {
         debug(3,("Session: user=%s. time_on=%d. tty=%s.  sesstime=%d",(*pusers[who]).uid,(*pusers[who]).time_on,
                                                                    (*pusers[who]).line,((tempus - pusers[who]->time_on))));

         /* If console windows are immune from session (-1), then skip them.
          * If they are not normal (-2) then they have a set time, so use it.
          */
         if ((strcmp(console_user,pusers[who]->uid) == 0) && (conswins_sess == -1))
         {
            debug(3,("   not checking console user %s for session",console_user));
            continue;
         }
         else if ((strcmp(console_user,pusers[who]->uid) == 0) && (conswins_sess != -2))
         {
            allowed_time = conswins_sess;
         }

         /* If the user has not been on for their session time yet, skip it */
         if ((tempus - pusers[who]->time_on) < allowed_time)
         {
            pusers[who]->warned &= ~IS_LIMIT;
            continue;
         }

         /* * Then if the user has NOT been warned, warn now.
          * * If the user HAS been warned, see that a warning period of
          *   warntime seconds has been given before doin' the nuken
          *   Give 5 seconds of leeway for determining if it is nukin' time
          */
         if (!(pusers[who]->warned & IS_LIMIT) ||
            ((tempus - pusers[who]->time_on) >= (allowed_time + warntime - 5)))
         {
            /* Add this user to the session refuse list, if it is in use */
            add_session_refuse(pusers[who]);

            /* If there is no warn time, just kill the user */
            if (warntime <= 0)
               pusers[who]->warned |= IS_LIMIT;

            save = pusers[who]->session;          /* Save original session value */
            pusers[who]->session = allowed_time;  /* Set it to what we are using */
            warn (who, IS_LIMIT, DO_MSG);         /* Warn or Zap, using right time */
            pusers[who]->session = save;          /* Restore original value */
         }

         /* Set the nextcheck time so that it is not longer than what we
          * need to kill the user on schedule.
          */
         timeon = tempus - pusers[who]->time_on;  /* How long this user
                                                   * has been on */
         nexttime = warntime - (timeon - allowed_time);
                                                  /* When to next check
                                                   * this tty */
         if (nexttime < 10)
            nexttime = 10;              /* Always allow 10 seconds */
         debug(2,("set mintime in chk_session() to be %d.",nexttime));
         if ((nexttime < mintime) || (mintime == 0))
            mintime = nexttime;
      }
   }
   if (mintime > 0)
      return (mintime + tempus);
   else
      return 0;
}


/**************************************************************************
 * given the number of users (tty's in use), warn any of them who have    *
 * multiple logins                                                        *
 **************************************************************************/
void chk_multiple(n_users)
   int n_users;
{
   int i, numper, save, numallowed;
   int     curuser;

   /* If we will not do multiple login checks, then set the IS_MULT
    * warned bit for all sessions to false.  This prevents problems
    * of warning a tty, then users log out so the threshold is not
    * met, then someone logging in to meet the threshold and the
    * previously warned tty will be killed immediately without
    * warning, even though much time may have elapsed.
    */
   if (m_threshold == 0 || n_users < m_threshold)
   {
      for (i = 0; i < n_users; i++)
	 (*pusers[i]).warned &= ~IS_MULT;

      return;
   }

   qsort ((char *) pusers, n_users, sizeof (struct user *), comp);

   /* Get the number of multiple logins allowed per user */
   if (mult_per_user < 0)
      numper = m_threshold / num_uids(n_users);
   else
      numper = mult_per_user;

   /* Ensure that users will get to keep at least 1 tty */
   if (numper <= 0)
      numper = 1;

   curuser = -1;
   for (i = 0; i < n_users; i++)
   {
      debug(3,("Multiple: user=%s. time_on=%d. tty=%s.",(*pusers[i]).uid,(*pusers[i]).time_on,(*pusers[i]).line));

      /* If this user is on console and the console user is immune
       * from multiple logins (conswins multiple off), then skip
       * the user.
       */
      if ((strcmp(console_user,(*pusers[i]).uid) == 0) && (conswins_mult == -1))
      {
	 debug(3,("   not checking console user %s for multiple",console_user));
	 continue;
      }
      /* If this user has not been checked yet, which we
       * know if curuser < 0 or if the curuser uid is not
       * the same as the i uid, then we consider this tty
       * to be their first.  If it is the same user, then
       * we check to see if this is a tty that should be
       * warned and killed.
       */
      if (curuser >= 0)
      {
         if (strcmp((*pusers[i]).uid, (*pusers[curuser]).uid) != 0)
            curuser = i;
      }
      else
         curuser = i;

      /* Set the number allowed to the default numper */
      numallowed = numper;

      /* If this is the console user and there is a separate multiple
       * specification for the console user, then set numallowed
       * to that value, so we can compare against it, instead.
       */
      if ((strcmp(console_user,(*pusers[i]).uid) == 0) && (conswins_mult >= 1))
      {
         numallowed = conswins_mult;
      }

      /* If we have not already skipped 'numper' ttys, then let
       * this one go as one they can keep.
       */
      if (i-curuser < numallowed)
      {
         (*pusers[i]).warned &= ~IS_MULT;	/* Unset warn bit, if set */
         continue;				/* Let this tty live */
      }

      /* Too many logins, but let the user go if exempt */
      if ((*pusers[i]).exempt & IS_MULT)
	 continue;

      debug(3,("   Warning/Zapping this tty."));

      /* Warn/Zap the offending tty */
      save = (*pusers[i]).next;          /* Save the next value we are going to use */
      (*pusers[i]).next = numallowed;    /* Set it to what we want to print in warn */
      warn (i, IS_MULT, DO_MSG);         /* Warn/Zap, saying correct # of logins */
      (*pusers[i]).next = save;          /* Restore original next value */
   }
}

/**************************************************************************
 * Return the number of different uids currently logged on.               *
 * This should only be called after pusers[] has been sorted by uid.      *
 **************************************************************************/
int num_uids(n_users)
   int n_users;
{
   int i, curuser, numusers;
   
   numusers = 0;
   curuser = -1;
   for (i = 0; i < n_users; i++)
   {
      if (curuser >= 0)
      {
         if (strcmp((*pusers[i]).uid, (*pusers[curuser]).uid) != 0)
         {
            curuser = i;
            numusers++;
         }
      }
      else
      {
         curuser = i;
         numusers++;
      }
   }
   debug(4,("num_uids is %d",numusers));
   return numusers;
}

/************************************************************************** 
 *	Given a user, see if we want to warn him about idle time.         *
 *	First check the exempt vector to see if he is exempt.             *
 *	As a side effect this routine also calculates the next time       *
 *	this user structure should be examined.                           *
 **************************************************************************/
void chk_idle(i)
   int i;
{
   struct stat statbuf;
   int     touch_time, allowed_time, save;
   time_t  tempus;

   /* If there is no idle time set for the user, than the user
    * can't be idle.
    */
   if (users[i].idle < 0)
      return;
   
   if (strcmp(users[i].line,CONSOLE_NAME) == 0)
   {
      console_check(i);
      return;
   }

   (void) time (&tempus);
   (void) stat (users[i].line, &statbuf);

   /* If we are considering output, only do so BEFORE warning.  If
    * we have warned, then we have messed up the modified time, so
    * we shouldn't look at it.
    */
   if (ioidle && (users[i].warned & IS_IDLE) == 0)
      touch_time = max (statbuf.st_atime, statbuf.st_mtime);
   else /* input_idle only */
      touch_time = statbuf.st_atime;

   /* Default allowed time for the user is whatever it is.  :-) */
   allowed_time = users[i].idle;

   /* If this terminal is one owned by the user on console, then
    * allow them "conswins_idle" instead (be it greater or smaller).
    * If conswins_idle == -1, then it is 'off', so don't check this tty.
    * If it is -2, "normal", then ignore the setting.  Otherwise, set it.
    */
   if ((strcmp(console_user,users[i].uid) == 0) && (conswins_idle == -1))
   {
      debug(3,("   not checking console user %s for idle",console_user));
      return;
   }
   else if ((strcmp(console_user,users[i].uid) == 0) && (conswins_idle != -2))
   {
      allowed_time = conswins_idle;
   }

   /* 5 seconds leeway here, captain!!! */
   /* If the user has not been logged on long enough to be idle,
    * or the user has been idle for less than the amount of allowed
    * time, give him the clean bit and set the next time appropriately
    */
   if ((tempus - users[i].time_on < allowed_time-5)
      || (tempus - touch_time < allowed_time-5))
   {
      users[i].warned &= ~IS_IDLE;

      if (users[i].session > 0)
         users[i].next = min (touch_time + allowed_time,
                              users[i].time_on + users[i].session);
      else
         users[i].next = touch_time + allowed_time;
   }
   else if (!(users[i].exempt & IS_IDLE))
   {
      if (users[i].warned & IS_IDLE)
      {
         /* If the user has already been warned, then they should
          * get zapped this time.  If the zap doesn't work, however,
          * let's not shorten the sleep time to have it try again.
          * Leaving the next time to check as less than the current
          * time means that doinkd will attempt to zap the person
          * again whenever it next wakes up (at sleeptime or less).
          */
         users[i].next = touch_time + allowed_time;
      }
      else
      {
         /* The user hasn't been warned yet, so allow 'warntime'
          * seconds to do something to become un-idle
          */
         users[i].next = tempus+warntime;

         /* If no warn time is allowed, mark them as warned and
          * nuke them now!
          */
         if (warntime <= 0)
            users[i].warned |= IS_IDLE;
      }

      save = users[i].idle;           /* Save original idle value */
      users[i].idle = allowed_time;   /* Set it to what we are REALLY using */
      warn (i, IS_IDLE, DO_MSG);      /* Warn/Zap, saying correct idle time */
      users[i].idle = save;           /* Restore original value */
   }

}

/**************************************************************************
 * Check to see how long the console has been idle.  "console" is defined *
 * by CONSOLE_NAME in doinkd.h, and defaults to "/dev/console".  Idle time *
 * is determined by last access time of the mouse, keyboard, and tty.     *
 * The console tty itself must still be checked since the keyboard and    *
 * mouse devices do not get updated when not in X, and a user who has     *
 * been in X and exited will be unjustly zapped for not using the mouse   *
 * or keyboard.                                                           *
 **************************************************************************/
time_t idle_time(allowed_console_idle, i, in_X)
   int allowed_console_idle;
   int i;
   int *in_X;
{

   struct stat sb;
   time_t  min_time;
   time_t  curr_time;
   time_t  touch_time;

   /* Assume user is in X.  Change later if this is not true. */
   *in_X = TRUE;

   (void) time(&curr_time);

   if (stat (KEYBOARD_NAME, &sb) == 0)
      touch_time = max (sb.st_atime, sb.st_mtime);
   else
      touch_time = 0;                   /* If it failed, then try mouse */

   min_time = curr_time - touch_time;

   if (min_time >= allowed_console_idle)
   {
      if (stat (MOUSE_NAME, &sb) == 0)
         touch_time = max (sb.st_atime, sb.st_mtime);
      else
         touch_time = 0;                /* If it failed, then never touched! */

      min_time = min (min_time, (unsigned) (curr_time - touch_time));
   }

   /* Check ONLY by the access time for console unless ioidle = TRUE,
    * since any message to CONSOLE_NAME (ie. echo "hello" > /dev/console)
    * will update the modified time.
    */
   stat (CONSOLE_NAME, &sb);

   /* If we are considering output, only do so BEFORE warning.  If
    * we have warned, then we have messed up the modified time, so
    * we shouldn't look at it.
    */
   if (ioidle && (users[i].warned & IS_IDLE) == 0)
      touch_time = max (sb.st_atime, sb.st_mtime);
   else /* input_idle only */
      touch_time = sb.st_atime;

   min_time = min (min_time, (unsigned) (curr_time - touch_time));

   /* No real way to detect in_X or not for sure. */
/*   *in_X = FALSE;*/

#ifdef OLD_STUFF
   if (user_not_in_X)
   {
      /* This user is not in X */
      debug(4,("Not in X!"));

      /* Set the min_time to the idle time from the last console access,
       * which was grabbed just before this if.
       */
      min_time = curr_time - touch_time;

      *in_X = FALSE;
   }
#endif /* OLD_STUFF */

   return min_time;
}

/**************************************************************************
 * Handle the special situation of looking after the console.  This means *
 * finding the idle time considering the mouse and keyboard using         *
 * idle_time(), checking for an xlock and finding its run time, and then  *
 * doing the actual warning and zapping.                                  *
 **************************************************************************/
void console_check(i)
   int i;
{
#define XLOCK_TIME users[i].idle-5
   time_t tempus;
   time_t idle, xlock;
   int    in_X;

   /* If the user is exempt from idleness, don't bother to check how
    * long the user has been idle.
    */
   if (users[i].exempt & IS_IDLE)
      return;

   (void) time(&tempus);

   idle = idle_time(users[i].idle,i,&in_X);
   xlock = xlock_check(users[i].uid);

   if (xlock > 0)
   {
      debug(2,("%s is running an xlock.  Has been running for %d seconds.",
             users[i].uid,xlock));
   }

   /* 5 seconds leeway (in case we are a couple of seconds too early) */
   if ((idle >= users[i].idle-5) || (xlock >= XLOCK_TIME))
   {
      if (xlock >= XLOCK_TIME)
      {
         users[i].warned &= IS_XLOCK;
         warn (i, IS_XLOCK, DO_MSG);
#ifdef XDM_HACK
         zapconsole = TRUE;
#endif
      }
      else
      {
         /* The user is idle, not an XLOCK situation */

         if (users[i].warned & IS_IDLE)
         {
            /* If the user has already been warned, then they should
             * get zapped this time.  If the zap doesn't work, however,
             * let's not shorten the sleep time to have it try again.
             * Leaving the next time to check as less than the current
             * time means that doinkd will attempt to zap the person
             * again whenever it next wakes up (at sleeptime or less).
             */
            users[i].next = max(idle,xlock) + users[i].idle;
         }
         else
         {
            /* The user on console hasn't been warned yet, so allow
             * 'warntime' seconds to do something to become un-idle
             */
            users[i].next = tempus+warntime;

            /* If no warn time is allowed, mark them as warned and
            * nuke them now!
            */
            if (warntime <= 0)
               users[i].warned |= IS_IDLE;
         }

         if (in_X)
         {
#ifdef XDM_HACK
            if (users[i].warned & IS_IDLE)
               zapconsole = TRUE;
#endif

            debug(2,("Console user has been warned."));
            warn (i, IS_IDLE, DO_MSG);
         }
         else
         {
#ifdef XDM_HACK
            if (users[i].warned & IS_IDLE)
               zapconsole = TRUE;
#endif

            warn (i, IS_IDLE, !DO_MSG);  /* Don't warn if not in X */
         }
      }
   }
   else /* not over idle time, so see when we need to check next */
   {
      users[i].warned &= ~IS_IDLE;

      if (users[i].session > 0)
         users[i].next = min (tempus-max(idle,xlock) + users[i].idle,
                              users[i].time_on + users[i].session);
      else
         users[i].next = tempus-max(idle,xlock) + users[i].idle;
   }
}

#ifdef XDM_HACK
/**************************************************************************
 * Kill the console user, as they have been idle.  This is a special hack *
 * to get kills to work with XDM by killing every tty the user has, so    *
 * that their XDM controlling tty will also be killed, and they will be   *
 * logged off.                                                            *
 **************************************************************************/
void console_zap (n_users)
   int n_users;
{
   int who;

   zapconsole = FALSE;

   for (who = 0; who < n_users; who++)
   {
      if (strcmp(pusers[who]->uid,console_user) == 0)
      {
         pusers[who]->warned = TRUE;
	 warn (who, IS_CIDLE, FALSE);
      }
   }
}
#endif /* XDM_HACK */

/**************************************************************************
 * get all the groups a user belongs to                                   *
 **************************************************************************/
void getgroups_func(pw_name, groups, pw_group)
   char	*pw_name;
   int	groups[];
   int	pw_group;
{
   register int i;
   register int ngroups = 0;
   register struct group *grp;

   if (pw_group >= 0)
      groups[ngroups++] = pw_group;

   setgrent ();

   while (grp = getgrent ())
   {
      if (grp->gr_gid == pw_group)
	 continue;

      for (i = 0; grp->gr_mem[i]; i++)
	 if (!strcmp (grp->gr_mem[i], pw_name))
	 {
	    groups[ngroups++] = grp->gr_gid;
	    break;
	 }

      if (ngroups == NGROUPS)
	 break;
   }

   if (ngroups < NGROUPS)
      groups[ngroups] = -1;

   endgrent ();
}


/**************************************************************************
 * comp -- used by qsort to sort by id                                    *
 *         if the id is the same, the time on is used.                    *
 *      This function is used in the chk_multiple() procedure to sort the *
 *      logins.  Hence, the sorting order here, which must be by uid      *
 *      first, determines the manner of which tty's to warn.              *
 *                                                                        *
 *      I would like to do something so idle time could be used, also.    *
 **************************************************************************/
int comp(h1, h2)
   struct user **h1;
   struct user **h2;
{
   int v;

   v = strcmp ((**h1).uid, (**h2).uid);

   if (v == 0)
      return ((**h1).time_on - (**h2).time_on);
   else
      return v;
}


/* process a log message */

/* VARARGS */
void logfile(s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
   char *s;
   char *a1;
   char *a2;
   char *a3;
   char *a4;
   char *a5;
   char *a6;
   char *a7;
   char *a8;
   char *a9;
   char *a10;
{
   FILE *logfd;

   if ((logfd = fopen (LOGFILE, "a")) == (FILE *) NULL)
      return;

   (void) fprintf (logfd, s, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
   (void) putc ('\n', logfd);
   (void) fclose (logfd);
}


/**************************************************************************
 * If doinkd was sent a SIGTERM or SIGALRM, then clean up house, report    *
 * the death, and exit gracefully.                                        *
 **************************************************************************/
void
finish()
{
   time_t  tempus;

   (void) signal (SIGTERM, SIG_IGN);
   tempus = time ((time_t *) NULL);
   logfile ("%24.24s  doinkd killed.", ctime (&tempus));
   exit (1);
}

/**************************************************************************
 * prepare to exit due to an error signal.  What we want to do is         *
 * leave a message about the death and what error signal caused it,       *
 * and then continue on so that the OS will create a core dump at         *
 * the proper locale for us.                                              *
 **************************************************************************/
void core_time(signal_num)
   int signal_num;
{
   time_t  tempus;

   (void) signal (SIGTERM, SIG_IGN);
   tempus = time ((time_t *) NULL);
   logfile ("%24.24s  doinkd died due to error signal %d.", ctime (&tempus), signal_num);
   signal(SIGILL, SIG_DFL);
   signal(SIGBUS, SIG_DFL);
   signal(SIGSEGV, SIG_DFL);
}

/**************************************************************************
 * Do the work of daemonizing doinkd by forking the child that will go on  *
 * to do all the work and letting the parent exit.  The child also needs  *
 * to close all of its files (stdout, stderr, etc.) and then tries to     *
 * get its own session id to completely detach it from the calling tty.   *
 **************************************************************************/
void daemonize()
{
   int     nfds, fd;
   struct rlimit rl;

   switch (fork ())
    {
    case SYSERROR:
       logfile ("Cannot fork.");
       (void) fprintf (stderr, "doinkd: Cannot fork\n");
       exit (1);

    case 0:
       break;           /* Child process continues. */

    default:
       exit (0);        /* Parent exits here. */
    }

#ifdef RLIMIT_NOFILE
   if (getrlimit (RLIMIT_NOFILE, &rl) < 0)
      logfile ("Error calling system function: getrlimit");
   nfds = rl.rlim_max;
#else /* RLIMIT_NOFILE */
   nfds = getdtablesize ();
#endif /* RLIMIT_NOFILE */

   /* Close all fds. */
#ifdef BSD_OS2
   for (fd = 1; fd < nfds; ++fd)        /* Don't close fd 0 on BSDI BSD/OS 2 */
                                        /* This works around some weird bug */
#else
   for (fd = 0; fd < nfds; ++fd)
#endif
   {
      (void) close (fd);
   }

#if HAVE_SETSID
   (void) setsid ();
#endif
}

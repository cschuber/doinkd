#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <utmp.h>

#define qelem qelem_sys	/* Work around to use our own qelem below */
#include <stdlib.h>
#undef qelem

#ifdef HAVE_UTMPX
#include <utmpx.h>
#endif

#define	SYSERROR	(-1)

#define min(a,b)	( (a)<(b)?(a):(b) )
#define max(a,b)	( (a)>(b)?(a):(b) )

#ifndef TRUE
#  define TRUE (1==1)
#endif

#ifndef FALSE
#  define FALSE !TRUE
#endif

/* Used to specify whether warn/zaps should talk to the user */
/* This should remain TRUE -- the different name is for clarity */
#define DO_MSG  TRUE

#ifndef UTMP_FILE
#  ifdef _PATH_UTMP       /* BSDI BSD/OS2 and FreeBSD use this define */
#    define UTMP_FILE _PATH_UTMP
#  else
#    define UTMP_FILE	"/etc/utmp"	/* name of utmp file */
#  endif
#endif

/* If we are using UTMPX and its define exists, set it now.
 * If your system has a different file for the extended utmp
 * format, but does not use the define UTMPX_FILE, then you
 * will have to define UTMP_FILE below to point to the
 * appropriate file on your system.  Please inform me of any
 * necessary changes.
 */
#if defined(HAVE_UTMPX) && defined(UTMPX_FILE)
#   undef UTMP_FILE
#   define UTMP_FILE UTMPX_FILE
#endif

/*********************** MAIL MESSAGE PATHS *************************/
#define MAILPATH        "/bin/mail"
#define MAILMESSAGEFILE "/usr/local/lib/logout.msg"


#define DEV		"/dev/"

/* Name of the console device */
#define CONSOLE_NAME    "/dev/console"

/* Name of the XDM console tty, if XDM is used */
#define XDM_DEV         ":0"

/* Keyboard and Mouse Devices to Check in Place of Console */
#define KEYBOARD_NAME   "/dev/kbd"
#define MOUSE_NAME      "/dev/mouse"
/* On an IBM compatible, the mouse may be on a serial port,
 * in which case the mouse device will be ttya or ttyb.
 * Should this be the case, be sure to change the MOUSE_NAME
 * appropriately.  This includes Solaris x86 (v2.4), at least.
 * Solaris x86 info:
 *   Mouse Mode     MOUSE_NAME
 *   ==========     ==========
 *   PS/2 style     "/dev/kdmouse"
 *   Logitech Bus   "/dev/logi"
 *   Microsft Bus   "/dev/msm"
 *   Serial         "/dev/ttya"  (or ttyb, for port 2)
 * Note that the keyboard also goes through the same device,
 * so the KEYBOARD_NAME definition is irrelevant (it will be
 * ignored if it is invalid, just be sure to leave it invalid).
 */

/* Name of the X console locking program */
#define XLOCK_NAME      "xlock"

#ifndef AIX
#  if !defined(NGROUPS) && defined(NGROUPS_MAX_DEFAULT)
#    define NGROUPS NGROUPS_MAX_DEFAULT
#  else
#    if !defined(NGROUPS)
#      define NGROUPS 16
#    endif
#  endif
#endif /* AIX */

/* MAXUSERS in general represents approximately the maximum number of
 * users logged on at any one moment.  In reality, it represents the
 * maximum number of entries in the utmp file.  Due to the handling
 * of the utmp file on some systems, the number of users on at a time
 * may be much smaller than the number of lines in the utmp file.
 * Doing a "make utmplines" will compile a run a small test program
 * to print out the current number of lines in your utmp file.
 * MAXUSERS should be greater than this number.
 */
#define MAXUSERS	500

#ifdef UT_NAMESIZE
#  define NAMELEN UT_NAMESIZE   /* max username len = define in utmp.h */
#else
#  define NAMELEN 18		/* max length of login name */
#endif

/* The maximum length of a host name.  This may be smaller than the
 * host variable on your system, as specified in /usr/include/utmp.h or
 * utmpx.h, but it is safer to have it be shorter so that doinkd does
 * not try to copy data from the utmp file that doesn't exist.
 * Unfortunately not all systems have defines that could be used from
 * the system files.
 */
#ifdef UT_HOSTSIZE
#   define HOSTLEN         UT_HOSTSIZE
#else
#   define HOSTLEN         16
#endif

#define IS_IDLE		(1 << 0)
#define IS_MULT		(1 << 1)
#define IS_LIMIT	(1 << 2)
#define	IS_REFU		(1 << 3)
#define IS_XLOCK	(1 << 4)
#define IS_CIDLE	(1 << 5)
#define	IS_MAXU		(1 << 6)

typedef	enum	{ false = 0, true = 1 }	bool;

struct user
{
	int	idle;		  /* max idle limit for this user */
	int	groups[NGROUPS];  /* gids from passwd and group files */
	bool	refuse;		  /* true is user should be refused access */
	int	session;	  /* session limit for this user */
	int	warned;		  /* if he has been warned before */
	int	exempt;		  /* what is this guy exempt from? */
    /* LEC */
    int maxed;  /* max users met, log out */
    int mgroup; /* selected group to max against */
    int maxuser; /* number of max users */
    /* LEC */
	time_t	next;		  /* next time to examine this structure */
	time_t	time_on;	  /* loggin time */
	char	uid[NAMELEN + 1]; /* who is this is? */
	char	line[14];	  /* his tty in the form "/dev/ttyxx" or "/dev/pts/X" */
	char	host[HOSTLEN + 1];/* the host this user is connecting from */
        int     pid;              /* the process id of this login */
};

/* 
** next will be cur_time+limit-idle do all we have to do is check
** the current time against the action field when the daemon comes
** around.  if >= then it's time to check the idle time again, else
** just skip him.
*/

extern	struct	user	users[];
extern	struct	user	*pusers[];

/* records that the nodes of the linked list will have pointers too */

struct item
{
	int	name_t;		/* is it a login, group, etc... */
	char	*name;		/* which login, etc */
	int	num;		/* group */
	int	flag;		/* what is the timeout/exemption ? */
};

/* necessary structures to use the system linked list stuff */

struct qelem
{
	struct	qelem	*q_forw;
	struct	qelem	*q_back;
	struct	item	*q_item;
};

/* These items are gleaned from the configuration file...	*/

extern	struct	qelem	*rules; 	/* list of idle timeout rules	*/
extern	struct	qelem	*exmpt; 	/* list of exemptions		*/
extern	struct	qelem	*refuse;	/* list of refuse rules		*/
extern	struct	qelem	*session;	/* list of session limit rules  */
/* LEC */
extern	struct	qelem	*maxuser;	/* list of session limit rules  */
/* LEC */

extern	int	sleeptime;	/* max time to sleep between scans of utmp */
extern	int	warntime;	/* how long to allow for warnings */
extern	int	conswins_idle;	/* max idle time for wins of console user */
extern	int	conswins_sess;	/* max idle time for wins of console user */
extern	int	conswins_mult;	/* max idle time for wins of console user */
extern	int	m_threshold;	/* multiple login warning threshold */
extern	int	s_threshold;	/* session limit warning threshold */
extern	int	session_default;/* The default session limit time */
extern	int	sess_refuse_len;/* The refuse time after a session logout */
extern	int	warn_flags;	/* what warnings to make */
extern	int	mult_per_user;  /* number of multiple logins allowed per user */
extern	int	ioidle;         /* True=use i&o as idle time ; False=i only */

extern	void	logfile();

/* This is used for trapping errors caused by the utmp file getting
 * out of touch with reality and telling doinkd that a user is logged
 * in still when the user has actually been logged out.  Use of this
 * structure in a linked list allows doinkd to check and only print
 * the error once.
 */
struct linetime
{
   char		line[14];	/* his tty in the form "/dev/ttyxx" or "/dev/pts/X" */
   time_t	time_on;	/* loggin time */
   struct linetime *next;	/* The next entry, if any */
};

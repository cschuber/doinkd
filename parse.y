%{
#include <stdio.h> 
#include <grp.h>
#include "doinkd.h"
#include <pwd.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define debug if (DEBUG > 0) logfile

int		num;
char		*name;
struct	group	*grp;

extern	char	*yytext;

extern	char	*config_file;	/* The name of the config file, from doinkd.c */
extern  char    *strchr();
extern	void	addlist();

  /************************************************************************
   *  The order of the tokens in the *first line* is significant.         *
   *                                                                      *
   *  They dictate which rules and exemptions have precedence.            *
   *  Hence, TTY has precedence over HOST has precedence over LOGIN, etc. *
   *                                                                      *
   *  The second two %token lines may be ordered anyway.                  *
   *  DEFAULT is the least specific, but will always match.               *
   *  It must always remain in the last position.                         *
   ************************************************************************/
%}

%token TTY HOST LOGIN GROUP FILECOM DEFAULT

%token EXEMPT TIMEOUT SLEEP WARN IDLEMETHOD CONSWINS SESSION REFUSE MULTIPLES MAXUSER
%token NUM IDLE MULTIPLE NAME ALL
%token THRESHOLD NL
%token USERINPUT INPUTOUTPUT
%token NORMAL OFF

%union {
	char *sb; 
	int nb;
       }

%type <sb> NAME
%type <nb> NUM LOGIN GROUP TTY ALL IDLE MULTIPLE 
%type <nb> who exempt_type name_type

%start cmd_cmd

%%

cmd_cmd		: /*EMPTY*/
		| cmd_cmd exempt_cmd
		| cmd_cmd idle_cmd
		| cmd_cmd refuse_cmd
		| cmd_cmd sleep_cmd
		| cmd_cmd warn_cmd
                | cmd_cmd idlemethod_cmd
		| cmd_cmd conswins_cmd
		| cmd_cmd session_cmd
		| cmd_cmd thresh_cmd
		| cmd_cmd mult_cmd
		| cmd_cmd maxuser_cmd
		| cmd_cmd error NL
		| cmd_cmd NL
		;

thresh_cmd	: THRESHOLD MULTIPLE NUM NL
		{
			m_threshold = $3; 
		}
		| THRESHOLD SESSION NUM NL
		{
			s_threshold = $3; 
		}
		| THRESHOLD error NL
		{
			yyerror("Malformed threshold command.");
		}
		;
	

exempt_cmd	: EXEMPT who exempt_type NL
		{
			addlist(exmpt, $2, name, num, $3);
		}
		| EXEMPT FILECOM NAME exempt_type NL
		{
                        filecom_parse(EXEMPT,$3,$4);
		}
		| EXEMPT error NL
		{
			yyerror("Malformed exempt command.");
		}
		;

refuse_cmd	: REFUSE who NL
		{
			addlist(refuse, $2, name, num, 0);
		}
                | REFUSE FILECOM NAME NL
                {
                        filecom_parse(REFUSE,$3,/* time or thing if any */0);
                }
		| REFUSE error NL
		{
			yyerror("Malformed refuse command.");
		}
		;

session_cmd     : SESSION who NUM NL
		{
			addlist(session, $2, name, num, $3);
		}
		| SESSION FILECOM NAME NUM NL
		{
                        filecom_parse(SESSION,$3,$4);
		}
		| SESSION DEFAULT NUM NL
		{
			session_default = $3;
		}
		| SESSION REFUSE NUM NL
		{
			sess_refuse_len = $3 * 60;
		}
		| SESSION error NL
		{
			yyerror("Malformed session command.");
		}
		;

idle_cmd	: TIMEOUT who NUM NL
		{
			addlist(rules, $2, name, num, $3);
		}
		| TIMEOUT FILECOM NAME NUM NL
		{
                        filecom_parse(TIMEOUT,$3,$4);
		}
		| TIMEOUT DEFAULT NUM NL
		{
			addlist(rules, DEFAULT, NULL, 0, $3);
		}
		| TIMEOUT error NL
		{
			yyerror("Malformed timeout command.");
		}
		;

sleep_cmd	: SLEEP NUM NL
		{
			sleeptime = $2;
		}
		| SLEEP error NL
		{
			yyerror("Malformed sleep command.");
		}
		;

warn_cmd	: WARN NUM NL
		{
			warntime = $2;
		}
		| WARN error NL
		{
			yyerror("Malformed warn command.");
		}
		;

idlemethod_cmd	: IDLEMETHOD USERINPUT NL
		{
			ioidle = FALSE;
		}
                | IDLEMETHOD INPUTOUTPUT NL
                {
			ioidle = TRUE;
                }
		| IDLEMETHOD error NL
		{
			yyerror("Malformed idlemethod command.");
		}
		;

conswins_cmd	: CONSWINS IDLE NUM NL
		{
			conswins_idle = $3 * 60;
		}
                | CONSWINS IDLE NORMAL NL
                {
                        conswins_idle = -2;
                }
                | CONSWINS IDLE OFF NL
                {
                        conswins_idle = -1;
                }
                | CONSWINS SESSION NUM NL
		{
			conswins_sess = $3 * 60;
		}
                | CONSWINS SESSION NORMAL NL
                {
                        conswins_sess = -2;
                }
                | CONSWINS SESSION OFF NL
                {
                        conswins_sess = -1;
                }
                | CONSWINS MULTIPLE NUM NL
		{
			conswins_mult = $3;
		}
                | CONSWINS MULTIPLE NORMAL NL
                {
                        conswins_mult = -2;
                }
                | CONSWINS MULTIPLE OFF NL
                {
                        conswins_mult = -1;
                }
		| CONSWINS error NL
		{
			yyerror("Malformed cons(ole) win(dow)s command.");
		}
		;

mult_cmd	: MULTIPLES NUM NL
		{
			mult_per_user = $2;
		}
		| MULTIPLES error NL
		{
			yyerror("Malformed multiples command.");
		}
		;
maxuser_cmd     : MAXUSER who NUM NL
		{
			addlist(maxuser, $2, name, num, $3);
		}
		| MAXUSER FILECOM NAME NUM NL
		{
                        filecom_parse(MAXUSER,$3,$4);
		}
		| MAXUSER error NL
		{
			yyerror("Malformed maxuser command.");
		}
		;

who		: name_type NAME
		{ 
			$$ = $1;
			name = $2;

			if ($1 == GROUP)
			{
				grp = getgrnam(name);
				if (grp != NULL)
                                	num = grp->gr_gid;
				else
					logfile("Error parsing conf file:  unknown group name '%s'.",name);
			}
			if ($1 == LOGIN)
			{
				if (getpwnam(name) == NULL)
					logfile("Warning parsing conf file:  unknown login name '%s'.",name);
			}
		}
		;

name_type	: LOGIN 	{ $$ = LOGIN;   }
		| HOST		{ $$ = HOST;    }
		| GROUP		{ $$ = GROUP;   }
		| TTY		{ $$ = TTY;     }
		;

exempt_type	: ALL		{ $$ = ALL; 	 }
		| IDLE		{ $$ = IDLE; 	 }
		| MULTIPLE	{ $$ = MULTIPLE; }
		| REFUSE	{ $$ = REFUSE;	 }
		| SESSION	{ $$ = SESSION;  }
		;

%%

static	int	errorcnt = 0;



int
yyerror(sb)
   char *sb;
{
	extern	int	linenum;

	logfile("%s: line %d: %s", config_file, linenum, sb);
	errorcnt++;
	return 0;
}



int
yywrap()
{
	extern	int	linenum;
        extern  time_t  conf_oldstamp;

	if ( errorcnt > 0 && conf_oldstamp <= 1 )
	{
	    logfile("Aborting due to conf file syntax errors.");
	    exit(1);
	}

	linenum = 1;
	return 1;
}

#ifndef HAVE_YYRESTART
void yyrestart(handle)
   FILE *handle;
{
        extern  int     linenum;

        linenum = 1;
}
#endif

/**************************************************************************
 * Reads in more rules from a separate file, which contains the           *
 * login names of the users for that type, one per line.                  *
 **************************************************************************/
void filecom_parse(type,filename,param)
   int type;               /* REFUSE, SESSION, TIMEOUT, or EXEMPT */
   char *filename;
   int param;              /* idle/session time or exempt type */
{
   FILE *handle;
   handle = fopen(filename,"r");
   if (handle == NULL)
   {
      char *buffer;
      buffer = (char *) malloc(20+sizeof(filename));
      sprintf(buffer,"Could not open file '%s'",filename);
      yyerror(buffer);
      free(buffer);
   }
   else
   {
      char lname[NAMELEN+1], trash[100], *c;
      while (!feof(handle) && (fgets(lname,NAMELEN+1,handle) != NULL))
      {
         /* If we didn't read in the newline, do so now */
         if (strchr(lname,'\n') == NULL)
            fscanf(handle,"%[^\n]\n",trash);

         /* First, strip away beyond the first space or newline */
         c = strchr(lname,' ');  if (c != NULL) *c = '\0';
         c = strchr(lname,'\n'); if (c != NULL) *c = '\0';

         if ((int)strlen(lname) > 0)
         {
            char *username;
            username = (char *) malloc (strlen(lname)+1);
            strcpy(username,lname);
            switch(type)
            {
               case REFUSE:
                  debug("Refusing user %s.",username);
                  addlist(refuse, LOGIN, username, 0, 0);
                  break;
               case MAXUSER:
                  debug("MaxUser Limiting group %s to %d logins.",username,param);
                  addlist(maxuser, GROUP, username, 0, param);
                  break;
               case SESSION:
                  debug("Session Limiting user %s to %d minutes.",username,param);
                  addlist(session, LOGIN, username, 0, param);
                  break;
               case TIMEOUT:
                  debug("Setting Idle timeout for user %s to %d minutes.",username,param);
                  addlist(rules, LOGIN, username, 0, param);
                  break;
               case EXEMPT:
                  debug("Exempting user %s from type %d.",username,param);
                  addlist(exmpt, LOGIN, username, 0, param);
                  break;
            }
         }
      }
   }
}


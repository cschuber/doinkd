%{

/**************************************************************************
 *  Lex grammer to scan input file for doinkd                              *
 **************************************************************************/

#include <stdio.h>
#include "y.tab.h"

#define makestr(Z)	((char *)strcpy((char *)malloc(strlen(Z)+1),Z))

int linenum = 1;	/* current line number for error messages */

%}

%%
all		return ALL;
default		return DEFAULT;
exempt		return EXEMPT;
group		return GROUP;
host            return HOST;
idle		return IDLE;
idlemethod	return IDLEMETHOD;
conswins        return CONSWINS;
login		return LOGIN;
multiple	return MULTIPLE;
multiples	return MULTIPLES;
refuse		return REFUSE;
session		return SESSION;
sleep		return SLEEP;
warn            return WARN;
threshold	return THRESHOLD;
timeout		return TIMEOUT;
tty		return TTY;
normal          return NORMAL;
off             return OFF;
userinput       return USERINPUT;
inputoutput     return INPUTOUTPUT;
file            return FILECOM;
maxuser		return MAXUSER;

[/A-Za-z][/A-Za-z0-9._:]*	{
				yylval.sb = makestr(yytext);
				return NAME; 
			}

[0-9]+			{
				yylval.nb = atoi(yytext);
				return NUM;
			}

-[0-9]+			{
				yylval.nb = atoi(yytext);
				return NUM;
			}

"#".*			;

"\n"			{ 
				linenum++; 
				return NL;
			}
[ \t]*			;

.			{
				static	char	errormsg[] = "Illegal character ' '.";

				errormsg[19] = yytext[0];
				yyerror(errormsg);
			}

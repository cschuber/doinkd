/*
 *  List manipulation routines
 */

#include <sys/types.h>
#include <sys/file.h>
#include <string.h>     /* For strlen(), strcpy(), etc. */
#include "doinkd.h"
#include "y.tab.h"

int	find();
extern	char	*ctime();
extern	time_t	time();
extern  void    insque();

/**************************************************************************
 *	adds a record to the list "list".                                 *
 *      list -- which list to add the rule                                *
 *      type -- what kind of rule (LOGIN, GROUP, etc)                     *
 *      name -- who it applies to (ajw, console, etc)                     *
 *      num  -- who it applies to (5 (group staff) )                      *
 *      flag -- idle time for a "rule" rule, or exemption type            *
 *	        for "exempt" rule: IDLE, MULTIPLE, etc.                   *
 **************************************************************************/
void addlist(list, type, name, num, flag)
   struct qelem *list;
   int           type;
   char         *name;
   int           num;
   int           flag;
{
   register struct item *new_data;
   register struct qelem *new_node;
   register struct qelem *ptr;

   /* make all the new structures */

   new_node = (struct qelem *) malloc (sizeof (struct qelem));
   new_data = (struct item *) malloc (sizeof (struct item));

   new_data->name_t = type;
   new_data->name = name;
   new_data->num = num;
   new_data->flag = flag;
   new_node->q_item = new_data;

   /* find where to insert it in the list, and insert it */

   for (ptr = list->q_forw; ptr != list; ptr = ptr->q_forw)
      if (ptr->q_item->name_t <= new_data->name_t)
	 break;

   insque (new_node, ptr->q_back);
}


/**************************************************************************
 * frees up the space in the list pointed to by ptr                       *
 **************************************************************************/
void freelist(ptr)
   struct qelem *ptr;
{
   register struct qelem *dead;
   register struct qelem *elemp;
   register struct qelem *start = ptr;

   for (elemp = start->q_forw; elemp != start;)
   {
      dead = elemp;
      elemp = elemp->q_forw;
      free ((char *) dead->q_item);	/* kill the data */
      free ((char *) dead);		/* now get the node */
   }

   start->q_forw = start;	/* reset pointers for a null list */
   start->q_back = start;
}


/**************************************************************************
 *	looks through the rules list and uses the most                    *
 *	specific rule for users[i], then looks through                    *
 *	all the exemptions setting them as they apply.                    *
 **************************************************************************/
void setlimits(i)
   int i;
{
   register int rule;
   register struct qelem *ptr;
   time_t  tempus;

   (void) time (&tempus);
   users[i].exempt = 0;		/* clear exemption flag */
   users[i].idle = -1;		/* clear his idle length */
   users[i].refuse = false;	/* clear refuse flag */
   users[i].maxed = false;	/* clear maxed flag */
   users[i].session = 0;	/* clear his session length */
   users[i].warned = false;		/* clear his warning flag */
   users[i].mgroup = 0;
   users[i].maxuser = 0;

   /* next time is set to now, so the new rules take affect immediately */

   users[i].next = tempus;

   for (rule = 0, ptr = maxuser->q_back; ptr != maxuser && !rule; ptr = ptr->q_back)
      if (rule = find (ptr, i)) {
	    users[i].mgroup = (ptr->q_item)->num;
	    users[i].maxuser = (ptr->q_item)->flag;
      }

   for (rule = 0, ptr = rules->q_back; ptr != rules && !rule; ptr = ptr->q_back)
      if (rule = find (ptr, i))
	 users[i].idle = (ptr->q_item)->flag * 60;

   for (rule = 0, ptr = refuse->q_back; ptr != refuse && !rule; ptr = ptr->q_back)
      if (rule = find (ptr, i))
	 users[i].refuse = true;

   for (rule = 0, ptr = session->q_back; ptr != session && !rule; ptr = ptr->q_back)
      if (rule = find (ptr, i))
	 users[i].session = (ptr->q_item)->flag * 60;

   for (ptr = exmpt->q_forw; ptr != exmpt; ptr = ptr->q_forw)
      if (find (ptr, i))
	 switch ((ptr->q_item)->flag)
	  {
	  case ALL:
	     users[i].exempt = IS_IDLE | IS_MULT | IS_LIMIT | IS_REFU | IS_MAXU;
	     break;

	  case IDLE:
	     users[i].exempt |= IS_IDLE;
	     break;

	  case MULTIPLE:
	     users[i].exempt |= IS_MULT;
	     break;

	  case MAXUSER:
	     users[i].exempt |= IS_MAXU;
	     break;

	  case REFUSE:
	     users[i].exempt |= IS_REFU;
	     break;

	  case SESSION:
	     users[i].exempt |= IS_LIMIT;
	     break;
	  }
}

/**************************************************************************
 * given a rule and a users structure, see if it applies                  *
 **************************************************************************/
int find(ptr,i)
   struct qelem *ptr;
   int i;
{
   register int j;

   switch ((ptr->q_item)->name_t)
    {
    case DEFAULT:
       return (1);

    case HOST:
       if (!strncmp ((ptr->q_item)->name, users[i].host, HOSTLEN))
	  return (1);

       break;

    case GROUP:
       for (j = 0; j < NGROUPS && users[i].groups[j] >= 0; j++)
	  if ((ptr->q_item)->num == users[i].groups[j])
	     return (1);

       break;

    case LOGIN:
       if (!strcmp ((ptr->q_item)->name, users[i].uid))
	  return (1);

       break;

    case TTY:
       if (!strcmp ((ptr->q_item)->name, users[i].line + 5))
	  return (1);

       break;
    }

   return (0);
}

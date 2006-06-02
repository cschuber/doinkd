/* Some functions to handle the linetime linked list, which allows
 * doinkd to log an error message that it can't warn a user, but also
 * be sure that it only logs the error once.  The error is usually
 * formed from the user getting killed (most likely from doinkd) and
 * the utmp file not getting updated to say that the user is no
 * longer logged in.  As soon as the utmp file is updated, the bad
 * line is removed from the list.
 */

#include "doinkd.h"

/* Check to see if this user structure can remove any old items */
void checklinetimeremove(him,list)
   struct user *him;
   struct linetime **list;
{
   struct linetime **ptr, *tmp;

   /* Can't do anything with an empty list */
   if (*list == NULL)
      return;

   ptr = list;
   while (*ptr != NULL)
   {
      /* If it is the same line, and the times are not the same,
       * then this line hs being used again and should be removed
       * from the list
       */
      if (strcmp(him->line,(*ptr)->line) == 0 && him->time_on != (*ptr)->time_on)
      {
         /* Remove this one! */
         tmp = *ptr;
         *ptr = (*ptr)->next;
         free(tmp);
      }
      else /* Move on to the next one */
         ptr = &((*ptr)->next);
   }
}

/* Check to see if this user structure can remove any old items */
void addlinetime(him,list)
   struct user *him;
   struct linetime **list;
{
   struct linetime *tmp;

   /* Make the start of the list, if necessary */
   if (*list == NULL)
   {
      *list = (struct linetime *) malloc(sizeof (struct linetime));
      strcpy((*list)->line,him->line);
      (*list)->time_on = him->time_on;
      (*list)->next = NULL;
      return;
   }

   /* Insert this baby right at the start */
   tmp = (struct linetime *) malloc(sizeof (struct linetime));
   strcpy(tmp->line,him->line);
   tmp->time_on = him->time_on;
   tmp->next = *list;
   *list = tmp;
}

/* Check to see if this user/line has already been inserted in the
 * list.  If so, return TRUE so that warn.c knows not to log the
 * error again.
 */
int inlinetime(him,list)
   struct user *him;
   struct linetime **list;
{
   struct linetime *ptr;

   /* Can't do anything with an empty list */
   if (*list == NULL)
      return FALSE;

   ptr = *list;
   while (ptr != NULL)
   {
      /* If it is the same line, and the times are the same, return TRUE */
      if (strcmp(him->line,ptr->line) == 0 && him->time_on == ptr->time_on)
      {
         return TRUE;
      }

      /* Move on to the next one */
      ptr = ptr->next;
   }
   return FALSE;
}

void printlinetimelist(list)
   struct linetime *list;
{
   while (list != NULL)
   {
      /* If it is the same line, and the times are the same, return TRUE */
      printf("Element:  %14s : %d\n",list->line,list->time_on);

      /* Move on to the next one */
      list = list->next;
   }
}

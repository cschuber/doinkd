/*
 *	Queue insertion and deletion routines for non-Vaxen
 */


struct	qelem
{ 
	struct	qelem	*q_forw;
	struct	qelem	*q_back;
#ifdef LINT
	char q_data[];
#endif /* LINT */
};


#ifndef VAX

void
insque(elem , pred)
   register struct	qelem	*elem;
   struct	 qelem		*pred;
{
   register struct qelem *pred_ptr = pred;

   elem->q_forw = pred_ptr->q_forw;
   pred_ptr->q_forw = elem;
   elem->q_forw->q_back = elem;
   elem->q_back = pred_ptr;
}

#endif /* VAX */

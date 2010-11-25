/*
 * Graph Engine - Copyright (C) 2007 by Arjen Lentz (arjen@openquery.com.au)
 * graphstore.c internal storage system
 */
#include <stdlib.h>
#include <string.h>
#include <my_global.h>
#include <my_sys.h>
#include "graphstore.h"


/*
	create a new vertex, and add it to the list (or start a list)
	NOTE! gspp is ptr to base ptr

	returns 1 for ok, 0 for error
*/
static int _add_vertex (GRAPHSTORE **gspp, GRAPH_VERTEXID id)
{
	GRAPHSTORE *newgsp;
	GRAPHSTORE *gscurp;

	if (gspp == NULL)
		return 0;

	/* not allowing 0 */
	if (!id)
		return 0;

	if (*gspp != NULL) {
		for (gscurp = *gspp; gscurp != NULL; gscurp = gscurp->next) {
			if (gscurp->vertex->id == id)
				return 1;	/* we can ignore, id already exists */
		}
	}

	/* allocate and initialise */
	if ((newgsp = my_malloc(sizeof (GRAPHSTORE),MYF(MY_ZEROFILL))) == NULL)
		return 0;

	if ((newgsp->vertex = my_malloc(sizeof (GRAPH_VERTEX),MYF(MY_ZEROFILL))) == NULL) {
		my_free(newgsp,MYF(0));
		return 0;
	}

	newgsp->vertex->id = id;
	/* add new vertex to end of list */
	if (*gspp != NULL) {
		for (gscurp = *gspp; gscurp->next != NULL; gscurp = gscurp->next);
		gscurp->next = newgsp;
	}
	else /* new list */
		*gspp = newgsp;

	/* ok */
	return 1;
}


/*
	find a vertex by id

	returns ptr or NULL
*/
static GRAPH_VERTEX *_find_vertex (GRAPHSTORE *gsp, GRAPH_VERTEXID id)
{
	/* just loop through the list to find id */
	while (gsp != NULL && gsp->vertex->id != id)
		gsp = gsp->next;

	/* return ptr to vertex, or NULL */
	return (gsp != NULL ? gsp->vertex : NULL);
}


/*
	add edge
	both vertices must already exist; graphstore_insert() does this

	return 1 for ok, 0 for error (already exists, alloc error, etc)
*/
static int _add_edge (GRAPHSTORE *gsp, GRAPH_VERTEXID origid, GRAPH_VERTEXID destid, GRAPH_WEIGHT weight)
{
	GRAPH_VERTEX *origvp, *destvp;
	GRAPH_EDGE	*ep, *newep;

	/* find both vertices */
	if ((origvp = _find_vertex(gsp,origid)) == NULL ||
		(destvp = _find_vertex(gsp,destid)) == NULL)
		return 0;

	/* check if edge already exists */
	for (ep = origvp->forward_edge; ep != NULL; ep = ep->next_edge) {
		if (ep->vertex->id == destid)
			return 0;
	}

	/* allocate and initialise new edge */
	if ((newep = my_malloc(sizeof (GRAPH_EDGE),MYF(MY_ZEROFILL))) == NULL)
		return 0;

	newep->vertex = destvp;
	newep->weight = weight;

	/* insert new edge at start of chain, that's easiest */
	ep = origvp->forward_edge;
	origvp->forward_edge = newep;
	newep->next_edge = ep;

	/* ok */
	return 1;
}


/*
	create a new row, and add it to the graph set (or start set)
	NOTE! gsetpp is ptr to base ptr

	returns 1 for ok, 0 for error
*/
static int _add_graph_set (GRAPH_SET **gsetpp, GRAPH_TUPLE *gtp)
{
	GRAPH_SET *newgsetp;
	GRAPH_SET *gsetcurp;

	if (gsetpp == NULL || gtp == NULL)
		return 0;

	/* allocate and initialise */
	if ((newgsetp = my_malloc(sizeof (GRAPH_SET),MYF(MY_ZEROFILL))) == NULL)
		return 0;

	/* put in the data */
	memcpy(&newgsetp->tuple,gtp,sizeof (GRAPH_TUPLE));

	/* add new row to end of set */
	if (*gsetpp != NULL) {
		for (gsetcurp = *gsetpp; gsetcurp->next != NULL; gsetcurp = gsetcurp->next);
		gsetcurp->next = newgsetp;
	}
	else {	/* new set */
		*gsetpp = newgsetp;
	}

	/* ok */
	return 1;
}


/*
	free a graph set (release memory)

	returns 1 for ok, 0 for error
*/
int free_graph_set (GRAPH_SET *gsetp)
{
	GRAPH_SET *nextgsetp;

	if (gsetp == NULL)
		return 0;

	while (gsetp != NULL) {
		nextgsetp = gsetp->next;
		/* free() is a void function, nothing to check */
		my_free(gsetp,MYF(0));
		gsetp = nextgsetp;
	}

	/* ok */
	return 1;
}


/*
	insert new data into graphstore
	this can be either a vertex or an edge, depending on the params
	NOTE! gspp is ptr to base ptr

	returns 1 for ok, 0 for error
*/
int graphstore_insert (GRAPHSTORE **gspp, GRAPH_TUPLE *gtp)
{
	if (gspp == NULL)
		return 0;

	/* if nada or no orig vertex, we can't do anything */
	if (gtp == NULL || !gtp->origid)
		return 0;

#if 0
printf("inserting: origid=%lu destid=%lu weight=%lu\n",gtp->origid,gtp->destid,gtp->weight);
#endif

	if (!gtp->destid)	/* no edge param so just adding vertex */
		return _add_vertex(gspp,gtp->origid);

	/*
		add an edge
		first add both vertices just in case they didn't yet exist...
		not checking result there: if there's a prob, _add_edge() will catch.
	*/
	_add_vertex(gspp,gtp->origid);
	_add_vertex(gspp,gtp->destid);
	return _add_edge(*gspp,gtp->origid,gtp->destid,gtp->weight);
}


/*
	this is an internal function used by graphstore_query()

	find any path from originating vertex to destid
	if found, add to the result set on the way back
	NOTE: recursive function!
	
	returns 1 for hit, 0 for nothing, -1 for error
*/
int _find_any_path(GRAPH_SET **gsetpp, GRAPH_VERTEXID origid, GRAPH_VERTEXID destid, GRAPH_VERTEX *gvp, GRAPH_SEQ depth)
{
	GRAPH_EDGE *gep;
	GRAPH_TUPLE tup;
	int res;

	if (gvp->id == destid) {
		/* found target! */
		bzero(&tup,sizeof (GRAPH_TUPLE));
		tup.origid	= origid;
		tup.destid	= destid;
		tup.seq		= depth;
		tup.linkid	= gvp->id;
		return (_add_graph_set(gsetpp,&tup) ? 1 : -1);
	}

	/* walk through all edges for this vertex */
	for (gep = gvp->forward_edge; gep; gep = gep->next_edge) {
		/* recurse */
		res = _find_any_path(gsetpp,origid,destid,gep->vertex,depth+1);
		if (res < 0)
			return res;
		if (res > 0) {
			/* found somewhere below this one, insert ourselves and return */
			bzero(&tup,sizeof (GRAPH_TUPLE));
			tup.origid	= origid;
			tup.destid	= destid;
			tup.weight  = gep->weight;
			tup.seq		= depth;
			tup.linkid	= gvp->id;
			return (_add_graph_set(gsetpp,&tup) ? 1 : -1);			
		}
	}

	/* nothing found but no error */
	return 0;
}


/*
	query graphstore
	latch specifies what operation to perform

	we need to feed the conditions in... (through engine condition pushdown)
	for now we just presume one condition per field so we just feed in a tuple
	this also means we can just find constants, not ranges

	return ptr to GRAPH_SET
	caller must free with free_graph_set()
*/
GRAPH_SET *graphstore_query (GRAPHSTORE *gsp, GRAPH_TUPLE *gtp)
{
	GRAPH_SET *gsetp = NULL;
	GRAPH_SET *gsetcurp;
	GRAPH_SET *newgsetp;

	if (gsp == NULL || gtp == NULL)
		return (NULL);

	switch (gtp->latch) {
		case 0: /* return all vertices/edges */
			{
				GRAPHSTORE *gscurp;
				GRAPH_EDGE *gep;
				GRAPH_TUPLE tup;

				/* walk through all vertices */
				for (gscurp = gsp; gscurp != NULL; gscurp = gscurp->next) {
					/* check for condition */
					if (gtp->origid && gscurp->vertex->id != gtp->origid)
						continue;

					bzero(&tup,sizeof (GRAPH_TUPLE));
					tup.origid = gscurp->vertex->id;

					/* no edges? */
					if (gscurp->vertex->forward_edge == NULL) {
						/* just add vertex to set */
						if (!_add_graph_set(&gsetp,&tup)) {
							if (gsetp != NULL)	/* clean up */
								my_free(gsetp,MYF(0));
							return (NULL);
						}
					}
					else {
						/* walk through all edges */
						for (gep = gscurp->vertex->forward_edge; gep; gep = gep->next_edge) {
							tup.destid	= gep->vertex->id;
							tup.weight	= gep->weight;

							/* just add vertex to set */
							if (!_add_graph_set(&gsetp,&tup)) {
								if (gsetp != NULL)	/* clean up */
									my_free(gsetp,MYF(0));
								return (NULL);
							}
						}
					}
				}
			}
			break;

		case 1:	/* find a path between origid and destid */
				/* yes it'll just go with the first path it finds! */
			{
				GRAPHSTORE *gscurp;
				GRAPH_VERTEX *origvp;
				GRAPH_TUPLE tup;

				if (!gtp->origid || !gtp->destid)
					return NULL;

				/* find both vertices */
				if ((origvp = _find_vertex(gsp,gtp->origid)) == NULL ||
					_find_vertex(gsp,gtp->destid) == NULL)
					return NULL;

				if (_find_any_path(&gsetp,gtp->origid,gtp->destid,origvp,0) < 0) {	/* error? */
					if (gsetp != NULL)	/* clean up */
						my_free(gsetp,MYF(0));
					return NULL;
				}
			}
			break;

		default:
			/* this ends up being an empty set */
			break;
	}

	/* Fix up latch column with the proper value - to be relationally correct */
	for (gsetcurp = gsetp; gsetcurp != NULL; gsetcurp = gsetcurp->next)
		gsetcurp->tuple.latch = gtp->latch;

	return gsetp;
}



/* end of graphstore.c */
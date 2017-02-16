/*
 * Graph Engine - Copyright (C) 2007 by Arjen Lentz (arjen@openquery.com.au)
 * graphstore.h internal storage system
 */
//typedef unsigned short uint16;
//typedef unsigned long long uint64;


/*
	This is essentially what a GRAPH engine table looks like on the MySQL end:
	CREATE TABLE foo (
		latch	SMALLINT	UNSIGNED NULL,
		origid	BIGINT		UNSIGNED NULL,
		destid	BIGINT		UNSIGNED NULL,
		weight	BIGINT		UNSIGNED NULL,
		seq		BIGINT		UNSIGNED NULL,
		linkid	BIGINT		UNSIGNED NULL
 	) ENGINE=OQGRAPH
*/


/*
	We represent the above in C in the following way:
*/
typedef uint16	GRAPH_LATCH;
typedef uint64	GRAPH_VERTEXID;
typedef uint64	GRAPH_WEIGHT;
typedef uint64	GRAPH_SEQ;

typedef struct graph_tuple {
	GRAPH_LATCH		latch;		/* function 							*/
	GRAPH_VERTEXID	origid;		/* vertex (should be != 0)				*/
	GRAPH_VERTEXID	destid;		/* edge									*/
	GRAPH_WEIGHT	weight;		/* weight								*/
	GRAPH_SEQ		seq;		/* seq# within (origid)					*/
	GRAPH_VERTEXID	linkid;		/* current step between origid/destid	*/
} GRAPH_TUPLE;

typedef struct graph_set {
	GRAPH_TUPLE			tuple;
	struct graph_set	*next;
} GRAPH_SET;


/*
	Internally, sets look nothing like the above

	- We have vertices, connected by edges.
	- Each vertex' edges are maintained in a linked list.
	- Edges can be weighted.

	There are some issues with this structure, it'd be a pest to do a delete
	So for now, let's just not support deletes!
*/
/* the below is half-gross and will likely change */
typedef struct graph_edge {
	struct graph_vertex {
		GRAPH_VERTEXID		 id;
		struct graph_edge	*forward_edge;
	}					*vertex;
	GRAPH_WEIGHT	 	 weight;
	struct graph_edge	*next_edge;
} GRAPH_EDGE;

typedef struct graph_vertex GRAPH_VERTEX;


/*
	A rough internal storage system for a set
*/
/* this below is fully gross and will definitely change */
typedef struct graphstore {
	GRAPH_VERTEX		*vertex;	/* changed to ptr when integrating into MySQL */
	struct graphstore	*next;
} GRAPHSTORE;

#ifdef __cplusplus
extern "C" {
#endif

/* public function declarations */
int graphstore_insert (GRAPHSTORE **gspp, GRAPH_TUPLE *gtp);
GRAPH_SET *graphstore_query (GRAPHSTORE *gsp, GRAPH_TUPLE *gtp);
int free_graph_set (GRAPH_SET *gsetp);

#ifdef __cplusplus
}
#endif

/* end of graphstore.h */
#if !defined(OM_H)
#define OM_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* I'm writing this in C to demonstrate how it is used.  We can implement it
   later either using void*s
   or templates under the hood. */

/* I've made the following assumptions which very well might be wrong.

    1:  We are storing key/value pairs, not just keys.
    1a: We want to abstract a key/value pair to an OMITEM.
    2:  OMITEM will NOT support telling you the index number (for now).
    2a: Indexs (for purposes of logging) will be retrieved by an output
        parameter.
    3:  The CALLER of the OMS functions own the memory of the DBTs.
        The OM structure will copy the OMITEM, but
        key.data and value.data will be owned by the caller
        responsibility for freeing/etc belongs to the caller.
        Should not free anything till its been removed from teh OMS.
    4:  I don't know what to call it so I'm just calling it 'oms_blah'
    5:  We do not need to do multiple interleaving iterations.
    5a: If we do, we need to change prototypes, perhaps pass a status object along.
    6:  For inserting (with search), it will not replace already existing items
        it will just report that it was already inside.
*/

/* This is my guess of what an OMITEM should be. */
typedef struct {
    DBT key;
    DBT value;
} OMITEM;

/*
    Create an empty OMS.

    Possible Error codes
        0
        ENOMEM
    Will assert ptree, db, cmp are NOT NULL.
*/
int oms_create(OMS** ptree,
               DB* db, int (*cmp)(DB*, const OMITEM*, const OMITEM*));

/*
    Create an OMS containing the elements in a presorted array.

    Possible Error codes
        0
        ENOMEM
    Will assert ptree, db, cmp, items are NOT NULL.
*/
int oms_create_from_presorted_array(OMS** ptree, DB* db,
                                  int (*cmp)(DB*, const OMITEM*, const OMITEM*),
                                  OMITEM* items, u_int32_t num_items);

/*
    Create an OMS containing presorted elements accessed by an iterator.

    Possible Error codes
        0
        ENOMEM
    Will assert ptree is NOT NULL.

    NOTE: I'm using void* here cause I don't know what the parameters should be.
    In the actual implementation I will use the real data types.
    We can also change the iterator type, i.e. make it return int
    and we get next via an output parameter.

    Note: May just be a wrapper for oms_create_presorted_array.

    Will assert ptree, db, cmp, items are NOT NULL.
*/
int oms_create_from_presorted_iterator(OMS** ptree, DB* db,
                                  int (*cmp)(DB*, const OMITEM*, const OMITEM*),
                                  OMITEM* (*get_next)(void* param));

/*
    Close/free an OMS.
    Note: This will not free key.data/value.data for entries inside.
    Those should be freed immediately before or after calling oms_destroy.

    Will assert tree is NOT NULL.
*/
void oms_destroy(OMS* tree);

/*
    NOTE: USES THE COMPARISON FUNCTION
    Initializes iteration over the tree.
    if start is NULL, we start at the head, otherwise we search for it.
    Searching requires a comparison function!

    Will assert tree is NOT NULL.

    if not found, it will allow you to find 
*/
void oms_init_iteration(OMS* tree, OMITEM* start);

/*
    Initializes iteration over the tree.
    if start is NULL, we start at the head, otherwise we search for it.
    Searching requires a comparison function!

    Will assert tree is NOT NULL.

    Possible error codes
        0
        ERANGE: If start_index >= the number of elements in the structure
*/
int oms_init_iteration_at(OMS* tree, u_int32_t start_index);

/*
    Gets the next item in the tree.
    When you go off the end, it returns NULL, as will subsequent calls.

    Use oms_init_iteration(_at) to reset the iterator.
*/
OMITEM* oms_get_next(OMS* tree);

/*
    NOTE: USES THE COMPARISON FUNCTION
    Insert an item at the appropriate place.

    Will assert tree, item, and already_exists are NOT NULL.
    already_exists is an out parameter.
    If the exact OMITEM is already there, it will NOT be replaced,
    but we will report that.
    Reports the index it was found at.

    Possible error codes:
        0
        ENOMEM
        DB_KEYEXIST:    If it already exists in the structure.
*/
int oms_insert(OMS* tree, OMITEM* item, u_int32_t* index);

/*
    Insert an item at a given index.

    Will assert tree, item, and already_exists are NOT NULL.
    already_exists is an out parameter.
    If the exact OMITEM is already there, it will NOT be replaced,
    but we will report that.

    Possible error codes:
        0
        ENOMEM
*/
int oms_insert_at(OMS* tree, OMITEM* item, u_int32_t index);

/*
    NOTE: USES THE COMPARISON FUNCTION
    Deletes a given item.

    Will assert tree, item, and found are NOT NULL.
    Reports the index it was found at.
    Possible error codes:
        0
        DB_NOTFOUND
*/
int oms_delete(OMS* tree, OMITEM* item, u_int32_t* index);

/*
    Deletes the item at a given index.

    Possible error codes:
        0
        ERANGE: If index >= num elements in the structure
*/
int oms_delete_at(OMS* tree, u_int32_t index);

/*
    I don't know what kind of 'finds' we need here.
*/
int oms_find(OMS* tree, OMITEM* item, u_int32_t find_flags);

/*
    Creates 2 new trees caused by splitting the current one evently.
    Reports the split index.
    Does NOT free the old one.
*/
int oms_split_evenly(OMS* tree, OMS** pleft_tree, OMS** pright_tree,
                     u_int32_t* index);

/*
    Creates 2 new trees caused by splitting the current one at the
    given index.  (0..index-1) are in left, (index..end) are in right.
    Does NOT free the old one.
*/
int oms_split_at(OMS* tree, OMS** pleft_tree, OMS** pright_tree,
                 u_int32_t index);
 

/*
    Creates one tree from merging 2 of them.
    Does not free the old one.
    reports the 'split index' that you would use to undo the operation.
*/
int oms_merge(OMS** ptree, OMS* left_tree, OMS* right_tree, u_int32_t* index);

u_int32_t oms_get_num_elements(OMS* tree);



#endif  /* #ifndef OM_H */

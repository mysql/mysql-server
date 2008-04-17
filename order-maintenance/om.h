#if !defined(OM_H)
#define OM_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Each of these C++ templated items can be wrapped with a simple header that uses pure C. */
template <typename ITEM_TYPE, typename EXTRA_RENUMBER>
struct OMS {
    /* Stuff */
    OMSITEM* foo;    
};

/* The actual header would be written entirely in C, using wrapper functions, this is just a starting example. */

/* The templated functions are static and inline so the wrapper C functions don't add any overhead. */


/*
    Questions/issues:
        1-  Do we really need to wrap items in an OMITEM<ITEM_TYPE> container?
            I assume yes.. for example, the ITEM_TYPE could be a DBT,
            and the OMITEM<DBT*> would also hold the index (plus maybe additional stuff).
        2-  For a single OMS, do we need to support CHANGING the renumberf function?
            i.e. won't it be the same function for every insert for a given OMS?
            I'm assuming the renumberf function stays the same, so I provide it just
            once in the constructor.
            Actually, if the function is constant, and only the 'extra' can differ,
            it can be a template parameter and be even faster.
        3-  Similarly to #2, will the 'extra info' to the renumberf function ever change?
            I'm assuming it stays the same for the duration of an OMS,
            and am passing it to the constructor.
        4-  For 'insert_in_appropriate place', I know the comparison function is not always available
            So I'm giving it as a parameter to that function.
        5-  Extra info to the comparison function.  This can change (for the lock tree),
            so its a parameter to the functoins that use comparisons.
        6-  Do we need some way of 'loading' an order maintenance structure?
            i.e. use these tags for the following items instead of 'inserting' over and over.
        7-  The tag might not be able to be just 64 bits, is it ok to use 2 64 bit ints?
*/

template <typename ITEM_TYPE, typename EXTRA_RENUMBER, typename EXTRA_CMP>
static inline int toku_oms_create(OMS<ITEM_TYPE, EXTRA_RENUMBER, EXTRA_CMP>** poms,
                                  void (*renumberf)(OMITEM<ITEM_TYPE>*, u_int64_t old_index, u_int64_t new_index, EXTRA_RENUMBER* extra_for_renumberf),
                                  /* Additional parameters to pass to the callback function. */
                                  EXTRA_RENUMBER* extra_for_renumberf);

template <typename ITEM_TYPE, typename EXTRA_RENUMBER, typename EXTRA_CMP>
static inline int toku_oms_close(OMS<ITEM_TYPE, EXTRA_RENUMBER, EXTRA_CMP>* oms);

static inline int toku_oms_insert(OMS* oms,             /* The order maintenance structure. */

template <typename ITEM_TYPE, typename EXTRA_RENUMBER, typename EXTRA_CMP>
static inline int toku_oms_insert(OMS<ITEM_TYPE, EXTRA_RENUMBER, EXTRA_CMP>* oms,                     /* The order maintenance structure. */
                                  OMITEM<ITEM_TYPE>* prev_omi,  /* Pass in NULL if the new item is at the head, otherwise pass in the predecessor. */
                                  DATA_ITEM* item);             /* The user-provided data item. */


template <typename ITEM_TYPE, typename EXTRA_RENUMBER, typename EXTRA_CMP>
static inline int toku_oms_delete(OMS<ITEM_TYPE, EXTRA_RENUMBER, EXTRA_CMP>* oms,   /* The order maintenance structure. */
                                  OMSITEM<ITEM_TYPE>* to_remove);                   /* The user-provided data item. */


/* This will use the comparison function to find the appropriate location,
   and then call toku_oms_insert with the appropriate predecessor. */
static inline int toku_oms_insert_appropriately(OMS<ITEM_TYPE, EXTRA_RENUMBER, EXTRA_CMP>* oms,                     /* The order maintenance structure. */
                                                DATA_ITEM* item,
                                                void (*cmp)(EXTRA_CMP* extra_for_cmp, ITEM_TYPE*, ITEM_TYPE*),
                                                /* Additional parameters to pass to the comparison function. */
                                                EXTRA_CMP*      extra_for_cmp);


/* Example wrapper */
extern "C" {
    int toku_node_node_oms_insert(toku_node_oms* oms,
                                  toku_node_omsitem* prev_omi,
                                  DBT* item) {
        return toku_oms_insert<toku_node_omsitem, DBT, int, DB>(oms, prev_omi, item);
    }
}


#endif  /* #ifndef OM_H */

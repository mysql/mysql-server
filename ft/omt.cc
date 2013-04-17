/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <memory.h>
#include <string.h>
#include <db.h>

#include "omt.h"

int
toku_omt_create_steal_sorted_array(OMT *omtp, OMTVALUE **valuesp, uint32_t numvalues, uint32_t capacity) {
    OMT XMALLOC(omt);
    omt->create_steal_sorted_array(valuesp, numvalues, capacity);
    *omtp = omt;
    return 0;
}

//TODO: Put all omt API functions here.
int toku_omt_create (OMT *omtp) {
    OMT XMALLOC(omt);
    omt->create();
    *omtp = omt;
    return 0;
}

void toku_omt_destroy(OMT *omtp) {
    OMT omt=*omtp;
    omt->destroy();
    toku_free(omt);
    *omtp=NULL;
}

uint32_t toku_omt_size(OMT V) {
    return V->size();
}

int toku_omt_create_from_sorted_array(OMT *omtp, OMTVALUE *values, uint32_t numvalues) {
    OMT XMALLOC(omt);
    omt->create_from_sorted_array(values, numvalues);
    *omtp=omt;
    return 0;
}

int toku_omt_insert_at(OMT omt, OMTVALUE value, uint32_t index) {
    return omt->insert_at(value, index);
}

int toku_omt_set_at (OMT omt, OMTVALUE value, uint32_t index) {
    return omt->set_at(value, index);
}

int toku_omt_delete_at(OMT omt, uint32_t index) {
    return omt->delete_at(index);
}

int toku_omt_fetch(OMT omt, uint32_t i, OMTVALUE *v) {
    return omt->fetch(i, v);
}

struct functor {
    int (*f)(OMTVALUE, uint32_t, void *);
    void *v;
};
static_assert(std::is_pod<functor>::value, "not POD");

int call_functor(const OMTVALUE &v, uint32_t idx, functor *const ftor);
int call_functor(const OMTVALUE &v, uint32_t idx, functor *const ftor) {
    return ftor->f(const_cast<OMTVALUE>(v), idx, ftor->v);
}

int toku_omt_iterate(OMT omt, int (*f)(OMTVALUE, uint32_t, void*), void*v) {
    struct functor ftor = { .f = f, .v = v };
    return omt->iterate<functor, call_functor>(&ftor);
}

int toku_omt_iterate_on_range(OMT omt, uint32_t left, uint32_t right, int (*f)(OMTVALUE, uint32_t, void*), void*v) {
    struct functor ftor = { .f = f, .v = v };
    return omt->iterate_on_range<functor, call_functor>(left, right, &ftor);
}

struct heftor {
    int (*h)(OMTVALUE, void *v);
    void *v;
};
static_assert(std::is_pod<heftor>::value, "not POD");

int call_heftor(const OMTVALUE &v, const heftor &htor);
int call_heftor(const OMTVALUE &v, const heftor &htor) {
    return htor.h(const_cast<OMTVALUE>(v), htor.v);
}

int toku_omt_insert(OMT omt, OMTVALUE value, int(*h)(OMTVALUE, void*v), void *v, uint32_t *index) {
    struct heftor htor = { .h = h, .v = v };
    return omt->insert<heftor, call_heftor>(value, htor, index);
}

int toku_omt_find_zero(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    return V->find_zero<heftor, call_heftor>(htor, value, index);
}

int toku_omt_find(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, int direction, OMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    return V->find<heftor, call_heftor>(htor, direction, value, index);
}

int toku_omt_split_at(OMT omt, OMT *newomtp, uint32_t index) {
    OMT XMALLOC(newomt);
    int r = omt->split_at(newomt, index);
    if (r != 0) {
        toku_free(newomt);
    } else {
        *newomtp = newomt;
    }
    return r;
}

int toku_omt_merge(OMT leftomt, OMT rightomt, OMT *newomtp) {
    OMT XMALLOC(newomt);
    newomt->merge(leftomt, rightomt);
    toku_free(leftomt);
    toku_free(rightomt);
    *newomtp = newomt;
    return 0;
}

int toku_omt_clone_noptr(OMT *dest, OMT src) {
    OMT XMALLOC(omt);
    omt->clone(*src);
    *dest = omt;
    return 0;
}

void toku_omt_clear(OMT omt) {
    omt->clear();
}

size_t toku_omt_memory_size (OMT omt) {
    return omt->memory_size();
}


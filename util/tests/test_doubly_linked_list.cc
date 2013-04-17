/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include <stdlib.h>
#include <util/doubly_linked_list.h>

using namespace toku;

static void check_is_empty (DoublyLinkedList<int> *l) {
    LinkedListElement<int> *re;
    bool r = l->pop(&re);
    assert(!r);
}

static void test_doubly_linked_list (void) {
    DoublyLinkedList<int> l;
    l.init();
    LinkedListElement<int> e0, e1;

    l.insert(&e0, 3);
    {
	LinkedListElement<int> *re;
	bool r = l.pop(&re);
	assert(r);
	assert(re==&e0);
	assert(re->get_container()==3);
    }
    check_is_empty(&l);

    l.insert(&e0, 0);
    l.insert(&e1, 1);
    {
	bool in[2]={true,true};
	for (int i=0; i<2; i++) {
	    LinkedListElement<int> *re;
	    bool r = l.pop(&re);
	    assert(r);
	    int  v = re->get_container();
	    assert(v==0 || v==1);
	    assert(in[v]);
	    in[v]=false;
	}
    }
    check_is_empty(&l);
}

const int N=100;
bool in[N];
DoublyLinkedList<int> l;
LinkedListElement<int> elts[N];

static void maybe_insert_random(void) {
    int x = random()%N;
    if (!in[x]) {
	if (verbose) printf("I%d ", x);
	l.insert(&elts[x], x);
	in[x]=true;
    }
}

static bool checked[N];
static int  check_count;
static int check_is_in(int v, int deadbeef) {
    assert(deadbeef=0xdeadbeef);
    assert(0<=v && v<N);
    assert(!checked[v]);
    assert(in[v]);
    checked[v]=true;
    check_count++;
    return 0;
}
static int quit_count=0;
static int quit_early(int v __attribute__((__unused__)), int beefbeef) {
    assert(beefbeef=0xdeadbeef);
    quit_count++;
    if (quit_count==check_count) return check_count;
    else return 0;
}

static void check_equal(void) {
    check_count=0;
    for (int i=0; i<N; i++) checked[i]=false;
    {
	int r = l.iterate<int>(check_is_in, 0xdeadbeef);
	assert(r==0);
    }
    for (int i=0; i<N; i++) assert(checked[i]==in[i]);

    if (check_count>0) {
	check_count=1+random()%check_count; // quit after 1 or more iterations
	quit_count=0;
	int r = l.iterate<int>(quit_early, 0xbeefbeef);
	assert(r==check_count);
    }
}

static void test_doubly_linked_list_randomly(void) {
    l.init();
    for (int i=0; i<N; i++) in[i]=false;

    for (int i=0; i<N/2; i++) maybe_insert_random();
    if (verbose) printf("\n");

    for (int i=0; i<N*N; i++) {
	int x = random()%N;
	if (in[x]) {
	    if (random()%2==0) {
		if (verbose) printf("%dR%d ", i, x);
		l.remove(&elts[x]);
		in[x]=false;
	    } else {
		LinkedListElement<int> *re;
		bool r = l.pop(&re);
		assert(r);
		int v = re->get_container();
		assert(in[v]);
		in[v]=false;
		if (verbose) printf("%dP%d ", i, v);
	    }
	} else {
	    l.insert(&elts[x], x);
	    in[x]=true;
	    if (verbose) printf("%dI%d ", i, x);
	}

	check_equal();
    }
    if (verbose) printf("\n");

    LinkedListElement<int> *re;
    while (l.pop(&re)) {
	int v = re->get_container();
	assert(in[v]);
	in[v]=false;
	if (verbose) printf("P%d ", v);
    }
    for (int i=0; i<N; i++) assert(!in[i]);
    if (verbose) printf("\n");
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_doubly_linked_list();
    for (int i=0; i<4; i++) {
	test_doubly_linked_list_randomly();
    }
    return 0;
}

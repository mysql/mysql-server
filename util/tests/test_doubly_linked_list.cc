/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
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

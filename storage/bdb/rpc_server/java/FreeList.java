/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: FreeList.java,v 1.7 2004/04/06 20:43:41 mjc Exp $
 */

package com.sleepycat.db.rpcserver;

import java.util.*;

/**
 * Keep track of a list of objects by id with a free list.
 * Intentionally package-protected exposure.
 */
class FreeList {
    class FreeIndex {
        int index;
        FreeIndex(int index) { this.index = index; }
        int getIndex() { return index; }
    }

    Vector items = new Vector();
    FreeIndex free_head = null;

    public synchronized int add(Object obj) {
        int pos;
        if (free_head == null) {
            pos = items.size();
            items.addElement(obj);
            if (pos + 1 % 1000 == 0)
                Server.err.println(this + " grew to size " + (pos + 1));
        } else {
            pos = free_head.getIndex();
            free_head = (FreeIndex)items.elementAt(pos);
            items.setElementAt(obj, pos);
        }
        return pos;
    }

    public synchronized void del(int pos) {
        Object obj = items.elementAt(pos);
        if (obj != null && obj instanceof FreeIndex)
            throw new NoSuchElementException("index " + pos + " has already been freed");
        items.setElementAt(free_head, pos);
        free_head = new FreeIndex(pos);
    }

    public void del(Object obj) {
        del(items.indexOf(obj));
    }

    public Object get(int pos) {
        Object obj = items.elementAt(pos);
        if (obj instanceof FreeIndex)
            obj = null;
        return obj;
    }

    public LocalIterator iterator() {
        return new FreeListIterator();
    }

    /**
     * Iterator for a FreeList.  Note that this class doesn't implement
     * java.util.Iterator to maintain compatibility with Java 1.1
     * Intentionally package-protected exposure.
     */
    class FreeListIterator implements LocalIterator {
        int current;

        FreeListIterator() { current = findNext(-1); }

        private int findNext(int start) {
            int next = start;
            while (++next < items.size()) {
                Object obj = items.elementAt(next);
                if (obj == null || !(obj instanceof FreeIndex))
                    break;
            }
            return next;
        }

        public boolean hasNext() {
            return (findNext(current) < items.size());
        }

        public Object next() {
            current = findNext(current);
            if (current == items.size())
                throw new NoSuchElementException("enumerated past end of FreeList");
            return items.elementAt(current);
        }

        public void remove() {
            del(current);
        }
    }
}

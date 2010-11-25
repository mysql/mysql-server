/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-05-16	Paul McCullagh
 *
 * H&G2JCtL
 *
 * C++ Utilities
 */

#ifndef __ccutils_xt_h__
#define __ccutils_xt_h__

#include <errno.h>

#include "xt_defs.h"
#include "thread_xt.h"

class XTObject
{
	private:
	u_int			o_refcnt;

	public:
	inline XTObject() { o_refcnt = 1; }
	
	virtual ~XTObject() { }

	inline void reference() {
		o_refcnt++;
	}

	inline void release(XTThreadPtr self) {
		ASSERT(o_refcnt > 0);
		o_refcnt--;
		if (o_refcnt == 0) {
			finalize(self);
			delete this;
		}
	}

	virtual XTObject *factory(XTThreadPtr self) {
		XTObject *new_obj;
		
		if (!(new_obj = new XTObject()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return new_obj;
	}

	virtual XTObject *clone(XTThreadPtr self) {
		XTObject *new_obj;
		
		new_obj = factory(self);
		new_obj->init(self, this);
		return new_obj;
	}

	virtual void init(XTThreadPtr self) { (void) self; }
	virtual void init(XTThreadPtr self, XTObject *obj) { (void) obj; init(self); }
	virtual void finalize(XTThreadPtr self) { (void) self; }
	virtual int compare(const void *key) { (void) key; return -1; }
};

class XTListImp
{
	protected:
	bool		li_referenced;
	u_int		li_item_count;
	XTObject	**li_items;

	public:
	inline XTListImp() : li_referenced(true), li_item_count(0), li_items(NULL) { }

	inline void setNonReferenced() { li_referenced = false; }

	void append(XTThreadPtr self, XTObject *info) {
		if (!xt_realloc(NULL, (void **) &li_items, (li_item_count + 1) * sizeof(void *))) {
			if (li_referenced)
				info->release(self);
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
			return;
		}
		li_items[li_item_count] = info;
		li_item_count++;
	}

	void insert(XTThreadPtr self, XTObject *info, u_int i) {
		if (!xt_realloc(NULL, (void **) &li_items, (li_item_count + 1) * sizeof(void *))) {
			if (li_referenced)
				info->release(self);
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
			return;
		}
		memmove(&li_items[i+1], &li_items[i], (li_item_count-i) * sizeof(XTObject *));
		li_items[i] = info;
		li_item_count++;
	}

	void addToFront(XTThreadPtr self, XTObject *info) {
		insert(self, info, 0);
	}

	/* Will sort! */
	void append(XTThreadPtr self, XTObject *info, void *key);

	inline bool remove(XTObject *info) {
		for (u_int i=0; i<li_item_count; i++) {
			if (li_items[i] == info) {
				li_item_count--;
				memmove(&li_items[i], &li_items[i+1], (li_item_count - i) * sizeof(XTObject *));
				return true;
			}
		}
		return false;
	}

	inline bool remove(XTThreadPtr self, u_int i) {
		XTObject *item;

		if (i >= li_item_count)
			return false;
		item = li_items[i];
		li_item_count--;
		memmove(&li_items[i], &li_items[i+1], (li_item_count - i) * sizeof(void *));
		if (li_referenced)
			item->release(self);
		return true;
	}

	inline XTObject *take(u_int i) {
		XTObject *item;

		if (i >= li_item_count)
			return NULL;
		item = li_items[i];
		li_item_count--;
		memmove(&li_items[i], &li_items[i+1], (li_item_count - i) * sizeof(void *));
		return item;
	}

	inline u_int size() const { return li_item_count; }

	inline void setEmpty(XTThreadPtr self) {
		if (li_items)
			xt_free(self, li_items);
		li_item_count = 0;
		li_items = NULL;
	}

	inline bool isEmpty() { return li_item_count == 0; }

	inline XTObject *itemAt(u_int i) const {
		if (i >= li_item_count)
			return NULL;
		return li_items[i];
	}
};


template <class T> class XTList : public XTListImp
{
	public:
	inline XTList() : XTListImp() { }

	inline void append(XTThreadPtr self, T *a) { XTListImp::append(self, a); }
	inline void insert(XTThreadPtr self, T *a, u_int i) { XTListImp::insert(self, a, i); }
	inline void addToFront(XTThreadPtr self, T *a) { XTListImp::addToFront(self, a); }

	inline bool remove(T *a) { return XTListImp::remove(a); }

	inline bool remove(XTThreadPtr self, u_int i) { return XTListImp::remove(self, i); }

	inline T *take(u_int i) { return (T *) XTListImp::take(i); }

	inline T *itemAt(u_int i) const { return (T *) XTListImp::itemAt(i); }

	inline u_int indexOf(T *a) {
		u_int i;

		for (i=0; i<size(); i++) {
			if (itemAt(i) == a)
				break;
		}
		return i;
	}

	void deleteAll(XTThreadPtr self)
	{
		for (u_int i=0; i<size(); i++) {
			if (li_referenced)
				itemAt(i)->release(self);
		}
		setEmpty(self);
	}

	void clone(XTThreadPtr self, XTListImp *list)
	{
		deleteAll(self);
		for (u_int i=0; i<list->size(); i++) {
			XTListImp::append(self, list->itemAt(i)->clone(self));
		}
	}
};

#endif

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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2004-01-03	Paul McCullagh
 *
 * H&G2JCtL
 *
 * Implementation of the PBXT internal data dictionary.
 */

#ifndef __datadic_xt_h__
#define __datadic_xt_h__

#include <stddef.h>
#include <limits.h>

#include "ccutils_xt.h"
#include "util_xt.h"

struct XTDatabase;
struct XTTable;
struct XTIndex;
struct XTOpenTable;
struct XTIndex;

/* Constraint types: */
#define XT_DD_UNKNOWN				((u_int) -1)
#define XT_DD_INDEX					0
#define XT_DD_INDEX_UNIQUE			1
#define XT_DD_KEY_PRIMARY			2
#define XT_DD_KEY_FOREIGN			3

#define XT_KEY_ACTION_DEFAULT		0
#define XT_KEY_ACTION_RESTRICT		1
#define XT_KEY_ACTION_CASCADE		2
#define XT_KEY_ACTION_SET_NULL		3
#define XT_KEY_ACTION_SET_DEFAULT	4
#define XT_KEY_ACTION_NO_ACTION		5		/* Like RESTRICT, but check at end of statement. */ 

class XTDDEnumerableColumn;
class XTDDColumnFactory;

class XTDDColumn : public XTObject {

protected:

	XTDDColumn() : XTObject(),
		dc_name(NULL),
		dc_data_type(NULL),
		dc_null_ok(true),
		dc_auto_inc(false) {
	}

public:
	char	*dc_name;
	char	*dc_data_type;
	bool	dc_null_ok;
	bool	dc_auto_inc;

	virtual XTObject *factory(XTThreadPtr self) {
		XTObject *new_obj;
		
		if (!(new_obj = new XTDDColumn()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return new_obj;
	}

	virtual void init(XTThreadPtr self) { 
		XTObject::init(self);
	}
	virtual void init(XTThreadPtr self, XTObject *obj);
	virtual void finalize(XTThreadPtr self);
	virtual void loadString(XTThreadPtr self, XTStringBufferPtr sb);

	virtual XTDDEnumerableColumn *castToEnumerable() { 
		return NULL;
	}

	friend class XTDDColumnFactory;
};

/*
 * subclass for ENUMs and SETs
 */
class XTDDEnumerableColumn : public XTDDColumn {

protected:
	XTDDEnumerableColumn() : XTDDColumn(), 
		enum_size(0), is_enum(0) {
	}

public:
	int enum_size;	/* number of elements in the ENUM or SET */
	xtBool is_enum;	/* TRUE if this is ENUM, FALSE if SET */

	virtual XTObject *factory(XTThreadPtr self) {
		XTObject *new_obj;
		
		if (!(new_obj = new XTDDEnumerableColumn()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return new_obj;
	}

	virtual XTDDEnumerableColumn *castToEnumerable() { 
		return this;
	}

	friend class XTDDColumnFactory;
};

class XTDDColumnRef : public XTObject {
	public:
	char					*cr_col_name;

	XTDDColumnRef() : XTObject(), cr_col_name(NULL) { }

	virtual XTObject *factory(XTThreadPtr self) {
		XTObject *new_obj;
		
		if (!(new_obj = new XTDDColumnRef()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return new_obj;
	}

	virtual void init(XTThreadPtr self) { XTObject::init(self); }
	virtual void init(XTThreadPtr self, XTObject *obj);
	virtual void finalize(XTThreadPtr self);
};

class XTDDConstraint : public XTObject {
	public:
	class XTDDTable			*co_table;								/* The table of this constraint (non-referenced). */
	u_int					co_type;
	char					*co_name;
	char					*co_ind_name;
	XTList<XTDDColumnRef>	co_cols;

	XTDDConstraint(u_int t) : XTObject(),
		co_table(NULL),
		co_type(t),
		co_name(NULL),
		co_ind_name(NULL) {
	}

	virtual void init(XTThreadPtr self) { XTObject::init(self); }
	virtual void init(XTThreadPtr self, XTObject *obj);
	virtual void finalize(XTThreadPtr self) {
		if (co_name)
			xt_free(self, co_name);
		if (co_ind_name)
			xt_free(self, co_ind_name);
		co_cols.deleteAll(self);
		XTObject::finalize(self);
	}
	virtual void loadString(XTThreadPtr self, XTStringBufferPtr sb);
	virtual void alterColumnName(XTThreadPtr self, char *from_name, char *to_name);
	void getColumnList(char *buffer, size_t size);
	bool sameColumns(XTDDConstraint *co);
	bool samePrefixColumns(XTDDConstraint *co);
	bool attachColumns();
};

class XTDDTableRef : public XTObject {
	public:
	class XTDDTableRef		*tr_next;								/* The next reference in the list. */
	class XTDDForeignKey	*tr_fkey;								/* The foreign key that references this table (if not-NULL). */

	XTDDTableRef() : XTObject(), tr_next(NULL), tr_fkey(NULL) { }
	virtual void finalize(XTThreadPtr self);
	bool modifyRow(struct XTOpenTable *tab, xtWord1 *before, xtWord1 *after, XTThreadPtr thread);
	bool checkReference(xtWord1 *before, XTThreadPtr thread);
	void deleteAllRows(XTThreadPtr self);
};

class XTDDIndex : public XTDDConstraint {	
	public:
	u_int					in_index;

	XTDDIndex(u_int type) : XTDDConstraint(type), in_index((u_int) -1) { }

	virtual XTObject *factory(XTThreadPtr self) {
		XTObject *new_obj;
		
		if (!(new_obj = new XTDDIndex(XT_DD_UNKNOWN)))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return new_obj;
	}

        virtual void init(XTThreadPtr self) { XTDDConstraint::init(self); };
	virtual void init(XTThreadPtr self, XTObject *obj);
	struct XTIndex *getIndexPtr();
};

/*
 * A foreign key is based on a local index.
 */
class XTDDForeignKey : public XTDDIndex {
	public:
	XTPathStrPtr			fk_ref_tab_name;
	XTDDTable				*fk_ref_table;
	u_int					fk_ref_index;							/* The index on which this foreign key references. */
	XTList<XTDDColumnRef>	fk_ref_cols;
	int						fk_on_delete;
	int						fk_on_update;

	XTDDForeignKey() : XTDDIndex(XT_DD_KEY_FOREIGN),
		fk_ref_tab_name(NULL),
		fk_ref_table(NULL),
		fk_ref_index(UINT_MAX),
		fk_on_delete(0),
		fk_on_update(0) {
	}

	virtual XTObject *factory(XTThreadPtr self) {
		XTObject *new_obj;
		
		if (!(new_obj = new XTDDForeignKey()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return new_obj;
	}

        virtual void init(XTThreadPtr self) { XTDDIndex::init(self); }
	virtual void init(XTThreadPtr self, XTObject *obj);
	virtual void finalize(XTThreadPtr self);
	virtual void loadString(XTThreadPtr self, XTStringBufferPtr sb);
	void getReferenceList(char *buffer, size_t size);
	struct XTIndex *getReferenceIndexPtr();
	bool sameReferenceColumns(XTDDConstraint *co);
	bool samePrefixReferenceColumns(XTDDConstraint *co);
	bool checkReferencedTypes(XTDDTable *dt);
	void removeReference(XTThreadPtr self);
	bool insertRow(xtWord1 *before, xtWord1 *after, XTThreadPtr thread);
	bool updateRow(xtWord1 *before, xtWord1 *after, XTThreadPtr thread);

	static const char *actionTypeToString(int action);
};

class XTDDTable : public XTObject {
	private:

	public:
	struct XTTable			*dt_table;

	XTList<XTDDColumn>		dt_cols;
	XTList<XTDDIndex>		dt_indexes;

	xt_rwlock_type			dt_ref_lock;			/* The lock for adding and using references. */
	XTList<XTDDForeignKey>	dt_fkeys;				/* The foreign keys on this table. */
	XTDDTableRef			*dt_trefs;				/* A list of tables that reference this table. */

	virtual XTObject *factory(XTThreadPtr self) {
		XTObject *new_obj;
		
		if (!(new_obj = new XTDDTable()))
			xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return new_obj;
	}

	virtual void init(XTThreadPtr self);
	virtual void init(XTThreadPtr self, XTObject *obj);
	virtual void finalize(XTThreadPtr self);

	XTDDColumn *findColumn(char *name);
	void loadString(XTThreadPtr self, XTStringBufferPtr sb);
	void loadForeignKeyString(XTThreadPtr self, XTStringBufferPtr sb);
	void checkForeignKeyReference(XTThreadPtr self, XTDDForeignKey *fk);
	void attachReferences(XTThreadPtr self, struct XTDatabase *db);
	void attachReference(XTThreadPtr self, XTDDForeignKey *fk);
	void alterColumnName(XTThreadPtr self, char *from_name, char *to_name);
	void attachReference(XTThreadPtr self, XTDDTable *dt);
	void removeReferences(XTThreadPtr self);
	void removeReference(XTThreadPtr self, XTDDForeignKey *fk);
	void checkForeignKeys(XTThreadPtr self, bool temp_table);
	XTDDIndex *findIndex(XTDDConstraint *co);
	XTDDIndex *findReferenceIndex(XTDDForeignKey *fk);
	bool insertRow(struct XTOpenTable *rec_ot, xtWord1 *buffer);
	bool checkNoAction(struct XTOpenTable *ot, xtRecordID rec_id);
	xtBool checkCanDrop(xtBool drop_db);
	bool deleteRow(struct XTOpenTable *rec_ot, xtWord1 *buffer);
	void deleteAllRows(XTThreadPtr self);
	bool updateRow(struct XTOpenTable *rec_ot, xtWord1 *before, xtWord1 *after);
};

XTDDTable *xt_ri_create_table(XTThreadPtr self, bool convert, XTPathStrPtr tab_path, char *sql, XTDDTable *my_tab);

#endif

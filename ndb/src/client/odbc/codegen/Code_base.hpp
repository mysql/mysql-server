/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ODBC_CODEGEN_Code_base_hpp
#define ODBC_CODEGEN_Code_base_hpp

#include <set>
#include <list>
#include <vector>
#include <common/common.hpp>
#include <common/CodeTree.hpp>
#include <common/DescArea.hpp>

class Ctx;
class ConnArea;
class StmtArea;
class DescArea;
class DictCatalog;
class DictSchema;
class ResultArea;
class ResultSet;
class SpecRow;
class Ndb;
class NdbSchemaCon;
class NdbConnection;
class NdbOperation;
class NdbScanFilter;

class Plan_root;
class Plan_table;
class Plan_column;
class Plan_expr;
class Plan_expr_param;
class Plan_pred;
class Plan_dml_row;
class Plan_dml_column;
class Plan_ddl_column;
class Plan_ddl_constr;
class Plan_idx_column;
class Exec_root;
class Exec_base;
class Exec_query;
class Exec_expr;
class Exec_expr_row;
class Exec_expr_param;

/**
 * @class Plan_base
 * @brief Base class for plan trees
 */
class Plan_base : public PlanTree {
public:
    Plan_base(Plan_root* root);
    virtual ~Plan_base() = 0;
    // get references to StmtArea via Plan_root
    StmtArea& stmtArea() const;
    DescArea& descArea(DescUsage u) const;
    ConnArea& connArea() const;
    // catalogs
    DictCatalog& dictCatalog() const;
    DictSchema& dictSchema() const;
    // ndb
    Ndb* ndbObject() const;
    NdbSchemaCon* ndbSchemaCon() const;
    NdbConnection* ndbConnection() const;
    // containers for Plan classes
    typedef std::vector<Plan_table*> TableVector;
    typedef std::vector<Plan_column*> ColumnVector;
    typedef std::vector<Plan_dml_column*> DmlColumnVector;
    typedef std::vector<Plan_ddl_column*> DdlColumnVector;
    typedef std::vector<Plan_ddl_constr*> DdlConstrVector;
    typedef std::vector<Plan_idx_column*> IdxColumnVector;
    typedef std::vector<Plan_expr*> ExprVector;
    typedef std::list<Plan_expr*> ExprList;
    typedef std::vector<ExprList> ExprListVector;
    typedef std::list<Plan_pred*> PredList;
    typedef std::set<Plan_table*> TableSet;
    typedef std::vector<Plan_expr_param*> ParamVector;
    // control area on the stack  XXX needs to be designed
    struct Ctl {
	Ctl(Ctl* up);
	Ctl* m_up;			// up the stack
	// analyze
	TableVector m_tableList;	// resolve column names
	bool m_topand;			// in top-level where clause
	bool m_extra;			// anything but single pk=expr
	bool m_aggrok;			// aggregate allowed
	bool m_aggrin;			// within aggregate args
	bool m_const;			// only constants in set clause
	PredList m_topcomp;		// top level comparisons
	Plan_dml_row *m_dmlRow;		// row type to convert to
	Plan_table* m_topTable;		// top level table for interpreted progs
	bool m_having;			// in having-predicate
	// codegen
	Exec_root* m_execRoot;		// root of Exec tree
	const Exec_query* m_execQuery;	// pass to column
    };
    // semantic analysis and optimization
    virtual Plan_base* analyze(Ctx& ctx, Ctl& ctl) = 0;
    // generate "executable" code
    virtual Exec_base* codegen(Ctx& ctx, Ctl& ctl) = 0;
    // misc
    virtual void print(Ctx& ctx) = 0;
protected:
    Plan_root* m_root;
    void printList(Ctx& ctx, Plan_base* a[], unsigned n);
};

inline
Plan_base::Plan_base(Plan_root* root) :
    m_root(root)
{
    ctx_assert(m_root != 0);
}

inline
Plan_base::Ctl::Ctl(Ctl* up) :
    m_up(up),
    m_tableList(1),		// 1-based
    m_topand(false),
    m_extra(false),
    m_aggrok(false),
    m_aggrin(false),
    m_dmlRow(0),
    m_topTable(0),
    m_having(false),
    m_execRoot(0),
    m_execQuery(0)
{
}

/**
 * @class Exec_base
 * @brief Base class for exec trees
 */
class Exec_base : public ExecTree {
public:
    class Code : public ExecTree::Code {
    public:
	virtual ~Code() = 0;
    };
    class Data : public ExecTree::Data {
    public:
	virtual ~Data() = 0;
    };
    Exec_base(Exec_root* root);
    virtual ~Exec_base() = 0;
    // get references to StmtArea via Exec_root
    virtual StmtArea& stmtArea() const;
    DescArea& descArea(DescUsage u) const;
    ConnArea& connArea() const;
    // catalogs
    DictSchema& dictSchema() const;
    // ndb
    Ndb* ndbObject() const;
    NdbSchemaCon* ndbSchemaCon() const;
    NdbConnection* ndbConnection() const;
    // containers for Exec classes
    typedef std::vector<Exec_expr*> ExprVector;
    typedef std::vector<Exec_expr_param*> ParamVector;
    // control area on the stack
    struct Ctl {
	Ctl(Ctl* up);
	Ctl* m_up;			// up the stack
	const Exec_query* m_query;	// pass Data
	ExprVector m_exprList;		// pass Data
	NdbOperation* m_scanOp;		// scan operation
	bool m_postEval;		// for rownum
	unsigned m_groupIndex;		// for group by
	bool m_groupInit;		// first in group
	Exec_expr_row* m_sortRow;	// from sort to group by
	NdbScanFilter* m_scanFilter;	// scan filter
    };
    // allocate and deallocate Data instances
    virtual void alloc(Ctx& ctx, Ctl& ctl) = 0;
    virtual void close(Ctx& ctx) = 0;
    // set Code and Data
    void setCode(const Code& code);
    void setData(Data& data);
    // misc
    virtual void print(Ctx& ctx) = 0;
protected:
    const Code* m_code;
    Data* m_data;
    Exec_root* m_root;
    void printList(Ctx& ctx, Exec_base* a[], unsigned n);
};

inline
Exec_base::Exec_base(Exec_root* root) :
    m_code(0),
    m_data(0),
    m_root(root)
{
    ctx_assert(m_root != 0);
}

inline void
Exec_base::setCode(const Code& code)
{
    ctx_assert(m_code == 0);
    m_code = &code;
}

inline void
Exec_base::setData(Data& data)
{
    ctx_assert(m_data == 0);
    m_data = &data;
}

inline
Exec_base::Ctl::Ctl(Ctl* up) :
    m_up(up),
    m_scanOp(0),
    m_postEval(false),
    m_groupIndex(0),
    m_groupInit(false),
    m_sortRow(0),
    m_scanFilter(0)
{
}

#endif

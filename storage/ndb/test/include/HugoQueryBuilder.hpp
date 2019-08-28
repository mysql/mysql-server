/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef HUGO_QUERY_BUILDER_HPP
#define HUGO_QUERY_BUILDER_HPP

#include <NDBT.hpp>
#include <Vector.hpp>
#include "../../src/ndbapi/NdbQueryBuilder.hpp"

class HugoQueryBuilder {
public:

  typedef Uint64 OptionMask;

  /**
   * Options that affects what kind of query is built
   */
  enum QueryOption
  {
    /**
     * Query should be a lookup
     */
    O_LOOKUP = 0x1,

    /**
     * Query should be a scan
     */
    O_SCAN = 0x2,

    /**
     * Query might use primary key index
     */
    O_PK_INDEX = 0x4,

    /**
     * Query might use unique index
     */
    O_UNIQUE_INDEX = 0x8,

    /**
     * Query might use ordered index
     */
    O_ORDERED_INDEX = 0x10,

    /**
     * Query might table scan
     */
    O_TABLE_SCAN = 0x20,

    /**
     * Column referrences may also include grandparents (Default 'on')
     */
    O_GRANDPARENT = 0x100
  };
  static const OptionMask OM_RANDOM_OPTIONS = 
       (OptionMask)(O_PK_INDEX | O_UNIQUE_INDEX | O_ORDERED_INDEX | O_TABLE_SCAN | O_GRANDPARENT);

  HugoQueryBuilder(Ndb* ndb, const NdbDictionary::Table**tabptr, 
                   OptionMask om = OM_RANDOM_OPTIONS)
  : m_ndb(ndb)
  {
    init();
    for (; * tabptr != 0; tabptr++)
      addTable(*tabptr);
    setOptionMask(om);
    fixOptions();
  }
  HugoQueryBuilder(Ndb* ndb, const NdbDictionary::Table* tab, QueryOption o)
  : m_ndb(ndb)
  {
    init();
    addTable(tab);
    setOption(o);
    fixOptions();
  }
  virtual ~HugoQueryBuilder();

  void setMinJoinLevel(int level) { m_joinLevel[0] = level;}
  int getMinJoinLevel() const { return m_joinLevel[0];}
  void setMaxJoinLevel(int level) { m_joinLevel[1] = level;}
  int getMaxJoinLevel() const { return m_joinLevel[1];}

  void setJoinLevel(int level) { setMinJoinLevel(level);setMaxJoinLevel(level);}
  int getJoinLevel() const;

  void addTable(const NdbDictionary::Table*);
  void removeTable(const NdbDictionary::Table*);

  void setOption(QueryOption o) { m_options |= (OptionMask)o;}
  void clearOption(QueryOption o) const { m_options &= ~(OptionMask)o;}
  bool testOption(QueryOption o) const { return (m_options & o) != 0;}

  OptionMask getOptionMask() const { return m_options;}
  void setOptionMask(OptionMask om) { m_options = om;}

  const NdbQueryDef * createQuery(bool takeOwnership = false);

private:
  const Ndb* m_ndb;

  struct TableDef
  {
    const NdbDictionary::Table * m_table;
    Vector<const NdbDictionary::Index*> m_unique_indexes;
    Vector<const NdbDictionary::Index*> m_ordered_indexes;
  };

  void init();
  mutable OptionMask m_options;
  int m_joinLevel[2]; // min/max
  Vector<TableDef> m_tables;
  Vector<const NdbQueryDef*> m_queries;

  // get random table
  struct TableDef getTable() const;

  struct OpIdx
  {
    NdbQueryOperationDef::Type m_type;
    const NdbDictionary::Table * m_table;
    const NdbDictionary::Index * m_index;
  };
  OpIdx getOp() const;

  struct Op
  {
    int m_parent;
    int m_idx;
    const NdbQueryOperationDef * m_op;
  };

  Vector<Op> m_query; // Query built sofar

  /**
   * Check if all column in cols can be bound to a column in tables in
   *   ops
   */
  static bool checkBindable(Vector<const NdbDictionary::Column*> cols,
                            Vector<Op> ops,
                            bool allow_bind_nullable);

  Vector<Op> getParents(OpIdx); //
  NdbQueryOperand * createLink(NdbQueryBuilder&, const NdbDictionary::Column*,
                               Vector<Op> & parents,
                               bool allow_bind_nullable);
  const NdbQueryOperationDef* createOp(NdbQueryBuilder&);

  void fixOptions();

  /**
   * We currently don't support busy-scan joins
   */
  bool checkBusyScan(Op) const;
  bool isAncestor(const Op& parent, const Op& child) const;

  friend NdbOut& operator<<(NdbOut& out, const HugoQueryBuilder::Op& op);
};

#endif

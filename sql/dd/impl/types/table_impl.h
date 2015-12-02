/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__TABLE_IMPL_INCLUDED
#define DD__TABLE_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/os_specific.h"               // DD_HEADER_BEGIN
#include "dd/impl/types/abstract_table_impl.h" // dd::Abstract_table_impl
#include "dd/types/dictionary_object_table.h"  // dd::Dictionary_object_table
#include "dd/types/table.h"                    // dd:Table

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Table_impl : virtual public Abstract_table_impl,
                   virtual public Table
{
public:
  typedef Collection<Index> Index_collection;
  typedef Collection<Foreign_key> Foreign_key_collection;
  typedef Collection<Partition> Partition_collection;

public:
  Table_impl();
  virtual ~Table_impl()
  { }

public:
  /////////////////////////////////////////////////////////////////////////
  // enum_table_type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_table_type type() const
  { return TT_BASE_TABLE; }

public:
  virtual const Dictionary_object_table &object_table() const
  { return Table::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx);

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(WriterVariant *wv) const;

  void deserialize(const RJ_Document *d);

  virtual void debug_print(std::string &outb) const;

public:
  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const
  { return m_collation_id; }

  virtual void set_collation_id(Object_id collation_id)
  { m_collation_id= collation_id; }

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const
  { return m_tablespace_id; }

  virtual void set_tablespace_id(Object_id tablespace_id)
  { m_tablespace_id= tablespace_id; }

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &engine() const
  { return m_engine; }

  virtual void set_engine(const std::string &engine)
  { m_engine= engine; }

  /////////////////////////////////////////////////////////////////////////
  // comment
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const
  { return m_comment; }

  virtual void set_comment(const std::string &comment)
  { m_comment= comment; }

  /////////////////////////////////////////////////////////////////////////
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual bool hidden() const
  { return m_hidden; }

  virtual void set_hidden(bool hidden)
  { m_hidden= hidden; }

  /////////////////////////////////////////////////////////////////////////
  //se_private_data.
  /////////////////////////////////////////////////////////////////////////

  using Table::se_private_data;

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw);
  virtual void set_se_private_data(const Properties &se_private_data);

  /////////////////////////////////////////////////////////////////////////
  //se_private_id.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong se_private_id() const
  { return m_se_private_id; }

  virtual void set_se_private_id(ulonglong se_private_id)
  { m_se_private_id= se_private_id; }

  /////////////////////////////////////////////////////////////////////////
  // Partition type
  /////////////////////////////////////////////////////////////////////////

  virtual enum_partition_type partition_type() const
  { return m_partition_type; }

  virtual void set_partition_type(
    enum_partition_type partition_type)
  { m_partition_type= partition_type; }

  /////////////////////////////////////////////////////////////////////////
  // default_partitioning
  /////////////////////////////////////////////////////////////////////////

  virtual enum_default_partitioning default_partitioning() const
  { return m_default_partitioning; }

  virtual void set_default_partitioning(
    enum_default_partitioning default_partitioning)
  { m_default_partitioning= default_partitioning; }

  /////////////////////////////////////////////////////////////////////////
  // partition_expression
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &partition_expression() const
  { return m_partition_expression; }

  virtual void set_partition_expression(
    const std::string &partition_expression)
  { m_partition_expression= partition_expression; }

  /////////////////////////////////////////////////////////////////////////
  // subpartition_type
  /////////////////////////////////////////////////////////////////////////

  virtual enum_subpartition_type subpartition_type() const
  { return m_subpartition_type; }

  virtual void set_subpartition_type(
    enum_subpartition_type subpartition_type)
  { m_subpartition_type= subpartition_type; }

  /////////////////////////////////////////////////////////////////////////
  // default_subpartitioning
  /////////////////////////////////////////////////////////////////////////

  virtual enum_default_partitioning default_subpartitioning() const
  { return m_default_subpartitioning; }

  virtual void set_default_subpartitioning(
    enum_default_partitioning default_subpartitioning)
  { m_default_subpartitioning= default_subpartitioning; }

  /////////////////////////////////////////////////////////////////////////
  // subpartition_expression
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &subpartition_expression() const
  { return m_subpartition_expression; }

  virtual void set_subpartition_expression(
    const std::string &subpartition_expression)
  { m_subpartition_expression= subpartition_expression; }

  /////////////////////////////////////////////////////////////////////////
  // Index collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Index *add_index();

  virtual Index *add_first_index();

  virtual Index_const_iterator *indexes() const;

  virtual Index_iterator *indexes();

  virtual Index_const_iterator *user_indexes() const;

  virtual Index_iterator *user_indexes();

  const Index *get_index(Object_id index_id) const
  { return const_cast<Table_impl *> (this)->get_index(index_id); }

  Index *get_index(Object_id index_id);

  Index_collection *index_collection()
  { return m_indexes.get(); }

  /////////////////////////////////////////////////////////////////////////
  // Foreign key collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key *add_foreign_key();

  virtual Foreign_key_const_iterator *foreign_keys() const;

  virtual Foreign_key_iterator *foreign_keys();

  Foreign_key_collection *foreign_key_collection()
  { return m_foreign_keys.get(); }

  /////////////////////////////////////////////////////////////////////////
  // Partition collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition *add_partition();

  virtual Partition_const_iterator *partitions() const;

  virtual Partition_iterator *partitions();

  const Partition *get_partition(Object_id partition_id) const
  { return const_cast<Table_impl *> (this)->get_partition(partition_id); }

  const Partition *get_partition_by_se_private_id(Object_id se_private_id) const;

  Partition *get_partition(Object_id partition_id);

  const Partition *get_last_partition() const;

  Partition_collection *partition_collection()
  { return m_partitions.get(); }

private:
  // Fields.

  bool m_hidden;

  ulonglong m_se_private_id;

  std::string m_engine;
  std::string m_comment;
  std::unique_ptr<Properties> m_se_private_data;

  // - Partitioning related fields.

  enum_partition_type           m_partition_type;
  std::string                   m_partition_expression;
  enum_default_partitioning     m_default_partitioning;

  enum_subpartition_type        m_subpartition_type;
  std::string                   m_subpartition_expression;
  enum_default_partitioning     m_default_subpartitioning;

  // References to tightly-coupled objects.

  std::unique_ptr<Index_collection> m_indexes;
  std::unique_ptr<Foreign_key_collection> m_foreign_keys;
  std::unique_ptr<Partition_collection> m_partitions;

  // References to other objects.

  Object_id m_collation_id;
  Object_id m_tablespace_id;

#ifndef DBUG_OFF
  Table_impl(const Table_impl &src);
  Table_impl *clone() const
  {
    return new Table_impl(*this);
  }
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////

class Table_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Table_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__TABLE_IMPL_INCLUDED

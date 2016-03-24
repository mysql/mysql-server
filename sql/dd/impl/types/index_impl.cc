/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/types/index_impl.h"

#include "mysqld_error.h"                       // ER_*

#include "dd/impl/collection_impl.h"            // Collection
#include "dd/impl/properties_impl.h"            // Properties_impl
#include "dd/impl/sdi_impl.h"                   // sdi read/write functions
#include "dd/impl/transaction_impl.h"           // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"             // Raw_record
#include "dd/impl/tables/indexes.h"             // Indexes
#include "dd/impl/tables/index_column_usage.h"  // Index_column_usage
#include "dd/impl/types/index_element_impl.h"   // Index_element_impl
#include "dd/impl/types/table_impl.h"           // Table_impl
#include "dd/types/column.h"                    // Column::name()

#include <sstream>


using dd::tables::Indexes;
using dd::tables::Index_column_usage;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Index implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Index::OBJECT_TABLE()
{
  return Indexes::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Index::TYPE()
{
  static Index_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Index_impl implementation.
///////////////////////////////////////////////////////////////////////////

Index_impl::Index_impl()
 :m_hidden(false),
  m_is_generated(false),
  m_ordinal_position(0),
  m_options(new (std::nothrow) Properties_impl()),
  m_se_private_data(new (std::nothrow) Properties_impl()),
  m_type(IT_MULTIPLE),
  m_algorithm(IA_BTREE),
  m_is_algorithm_explicit(false),
  m_table(NULL),
  m_elements(new (std::nothrow) Element_collection()),
  m_tablespace_id(INVALID_OBJECT_ID),
  m_user_elements_count_cache(INVALID_USER_ELEMENTS_COUNT)
{ }

///////////////////////////////////////////////////////////////////////////

Table &Index_impl::table()
{
  return *m_table;
}

///////////////////////////////////////////////////////////////////////////

bool Index_impl::set_options_raw(const std::string &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Index_impl::set_se_private_data(const Properties &se_private_data)
{ m_se_private_data->assign(se_private_data); }

///////////////////////////////////////////////////////////////////////////

bool Index_impl::set_se_private_data_raw(const std::string &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Index_impl::validate() const
{
  if (!m_table)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Index_impl::OBJECT_TABLE().name().c_str(),
             "No table object associated with this index.");
    return true;
  }
  if (m_engine.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Index_impl::OBJECT_TABLE().name().c_str(),
             "Engine name is not set.");
    return true;
  }

  if (m_elements->is_empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Index_impl::OBJECT_TABLE().name().c_str(),
             "The index has no elements.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Index_impl::restore_children(Open_dictionary_tables_ctx *otx)
{

  return m_elements->restore_items(
    // Column will be resolved in restore_attributes() called from
    // Collection::restore_items().
    Index_element_impl::Factory(this, NULL),
    otx,
    otx->get_table<Index_element>(),
    Index_column_usage::create_key_by_index_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Index_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_elements->store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Index_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  return m_elements->drop_items(
    otx,
    otx->get_table<Index_element>(),
    Index_column_usage::create_key_by_index_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Index_impl::restore_attributes(const Raw_record &r)
{
  if (check_parent_consistency(m_table,
                               r.read_ref_id(Indexes::FIELD_TABLE_ID)))
    return true;

  restore_id(r, Indexes::FIELD_ID);
  restore_name(r, Indexes::FIELD_NAME);

  m_hidden=          r.read_bool(Indexes::FIELD_HIDDEN);
  m_is_generated=    r.read_bool(Indexes::FIELD_IS_GENERATED);
  m_ordinal_position=r.read_uint(Indexes::FIELD_ORDINAL_POSITION);
  m_comment=         r.read_str(Indexes::FIELD_COMMENT);

  m_type= (enum_index_type) r.read_int(Indexes::FIELD_TYPE);
  m_algorithm= (enum_index_algorithm) r.read_int(Indexes::FIELD_ALGORITHM);
  m_is_algorithm_explicit= r.read_bool(Indexes::FIELD_IS_ALGORITHM_EXPLICIT);

  m_tablespace_id= r.read_ref_id(Indexes::FIELD_TABLESPACE_ID);

  set_options_raw(r.read_str(Indexes::FIELD_OPTIONS, ""));
  set_se_private_data_raw(r.read_str(Indexes::FIELD_SE_PRIVATE_DATA, ""));

  m_engine= r.read_str(Indexes::FIELD_ENGINE);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Index_impl::store_attributes(Raw_record *r)
{
  //
  // Special cases dealing with NULL values for nullable fields
  //   - Store NULL if tablespace id is not set
  //     Eg: A non-innodb table may not have tablespace
  //   - Store NULL if se_private_id is not set
  //     Eg: A non-innodb table may not have se_private_id
  //   - Store NULL in options if there are no key=value pairs
  //   - Store NULL in se_private_data if there are no key=value pairs

  return store_id(r, Indexes::FIELD_ID) ||
         store_name(r, Indexes::FIELD_NAME) ||
         r->store(Indexes::FIELD_TABLE_ID, m_table->id()) ||
         r->store(Indexes::FIELD_TYPE, m_type) ||
         r->store(Indexes::FIELD_ALGORITHM, m_algorithm) ||
         r->store(Indexes::FIELD_IS_ALGORITHM_EXPLICIT, m_is_algorithm_explicit) ||
         r->store(Indexes::FIELD_IS_GENERATED, m_is_generated) ||
         r->store(Indexes::FIELD_HIDDEN, m_hidden) ||
         r->store(Indexes::FIELD_ORDINAL_POSITION, m_ordinal_position) ||
         r->store(Indexes::FIELD_COMMENT, m_comment) ||
         r->store(Indexes::FIELD_OPTIONS, *m_options) ||
         r->store(Indexes::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
         r->store_ref_id(Indexes::FIELD_TABLESPACE_ID, m_tablespace_id) ||
         r->store(Indexes::FIELD_ENGINE, m_engine);
}

///////////////////////////////////////////////////////////////////////////
static_assert(Indexes::FIELD_ENGINE==13,
              "Indexes definition has changed, review (de)ser memfuns!");
void
Index_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  Entity_object_impl::serialize(wctx, w);

  write(w, m_hidden, STRING_WITH_LEN("hidden"));
  write(w, m_is_generated, STRING_WITH_LEN("is_generated"));
  write(w, m_ordinal_position, STRING_WITH_LEN("ordinal_position"));
  write(w, m_comment, STRING_WITH_LEN("comment"));

  write_properties(w, m_options, STRING_WITH_LEN("options"));
  write_properties(w, m_se_private_data, STRING_WITH_LEN("se_private_data"));
  write_enum(w, m_type, STRING_WITH_LEN("type"));
  write_enum(w, m_algorithm, STRING_WITH_LEN("algorithm"));
  write(w, m_is_algorithm_explicit, STRING_WITH_LEN("is_algorithm_explicit"));
  write(w, m_engine, STRING_WITH_LEN("engine"));

  serialize_each(wctx, w, m_elements.get(), STRING_WITH_LEN("elements"));

  serialize_tablespace_ref(wctx, w, m_tablespace_id,
                           STRING_WITH_LEN("tablespace_ref"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Index_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  Entity_object_impl::deserialize(rctx, val);

  read(&m_hidden, val, "hidden");
  read(&m_is_generated, val, "is_generated");
  read(&m_ordinal_position, val, "ordinal_position");
  read(&m_comment, val, "comment");
  read_properties(&m_options, val, "options");
  read_properties(&m_se_private_data, val, "se_private_data");
  read_enum(&m_type, val, "type");
  read_enum(&m_algorithm, val, "algorithm");
  read(&m_is_algorithm_explicit, val, "is_algorithm_explicit");
  read(&m_engine, val, "engine");

  deserialize_each(rctx, [this] () { return add_element(nullptr); },
                   val, "elements");

  if (deserialize_tablespace_ref(rctx, &m_tablespace_id, val,
                                 "tablespace_name"))
  {
    return true;
  }

  track_object(rctx, this);

  return false;
}

///////////////////////////////////////////////////////////////////////////

void Index_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "INDEX OBJECT: { "
    << "id: {OID: " << id() << "}; "
    << "m_table: {OID: " << m_table->id() << "}; "
    << "m_name: " << name() << "; "
    << "m_type: " << m_type << "; "
    << "m_algorithm: " << m_algorithm << "; "
    << "m_is_algorithm_explicit: " << m_is_algorithm_explicit << "; "
    << "m_is_generated: " << m_is_generated << "; "
    << "m_comment: " << m_comment << "; "
    << "m_hidden: " << m_hidden << "; "
    << "m_ordinal_position: " << m_ordinal_position << "; "
    << "m_options " << m_options->raw_string() << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_tablespace {OID: " << m_tablespace_id << "}; "
    << "m_engine: "<< m_engine << "; "
    << "m_elements: " << m_elements->size()
    << " [ ";

  {
    std::unique_ptr<Index_element_const_iterator> it(elements());

    while (true)
    {
      const Index_element *c= it->next();

      if (!c)
        break;

      std::string ob;
      c->debug_print(ob);
      ss << ob;
    }
  }
  ss << "] ";

  ss << " }";

  outb= ss.str();
}

/////////////////////////////////////////////////////////////////////////

void Index_impl::drop()
{
  m_table->index_collection()->remove(this);
}

/////////////////////////////////////////////////////////////////////////

Index_element *Index_impl::add_element(Column *c)
{
  invalidate_user_elements_count_cache();
  return m_elements->add(Index_element_impl::Factory(this, c));
}

/////////////////////////////////////////////////////////////////////////

Index_element *Index_impl::add_element(const Index_element &e)
{
  invalidate_user_elements_count_cache();
  return m_elements->add(Index_element_impl::Factory_clone(this, e));
}

///////////////////////////////////////////////////////////////////////////

Index_element_const_iterator *Index_impl::elements() const
{
  return m_elements->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

Index_element_iterator *Index_impl::elements()
{
  return m_elements->iterator();
}

///////////////////////////////////////////////////////////////////////////

Index_element_const_iterator *Index_impl::user_elements() const
{
  return m_elements->const_iterator(Collection<Index_element>::SKIP_HIDDEN_ITEMS);
}

///////////////////////////////////////////////////////////////////////////

Index_element_iterator *Index_impl::user_elements()
{
  return m_elements->iterator(Collection<Index_element>::SKIP_HIDDEN_ITEMS);
}

///////////////////////////////////////////////////////////////////////////

uint Index_impl::user_elements_count() const
{

  if (m_user_elements_count_cache == INVALID_USER_ELEMENTS_COUNT)
  {
    uint count= 0;
    std::unique_ptr<Index_element_const_iterator> it(user_elements());

    while ((it->next()!= NULL))
      count++;
    m_user_elements_count_cache= count;
  }

  return m_user_elements_count_cache;
}

///////////////////////////////////////////////////////////////////////////
// Index_impl::Factory implementation.
///////////////////////////////////////////////////////////////////////////

Collection_item *Index_impl::Factory::create_item() const
{
  Index_impl *i= new (std::nothrow) Index_impl();
  i->m_table= m_table;
  return i;
}

///////////////////////////////////////////////////////////////////////////

Index_impl::Index_impl(const Index_impl &src, Table_impl *parent)
  : Weak_object(src), Entity_object_impl(src), m_hidden(src.m_hidden),
    m_is_generated(src.m_is_generated),
    m_ordinal_position(src. m_ordinal_position),
    m_comment(src.m_comment),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_type(src.m_type), m_algorithm(src.m_algorithm),
    m_is_algorithm_explicit(src.m_is_algorithm_explicit),
    m_engine(src.m_engine),
    m_table(parent), m_elements(new Element_collection()),
    m_tablespace_id(src.m_tablespace_id),
    m_user_elements_count_cache(src.m_user_elements_count_cache)
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_elements->aref().end();
  m_elements->aref().reserve(src.m_elements->size());
  for (i_type i= src.m_elements->aref().begin(); i != end; ++i)
  {
    Column *dstcol= NULL;
    const Column &srccol= dynamic_cast<Index_element_impl*>(*i)->column();
    dstcol= parent->get_column(srccol.name());
    m_elements->aref().push_back(dynamic_cast<Index_element_impl*>(*i)->
                                 clone(this, dstcol));
  }
}

///////////////////////////////////////////////////////////////////////////
// Index_type implementation.
///////////////////////////////////////////////////////////////////////////

void Index_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Indexes>();

  otx->register_tables<Index_element>();
}

///////////////////////////////////////////////////////////////////////////

}

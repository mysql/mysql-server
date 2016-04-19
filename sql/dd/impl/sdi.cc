/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/sdi.h"

#include "handler.h"              // ha_resolve_by_name_raw
#include "m_string.h"             // STRING_WITH_LEN
#include "sql_class.h"            // THD

#include "dd/dd.h"                      // dd::create_object
#include "dd/dd_tablespace.h"           // dd::get_tablespace_name
#include "dd/sdi_file.h"                // dd::sdi_file::store
#include "dd/sdi_tablespace.h"          // dd::sdi_tablespace::store
#include "dd/cache/dictionary_client.h" // dd::Dictionary_client
#include "dd/impl/dictionary_impl.h"    // dd::Dictionary_impl::get_target_dd_version
#include "dd/impl/sdi_impl.h"           // sdi read/write functions
#include "dd/impl/sdi_utils.h"          // dd::checked_return
#include "dd/types/column.h"            // dd::Column
#include "dd/types/index.h"             // dd::Index
#include "dd/types/object_type.h"       // dd::create_object needs this
#include "dd/types/schema.h"            // dd::Schema
#include "dd/types/table.h"             // dd::Table
#include "dd/types/tablespace.h"        // dd::Tablespace

#include <rapidjson/document.h>     // rapidjson::GenericValue
#include <rapidjson/prettywriter.h> // rapidjson::PrettyWriter

#include <iostream>

/**
  @defgroup sdi Serialized Dictionary Information
  @ingroup Runtime_Environment
  @{
  Code to serialize and deserialize data dictionary objects, and for
  storing and retrieving the serialized representation from files or
  tablespaces.

  @file
  Definition of all sdi functions, except those that are -
  (de)serialize() member function in data dictionary objects -
  function templates which are defined in sdi_impl.h

  The file is made up of 4 groups:
  - @ref sdi_cc_internal
  - @ref sdi_internal
  - @ref sdi_api
  - @ref sdi_ut
  @}
*/

/**
  @defgroup sdi_cc_internal TU-internal definitions
  @ingroup sdi
  @{
  Functions and classes internal to the
  translation unit in the anonymous namespace.
*/

using namespace dd::sdi_utils;

namespace {
const std::string empty_= "";

char *generic_buf_handle(Byte_buffer *buf, size_t sz)
{
  if (buf->reserve(sz))
  {
    DBUG_ASSERT(false);
    return nullptr;
  }
  return &(*(buf->begin()));
}

}

/** @} */ // sdi_cc_internal


namespace dd {
/**
  @defgroup sdi_internal SDI Internal
  @ingroup sdi

  Objects internal to sdi-creation, and not callable from general server code.
*/

/**
  Opaque context which keeps reusable resources needed during
  serialization.
*/

class Sdi_wcontext
{
  /** A reusable byte buffer for e.g. base64 encoding. */
  Byte_buffer m_buf;
  /** Thread context */
  THD *m_thd;
  /** Pointer to schema name to use for schema references in SDI */
  const std::string *m_schema_name;

  /** Flag indicating that an error has occured */
  bool m_error;

  friend char *buf_handle(Sdi_wcontext *wctx, size_t sz);

  friend const std::string &lookup_schema_name(Sdi_wcontext *wctx);

  friend const std::string &lookup_tablespace_name(Sdi_wcontext *wctx,
                                                   dd::Object_id id);

public:
  Sdi_wcontext(THD *thd, const std::string *schema_name) :
    m_thd(thd), m_schema_name(schema_name), m_error(false) {}

  bool error() const
  {
    return m_error;
  }
};


char *buf_handle(Sdi_wcontext *wctx, size_t sz)
{
  return generic_buf_handle(&wctx->m_buf, sz);
}


const std::string &lookup_schema_name(Sdi_wcontext *wctx)
{
  return *wctx->m_schema_name;
}

static constexpr std::uint64_t sdi_version= 1;
template <typename T>
std::string generic_serialize(THD *thd, const char *dd_object_type,
                              size_t dd_object_type_size, const T &dd_obj,
                              const std::string *schema_name)
{
  dd::Sdi_wcontext wctx(thd, schema_name);
  dd::RJ_StringBuffer buf;
  dd::Sdi_writer w(buf);

  w.StartObject();
  w.String(STRING_WITH_LEN("sdi_version"));
  w.Uint64(sdi_version);
  w.String(STRING_WITH_LEN("dd_version"));
  w.Uint(Dictionary_impl::get_target_dd_version());
  w.String(STRING_WITH_LEN("dd_object_type"));
  w.String(dd_object_type, dd_object_type_size);
  w.String(STRING_WITH_LEN("dd_object"));
  dd_obj.serialize(&wctx, &w);
  w.EndObject();

  return (wctx.error() ? empty_ : std::string(buf.GetString(), buf.GetSize()));
}


const std::string &lookup_tablespace_name(Sdi_wcontext *wctx, dd::Object_id id)
{
  if (wctx->m_thd == nullptr || id == INVALID_OBJECT_ID)
  {
    return empty_;
  }

  // FIXME: Need to
  // - Check if id already in wctx cache
  // - if not; acquire_uncached, store the (id, name) pair in wctx,
  // - return reference to name corresponding to id in wctx

  dd::cache::Dictionary_client *dc= wctx->m_thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);
  const Tablespace *tsp= nullptr;
  if (dc->acquire(id, &tsp))
  {
    wctx->m_error= true;
    return empty_;
  }
  DBUG_ASSERT(tsp != nullptr);

  return tsp->name();
}

/**
  Opaque context which keeps reusable resoureces needed during
  deserialization.
*/

class Sdi_rcontext
{
  /** A reusable byte buffer for e.g. base64 decoding. */
  Byte_buffer buf;

  /** Column objects created during deserialization */
  dd_vector<Column*> m_column_object_opx;

  /** Index objects created during deserialization */
  dd_vector<Index*> m_index_object_opx;

  /** Thread context */
  THD *m_thd;

  /** Target dd version from SDI */
  uint m_target_dd_version;

  /** Sdi version from SDI */
  std::uint64_t m_sdi_version;

  /** Flag indicating that an error has occured */
  bool m_error;

  friend void track_object(Sdi_rcontext *rctx, Column *column_object);
  friend void track_object(Sdi_rcontext *rctx, Index *index_object);

  friend void lookup_opx_reference(Sdi_rcontext *rctx, Index** index_var, uint opx);
  friend void lookup_opx_reference(Sdi_rcontext *rctx, Column** column_var, uint opx);

  friend char *buf_handle(Sdi_rcontext *rctx, size_t sz);

  friend bool lookup_schema_ref(Sdi_rcontext *rctx,
                                const std::string &name, dd::Object_id *idp);
  friend bool lookup_tablespace_ref(Sdi_rcontext *rctx,
                                    const std::string &name, Object_id *idp);

public:
  Sdi_rcontext(THD *thd, uint target_dd_version, std::uint64_t sdi_version) :
    m_thd(thd),
    m_target_dd_version(target_dd_version),
    m_sdi_version(sdi_version),
    m_error(false)
  {}

  bool error() const
  {
    return m_error;
  }
};


template <typename T>
void generic_track_object(dd_vector<T*> *tvp, T *t)
{
  DBUG_ASSERT(t->ordinal_position() > 0);
  uint opx= t->ordinal_position()-1;
  dd_vector<T*> &tv= *tvp;

  if (opx >= tv.size())
  {
    tv.resize(opx+1);
  }
  tv[opx]= t;
}

void track_object(Sdi_rcontext *sdictx, Column *column_object)
{
  generic_track_object(&sdictx->m_column_object_opx, column_object);
}


void track_object(Sdi_rcontext *sdictx, Index *index_object)
{
  generic_track_object(&sdictx->m_index_object_opx, index_object);
}


void lookup_opx_reference(dd::Sdi_rcontext *sdictx, dd::Column **column_var,
                          uint opx)
{
  *column_var= sdictx->m_column_object_opx[opx];
}


void lookup_opx_reference(dd::Sdi_rcontext *sdictx, dd::Index **index_var,
                          uint opx)
{
  *index_var= sdictx->m_index_object_opx[opx];
}


char *buf_handle(Sdi_rcontext *rctx, size_t sz)
{
  return generic_buf_handle(&rctx->buf, sz);
}


template <typename T>
bool generic_lookup_ref(THD *thd, MDL_key::enum_mdl_namespace mdlns,
                        const std::string &name, dd::Object_id *idp)
{
  if (thd == nullptr)
  {
    return false;
  }

  // Acquire MDL here so that it becomes possible to acquire the
  // schema to look up its id in the current DD
  if (mdl_lock(thd, mdlns, name, "", MDL_INTENTION_EXCLUSIVE))
  {
    return true;
  }

  dd::cache::Dictionary_client *dc= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);

  const T *p= nullptr;
  // TODO: Split in two. Use error flag in ctx object
  if (dc->acquire(name, &p) || p == nullptr)
  {
    return true;
  }
  *idp= p->id();
  return false;
}


bool lookup_schema_ref(Sdi_rcontext *sdictx, const std::string &name,
                       dd::Object_id *idp)
{
  return generic_lookup_ref<Schema>(sdictx->m_thd, MDL_key::SCHEMA, name, idp);
}

bool lookup_tablespace_ref(Sdi_rcontext *sdictx, const std::string &name,
                           Object_id *idp)
{
  return generic_lookup_ref<Tablespace>(sdictx->m_thd, MDL_key::TABLESPACE,
                                        name, idp);
}

/** @} */ // sdi_cc_internal


/**
  @defgroup sdi_api SDI API
  @ingroup sdi

  Definition of externally visible functions and classes, declared in sdi.h
  @{
*/

sdi_t serialize(const Schema &schema)
{
  return generic_serialize(nullptr, STRING_WITH_LEN("Schema"), schema, nullptr);
}


sdi_t serialize(THD *thd, const Table &table, const std::string &schema_name)
{
  return generic_serialize(thd, STRING_WITH_LEN("Table"), table, &schema_name);
}


sdi_t serialize(const Tablespace &tablespace)
{
  return generic_serialize(nullptr, STRING_WITH_LEN("Tablespace"), tablespace,
                           nullptr);
}

template <class Dd_type>
bool generic_deserialize(THD *thd, const sdi_t &sdi,
                         const std::string &object_type_name, Dd_type *dst)
{
  RJ_Document doc;
  doc.Parse<0>(sdi.c_str());
  if (doc.HasParseError())
  {
    my_error(ER_INVALID_JSON_DATA, MYF(0), "deserialize()",
             doc.GetParseError());
    return true;
  }

  DBUG_ASSERT(doc.HasMember("sdi_version"));
  RJ_Value &sdi_version_val= doc["sdi_version"];
  DBUG_ASSERT(sdi_version_val.IsUint64());

  std::uint64_t sdi_version_= sdi_version_val.GetUint64();
  DBUG_ASSERT(sdi_version_ == sdi_version);

  DBUG_ASSERT(doc.HasMember("dd_version"));
  RJ_Value &dd_version_val= doc["dd_version"];
  DBUG_ASSERT(dd_version_val.IsUint());
  uint dd_version= dd_version_val.GetUint();

  DBUG_ASSERT(doc.HasMember("dd_object_type"));
  RJ_Value &dd_object_type_val= doc["dd_object_type"];
  DBUG_ASSERT(dd_object_type_val.IsString());
  std::string dd_object_type(dd_object_type_val.GetString());
  DBUG_ASSERT(dd_object_type == object_type_name);

  DBUG_ASSERT(doc.HasMember("dd_object"));
  RJ_Value &dd_object_val= doc["dd_object"];
  DBUG_ASSERT(dd_object_val.IsObject());

  Sdi_rcontext rctx(thd, dd_version, sdi_version_);
  if (dst->deserialize(&rctx, dd_object_val))
  {
    return checked_return(true);
  }

  return false;
}

bool deserialize(THD *thd, const sdi_t &sdi, Schema *dst_schema)
{
  return generic_deserialize(thd, sdi, "Schema", dst_schema);
}

bool deserialize(THD *thd, const sdi_t &sdi, Table *dst_table)
{
  return generic_deserialize(thd, sdi, "Table", dst_table);
}

bool deserialize(THD *thd, const sdi_t &sdi, Tablespace *dst_tablespace)
{
  return generic_deserialize(thd, sdi, "Tablespace", dst_tablespace);
}


/**
  Templated convenience wrapper which first attempts to resolve the
  handlerton using the data dictionary object's engine() string.

  @param thd
  @param ddt    Data dictionary object

  @retval handlerton pointer on success, nullptr otherwise
*/

template <typename DDT>
static handlerton *resolve_hton(THD *thd, const DDT &ddt)
{
  plugin_ref pr= ha_resolve_by_name_raw(thd, ddt.engine());
  if (pr)
  {
    return plugin_data<handlerton*>(pr);
  }
  return nullptr;
}

Sdi_updater::Sdi_updater(const Schema *schema)
  : m_prev_sdi_fname(sdi_file::sdi_filename(schema, empty_))
{}


Sdi_updater::Sdi_updater(const Table *table, const std::string &old_schema_name)
  : m_prev_sdi_fname(sdi_file::sdi_filename(table, old_schema_name))
{}

static bool update_sdi(THD*, const dd::Schema*);
bool Sdi_updater::operator()(THD *thd, const Schema *new_schema) const
{
  if (update_sdi(thd, new_schema))
  {
    return true;
  }
  return checked_return(sdi_file::remove(m_prev_sdi_fname));
}

bool Sdi_updater::operator()(THD *thd, const Table *table,
  const Schema *new_schema) const
{
  if (store_sdi(thd, table, new_schema))
  {
    return true;
  }
  return (!m_prev_sdi_fname.empty() &&
          checked_return(sdi_file::remove(m_prev_sdi_fname)));
}


Sdi_updater make_sdi_updater(const Schema *schema)
{
  return Sdi_updater(schema);
}

Sdi_updater make_sdi_updater(THD *thd, const Table *table, const Schema *schema)
{
  const handlerton *hton= resolve_hton(thd, *table);
  return (hton->sdi_set == nullptr ?
          Sdi_updater(table, schema->name()) : Sdi_updater());
}

Sdi_updater make_sdi_updater(THD *thd, const View *, const Schema *)
{
  return Sdi_updater();
}


bool store_sdi(THD *thd, const dd::Schema *s)
{
  dd::sdi_t sdi= dd::serialize(*s);
  if (sdi.empty())
  {
    return checked_return(true);
  }

  // Note! When storing a schema for the first time it does not
  // contain any tables, so it is not possible to locate a handlerton
  // which the operation can be delegated to. Consequently, we store the
  // SDI as a file in this case.
  return checked_return(sdi_file::store(thd, sdi, s));
}


static bool update_sdi(THD *thd, const dd::Schema *s)
{
  dd::sdi_t sdi= dd::serialize(*s);
  if (sdi.empty())
  {
    return checked_return(true);
  }

  dd::cache::Dictionary_client *dc= thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);

  std::vector<const dd::Abstract_table*> tables;
  if (dc->fetch_schema_components(s, &tables))
  {
    return checked_return(true);
  }

  for (const dd::Abstract_table *at : tables)
  {
    const Table *tbl= dynamic_cast<const Table*>(at);
    if (!tbl)
    {
      continue;
    }

    // TODO: This will be sub-optimal as we may end up storing
    // the updated SDI multiple times in the same tablespace if
    // multiple tables in this schema are stored in the same tablespace.
    // Maybe we need to track which tablespace ids we have stored the
    // modified schema SDI for?
    handlerton *hton= resolve_hton(thd, *tbl);
    if (hton->store_schema_sdi &&
        hton->store_schema_sdi(thd, hton, sdi, s, tbl))
    {
      return checked_return(true);
    }
  }

  delete_container_pointers(tables);

  // Finally, update SDI file
  return checked_return(sdi_file::store(thd, sdi, s));
}


bool store_sdi(THD *thd, const dd::Table *t, const dd::Schema *s)
{
  dd::sdi_t sdi= dd::serialize(thd, *t, s->name());
  if (sdi.empty())
  {
    return checked_return(true);
  }
  handlerton *hton= resolve_hton(thd, *t);
  return checked_return(hton->store_table_sdi(thd, hton, sdi, t, s));
}


bool store_sdi(THD *thd, const dd::Tablespace *ts)
{
  dd::sdi_t sdi= dd::serialize(*ts);
  if (sdi.empty())
  {
    return checked_return(true);
  }
  handlerton *hton= resolve_hton(thd, *ts);
  return checked_return(sdi_tablespace::store(hton, sdi, ts));
}

bool remove_sdi(THD *thd, const dd::Schema *s)
{
  cache::Dictionary_client *dc= thd->dd_client();
  cache::Dictionary_client::Auto_releaser releaser(dc);

  std::vector<const dd::Abstract_table*> tables;
  if (dc->fetch_schema_components(s, &tables))
  {
    return checked_return(true);
  }

  for (const dd::Abstract_table *at : tables)
  {
    const Table *tbl= dynamic_cast<const Table*>(at);
    if (!tbl)
    {
      continue;
    }

    handlerton *hton= resolve_hton(thd, *tbl);
    if (hton->remove_schema_sdi &&
        hton->remove_schema_sdi(thd, hton, s, tbl))
    {
      return checked_return(true);
    }
  }

  delete_container_pointers(tables);

  // Finally, remove SDI file
  return checked_return(sdi_file::remove(thd, s));
}




bool remove_sdi(THD *thd, const dd::Table *t,
                const dd::Schema *s)
{
  handlerton *hton= resolve_hton(thd, *t);
  return checked_return(hton->remove_table_sdi(thd, hton, t, s));
}


bool remove_sdi(THD *thd, const dd::Tablespace *ts)
{
  handlerton *hton= resolve_hton(thd, *ts);
  return checked_return(sdi_tablespace::remove(hton, ts));
}


bool import_sdi(THD *thd, Schema *schema)
{
  dd::cache::Dictionary_client &dc= *thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

  if (dc.store(schema) || store_sdi(thd, schema))
  {
    return true;
  }
  return false;
}


bool import_sdi(THD *thd, Table *table)
{
  dd::cache::Dictionary_client &dc= *thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

  const Schema *schema= nullptr;
  if (dc.acquire(table->schema_id(), &schema))
  {
    return true;
  }
  if (mdl_lock(thd, MDL_key::TABLE, schema->name(), table->name()))
  {
    return true;
  }
  if (dc.store(table) || store_sdi(thd, table, schema))
  {
    return true;
  }
  return false;
}

bool import_sdi(THD *thd, Tablespace *tablespace)
{
  dd::cache::Dictionary_client &dc= *thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(&dc);

  if (dc.store(tablespace) || store_sdi(thd, tablespace))
  {
    return true;
  }
  return false;
}
} // namespace dd
/** @} */ // end of group sdi_api


/**
  @defgroup sdi_ut SDI Unit-testing API
  @ingroup sdi

  Special functions used by unit tests but which are not available in
  the normal api.

  @{
*/

/**
  @namespace sdi_unittest
  Namespace from dd_sdi-t unit-test. Also used to contain driver/hook
  functions only used by unit-testing.
*/

namespace sdi_unittest {

typedef void (*cb)(dd::Sdi_wcontext*, const dd::Weak_object*, dd::Sdi_writer*);
void setup_wctx(cb fp, const dd::Weak_object *wo, dd::Sdi_writer *w)
{
  std::string s("driver_schema");
  dd::Sdi_wcontext wctx(nullptr, &s);

  fp(&wctx, wo, w);
}

typedef void (*dcb)(dd::Sdi_rcontext*, dd::Weak_object*,
                    dd::RJ_Document &doc);
void setup_rctx(dcb fp, dd::Weak_object *wo, dd::RJ_Document &doc)
{
  dd::Sdi_rcontext rctx(nullptr, 0, 0); // restore ids for comparison
  fp(&rctx, wo, doc);
}
} // namespace sdi_unittest

/** @} */ // End of group sdi_ut

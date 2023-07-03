/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/item_geofunc_internal.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <memory>

#include <boost/concept/usage.hpp>
#include <boost/geometry/algorithms/centroid.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/algorithms/overlaps.hpp>
#include <boost/geometry/core/exception.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/predicates.hpp>
#include <boost/geometry/strategies/strategies.hpp>
#include <boost/iterator/iterator_facade.hpp>

#include "m_ctype.h"
#include "m_string.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/current_thd.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/item_func.h"
#include "sql/mdl.h"
#include "sql/parse_location.h"  // POS
#include "sql/sql_class.h"       // THD
#include "sql/srs_fetcher.h"
#include "sql/system_variables.h"
#include "sql_string.h"
#include "template_utils.h"

namespace dd {
class Spatial_reference_system;
}  // namespace dd

bool Srs_fetcher::lock(gis::srid_t srid, enum_mdl_type lock_type) {
  DBUG_TRACE;
  assert(srid != 0);

  char id_str[11];  // uint32 => max 10 digits + \0
  longlong10_to_str(srid, id_str, 10);

  MDL_request mdl_request;
  mdl_request.init_with_source(MDL_key::SRID, "", id_str, lock_type,
                               MDL_TRANSACTION, __FILE__, __LINE__);
  if (m_thd->mdl_context.acquire_lock(&mdl_request,
                                      m_thd->variables.lock_wait_timeout)) {
    /* purecov: begin inspected */
    // If locking fails, an error has already been flagged.
    return true;
    /* purecov: end */
  }

  return false;
}

bool Srs_fetcher::acquire(gis::srid_t srid,
                          const dd::Spatial_reference_system **srs) {
  if (lock(srid, MDL_SHARED_READ)) return true; /* purecov: inspected */

  if (m_thd->dd_client()->acquire(srid, srs))
    return true; /* purecov: inspected */
  return false;
}

bool Srs_fetcher::acquire_for_modification(gis::srid_t srid,
                                           dd::Spatial_reference_system **srs) {
  if (lock(srid, MDL_EXCLUSIVE)) return true; /* purecov: inspected */

  if (m_thd->dd_client()->acquire_for_modification(srid, srs))
    return true; /* purecov: inspected */
  return false;
}

bool Srs_fetcher::srs_exists(THD *thd, gis::srid_t srid, bool *exists) {
  assert(exists);
  std::unique_ptr<dd::cache::Dictionary_client::Auto_releaser> releaser(
      new dd::cache::Dictionary_client::Auto_releaser(thd->dd_client()));
  Srs_fetcher fetcher(thd);
  const dd::Spatial_reference_system *srs = nullptr;
  if (fetcher.acquire(srid, &srs)) return true; /* purecov: inspected */
  *exists = (srs != nullptr);
  return false;
}

/**
  Create this class for exception safety --- destroy the objects referenced
  by the pointers in the set when destroying the container.
 */
template <typename T>
class Pointer_vector : public std::vector<T *> {
  typedef std::vector<T *> parent;

 public:
  ~Pointer_vector() {
    for (typename parent::iterator i = this->begin(); i != this->end(); ++i)
      delete (*i);
  }
};

// A unary predicate to locate a target Geometry object pointer from a sequence.
class Is_target_geometry {
  Geometry *m_target;

 public:
  Is_target_geometry(Geometry *t) : m_target(t) {}

  bool operator()(Geometry *g) { return g == m_target; }
};

class Rtree_entry_compare {
 public:
  Rtree_entry_compare() = default;

  bool operator()(const BG_rtree_entry &re1, const BG_rtree_entry &re2) const {
    return re1.second < re2.second;
  }
};

inline void reassemble_geometry(Geometry *g) {
  Geometry::wkbType gtype = g->get_geotype();
  if (gtype == Geometry::wkb_polygon)
    down_cast<Gis_polygon *>(g)->to_wkb_unparsed();
  else if (gtype == Geometry::wkb_multilinestring)
    down_cast<Gis_multi_line_string *>(g)->reassemble();
  else if (gtype == Geometry::wkb_multipolygon)
    down_cast<Gis_multi_polygon *>(g)->reassemble();
}

template <typename BG_geotype>
bool post_fix_result(BG_result_buf_mgr *resbuf_mgr, BG_geotype &geout,
                     String *res) {
  assert(geout.has_geom_header_space());
  reassemble_geometry(&geout);

  // Such objects returned by BG never have overlapped components.
  if (geout.get_type() == Geometry::wkb_multilinestring ||
      geout.get_type() == Geometry::wkb_multipolygon)
    geout.set_components_no_overlapped(true);
  if (geout.get_ptr() == nullptr) return true;
  if (res) {
    char *resptr = geout.get_cptr() - GEOM_HEADER_SIZE;
    size_t len = geout.get_nbytes();

    /*
      The resptr buffer is now owned by resbuf_mgr and used by res, resptr
      will be released properly by resbuf_mgr.
     */
    resbuf_mgr->add_buffer(resptr);
    /*
      The memory for the result is owned by a BG_result_buf_mgr,
      so use String::set(char*, size_t, const CHARSET_INFO)
      which points the internal buffer to the input argument,
      and sets m_is_alloced = false, signifying the String object
      does not own the buffer.
    */
    res->set(resptr, len + GEOM_HEADER_SIZE, &my_charset_bin);

    // Prefix the GEOMETRY header.
    write_geometry_header(resptr, geout.get_srid(), geout.get_geotype());

    /*
      Give up ownership because the buffer may have to live longer than
      the object.
    */
    geout.set_ownmem(false);
  }

  return false;
}

// Explicit template instantiation
template bool post_fix_result<Gis_line_string>(BG_result_buf_mgr *,
                                               Gis_line_string &, String *);
template bool post_fix_result<Gis_multi_line_string>(BG_result_buf_mgr *,
                                                     Gis_multi_line_string &,
                                                     String *);
template bool post_fix_result<Gis_multi_point>(BG_result_buf_mgr *,
                                               Gis_multi_point &, String *);
template bool post_fix_result<Gis_multi_polygon>(BG_result_buf_mgr *,
                                                 Gis_multi_polygon &, String *);
template bool post_fix_result<Gis_point>(BG_result_buf_mgr *, Gis_point &,
                                         String *);
template bool post_fix_result<Gis_polygon>(BG_result_buf_mgr *, Gis_polygon &,
                                           String *);

class Is_empty_geometry : public WKB_scanner_event_handler {
 public:
  bool is_empty;

  Is_empty_geometry() : is_empty(true) {}

  void on_wkb_start(Geometry::wkbByteOrder, Geometry::wkbType geotype,
                    const void *, uint32, bool) override {
    if (is_empty && geotype != Geometry::wkb_geometrycollection)
      is_empty = false;
  }

  void on_wkb_end(const void *) override {}

  bool continue_scan() const override { return is_empty; }
};

bool is_empty_geocollection(const Geometry *g) {
  if (g->get_geotype() != Geometry::wkb_geometrycollection) return false;

  uint32 num = uint4korr(g->get_cptr());
  if (num == 0) return true;

  Is_empty_geometry checker;
  uint32 len = g->get_data_size();
  wkb_scanner(current_thd, g->get_cptr(), &len,
              Geometry::wkb_geometrycollection, false, &checker);
  return checker.is_empty;
}

bool is_empty_geocollection(const String &wkbres) {
  if (wkbres.ptr() == nullptr) return true;

  uint32 geotype = uint4korr(wkbres.ptr() + SRID_SIZE + 1);

  if (geotype != static_cast<uint32>(Geometry::wkb_geometrycollection))
    return false;

  if (uint4korr(wkbres.ptr() + SRID_SIZE + WKB_HEADER_SIZE) == 0) return true;

  Is_empty_geometry checker;
  uint32 len = static_cast<uint32>(wkbres.length()) - GEOM_HEADER_SIZE;
  wkb_scanner(current_thd, wkbres.ptr() + GEOM_HEADER_SIZE, &len,
              Geometry::wkb_geometrycollection, false, &checker);
  return checker.is_empty;
}

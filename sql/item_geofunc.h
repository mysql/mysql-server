#ifndef ITEM_GEOFUNC_INCLUDED
#define ITEM_GEOFUNC_INCLUDED

/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

#include <assert.h>
#include <sys/types.h>

#include <cstddef>
#include <vector>

#include "field_types.h"  // MYSQL_TYPE_BLOB

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql/enum_query_type.h"
#include "sql/field.h"
#include "sql/gis/buffer_strategies.h"  // gis::buffer_strategies
#include "sql/gis/srid.h"
#include "sql/parse_location.h"  // POS
/* This file defines all spatial functions */
#include "sql/inplace_vector.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"  // Item_bool_func2
#include "sql/item_func.h"
#include "sql/item_json_func.h"  // Item_json_func
#include "sql/item_strfunc.h"    // Item_str_func
#include "sql/spatial.h"         // gis_wkb_raw_free
#include "sql_string.h"

class Json_array;
class Json_dom;
class Json_object;
class Json_wrapper;
class PT_item_list;
class THD;
struct Parse_context;
struct TABLE;

enum class enum_json_type;

namespace dd {
class Spatial_reference_system;
}  // namespace dd

namespace gis {
class Geometry;
class Point;
}  // namespace gis

/**
   We have to hold result buffers in functions that return a GEOMETRY string,
   because such a function's result geometry's buffer is directly used and
   set to String result object. We have to release them properly manually
   since they won't be released when the String result is destroyed.
*/
class BG_result_buf_mgr {
  typedef Prealloced_array<void *, 64> Prealloced_buffers;

 public:
  BG_result_buf_mgr() : bg_result_buf(nullptr), bg_results(PSI_INSTRUMENT_ME) {}

  ~BG_result_buf_mgr() {
    free_intermediate_result_buffers();
    free_result_buffer();
  }

  void add_buffer(void *buf) { bg_results.insert_unique(buf); }

  void forget_buffer(void *buf) {
    if (bg_result_buf == buf) bg_result_buf = nullptr;
    bg_results.erase_unique(buf);
  }

  /* Free intermediate result buffers accumulated during GIS calculation. */
  void free_intermediate_result_buffers() {
    bg_results.erase_unique(bg_result_buf);
    for (Prealloced_buffers::iterator itr = bg_results.begin();
         itr != bg_results.end(); ++itr)
      gis_wkb_raw_free(*itr);
    bg_results.clear();
  }

  // Free the final result buffer, should be called after the result used.
  void free_result_buffer() {
    gis_wkb_raw_free(bg_result_buf);
    bg_result_buf = nullptr;
  }

  void set_result_buffer(void *buf) {
    bg_result_buf = buf;
    bg_results.erase_unique(bg_result_buf);
  }

 private:
  /*
    Hold data buffer of this set operation's final result geometry which is
    freed next time val_str is called since it can be used by upper Item nodes.
  */
  void *bg_result_buf;

  /*
    Result buffers for intermediate set operation results, which are freed
    before val_str returns.
  */
  Prealloced_buffers bg_results;
};

class Item_func_st_union;

/**
  A utility class to flatten any hierarchy of geometry collection into one
  with no nested geometry collections. All components are stored separately
  and all their data stored in this class, in order to easily manipulate them.
 */
class BG_geometry_collection {
  bool comp_no_overlapped;
  gis::srid_t m_srid;
  size_t m_num_isolated;
  std::vector<Geometry *> m_geos;
  Inplace_vector<Geometry_buffer> m_geobufs;
  Inplace_vector<String> m_geosdata;

 public:
  typedef std::vector<Geometry *> Geometry_list;

  BG_geometry_collection();

  bool is_comp_no_overlapped() const { return comp_no_overlapped; }

  void set_comp_no_overlapped(bool b) { comp_no_overlapped = b; }

  gis::srid_t get_srid() const { return m_srid; }

  void set_srid(gis::srid_t srid) { m_srid = srid; }

  bool fill(const Geometry *geo, bool break_multi_geom = false) {
    return store_geometry(geo, break_multi_geom);
  }

  const Geometry_list &get_geometries() const { return m_geos; }

  Geometry_list &get_geometries() { return m_geos; }

  bool all_isolated() const { return m_num_isolated == m_geos.size(); }

  size_t num_isolated() const { return m_num_isolated; }

 private:
  bool store_geometry(const Geometry *geo, bool break_multi_geom);
  Geometry *store(const Geometry *geo);
};

class Item_geometry_func : public Item_str_func {
 public:
  Item_geometry_func() : Item_str_func() {}

  Item_geometry_func(Item *a) : Item_str_func(a) {}
  Item_geometry_func(const POS &pos, Item *a) : Item_str_func(pos, a) {}

  Item_geometry_func(Item *a, Item *b) : Item_str_func(a, b) {}
  Item_geometry_func(const POS &pos, Item *a, Item *b)
      : Item_str_func(pos, a, b) {}

  Item_geometry_func(Item *a, Item *b, Item *c) : Item_str_func(a, b, c) {}
  Item_geometry_func(const POS &pos, Item *a, Item *b, Item *c)
      : Item_str_func(pos, a, b, c) {}
  Item_geometry_func(const POS &pos, PT_item_list *list);

  bool resolve_type(THD *) override;
  Field *tmp_table_field(TABLE *t_arg) override;
};

class Item_func_geometry_from_text : public Item_geometry_func {
 public:
  enum class Functype {
    GEOMCOLLFROMTEXT,
    GEOMCOLLFROMTXT,
    GEOMETRYCOLLECTIONFROMTEXT,
    GEOMETRYFROMTEXT,
    GEOMFROMTEXT,
    LINEFROMTEXT,
    LINESTRINGFROMTEXT,
    MLINEFROMTEXT,
    MPOINTFROMTEXT,
    MPOLYFROMTEXT,
    MULTILINESTRINGFROMTEXT,
    MULTIPOINTFROMTEXT,
    MULTIPOLYGONFROMTEXT,
    POINTFROMTEXT,
    POLYFROMTEXT,
    POLYGONFROMTEXT
  };

 private:
  typedef Item_geometry_func super;
  Functype m_functype;
  /**
    Get the type of geometry that this Item can return.

    @return The geometry type
  */
  Geometry::wkbType allowed_wkb_type() const;
  /**
    Check if a geometry type is a valid return type for this Item.

    @param type The type to check

    @retval true The geometry type is allowed
    @retval false The geometry type is not allowed
  */
  bool is_allowed_wkb_type(Geometry::wkbType type) const;

 public:
  Item_func_geometry_from_text(const POS &pos, Item *a, Functype functype)
      : Item_geometry_func(pos, a), m_functype(functype) {}
  Item_func_geometry_from_text(const POS &pos, Item *a, Item *srid,
                               Functype functype)
      : Item_geometry_func(pos, a, srid), m_functype(functype) {}
  Item_func_geometry_from_text(const POS &pos, Item *a, Item *srid,
                               Item *option, Functype functype)
      : Item_geometry_func(pos, a, srid, option), m_functype(functype) {}

  bool itemize(Parse_context *pc, Item **res) override;
  const char *func_name() const override;
  String *val_str(String *) override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONGLONG)) return true;
    if (param_type_is_default(thd, 2, 3)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_geometry_from_wkb : public Item_geometry_func {
 public:
  enum class Functype {
    GEOMCOLLFROMWKB,
    GEOMETRYCOLLECTIONFROMWKB,
    GEOMETRYFROMWKB,
    GEOMFROMWKB,
    LINEFROMWKB,
    LINESTRINGFROMWKB,
    MLINEFROMWKB,
    MPOINTFROMWKB,
    MPOLYFROMWKB,
    MULTILINESTRINGFROMWKB,
    MULTIPOINTFROMWKB,
    MULTIPOLYGONFROMWKB,
    POINTFROMWKB,
    POLYFROMWKB,
    POLYGONFROMWKB
  };

 private:
  typedef Item_geometry_func super;
  String tmp_value;
  Functype m_functype;
  /**
    Get the type of geometry that this Item can return.

    @return The geometry type
  */
  Geometry::wkbType allowed_wkb_type() const;
  /**
    Check if a geometry type is a valid return type for this Item.

    @param type The type to check

    @retval true The geometry type is allowed
    @retval false The geometry type is not allowed
  */
  bool is_allowed_wkb_type(Geometry::wkbType type) const;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONGLONG)) return true;
    if (param_type_is_default(thd, 2, 3)) return true;
    return Item_geometry_func::resolve_type(thd);
  }

 public:
  Item_func_geometry_from_wkb(const POS &pos, Item *a, Functype functype)
      : Item_geometry_func(pos, a), m_functype(functype) {}
  Item_func_geometry_from_wkb(const POS &pos, Item *a, Item *srid,
                              Functype functype)
      : Item_geometry_func(pos, a, srid), m_functype(functype) {}
  Item_func_geometry_from_wkb(const POS &pos, Item *a, Item *srid, Item *option,
                              Functype functype)
      : Item_geometry_func(pos, a, srid, option), m_functype(functype) {}

  bool itemize(Parse_context *pc, Item **res) override;
  const char *func_name() const override;
  String *val_str(String *) override;
};

class Item_func_as_wkt : public Item_str_ascii_func {
 public:
  Item_func_as_wkt(const POS &pos, Item *a) : Item_str_ascii_func(pos, a) {}
  Item_func_as_wkt(const POS &pos, Item *a, Item *b)
      : Item_str_ascii_func(pos, a, b) {}
  const char *func_name() const override { return "st_astext"; }
  String *val_str_ascii(String *) override;
  bool resolve_type(THD *) override;
};

class Item_func_as_wkb : public Item_geometry_func {
 public:
  Item_func_as_wkb(const POS &pos, Item *a) : Item_geometry_func(pos, a) {}
  Item_func_as_wkb(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}
  const char *func_name() const override { return "st_aswkb"; }
  String *val_str(String *) override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 1, 2)) return true;
    if (Item_geometry_func::resolve_type(thd)) return true;
    set_data_type_blob(Field::MAX_LONG_BLOB_WIDTH);
    return false;
  }
};

class Item_func_geometry_type : public Item_str_ascii_func {
 public:
  Item_func_geometry_type(const POS &pos, Item *a)
      : Item_str_ascii_func(pos, a) {}
  String *val_str_ascii(String *) override;
  const char *func_name() const override { return "st_geometrytype"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    // "MultiLinestring" is the longest
    set_data_type_string(15, default_charset());
    set_nullable(true);
    return false;
  }
};

/**
  This handles one function:

    @<geometry@> = ST_GEOMFROMGEOJSON(@<string@>[, @<options@>[, @<srid@>]])

  Options is an integer argument which determines how positions with higher
  coordinate dimension than MySQL support should be handled. The function will
  accept both single objects, geometry collections and feature objects and
  collections. All "properties" members of GeoJSON feature objects is ignored.

  The implementation conforms with GeoJSON revision 1.0 described at
  http://geojson.org/geojson-spec.html.
*/
class Item_func_geomfromgeojson : public Item_geometry_func {
 public:
  /**
    Describing how coordinate dimensions higher than supported in MySQL
    should be handled.
  */
  enum enum_handle_coordinate_dimension {
    reject_document,
    strip_now_accept_future,
    strip_now_reject_future,
    strip_now_strip_future
  };
  Item_func_geomfromgeojson(const POS &pos, Item *json_string)
      : Item_geometry_func(pos, json_string),
        m_handle_coordinate_dimension(reject_document),
        m_user_provided_srid(false),
        m_srid_found_in_document(-1) {}
  Item_func_geomfromgeojson(const POS &pos, Item *json_string, Item *options)
      : Item_geometry_func(pos, json_string, options),
        m_user_provided_srid(false),
        m_srid_found_in_document(-1) {}
  Item_func_geomfromgeojson(const POS &pos, Item *json_string, Item *options,
                            Item *srid)
      : Item_geometry_func(pos, json_string, options, srid),
        m_srid_found_in_document(-1) {}
  String *val_str(String *) override;
  bool fix_fields(THD *, Item **ref) override;
  const char *func_name() const override { return "st_geomfromgeojson"; }
  Geometry::wkbType get_wkbtype(const char *typestring);
  bool get_positions(const Json_array *coordinates, Gis_point *point);
  bool get_linestring(const Json_array *data_array,
                      Gis_line_string *linestring);
  bool get_polygon(const Json_array *data_array, Gis_polygon *polygon);
  bool parse_object(const Json_object *object, bool *rollback, String *buffer,
                    bool is_parent_featurecollection, Geometry **geometry);
  bool parse_object_array(const Json_array *points, Geometry::wkbType type,
                          bool *rollback, String *buffer,
                          bool is_parent_featurecollection,
                          Geometry **geometry);
  static bool check_argument_valid_integer(Item *argument);
  bool parse_crs_object(const Json_object *crs_object);
  bool is_member_valid(const Json_dom *member, const char *member_name,
                       enum_json_type expected_type, bool allow_null,
                       bool *was_null);
  const Json_dom *my_find_member_ncase(const Json_object *object,
                                       const char *member_name);

  static const char *TYPE_MEMBER;
  static const char *CRS_MEMBER;
  static const char *GEOMETRY_MEMBER;
  static const char *PROPERTIES_MEMBER;
  static const char *FEATURES_MEMBER;
  static const char *GEOMETRIES_MEMBER;
  static const char *COORDINATES_MEMBER;
  static const char *CRS_NAME_MEMBER;
  static const char *NAMED_CRS;
  static const char *SHORT_EPSG_PREFIX;
  static const char *LONG_EPSG_PREFIX;
  static const char *CRS84_URN;
  static const char *POINT_TYPE;
  static const char *MULTIPOINT_TYPE;
  static const char *LINESTRING_TYPE;
  static const char *MULTILINESTRING_TYPE;
  static const char *POLYGON_TYPE;
  static const char *MULTIPOLYGON_TYPE;
  static const char *GEOMETRYCOLLECTION_TYPE;
  static const char *FEATURE_TYPE;
  static const char *FEATURECOLLECTION_TYPE;

 private:
  /**
    How higher coordinate dimensions than currently supported should be handled.
  */
  enum_handle_coordinate_dimension m_handle_coordinate_dimension;
  /// Is set to true if user provided a SRID as an argument.
  bool m_user_provided_srid;
  /// The SRID user provided as an argument.
  gis::srid_t m_user_srid;
  /**
    The SRID value of the document CRS, if one is found. Otherwise, this value
    defaults to -1.
  */
  longlong m_srid_found_in_document;
  /// The minimum allowed longitude value (non-inclusive).
  double m_min_longitude = -180.0;
  /// The maximum allowed longitude (inclusive).
  double m_max_longitude = 180.0;
  /// The minimum allowed latitude value (inclusive).
  double m_min_latitude = -90.0;
  /// The maximum allowed latitude (inclusive).
  double m_max_latitude = 90.0;
  /// True if we're currently parsing the top-level object.
  bool m_toplevel = true;
};

/// Max width of long CRS URN supported + max width of SRID + '\0'.
static const int MAX_CRS_WIDTH = (22 + MAX_INT_WIDTH + 1);

/**
  This class handles the following function:

  @<json@> = ST_ASGEOJSON(@<geometry@>[, @<maxdecimaldigits@>[, @<options@>]])

  It converts a GEOMETRY into a valid GeoJSON string. If maxdecimaldigits is
  specified, the coordinates written are rounded to the number of decimals
  specified (e.g with decimaldigits = 3: 10.12399 => 10.124).

  Options is a bitmask with the following flags:
  0  No options (default values).
  1  Add a bounding box to the output.
  2  Add a short CRS URN to the output. The default format is a
     short format ("EPSG:<srid>").
  4  Add a long format CRS URN ("urn:ogc:def:crs:EPSG::<srid>"). This
     implies 2. This means that, e.g., bitmask 5 and 7 mean the
     same: add a bounding box and a long format CRS URN.
*/
class Item_func_as_geojson : public Item_json_func {
 private:
  /// Maximum number of decimal digits in printed coordinates.
  int m_max_decimal_digits;
  /// If true, the output GeoJSON has a bounding box for each GEOMETRY.
  bool m_add_bounding_box;
  /**
    If true, the output GeoJSON has a CRS object in the short
    form (e.g "EPSG:4326").
  */
  bool m_add_short_crs_urn;
  /**
    If true, the output GeoJSON has a CRS object in the long
    form (e.g "urn:ogc:def:crs:EPSG::4326").
  */
  bool m_add_long_crs_urn;
  /// The SRID found in the input GEOMETRY.
  uint32 m_geometry_srid;

 public:
  Item_func_as_geojson(THD *thd, const POS &pos, Item *geometry)
      : Item_json_func(thd, pos, geometry),
        m_add_bounding_box(false),
        m_add_short_crs_urn(false),
        m_add_long_crs_urn(false) {}
  Item_func_as_geojson(THD *thd, const POS &pos, Item *geometry,
                       Item *maxdecimaldigits)
      : Item_json_func(thd, pos, geometry, maxdecimaldigits),
        m_add_bounding_box(false),
        m_add_short_crs_urn(false),
        m_add_long_crs_urn(false) {}
  Item_func_as_geojson(THD *thd, const POS &pos, Item *geometry,
                       Item *maxdecimaldigits, Item *options)
      : Item_json_func(thd, pos, geometry, maxdecimaldigits, options),
        m_add_bounding_box(false),
        m_add_short_crs_urn(false),
        m_add_long_crs_urn(false) {}
  bool fix_fields(THD *thd, Item **ref) override;
  bool val_json(Json_wrapper *wr) override;
  const char *func_name() const override { return "st_asgeojson"; }
  bool parse_options_argument();
  bool parse_maxdecimaldigits_argument();
};

/**
  This class handles two forms of the same function:

  @<string@> = ST_GEOHASH(@<point@>, @<maxlength@>);
  @<string@> = ST_GEOHASH(@<longitude@>, @<latitude@>, @<maxlength@>)

  It returns an encoded geohash string, no longer than @<maxlength@> characters
  long. Note that it might be shorter than @<maxlength@>.
*/
class Item_func_geohash : public Item_str_ascii_func {
 private:
  /// The latitude argument supplied by the user (directly or by a POINT).
  double latitude;
  /// The longitude argument supplied by the user (directly or by a POINT).
  double longitude;
  /// The maximum output length of the geohash, supplied by the user.
  uint geohash_max_output_length;

  /**
    The maximum input latitude. For now, this is set to 90.0. It can be
    changed to support a different range than the normal [90, -90].
  */
  const double max_latitude;

  /**
    The minimum input latitude. For now, this is set to -90.0. It can be
    changed to support a different range than the normal [90, -90].
  */
  const double min_latitude;

  /**
    The maximum input longitude. For now, this is set to 180.0. It can be
    changed to support a different range than the normal [180, -180].
  */
  const double max_longitude;

  /**
    The minimum input longitude. For now, this is set to -180.0. It can be
    changed to support a different range than the normal [180, -180].
  */
  const double min_longitude;

  /**
    The absolute upper limit of geohash output length. User will get an error
    if they supply a max geohash length argument greater than this.
  */
  const uint upper_limit_output_length;

 public:
  Item_func_geohash(const POS &pos, Item *point, Item *length)
      : Item_str_ascii_func(pos, point, length),
        max_latitude(90.0),
        min_latitude(-90.0),
        max_longitude(180.0),
        min_longitude(-180.0),
        upper_limit_output_length(100) {}
  Item_func_geohash(const POS &pos, Item *longitude, Item *latitude,
                    Item *length)
      : Item_str_ascii_func(pos, longitude, latitude, length),
        max_latitude(90.0),
        min_latitude(-90.0),
        max_longitude(180.0),
        min_longitude(-180.0),
        upper_limit_output_length(100) {}
  String *val_str_ascii(String *) override;
  bool resolve_type(THD *) override;
  bool fix_fields(THD *thd, Item **ref) override;
  const char *func_name() const override { return "st_geohash"; }
  char char_to_base32(char char_input);
  void encode_bit(double *upper_value, double *lower_value, double target_value,
                  char *char_value, int bit_number);
  bool fill_and_check_fields();
  bool check_valid_latlong_type(Item *ref);
};

/**
  This is a superclass for Item_func_longfromgeohash and
  Item_func_latfromgeohash, since they share almost all code.
*/
class Item_func_latlongfromgeohash : public Item_real_func {
 private:
  /**
   The lower limit for latitude output value. Normally, this will be
   set to -90.0.
  */
  const double lower_latitude;

  /**
   The upper limit for latitude output value. Normally, this will be
   set to 90.0.
  */
  const double upper_latitude;

  /**
   The lower limit for longitude output value. Normally, this will
   be set to -180.0.
  */
  const double lower_longitude;

  /**
   The upper limit for longitude output value. Normally, this will
   be set to 180.0.
  */
  const double upper_longitude;

  /**
   If this is set to true the algorithm will start decoding on the first bit,
   which decodes a longitude value. If it is false, it will start on the
   second bit which decodes a latitude value.
  */
  const bool start_on_even_bit;

 public:
  Item_func_latlongfromgeohash(const POS &pos, Item *a, double lower_latitude,
                               double upper_latitude, double lower_longitude,
                               double upper_longitude,
                               bool start_on_even_bit_arg)
      : Item_real_func(pos, a),
        lower_latitude(lower_latitude),
        upper_latitude(upper_latitude),
        lower_longitude(lower_longitude),
        upper_longitude(upper_longitude),
        start_on_even_bit(start_on_even_bit_arg) {}
  double val_real() override;
  bool resolve_type(THD *thd) override;
  bool fix_fields(THD *thd, Item **ref) override;
  static bool decode_geohash(String *geohash, double upper_latitude,
                             double lower_latitude, double upper_longitude,
                             double lower_longitude, double *result_latitude,
                             double *result_longitude);
  static double round_latlongitude(double latlongitude, double error_range,
                                   double lower_limit, double upper_limit);
  static bool check_geohash_argument_valid_type(Item *item);
};

/**
  This handles the @<double@> = ST_LATFROMGEOHASH(@<string@>) function.
  It returns the latitude-part of a geohash, in the range of [-90, 90].
*/
class Item_func_latfromgeohash : public Item_func_latlongfromgeohash {
 public:
  Item_func_latfromgeohash(const POS &pos, Item *a)
      : Item_func_latlongfromgeohash(pos, a, -90.0, 90.0, -180.0, 180.0,
                                     false) {}

  const char *func_name() const override { return "ST_LATFROMGEOHASH"; }
};

/**
  This handles the @<double@> = ST_LONGFROMGEOHASH(@<string@>) function.
  It returns the longitude-part of a geohash, in the range of [-180, 180].
*/
class Item_func_longfromgeohash : public Item_func_latlongfromgeohash {
 public:
  Item_func_longfromgeohash(const POS &pos, Item *a)
      : Item_func_latlongfromgeohash(pos, a, -90.0, 90.0, -180.0, 180.0, true) {
  }

  const char *func_name() const override { return "ST_LONGFROMGEOHASH"; }
};

class Item_func_centroid : public Item_geometry_func {
  BG_result_buf_mgr bg_resbuf_mgr;

  template <typename Coordsys>
  bool bg_centroid(const Geometry *geom, String *ptwkb);

 public:
  Item_func_centroid(const POS &pos, Item *a) : Item_geometry_func(pos, a) {}
  const char *func_name() const override { return "st_centroid"; }
  String *val_str(String *) override;
  Field::geometry_type get_geometry_type() const override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_convex_hull : public Item_geometry_func {
  BG_result_buf_mgr bg_resbuf_mgr;

  template <typename Coordsys>
  bool bg_convex_hull(const Geometry *geom, String *wkb);

 public:
  Item_func_convex_hull(const POS &pos, Item *a) : Item_geometry_func(pos, a) {}
  const char *func_name() const override { return "st_convexhull"; }
  String *val_str(String *) override;
  Field::geometry_type get_geometry_type() const override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_envelope : public Item_geometry_func {
 public:
  Item_func_envelope(const POS &pos, Item *a) : Item_geometry_func(pos, a) {}
  const char *func_name() const override { return "st_envelope"; }
  String *val_str(String *) override;
  Field::geometry_type get_geometry_type() const override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_make_envelope : public Item_geometry_func {
 public:
  Item_func_make_envelope(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}
  const char *func_name() const override { return "st_makeenvelope"; }
  String *val_str(String *) override;
  Field::geometry_type get_geometry_type() const override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_validate : public Item_geometry_func {
  String arg_val;

 public:
  Item_func_validate(const POS &pos, Item *a) : Item_geometry_func(pos, a) {}
  const char *func_name() const override { return "st_validate"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
  String *val_str(String *) override;
};

/// Item that implements function ST_Simplify, which simplifies a geometry using
/// the Douglas-Peucker algorithm.
class Item_func_st_simplify : public Item_geometry_func {
 public:
  Item_func_st_simplify(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_DOUBLE)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
  String *val_str(String *) override;

  const char *func_name() const override { return "st_simplify"; }
};

class Item_func_point : public Item_geometry_func {
 public:
  Item_func_point(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}
  const char *func_name() const override { return "point"; }
  String *val_str(String *) override;
  Field::geometry_type get_geometry_type() const override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_DOUBLE)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

/**
  This handles the @<point@> = ST_POINTFROMGEOHASH(@<string@>, @<srid@>)
  function.

  It returns a point containing the decoded geohash value, where X is the
  longitude in the range of [-180, 180] and Y is the latitude in the range
  of [-90, 90].
*/
class Item_func_pointfromgeohash : public Item_geometry_func {
 private:
  /// The maximum output latitude value when decoding the geohash value.
  const double upper_latitude;

  /// The minimum output latitude value when decoding the geohash value.
  const double lower_latitude;

  /// The maximum output longitude value when decoding the geohash value.
  const double upper_longitude;

  /// The minimum output longitude value when decoding the geohash value.
  const double lower_longitude;

 public:
  Item_func_pointfromgeohash(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b),
        upper_latitude(90.0),
        lower_latitude(-90.0),
        upper_longitude(180.0),
        lower_longitude(-180.0) {}
  const char *func_name() const override { return "st_pointfromgeohash"; }
  String *val_str(String *) override;
  bool fix_fields(THD *thd, Item **ref) override;
  bool resolve_type(THD *thd) override;
  Field::geometry_type get_geometry_type() const override {
    return Field::GEOM_POINT;
  }
};

class Item_func_spatial_decomp : public Item_geometry_func {
  enum Functype decomp_func;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }

 public:
  Item_func_spatial_decomp(const POS &pos, Item *a, Item_func::Functype ft)
      : Item_geometry_func(pos, a) {
    decomp_func = ft;
  }
  const char *func_name() const override {
    switch (decomp_func) {
      case SP_STARTPOINT:
        return "st_startpoint";
      case SP_ENDPOINT:
        return "st_endpoint";
      case SP_EXTERIORRING:
        return "st_exteriorring";
      default:
        assert(0);  // Should never happened
        return "spatial_decomp_unknown";
    }
  }
  String *val_str(String *) override;
};

class Item_func_spatial_decomp_n : public Item_geometry_func {
  enum Functype decomp_func_n;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONGLONG)) return true;
    return Item_geometry_func::resolve_type(thd);
  }

 public:
  Item_func_spatial_decomp_n(const POS &pos, Item *a, Item *b,
                             Item_func::Functype ft)
      : Item_geometry_func(pos, a, b) {
    decomp_func_n = ft;
  }
  const char *func_name() const override {
    switch (decomp_func_n) {
      case SP_POINTN:
        return "st_pointn";
      case SP_GEOMETRYN:
        return "st_geometryn";
      case SP_INTERIORRINGN:
        return "st_interiorringn";
      default:
        assert(0);  // Should never happened
        return "spatial_decomp_n_unknown";
    }
  }
  String *val_str(String *) override;
};

class Item_func_spatial_collection : public Item_geometry_func {
  String tmp_value;
  enum Geometry::wkbType coll_type;
  enum Geometry::wkbType item_type;

 public:
  Item_func_spatial_collection(const POS &pos, PT_item_list *list,
                               enum Geometry::wkbType ct,
                               enum Geometry::wkbType it)
      : Item_geometry_func(pos, list) {
    coll_type = ct;
    item_type = it;
  }
  String *val_str(String *) override;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    if (Item_geometry_func::resolve_type(thd)) return true;
    for (unsigned int i = 0; i < arg_count; ++i) {
      if (args[i]->fixed && args[i]->data_type() != MYSQL_TYPE_GEOMETRY) {
        String str;
        args[i]->print(thd, &str, QT_NO_DATA_EXPANSION);
        str.append('\0');
        my_error(ER_ILLEGAL_VALUE_FOR_TYPE, MYF(0), "non geometric", str.ptr());
        return true;
      }
    }
    return false;
  }

  const char *func_name() const override;
};

/*
  Spatial relations
*/

class Item_func_spatial_mbr_rel : public Item_bool_func2 {
  enum Functype spatial_rel;

 public:
  Item_func_spatial_mbr_rel(Item *a, Item *b, enum Functype sp_rel)
      : Item_bool_func2(a, b) {
    spatial_rel = sp_rel;
  }
  Item_func_spatial_mbr_rel(const POS &pos, Item *a, Item *b,
                            enum Functype sp_rel)
      : Item_bool_func2(pos, a, b) {
    spatial_rel = sp_rel;
  }
  longlong val_int() override;
  enum Functype functype() const override { return spatial_rel; }
  enum Functype rev_functype() const override {
    switch (spatial_rel) {
      case SP_CONTAINS_FUNC:
        return SP_WITHIN_FUNC;
      case SP_WITHIN_FUNC:
        return SP_CONTAINS_FUNC;
      default:
        return spatial_rel;
    }
  }

  const char *func_name() const override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override {
    Item_func::print(thd, str, query_type);
  }
  bool resolve_type(THD *) override {
    set_nullable(true);
    return false;
  }
  bool is_null() override {
    val_int();
    return null_value;
  }
};

class Item_func_spatial_relation : public Item_bool_func2 {
 public:
  Item_func_spatial_relation(const POS &pos, Item *a, Item *b)
      : Item_bool_func2(pos, a, b) {}
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    // Spatial relation functions may return NULL if either parameter is NULL or
    // an empty geometry. Since we can't check for empty geometries at resolve
    // time, this item is always nullable.
    set_nullable(true);
    return false;
  }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override {
    Item_func::print(thd, str, query_type);
  }
  longlong val_int() override;
  bool is_null() override {
    // The superclass implementation only checks is_null on the item's
    // arguments. However, relational functions may return NULL even if the
    // arguments are not NULL, e.g., if one or more argument is an empty
    // geometry. Therefore, we must evaluate the item to find out if it is NULL
    // or not.
    val_int();
    return null_value;
  }

  /**
    Evaluate the spatial relation function.

    @param[in] srs Spatial reference system common to both g1 and g2.
    @param[in] g1 First geometry.
    @param[in] g2 Second geometry.
    @param[out] result Result of the relational operation.
    @param[out] null True if the function should return NULL, false otherwise.

    @retval true An error has occurred and has been reported with my_error.
    @retval false Success.
  */
  virtual bool eval(const dd::Spatial_reference_system *srs,
                    const gis::Geometry *g1, const gis::Geometry *g2,
                    bool *result, bool *null) = 0;
};

class Item_func_st_contains final : public Item_func_spatial_relation {
 public:
  Item_func_st_contains(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_CONTAINS_FUNC; }
  enum Functype rev_functype() const override { return SP_WITHIN_FUNC; }
  const char *func_name() const override { return "st_contains"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_st_crosses final : public Item_func_spatial_relation {
 public:
  Item_func_st_crosses(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_CROSSES_FUNC; }
  enum Functype rev_functype() const override { return SP_CROSSES_FUNC; }
  const char *func_name() const override { return "st_crosses"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_st_disjoint final : public Item_func_spatial_relation {
 public:
  Item_func_st_disjoint(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_DISJOINT_FUNC; }
  enum Functype rev_functype() const override { return SP_DISJOINT_FUNC; }
  const char *func_name() const override { return "st_disjoint"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_st_equals final : public Item_func_spatial_relation {
 public:
  Item_func_st_equals(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_EQUALS_FUNC; }
  enum Functype rev_functype() const override { return SP_EQUALS_FUNC; }
  const char *func_name() const override { return "st_equals"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_st_intersects final : public Item_func_spatial_relation {
 public:
  Item_func_st_intersects(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_INTERSECTS_FUNC; }
  enum Functype rev_functype() const override { return SP_INTERSECTS_FUNC; }
  const char *func_name() const override { return "st_intersects"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrcontains final : public Item_func_spatial_relation {
 public:
  Item_func_mbrcontains(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_CONTAINS_FUNC; }
  enum Functype rev_functype() const override { return SP_WITHIN_FUNC; }
  const char *func_name() const override { return "mbrcontains"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrcoveredby final : public Item_func_spatial_relation {
 public:
  Item_func_mbrcoveredby(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_COVEREDBY_FUNC; }
  enum Functype rev_functype() const override { return SP_COVERS_FUNC; }
  const char *func_name() const override { return "mbrcoveredby"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrcovers final : public Item_func_spatial_relation {
 public:
  Item_func_mbrcovers(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_COVERS_FUNC; }
  enum Functype rev_functype() const override { return SP_COVEREDBY_FUNC; }
  const char *func_name() const override { return "mbrcovers"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrdisjoint final : public Item_func_spatial_relation {
 public:
  Item_func_mbrdisjoint(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_DISJOINT_FUNC; }
  enum Functype rev_functype() const override { return SP_DISJOINT_FUNC; }
  const char *func_name() const override { return "mbrdisjoint"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrequals final : public Item_func_spatial_relation {
 public:
  Item_func_mbrequals(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_EQUALS_FUNC; }
  enum Functype rev_functype() const override { return SP_EQUALS_FUNC; }
  const char *func_name() const override { return "mbrequals"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrintersects final : public Item_func_spatial_relation {
 public:
  Item_func_mbrintersects(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_INTERSECTS_FUNC; }
  enum Functype rev_functype() const override { return SP_INTERSECTS_FUNC; }
  const char *func_name() const override { return "mbrintersects"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbroverlaps final : public Item_func_spatial_relation {
 public:
  Item_func_mbroverlaps(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_OVERLAPS_FUNC; }
  enum Functype rev_functype() const override { return SP_OVERLAPS_FUNC; }
  const char *func_name() const override { return "mbroverlaps"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrtouches final : public Item_func_spatial_relation {
 public:
  Item_func_mbrtouches(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_TOUCHES_FUNC; }
  enum Functype rev_functype() const override { return SP_TOUCHES_FUNC; }
  const char *func_name() const override { return "mbrtouches"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_mbrwithin final : public Item_func_spatial_relation {
 public:
  Item_func_mbrwithin(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_WITHIN_FUNC; }
  enum Functype rev_functype() const override { return SP_CONTAINS_FUNC; }
  const char *func_name() const override { return "mbrwithin"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_st_overlaps final : public Item_func_spatial_relation {
 public:
  Item_func_st_overlaps(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_OVERLAPS_FUNC; }
  enum Functype rev_functype() const override { return SP_OVERLAPS_FUNC; }
  const char *func_name() const override { return "st_overlaps"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_st_touches final : public Item_func_spatial_relation {
 public:
  Item_func_st_touches(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_TOUCHES_FUNC; }
  enum Functype rev_functype() const override { return SP_TOUCHES_FUNC; }
  const char *func_name() const override { return "st_touches"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

class Item_func_st_within final : public Item_func_spatial_relation {
 public:
  Item_func_st_within(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_relation(pos, a, b) {}
  enum Functype functype() const override { return SP_WITHIN_FUNC; }
  enum Functype rev_functype() const override { return SP_CONTAINS_FUNC; }
  const char *func_name() const override { return "st_within"; }
  bool eval(const dd::Spatial_reference_system *srs, const gis::Geometry *g1,
            const gis::Geometry *g2, bool *result, bool *null) override;
};

/**
  Spatial operations
*/
class Item_func_spatial_operation : public Item_geometry_func {
 protected:
  Item_func_spatial_operation(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}

 private:
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_st_difference final : public Item_func_spatial_operation {
 public:
  Item_func_st_difference(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_operation(pos, a, b) {}
  String *val_str(String *) override;
  const char *func_name() const override { return "st_difference"; }
};

class Item_func_st_intersection final : public Item_func_spatial_operation {
 public:
  Item_func_st_intersection(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_operation(pos, a, b) {}
  String *val_str(String *) override;
  const char *func_name() const override { return "st_intersection"; }
};

class Item_func_st_symdifference final : public Item_func_spatial_operation {
 public:
  Item_func_st_symdifference(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_operation(pos, a, b) {}
  String *val_str(String *) override;
  const char *func_name() const override { return "st_symdifference"; }
};

class Item_func_st_union final : public Item_func_spatial_operation {
 public:
  Item_func_st_union(const POS &pos, Item *a, Item *b)
      : Item_func_spatial_operation(pos, a, b) {}
  String *val_str(String *) override;
  const char *func_name() const override { return "st_union"; }
};

class Item_func_buffer_strategy : public Item_str_func {
 private:
  enum enum_buffer_strategies {
    invalid_strategy = 0,
    end_round,
    end_flat,
    join_round,
    join_miter,
    point_circle,
    point_square,
    max_strategy = point_square

    // Distance and side strategies are fixed, so no need to implement
    // parameterization for them.
  };

  friend class Item_func_buffer;
  String tmp_value;
  char tmp_buffer[16];  // The buffer for tmp_value.
 public:
  Item_func_buffer_strategy(const POS &pos, PT_item_list *ilist);
  const char *func_name() const override { return "st_buffer_strategy"; }
  String *val_str(String *) override;
  bool resolve_type(THD *thd) override;
};

class Item_func_isempty : public Item_bool_func {
 public:
  Item_func_isempty(const POS &pos, Item *a) : Item_bool_func(pos, a) {}
  longlong val_int() override;
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_NONE; }
  const char *func_name() const override { return "st_isempty"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    set_nullable(true);
    return false;
  }
};

class Item_func_st_issimple : public Item_bool_func {
 public:
  Item_func_st_issimple(const POS &pos, Item *a) : Item_bool_func(pos, a) {}
  longlong val_int() override;
  const char *func_name() const override { return "st_issimple"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_bool_func::resolve_type(thd);
  }
};

class Item_func_isclosed : public Item_bool_func {
 public:
  Item_func_isclosed(const POS &pos, Item *a) : Item_bool_func(pos, a) {}
  longlong val_int() override;
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_NONE; }
  const char *func_name() const override { return "st_isclosed"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    set_nullable(true);
    return false;
  }
};

class Item_func_isvalid : public Item_bool_func {
 public:
  Item_func_isvalid(const POS &pos, Item *a) : Item_bool_func(pos, a) {}
  longlong val_int() override;
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_NONE; }
  const char *func_name() const override { return "st_isvalid"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_bool_func::resolve_type(thd);
  }
};

class Item_func_dimension : public Item_int_func {
  String value;

 public:
  Item_func_dimension(const POS &pos, Item *a) : Item_int_func(pos, a) {}
  longlong val_int() override;
  const char *func_name() const override { return "st_dimension"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    max_length = 10;
    set_nullable(true);
    return false;
  }
};

/// The abstract superclass for all geometry coordinate mutator functions (ST_X,
/// ST_Y, ST_Latitude and ST_Longitude with two parameters).
///
/// @see Item_func_coordinate_observer
class Item_func_coordinate_mutator : public Item_geometry_func {
 public:
  Item_func_coordinate_mutator(const POS &pos, Item *a, Item *b,
                               bool geographic_only)
      : Item_geometry_func(pos, a, b), m_geographic_only(geographic_only) {}
  String *val_str(String *) override;

 protected:
  const char *func_name() const override = 0;
  /// Returns the coordinate number accessed by this item.
  ///
  /// @param[in] srs The spatial reference system of the point.
  ///
  /// @return The coordinate number to access.
  virtual int coordinate_number(
      const dd::Spatial_reference_system *srs) const = 0;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_DOUBLE)) return true;
    return Item_geometry_func::resolve_type(thd);
  }

 private:
  /// Whether this item will accept only geographic geometries/SRSs.
  bool m_geographic_only;
};

/// The abstract superclass for all geometry coordinate oberserver functions
/// (ST_X, ST_Y, ST_Latitude, ST_Longitude with one parameter).
///
/// @see Item_func_coordinate_mutator
class Item_func_coordinate_observer : public Item_real_func {
 public:
  Item_func_coordinate_observer(const POS &pos, Item *a, bool geographic_only)
      : Item_real_func(pos, a), m_geographic_only(geographic_only) {}
  double val_real() override;

 protected:
  const char *func_name() const override = 0;
  /// Returns the coordinate number accessed by this item.
  ///
  /// @param[in] srs The spatial reference system of the point.
  ///
  /// @return The coordinate number to access.
  virtual int coordinate_number(
      const dd::Spatial_reference_system *srs) const = 0;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_real_func::resolve_type(thd);
  }

 private:
  /// Whether this item will accept only geographic geometries/SRSs.
  bool m_geographic_only;
};

/// This class implements the two-parameter ST_Latitude function which sets the
/// latitude of a geographic point.
class Item_func_st_latitude_mutator final
    : public Item_func_coordinate_mutator {
 public:
  Item_func_st_latitude_mutator(const POS &pos, Item *a, Item *b)
      : Item_func_coordinate_mutator(pos, a, b, true) {}

 protected:
  const char *func_name() const override { return "st_latitude"; }
  int coordinate_number(const dd::Spatial_reference_system *) const override {
    return 1;
  }
};

/// This class implements the one-parameter ST_Latitude function which returns
/// the latitude coordinate of a geographic point.
class Item_func_st_latitude_observer final
    : public Item_func_coordinate_observer {
 public:
  Item_func_st_latitude_observer(const POS &pos, Item *a)
      : Item_func_coordinate_observer(pos, a, true) {}

 protected:
  const char *func_name() const override { return "st_latitude"; }
  int coordinate_number(const dd::Spatial_reference_system *) const override {
    return 1;
  }
};

/// This class implements the two-parameter ST_Longitude function which sets the
/// longitude coordinate of a point.
class Item_func_st_longitude_mutator final
    : public Item_func_coordinate_mutator {
 public:
  Item_func_st_longitude_mutator(const POS &pos, Item *a, Item *b)
      : Item_func_coordinate_mutator(pos, a, b, true) {}

 protected:
  const char *func_name() const override { return "st_longitude"; }
  int coordinate_number(const dd::Spatial_reference_system *) const override {
    return 0;
  }
};

/// This class implements the one-parameter ST_Longitude function which returns
/// the longitude coordinate of a geographic point.
class Item_func_st_longitude_observer final
    : public Item_func_coordinate_observer {
 public:
  Item_func_st_longitude_observer(const POS &pos, Item *a)
      : Item_func_coordinate_observer(pos, a, true) {}

 protected:
  const char *func_name() const override { return "st_longitude"; }
  int coordinate_number(const dd::Spatial_reference_system *) const override {
    return 0;
  }
};

/// This class implements the two-parameter ST_X function which sets the X
/// coordinate of a point.
class Item_func_st_x_mutator final : public Item_func_coordinate_mutator {
 public:
  Item_func_st_x_mutator(const POS &pos, Item *a, Item *b)
      : Item_func_coordinate_mutator(pos, a, b, false) {}

 protected:
  const char *func_name() const override { return "st_x"; }
  int coordinate_number(const dd::Spatial_reference_system *srs) const override;
};

/// This class implements the one-parameter ST_X function which returns the X
/// coordinate of a point.
class Item_func_st_x_observer final : public Item_func_coordinate_observer {
 public:
  Item_func_st_x_observer(const POS &pos, Item *a)
      : Item_func_coordinate_observer(pos, a, false) {}

 protected:
  const char *func_name() const override { return "st_x"; }
  int coordinate_number(const dd::Spatial_reference_system *srs) const override;
};

/// This class implements the two-parameter ST_Y function which sets the Y
/// coordinate of a point.
class Item_func_st_y_mutator final : public Item_func_coordinate_mutator {
 public:
  Item_func_st_y_mutator(const POS &pos, Item *a, Item *b)
      : Item_func_coordinate_mutator(pos, a, b, false) {}

 protected:
  const char *func_name() const override { return "st_y"; }
  int coordinate_number(const dd::Spatial_reference_system *srs) const override;
};

/// This class implements the one-parameter ST_Y function which returns the Y
/// coordinate of a point.
class Item_func_st_y_observer final : public Item_func_coordinate_observer {
 public:
  Item_func_st_y_observer(const POS &pos, Item *a)
      : Item_func_coordinate_observer(pos, a, false) {}

 protected:
  const char *func_name() const override { return "st_y"; }
  int coordinate_number(const dd::Spatial_reference_system *srs) const override;
};

class Item_func_swap_xy : public Item_geometry_func {
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    return Item_geometry_func::resolve_type(thd);
  }

 public:
  Item_func_swap_xy(const POS &pos, Item *a) : Item_geometry_func(pos, a) {}
  const char *func_name() const override { return "st_swapxy"; }
  String *val_str(String *) override;
};

class Item_func_numgeometries : public Item_int_func {
  String value;

 public:
  Item_func_numgeometries(const POS &pos, Item *a) : Item_int_func(pos, a) {}
  longlong val_int() override;
  const char *func_name() const override { return "st_numgeometries"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    max_length = 10;
    set_nullable(true);
    return false;
  }
};

class Item_func_numinteriorring : public Item_int_func {
  String value;

 public:
  Item_func_numinteriorring(const POS &pos, Item *a) : Item_int_func(pos, a) {}
  longlong val_int() override;
  const char *func_name() const override { return "st_numinteriorrings"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    max_length = 10;
    set_nullable(true);
    return false;
  }
};

class Item_func_numpoints : public Item_int_func {
  String value;

 public:
  Item_func_numpoints(const POS &pos, Item *a) : Item_int_func(pos, a) {}
  longlong val_int() override;
  const char *func_name() const override { return "st_numpoints"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    max_length = 10;
    set_nullable(true);
    return false;
  }
};

class Item_func_st_area : public Item_real_func {
 public:
  Item_func_st_area(const POS &pos, Item *a) : Item_real_func(pos, a) {}
  double val_real() override;
  const char *func_name() const override { return "st_area"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    // ST_Area returns NULL if the geometry is empty.
    set_nullable(true);
    return false;
  }
};

class Item_func_st_buffer : public Item_geometry_func {
 public:
  /// Parses strategy stored in String object, and sets values in strats.
  bool parse_strategy(String *arg, gis::BufferStrategies &strats);

  Item_func_st_buffer(const POS &pos, PT_item_list *ilist)
      : Item_geometry_func(pos, ilist) {}

  String *val_str(String *) override;
  const char *func_name() const override { return "st_buffer"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_DOUBLE)) return true;
    if (param_type_is_default(thd, 2, -1))
      return true;  // Does nothing with the strategy args
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_st_length : public Item_real_func {
  String value;

 public:
  Item_func_st_length(const POS &pos, PT_item_list *ilist)
      : Item_real_func(pos, ilist) {}
  double val_real() override;
  const char *func_name() const override { return "st_length"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    if (Item_real_func::resolve_type(thd)) return true;
    set_nullable(true);
    return false;
  }
};

/// This class implements the two-parameter ST_SRID function which sets
/// the SRID of a geometry.
class Item_func_st_srid_mutator : public Item_geometry_func {
 public:
  Item_func_st_srid_mutator(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}
  String *val_str(String *) override;
  const char *func_name() const override { return "st_srid"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_LONGLONG)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

/// This class implements the one-parameter ST_SRID function which
/// returns the SRID of a geometry.
class Item_func_st_srid_observer : public Item_int_func {
 public:
  Item_func_st_srid_observer(const POS &pos, Item *a) : Item_int_func(pos, a) {}
  longlong val_int() override;
  const char *func_name() const override { return "st_srid"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    bool error = Item_int_func::resolve_type(thd);
    max_length = 10;
    return error;
  }
};

class Item_func_distance : public Item_real_func {
  double geometry_collection_distance(const Geometry *g1, const Geometry *g2);

  template <typename Coordsys, typename BG_geometry>
  double distance_dispatch_second_geometry(const BG_geometry &bg1,
                                           const Geometry *g2);

 public:
  template <typename Coordsys>
  double bg_distance(const Geometry *g1, const Geometry *g2);

  Item_func_distance(const POS &pos, PT_item_list *ilist)
      : Item_real_func(pos, ilist) {
    /*
      Either operand can be an empty geometry collection, and it's meaningless
      for a distance between them.
    */
    set_nullable(true);
  }

  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY)) return true;
    set_nullable(true);
    return false;
  }

  double val_real() override;
  const char *func_name() const override { return "st_distance"; }
};

class Item_func_st_frechet_distance : public Item_real_func {
 public:
  Item_func_st_frechet_distance(const POS &pos, PT_item_list *ilist)
      : Item_real_func(pos, ilist) {}
  double val_real() override;
  const char *func_name() const override { return "st_frechetdistance"; }
  bool resolve_type(THD *thd) override {
    param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY);
    if (Item_real_func::resolve_type(thd)) return true;
    set_nullable(true);
    return false;
  }
};

class Item_func_st_hausdorff_distance : public Item_real_func {
 public:
  Item_func_st_hausdorff_distance(const POS &pos, PT_item_list *ilist)
      : Item_real_func(pos, ilist) {}
  double val_real() override;
  const char *func_name() const override { return "st_hausdorffdistance"; }
  bool resolve_type(THD *thd) override {
    param_type_is_default(thd, 0, -1, MYSQL_TYPE_GEOMETRY);
    if (Item_real_func::resolve_type(thd)) return true;
    set_nullable(true);
    return false;
  }
};

class Item_func_st_distance_sphere : public Item_real_func {
 public:
  Item_func_st_distance_sphere(const POS &pos, PT_item_list *ilist)
      : Item_real_func(pos, ilist) {}
  double val_real() override;
  const char *func_name() const override { return "st_distance_sphere"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 2, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 2, 3, MYSQL_TYPE_DOUBLE)) return true;
    return Item_real_func::resolve_type(thd);
  }
};

// The abstract superclass for interpolating point(s) along a line.
class Item_func_lineinterpolate : public Item_geometry_func {
 public:
  Item_func_lineinterpolate(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}
  String *val_str(String *str) override;

 protected:
  virtual bool isFractionalDistance() const = 0;
  virtual bool returnMultiplePoints() const = 0;
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_DOUBLE)) return true;
    return Item_geometry_func::resolve_type(thd);
  }
};

class Item_func_lineinterpolatepoint final : public Item_func_lineinterpolate {
 public:
  Item_func_lineinterpolatepoint(const POS &pos, Item *a, Item *b)
      : Item_func_lineinterpolate(pos, a, b) {}

 protected:
  const char *func_name() const override { return "st_lineinterpolatepoint"; }
  bool isFractionalDistance() const override { return true; }
  bool returnMultiplePoints() const override { return false; }
};

class Item_func_lineinterpolatepoints final : public Item_func_lineinterpolate {
 public:
  Item_func_lineinterpolatepoints(const POS &pos, Item *a, Item *b)
      : Item_func_lineinterpolate(pos, a, b) {}

 protected:
  const char *func_name() const override { return "st_lineinterpolatepoints"; }
  bool isFractionalDistance() const override { return true; }
  bool returnMultiplePoints() const override { return true; }
};

class Item_func_st_pointatdistance final : public Item_func_lineinterpolate {
 public:
  Item_func_st_pointatdistance(const POS &pos, Item *a, Item *b)
      : Item_func_lineinterpolate(pos, a, b) {}

 protected:
  const char *func_name() const override { return "st_pointatdistance"; }
  bool isFractionalDistance() const override { return false; }
  bool returnMultiplePoints() const override { return false; }
};

/// This class implements ST_Transform function that transforms a geometry from
/// one SRS to another.
class Item_func_st_transform final : public Item_geometry_func {
 public:
  Item_func_st_transform(const POS &pos, Item *a, Item *b)
      : Item_geometry_func(pos, a, b) {}
  String *val_str(String *str) override;

 private:
  const char *func_name() const override { return "st_transform"; }
};

// This is an abstract class that is inherited by geometry cast items.
class Item_typecast_geometry : public Item_geometry_func {
 public:
  Item_typecast_geometry(const POS &pos, Item *a)
      : Item_geometry_func(pos, a) {}
  String *val_str(String *str) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override = 0;
  const char *func_name() const override = 0;
  enum Functype functype() const override { return TYPECAST_FUNC; }
  Field::geometry_type get_geometry_type() const override = 0;
  bool resolve_type(THD *thd) override {
    param_type_is_default(thd, 0, 1, MYSQL_TYPE_GEOMETRY);
    return Item_geometry_func::resolve_type(thd);
  }

  /// Casts certain geometry types to certain target geometry types.
  ///
  /// @param[in] srs The srs of the geometry being cast.
  /// @param[in] source_geometry The geometry being cast.
  /// @param[out] target_geometry The result geometry of the cast.
  ///
  /// @retval false Success.
  /// @retval true An error has occurred. The error has been reported with
  /// my_error().

  virtual bool cast(const dd::Spatial_reference_system *srs,
                    std::unique_ptr<gis::Geometry> *source_geometry,
                    std::unique_ptr<gis::Geometry> *target_geometry) const = 0;
};

// This class implements CAST from certain geometries to POINT type.
class Item_typecast_point : public Item_typecast_geometry {
 public:
  Item_typecast_point(const POS &pos, Item *a)
      : Item_typecast_geometry(pos, a) {}
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const char *func_name() const override { return "cast_as_point"; }
  Field::geometry_type get_geometry_type() const override;
  bool cast(const dd::Spatial_reference_system *,
            std::unique_ptr<gis::Geometry> *source_geometry,
            std::unique_ptr<gis::Geometry> *target_geometry) const override;
};

// This class implements CAST from certain geometries to LINESTRING type.
class Item_typecast_linestring : public Item_typecast_geometry {
 public:
  Item_typecast_linestring(const POS &pos, Item *a)
      : Item_typecast_geometry(pos, a) {}
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const char *func_name() const override { return "cast_as_linestring"; }
  Field::geometry_type get_geometry_type() const override;
  bool cast(const dd::Spatial_reference_system *,
            std::unique_ptr<gis::Geometry> *source_geometry,
            std::unique_ptr<gis::Geometry> *target_geometry) const override;
};

// This class implements CAST from certain geometries to POLYGON type.
class Item_typecast_polygon : public Item_typecast_geometry {
 public:
  Item_typecast_polygon(const POS &pos, Item *a)
      : Item_typecast_geometry(pos, a) {}
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const char *func_name() const override { return "cast_as_polygon"; }
  Field::geometry_type get_geometry_type() const override;
  bool cast(const dd::Spatial_reference_system *srs,
            std::unique_ptr<gis::Geometry> *source_geometry,
            std::unique_ptr<gis::Geometry> *target_geometry) const override;
};

// This class implements CAST from certain geometries to MULTIPOINT type.
class Item_typecast_multipoint : public Item_typecast_geometry {
 public:
  Item_typecast_multipoint(const POS &pos, Item *a)
      : Item_typecast_geometry(pos, a) {}
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const char *func_name() const override { return "cast_as_multipoint"; }
  Field::geometry_type get_geometry_type() const override;
  bool cast(const dd::Spatial_reference_system *,
            std::unique_ptr<gis::Geometry> *source_geometry,
            std::unique_ptr<gis::Geometry> *target_geometry) const override;
};

// This class implements CAST from certain geometries to MULTILINESTRING type.
class Item_typecast_multilinestring : public Item_typecast_geometry {
 public:
  Item_typecast_multilinestring(const POS &pos, Item *a)
      : Item_typecast_geometry(pos, a) {}
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const char *func_name() const override { return "cast_as_multilinestring"; }
  Field::geometry_type get_geometry_type() const override;
  bool cast(const dd::Spatial_reference_system *,
            std::unique_ptr<gis::Geometry> *source_geometry,
            std::unique_ptr<gis::Geometry> *target_geometry) const override;
};

// This class implements CAST from certain geometries to MULTIPOLYGON type.
class Item_typecast_multipolygon : public Item_typecast_geometry {
 public:
  Item_typecast_multipolygon(const POS &pos, Item *a)
      : Item_typecast_geometry(pos, a) {}
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const char *func_name() const override { return "cast_as_multipolygon"; }
  Field::geometry_type get_geometry_type() const override;
  bool cast(const dd::Spatial_reference_system *srs,
            std::unique_ptr<gis::Geometry> *source_geometry,
            std::unique_ptr<gis::Geometry> *target_geometry) const override;
};

// This class implements CAST from certain geometries to GEOMETRYCOLLECTION
// type.
class Item_typecast_geometrycollection : public Item_typecast_geometry {
 public:
  Item_typecast_geometrycollection(const POS &pos, Item *a)
      : Item_typecast_geometry(pos, a) {}
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const char *func_name() const override {
    return "cast_as_geometrycollection";
  }
  Field::geometry_type get_geometry_type() const override;
  bool cast(const dd::Spatial_reference_system *,
            std::unique_ptr<gis::Geometry> *source_geometry,
            std::unique_ptr<gis::Geometry> *target_geometry) const override;
};

#endif /*ITEM_GEOFUNC_INCLUDED*/

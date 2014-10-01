/* Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file

  @brief
  This file defines all spatial functions
*/
#include "my_config.h"

#include <sstream>
#include <string>
#include <set>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "sql_priv.h"
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          // THD, set_var.h: THD
#include "set_var.h"
#include <m_ctype.h>
#include "parse_tree_helpers.h"
#include "spatial.h"
#include "gis_bg_traits.h"
#include <boost/geometry/geometry.hpp>
#include <memory>
#include <rapidjson/document.h>

// GCC requires typename whenever needing to access a type inside a template,
// but MSVC forbids this.
#ifdef HAVE_IMPLICIT_DEPENDENT_NAME_TYPING
#define TYPENAME
#else
#define TYPENAME typename
#endif


/// A wrapper and interface for all geometry types used here. Make these
/// types as localized as possible. It's used as a type interface.
/// @tparam CoordinateElementType The numeric type for a coordinate value,
///         most often it's double.
/// @tparam CoordinateSystemType Coordinate system type, specified using
//          those defined in boost::geometry::cs.
template<typename CoordinateElementType, typename CoordinateSystemType>
class BG_models
{
public:
  typedef Gis_point Point;
  // An counter-clockwise, closed Polygon type. It can hold open Polygon data,
  // but not clockwise ones, otherwise things can go wrong, e.g. intersection.
  typedef Gis_polygon Polygon;
  typedef Gis_line_string Linestring;
  typedef Gis_multi_point Multipoint;
  typedef Gis_multi_line_string Multilinestring;
  typedef Gis_multi_polygon Multipolygon;

  typedef CoordinateElementType Coord_type;
  typedef CoordinateSystemType Coordsys;
};

namespace bgm= boost::geometry::model;
namespace bgcs= boost::geometry::cs;

template <typename Point_range>
static bool is_colinear(const Point_range &ls);


Item_geometry_func::Item_geometry_func(const POS &pos, PT_item_list *list)
  :Item_str_func(pos, list)
{}


Field *Item_geometry_func::tmp_table_field(TABLE *t_arg)
{
  Field *result;
  if ((result= new Field_geom(max_length, maybe_null, item_name.ptr(), t_arg->s,
                              get_geometry_type())))
    result->init(t_arg);
  return result;
}

void Item_geometry_func::fix_length_and_dec()
{
  collation.set(&my_charset_bin);
  decimals=0;
  max_length= 0xFFFFFFFFU;
  maybe_null= 1;
}


bool Item_func_geometry_from_text::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  DBUG_ASSERT(arg_count == 1 || arg_count == 2);
  if (arg_count == 1)
    pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_RAND);
  return false;
}


/**
  Parses a WKT string to produce a geometry encoded with an SRID prepending
  its WKB bytes, namely a byte string of GEOMETRY format.
  @param str buffer to hold result, may not be filled.
  @return the buffer that hold the GEOMETRY byte string result, may or may
  not be the same as 'str' parameter.
 */
String *Item_func_geometry_from_text::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  Geometry_buffer buffer;
  String arg_val;
  String *wkt= args[0]->val_str_ascii(&arg_val);

  if ((null_value= (!wkt || args[0]->null_value)))
    return 0;

  Gis_read_stream trs(wkt->charset(), wkt->ptr(), wkt->length());
  uint32 srid= 0;

  if ((arg_count == 2) && !args[1]->null_value)
    srid= (uint32)args[1]->val_int();

  str->set_charset(&my_charset_bin);
  if ((null_value= str->reserve(GEOM_HEADER_SIZE, 512)))
    return 0;
  str->length(0);
  str->q_append(srid);
  if (!Geometry::create_from_wkt(&buffer, &trs, str, 0))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }
  return str;
}


bool Item_func_geometry_from_wkb::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  DBUG_ASSERT(arg_count == 1 || arg_count == 2);
  if (arg_count == 1)
    pc->thd->lex->set_uncacheable(pc->select, UNCACHEABLE_RAND);
  return false;
}


/**
  Parses a WKT string to produce a geometry encoded with an SRID prepending
  its WKB bytes, namely a byte string of GEOMETRY format.
  @param str buffer to hold result, may not be filled.
  @return the buffer that hold the GEOMETRY byte string result, may or may
  not be the same as 'str' parameter.
 */
String *Item_func_geometry_from_wkb::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *wkb= NULL;
  uint32 srid= 0;

  if (arg_count == 2)
  {
    srid= static_cast<uint32>(args[1]->val_int());
    if ((null_value= args[1]->null_value))
      return NULL;
  }

  wkb= args[0]->val_str(&tmp_value);
  if ((null_value= (!wkb || args[0]->null_value)))
    return NULL;

  /*
    GeometryFromWKB(wkb [,srid]) understands both WKB (without SRID) and
    Geometry (with SRID) values in the "wkb" argument.
    In case if a Geometry type value is passed, we assume that the value
    is well-formed and can directly return it without going through
    Geometry::create_from_wkb(), and consequently such WKB data must be
    MySQL standard (little) endian. Note that users can pass via client
    any WKB/Geometry byte string, including those of big endianess.
  */
  if (args[0]->field_type() == MYSQL_TYPE_GEOMETRY)
  {
    Geometry_buffer buff;
    if (Geometry::construct(&buff, wkb->ptr(), wkb->length()) == NULL)
    {
      my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
      return error_str();
    }

    /*
      Check if SRID embedded into the Geometry value differs
      from the SRID value passed in the second argument.
    */
    if (srid == uint4korr(wkb->ptr()))
      return wkb; // Do not differ

    /*
      Replace SRID to the one passed in the second argument.
      Note, we cannot replace SRID directly in wkb->ptr(),
      because wkb can point to some value that we should not touch,
      e.g. to a SP variable value. So we need to copy to "str".
    */
    if ((null_value= str->copy(*wkb)))
      return NULL;
    str->write_at_position(0, srid);
    return str;
  }

  str->set_charset(&my_charset_bin);
  if (str->reserve(GEOM_HEADER_SIZE, 512))
  {
    null_value= true;                           /* purecov: inspected */
    return NULL;                                   /* purecov: inspected */
  }
  str->length(0);
  str->q_append(srid);
  Geometry_buffer buffer;
  if (!Geometry::create_from_wkb(&buffer, wkb->ptr(), wkb->length(), str,
                                 false/* Don't init stream. */))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  return str;
}


/**
  Definition of various string constants used for writing and reading
  GeoJSON data.
*/
const char *Item_func_geomfromgeojson::TYPE_MEMBER= "type";
const char *Item_func_geomfromgeojson::CRS_MEMBER= "crs";
const char *Item_func_geomfromgeojson::GEOMETRY_MEMBER= "geometry";
const char *Item_func_geomfromgeojson::PROPERTIES_MEMBER= "properties";
const char *Item_func_geomfromgeojson::FEATURES_MEMBER= "features";
const char *Item_func_geomfromgeojson::GEOMETRIES_MEMBER= "geometries";
const char *Item_func_geomfromgeojson::COORDINATES_MEMBER= "coordinates";
const char *Item_func_geomfromgeojson::CRS_NAME_MEMBER= "name";
const char *Item_func_geomfromgeojson::NAMED_CRS= "name";
const char *Item_func_geomfromgeojson::SHORT_EPSG_PREFIX= "EPSG:";
const char *Item_func_geomfromgeojson::POINT_TYPE= "Point";
const char *Item_func_geomfromgeojson::MULTIPOINT_TYPE= "MultiPoint";
const char *Item_func_geomfromgeojson::LINESTRING_TYPE= "LineString";
const char *Item_func_geomfromgeojson::MULTILINESTRING_TYPE= "MultiLineString";
const char *Item_func_geomfromgeojson::POLYGON_TYPE= "Polygon";
const char *Item_func_geomfromgeojson::MULTIPOLYGON_TYPE= "MultiPolygon";
const char *Item_func_geomfromgeojson::FEATURE_TYPE= "Feature";
const char *Item_func_geomfromgeojson::
FEATURECOLLECTION_TYPE= "FeatureCollection";
const char *Item_func_geomfromgeojson::
LONG_EPSG_PREFIX= "urn:ogc:def:crs:EPSG::";
const char *Item_func_geomfromgeojson::
CRS84_URN= "urn:ogc:def:crs:OGC:1.3:CRS84";
const char *Item_func_geomfromgeojson::
GEOMETRYCOLLECTION_TYPE= "GeometryCollection";


/**
  <geometry> = ST_GEOMFROMGEOJSON(<string>[, <options>[, <srid>]])

  Takes a GeoJSON input string and outputs a GEOMETRY.
  This function supports both single GeoJSON objects and geometry collections.

  In addition, feature objects and feature collections are supported (feature
  collections are translated into GEOMETRYCOLLECTION).

  It follows the standard described at http://geojson.org/geojson-spec.html
  (revision 1.0).
*/
String *Item_func_geomfromgeojson::val_str(String *buf)
{
  DBUG_ASSERT(m_srid_found_in_document == -1);

  String arg_val;
  String *json_string= args[0]->val_str_ascii(&arg_val);
  if ((null_value= args[0]->null_value))
    return NULL;

  if (arg_count > 1)
  {
    // Check and parse the OPTIONS parameter.
    longlong dimension_argument= args[1]->val_int();
    if ((null_value= args[1]->null_value))
      return NULL;

    if (dimension_argument == 1)
    {
      m_handle_coordinate_dimension= Item_func_geomfromgeojson::reject_document;
    }
    else if (dimension_argument == 2)
    {
      m_handle_coordinate_dimension=
        Item_func_geomfromgeojson::strip_now_accept_future;
    }
    else if (dimension_argument == 3)
    {
      m_handle_coordinate_dimension=
        Item_func_geomfromgeojson::strip_now_reject_future;
    }
    else if (dimension_argument == 4)
    {
      m_handle_coordinate_dimension=
        Item_func_geomfromgeojson::strip_now_strip_future;
    }
    else
    {
      char option_string[MAX_BIGINT_WIDTH + 1];
      llstr(dimension_argument, option_string);

      my_error(ER_WRONG_VALUE_FOR_TYPE, MYF(0), "option", option_string,
               func_name());
      return error_str();
    }
  }

  if (arg_count > 2)
  {
    /*
      Check and parse the SRID parameter. If this is set to a valid value,
      any CRS member in the GeoJSON document will be ignored.
    */
    longlong srid_argument= args[2]->val_int();
    if ((null_value= args[2]->null_value))
      return NULL;

    // Only allow unsigned 32 bits integer as SRID.
    if (srid_argument < 0 || srid_argument > UINT_MAX32)
    {
      char srid_string[MAX_BIGINT_WIDTH + 1];
      llstr(srid_argument, srid_string);

      my_error(ER_WRONG_VALUE_FOR_TYPE, MYF(0), "SRID", srid_string,
               func_name());
      return error_str();
    }
    else
    {
      m_user_srid= static_cast<Geometry::srid_t>(srid_argument);
      m_user_provided_srid= true;
    }
  }

  /*
    If this parsing fails, the document is not a valid JSON document.
    The root element must be a object, according to GeoJSON specification.
  */
  if (m_document.ParseInsitu<0>(json_string->c_ptr_safe()).HasParseError() ||
      !m_document.IsObject())
  {
    my_error(ER_INVALID_JSON_DATA, MYF(0), func_name(),
             m_document.GetParseError());
    return error_str();
  }

  /*
    Set the default SRID to 4326. This will be overwritten if a valid CRS is
    found in the GeoJSON input, or if the user has specified a SRID as an
    argument.

    It would probably be smart to allocate a percentage of the length of the
    input string (something like buf->realloc(json_string->length() * 0.2)).
    This would save a lot of reallocations and boost performance, especially for
    large inputs. But it is difficult to predict how much of the json input that
    will be parsed into output data.
  */
  if (buf->reserve(GEOM_HEADER_SIZE, 512))
  {
    my_error(ER_OUTOFMEMORY, GEOM_HEADER_SIZE);
    return error_str();
  }
  buf->set_charset(&my_charset_bin);
  buf->length(0);
  buf->q_append(static_cast<uint32>(4326));

  /*
    The rollback variable is used for detecting/accepting NULL objects inside
    collections (a feature with NULL geometry is allowed, and thus we can have
    a geometry collection with a NULL geometry translated into following WKT:
    GEOMETRYCOLLECTION()).

    parse_object() does a recursive parsing of the GeoJSON document.
  */
  String collection_buffer;
  bool rollback= false;
  Geometry *result_geometry= NULL;
  if (parse_object(&m_document, &rollback, &collection_buffer, false,
                   &result_geometry))
  {
    // Do a delete here, to be sure that we have no memory leaks.
    delete result_geometry;
    result_geometry= NULL;

    if (rollback)
    {
      null_value= true;
      return NULL;
    }
    return error_str();
  }

  // Set the correct SRID for the geometry data.
  if (m_user_provided_srid)
    buf->write_at_position(0, m_user_srid);
  else if (m_srid_found_in_document > -1)
    buf->write_at_position(0, static_cast<uint32>(m_srid_found_in_document));

  bool return_result= result_geometry->as_wkb(buf, false);

  delete result_geometry;
  result_geometry= NULL;

  if (return_result)
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }
  return buf;
}


/**
  Case insentitive lookup of a member in a rapidjson object.

  This is needed since the rapidjson library doesn't have a case insensitive
  variant of the method FindMember().

  @param v The object to look for the member in.
  @param member_name Name of the member to look after

  @return The member if one was found, NULL otherwise.
*/
rapidjson::Value::ConstMemberIterator
Item_func_geomfromgeojson::my_find_member_ncase(const rapidjson::Value *value,
                                                const char *member_name)
{
  DBUG_ASSERT(value->IsObject());
  rapidjson::Value::ConstMemberIterator itr;
  for (itr= value->MemberBegin(); itr != value->MemberEnd(); ++itr)
  {
    if (native_strcasecmp(member_name, itr->name.GetString()) == 0)
      return itr;
  }
  return NULL;
}


/**
  Takes a rapidjson object as input, and parses the data to a Geometry object.

  The call stack will be no larger than the maximum depth of the GeoJSON
  document, which is more or less equivalent to the number of nested
  collections in the document.

  @param object A rapidjson object to parse.
  @param rollback Pointer to a boolean indicating if parsed data should
         be reverted/rolled back.
  @param buffer A string buffer to be used by GeometryCollection
  @param is_parent_featurecollection Indicating if the current geometry is a
         child of a FeatureCollection.
  @param[out] geometry A pointer to the parsed geometry.

  @return true if the parsing failed, false otherwise. Note that if rollback is
          set to true and true is returned, the parsing succeeded, but no
          Geometry data could be parsed.
*/
bool Item_func_geomfromgeojson::
parse_object(const rapidjson::Value *object, bool *rollback, String *buffer,
             bool is_parent_featurecollection, Geometry **geometry)
{
  DBUG_ASSERT(object->IsObject());

  /*
    A GeoJSON object MUST have a type member, which MUST
    be of string type.
  */
  const rapidjson::Value::Member *type_member=
    my_find_member_ncase(object, TYPE_MEMBER);
  if (!is_member_valid(type_member, TYPE_MEMBER,
                       rapidjson::kStringType, false, NULL))
  {
    return true;
  }

  // Check if this object has a CRS member.
  const rapidjson::Value::Member *crs_member=
    my_find_member_ncase(object, CRS_MEMBER);
  if (crs_member != NULL)
  {
    if (parse_crs_object(&crs_member->value))
      return true;
  }

  // Handle feature objects and feature collection objects.
  if (strcmp(type_member->value.GetString(), FEATURE_TYPE) == 0)
  {
    /*
      Check if this feature object has the required "geometry" and "properties"
      member. Note that we do not use the member "properties" for anything else
      than checking for valid GeoJSON document.
    */
    bool dummy;
    const rapidjson::Value::Member *geometry_member=
      my_find_member_ncase(object, GEOMETRY_MEMBER);
    const rapidjson::Value::Member *properties_member=
      my_find_member_ncase(object, PROPERTIES_MEMBER);
    if (!is_member_valid(geometry_member, GEOMETRY_MEMBER,
                         rapidjson::kObjectType, true, rollback) ||
        !is_member_valid(properties_member, PROPERTIES_MEMBER,
                         rapidjson::kObjectType, true, &dummy) || *rollback)
    {
      return true;
    }
    return parse_object(&geometry_member->value, rollback, buffer, false,
                        geometry);
  }
  else if (strcmp(type_member->value.GetString(),
                  FEATURECOLLECTION_TYPE) == 0)
  {
    // FeatureCollections cannot be nested according to GeoJSON spec.
    if (is_parent_featurecollection)
    {
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }

    // We will handle a FeatureCollection as a GeometryCollection.
    const rapidjson::Value::Member *features=
      my_find_member_ncase(object, FEATURES_MEMBER);
    if (!is_member_valid(features, FEATURES_MEMBER,
                         rapidjson::kArrayType, false, NULL))
    {
      return true;
    }
    return parse_object_array(&features->value,
                              Geometry::wkb_geometrycollection, rollback,
                              buffer, true, geometry);
  }
  else
  {
    Geometry::wkbType wkbtype= get_wkbtype(type_member->value.GetString());
    if (wkbtype == Geometry::wkb_invalid_type)
    {
      // An invalid GeoJSON type was found.
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }
    else
    {
      /*
        All objects except GeometryCollection MUST have a member "coordinates"
        of type array. GeometryCollection MUST have a member "geometries" of
        type array.
      */
      const char *member_name;
      if (wkbtype == Geometry::wkb_geometrycollection)
        member_name= GEOMETRIES_MEMBER;
      else
        member_name= COORDINATES_MEMBER;

      const rapidjson::Value::Member *array_member=
        my_find_member_ncase(object, member_name);
      if (!is_member_valid(array_member, member_name, rapidjson::kArrayType,
                           false, NULL))
      {
        return true;
      }
      return parse_object_array(&array_member->value, wkbtype, rollback,
                                buffer, false, geometry);
    }
  }

  // Defensive code. This should never be reached.
  DBUG_ASSERT(false);
  return true;
}


/**
  Parse an array of coordinates to a Gis_point.

  Parses an array of coordinates to a Gis_point. This function must handle
  according to the handle_dimension parameter on how non 2D objects should be
  handled.

  According to the specification, a position array must have at least two
  elements, but there is no upper limit.

  @param coordinates rapidjson array of coordinates.
  @param[out] point A pointer to the parsed Gis_point.

  @return true if the parsing failed, false otherwise.
*/
bool Item_func_geomfromgeojson::
get_positions(const rapidjson::Value *coordinates, Gis_point *point)
{
  DBUG_ASSERT(coordinates->IsArray());
  /*
    According to GeoJSON specification, a position array must have at least
    two positions.
  */
  if (coordinates->Size() < 2)
  {
    my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
    return true;
  }

  switch (m_handle_coordinate_dimension)
  {
  case Item_func_geomfromgeojson::reject_document:
    if (coordinates->Size() > GEOM_DIM)
    {
      my_error(ER_DIMENSION_UNSUPPORTED, MYF(0), func_name(),
               coordinates->Size(), GEOM_DIM);
      return true;
    }
    break;
  case Item_func_geomfromgeojson::strip_now_reject_future:
    /*
      The version in development as of writing, only supports 2 dimensions.
      When dimension count is increased beyond 2, we want the function to fail.
    */
    if (GEOM_DIM > 2 && coordinates->Size() > 2)
    {
      my_error(ER_DIMENSION_UNSUPPORTED, MYF(0), func_name(),
               coordinates->Size(), GEOM_DIM);
      return true;
    }
    break;
  case Item_func_geomfromgeojson::strip_now_strip_future:
  case Item_func_geomfromgeojson::strip_now_accept_future:
    if (GEOM_DIM > 2)
      DBUG_ASSERT(false);
    break;
  default:
    // Unspecified behaviour.
    DBUG_ASSERT(false);
    return true;
  }

  // Check if all array members are numbers.
  int counter= 0;
  rapidjson::Value::ConstValueIterator itr;
  for (itr= coordinates->Begin(); itr != coordinates->End(); itr++)
  {
    if (!itr->IsNumber())
    {
      my_error(ER_INVALID_GEOJSON_WRONG_TYPE, MYF(0), func_name(),
               "array coordiante", "number");
      return true;
    }

    /*
      Even though we only need the two first coordinates, we check the rest of
      them to ensure that the GeoJSON is valid.
    */
    if (counter == 0)
      point->set<0>(itr->GetDouble());
    else if (counter == 1)
      point->set<1>(itr->GetDouble());
    counter++;
  }
  return false;
}


/**
  Takes a rapidjson array as input, does a recursive parsing and returns a
  Geometry object.

  This function differs from parse_object() in that it takes an array as input
  instead of a object. This is one of the members "coordinates" or "geometries"
  of a GeoJSON object.

  @param data_array A rapidjson array to parse.
  @param type The type of the GeoJSON object this array belongs to.
  @param rollback Pointer to a boolean indicating if parsed data should
         be reverted/rolled back.
  @param buffer A String buffer to be used by GeometryCollection.
  @param[out] geometry A pointer to the parsed Geometry.

  @return true on failure, false otherwise.
*/
bool Item_func_geomfromgeojson::
parse_object_array(const rapidjson::Value *data_array, Geometry::wkbType type,
                   bool *rollback, String *buffer,
                   bool is_parent_featurecollection, Geometry **geometry)
{
  DBUG_ASSERT(data_array->IsArray());
  switch (type)
  {
  case Geometry::wkb_geometrycollection:
    {
      /*
        Ensure that the provided buffer is empty, and then create a empty
        GeometryCollection using this buffer.
      */
      buffer->set_charset(&my_charset_bin);
      buffer->length(0);
      buffer->reserve(GEOM_HEADER_SIZE + SIZEOF_INT);
      write_geometry_header(buffer, 0, Geometry::wkb_geometrycollection, 0);

      Gis_geometry_collection *collection= new Gis_geometry_collection();
      *geometry= collection;

      collection->set_data_ptr(buffer->ptr() + GEOM_HEADER_SIZE, 4);
      collection->has_geom_header_space(true);
      for (rapidjson::Value::ConstValueIterator itr = data_array->Begin();
           itr != data_array->End(); ++itr)
      {
        if (!itr->IsObject())
        {
          my_error(ER_INVALID_GEOJSON_WRONG_TYPE, MYF(0), func_name(),
                   GEOMETRIES_MEMBER, "object array");
          return true;
        }

        String geo_buffer;
        Geometry *parsed_geometry= NULL;
        if (parse_object(itr, rollback, &geo_buffer,
            is_parent_featurecollection, &parsed_geometry))
        {
          /*
            This will happen if a feature object contains a NULL geometry
            object (which is a perfectly valid GeoJSON object).
          */
          if (*rollback)
          {
            *rollback= false;
          }
          else
          {
            delete parsed_geometry;
            parsed_geometry= NULL;

            return true;
          }
        }
        else
        {
          if (parsed_geometry->get_geotype() == Geometry::wkb_polygon)
          {
            // Make the Gis_polygon suitable for MySQL GIS code.
            Gis_polygon *polygon= static_cast<Gis_polygon*>(parsed_geometry);
            polygon->to_wkb_unparsed();
          }
          collection->append_geometry(parsed_geometry, buffer);
        }
        delete parsed_geometry;
        parsed_geometry= NULL;
      }
      return false;
    }
  case Geometry::wkb_point:
    {
      Gis_point *point= new Gis_point(false);
      *geometry= point;
      return get_positions(data_array, point);
    }
  case Geometry::wkb_linestring:
    {
      // Ensure that the LineString has at least one position.
      if (data_array->Empty())
      {
        my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
        return true;
      }

      Gis_line_string *linestring= new Gis_line_string(false);
      *geometry= linestring;

      if (get_linestring(data_array, linestring))
        return true;
      return false;
    }
  case Geometry::wkb_multipoint:
    {
      // Ensure that the MultiPoing has at least one Point.
      if (data_array->Empty())
      {
        my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
        return true;
      }

      Gis_multi_point *multipoint= new Gis_multi_point(false);
      *geometry= multipoint;

      for (rapidjson::Value::ConstValueIterator itr = data_array->Begin();
           itr != data_array->End(); ++itr)
      {
        if (!itr->IsArray())
        {
          my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
          return true;
        }
        else
        {
          Gis_point point;
          if (get_positions(itr, &point))
            return true;
          multipoint->push_back(point);
        }
      }
      return false;
    }
  case Geometry::wkb_multilinestring:
    {
      // Ensure that the MultiLineString has at least one LineString.
      if (data_array->Empty())
      {
        my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
        return true;
      }

      Gis_multi_line_string *multilinestring= new Gis_multi_line_string(false);
      *geometry= multilinestring;
      for (rapidjson::Value::ConstValueIterator itr = data_array->Begin();
           itr != data_array->End(); ++itr)
      {
        if (!itr->IsArray())
        {
          my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
          return true;
        }

        Gis_line_string linestring;
        if (get_linestring(itr, &linestring))
          return true;
        multilinestring->push_back(linestring);
      }
      return false;
    }
  case Geometry::wkb_polygon:
    {
      Gis_polygon *polygon= new Gis_polygon(false);
      *geometry= polygon;
      return get_polygon(data_array, polygon);
    }
  case Geometry::wkb_multipolygon:
    {
      // Ensure that the MultiPolygon has at least one Polygon.
      if (data_array->Empty())
      {
        my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
        return true;
      }

      Gis_multi_polygon *multipolygon= new Gis_multi_polygon(false);
      *geometry= multipolygon;

      for (rapidjson::Value::ConstValueIterator itr = data_array->Begin();
           itr != data_array->End(); ++itr)
      {
        if (!itr->IsArray())
        {
          my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
          return true;
        }
        Gis_polygon polygon;
        if (get_polygon(itr, &polygon))
          return true;
        multipolygon->push_back(polygon);
      }
      return false;
    }
  default:
    {
      DBUG_ASSERT(false);
      return false;
    }
  }
}


/**
  Create a Gis_line_string from a rapidjson array.

  @param data_array A rapidjson array containing the coordinates.
  @param linestring Pointer to a linestring to be filled with data.

  @return true on failure, false otherwise.
*/
bool
Item_func_geomfromgeojson::get_linestring(const rapidjson::Value *data_array,
                                          Gis_line_string *linestring)
{
  DBUG_ASSERT(data_array->IsArray());

  // Ensure that the linestring has at least one point.
  if (data_array->Empty())
  {
    my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
    return true;
  }

  for (rapidjson::Value::ConstValueIterator itr = data_array->Begin();
       itr != data_array->End(); ++itr)
  {
    if (!itr->IsArray())
    {
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }
    else
    {
      Gis_point point;
      if (get_positions(itr, &point))
        return true;
      linestring->push_back(point);
    }
  }
  return false;
}


/**
  Create a Gis_polygon from a rapidjson array.

  @param data_array A rapidjson array containing the coordinates.
  @param polygon A pointer to a Polygon to be filled with data.

  @return true on failure, false otherwise.
*/
bool Item_func_geomfromgeojson::get_polygon(const rapidjson::Value *data_array,
                                            Gis_polygon *polygon)
{
  DBUG_ASSERT(data_array->IsArray());

  // Ensure that the Polygon has at least one ring.
  if (data_array->Empty())
  {
    my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
    return true;
  }

  int ring_count= 0;
  for (rapidjson::Value::ConstValueIterator ring_itr = data_array->Begin();
       ring_itr != data_array->End(); ++ring_itr)
  {
    // Polygon rings must have at least four points, according to GeoJSON spec.
    if (!ring_itr->IsArray() || ring_itr->Size() < 4)
    {
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }

    polygon->inners().resize(ring_count);
    for (rapidjson::Value::ConstValueIterator point_itr= ring_itr->Begin();
         point_itr != ring_itr->End(); ++point_itr)
    {
      if (!point_itr->IsArray())
      {
        my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
        return true;
      }

      Gis_point point;
      if (get_positions(point_itr, &point))
        return true;

      if (ring_count == 0)
        polygon->outer().push_back(point);
      else
        polygon->inners()[ring_count - 1].push_back(point);
    }

    // Check if the ring is closed, which is must be according to GeoJSON spec.
    Gis_point first;
    Gis_point last;
    if (ring_count == 0)
    {
      first= polygon->outer()[0];
      last= polygon->outer().back();
    }
    else
    {
      first= polygon->inners()[ring_count - 1][0];
      last= polygon->inners()[ring_count - 1].back();
    }

    if (!(first == last))
    {
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }
    ring_count++;
  }
  return false;
}


/**
  Converts GeoJSON type string to a wkbType.

  Convert a string from a "type" member in GeoJSON to its equivalent Geometry
  enumeration. The type names are case sensitive as stated in the specification:

    The value of the type member must be one of: "Point", "MultiPoint",
    "LineString", "MultiLineString", "Polygon", "MultiPolygon",
    "GeometryCollection", "Feature", or "FeatureCollection". The case of the
    type member values must be as shown here.

  Note that even though Feature and FeatureCollection are added here, these
  types will be handled before this function is called (in parse_object()).

  @param typestring A GeoJSON type string.

  @return The corresponding wkbType, or wkb_invalid_type if no matching
          type was found.
*/
Geometry::wkbType Item_func_geomfromgeojson::get_wkbtype(const char *typestring)
{
  if (strcmp(typestring, POINT_TYPE) == 0)
    return Geometry::wkb_point;
  else if (strcmp(typestring, MULTIPOINT_TYPE) == 0)
    return Geometry::wkb_multipoint;
  else if (strcmp(typestring, LINESTRING_TYPE) == 0)
    return Geometry::wkb_linestring;
  else if (strcmp(typestring, MULTILINESTRING_TYPE) == 0)
    return Geometry::wkb_multilinestring;
  else if (strcmp(typestring, POLYGON_TYPE) == 0)
    return Geometry::wkb_polygon;
  else if (strcmp(typestring, MULTIPOLYGON_TYPE) == 0)
    return Geometry::wkb_multipolygon;
  else if (strcmp(typestring, GEOMETRYCOLLECTION_TYPE) == 0)
    return Geometry::wkb_geometrycollection;
  else
    return Geometry::wkb_invalid_type;
}


/**
  Takes a GeoJSON CRS object as input and parses it into a SRID. 

  If user has supplied a SRID, the parsing will be ignored.

  GeoJSON support two types of CRS objects; named and linked. Linked CRS will
  force us to download CRS parameters from the web, which we do not allow.
  Thus, we will only parse named CRS URNs in the"urn:ogc:def:crs:EPSG::<srid>"
  and "EPSG:<srid>" namespaces. In addition, "urn:ogc:def:crs:OGC:1.3:CRS84"
  will be recognized as SRID 4326. Note that CRS object with value JSON null is
  valid.

  @param crs_object A GeoJSON CRS object to parse.
  @param result The WKB string the result will be appended to.

  @return false if the parsing was successful, or true if it didn't understand
          the CRS object provided.
*/
bool Item_func_geomfromgeojson::
parse_crs_object(const rapidjson::Value *crs_object)
{
  if (m_user_provided_srid)
    return false;

  if (crs_object->IsNull())
  {
    return false;
  }
  else if (!crs_object->IsObject())
  {
    my_error(ER_INVALID_GEOJSON_WRONG_TYPE, MYF(0), func_name(),
             CRS_MEMBER, "object");
    return true;
  }

  /*
    Check if required CRS members "type" and "properties" exists, and that they
    are of correct type according to GeoJSON specification.
  */
  const rapidjson::Value::Member *type_member=
    my_find_member_ncase(crs_object, TYPE_MEMBER);
  const rapidjson::Value::Member *properties_member=
    my_find_member_ncase(crs_object, PROPERTIES_MEMBER);
  if (!is_member_valid(type_member, TYPE_MEMBER,
                       rapidjson::kStringType, false, NULL) ||
      !is_member_valid(properties_member, PROPERTIES_MEMBER,
                       rapidjson::kObjectType, false, NULL))
  {
    return true;
  }
  
  // Check that this CRS is a named CRS, and not a linked CRS.
  if (native_strcasecmp(type_member->value.GetString(),
                        NAMED_CRS) != 0)
  {
    // CRS object is not a named CRS.
    my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
    return true;
  }

  /*
    Check that CRS properties member has the required member "name"
    of type "string".
  */
  const rapidjson::Value::Member *crs_name_member=
    my_find_member_ncase(&properties_member->value, CRS_NAME_MEMBER);
  if (!is_member_valid(crs_name_member, CRS_NAME_MEMBER,
                       rapidjson::kStringType, false, NULL))
  {
    return true;
  }
  /*
    Now we can do the parsing of named CRS. The parsing happens as follows:

    1) Check if the named CRS is equal to urn:ogc:def:crs:OGC:1.3:CRS84". If so,
       return SRID 4326.
    2) Otherwise, check if we have a short or long format CRS URN in the
       EPSG namespace.
    3) If we have a CRS URN in the EPSG namespace, check if the ending after the
       last ':' is a valid SRID ("EPSG:<srid>" or
       "urn:ogc:def:crs:EPSG::<srid>"). An valid SRID must be greater than zero,
       and less than or equal to UINT_MAX32.
    4) If a SRID was returned from the parsing, check if we already have found
       a valid CRS earlier in the parsing. If so, and the SRID from the earlier
       CRS was different than the current, return an error to the user.

    If any of these fail, an error is returned to the user.
  */
  longlong parsed_srid= -1;
  if (native_strcasecmp(crs_name_member->value.GetString(), CRS84_URN) == 0)
  {
    parsed_srid= 4326;
  }
  else
  {
    size_t start_index;
    size_t name_length= crs_name_member->value.GetStringLength();
    const char *crs_name= crs_name_member->value.GetString();
    if (native_strncasecmp(crs_name, SHORT_EPSG_PREFIX, 5) == 0)
    {
      start_index= 5;
    }
    else if (native_strncasecmp(crs_name, LONG_EPSG_PREFIX, 22) == 0)
    {
      start_index= 22;
    }
    else
    {
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }

    char *end_of_parse;
    longlong parsed_value= strtoll(crs_name + start_index, &end_of_parse, 10);

    /*
      Check that the whole ending got parsed, and that the value is within
      valid SRID range.
    */
    if (end_of_parse == (crs_name + name_length) && parsed_value > 0 &&
        parsed_value <= UINT_MAX32)
    {
      parsed_srid= static_cast<uint32>(parsed_value);
    }
    else
    {
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }
  }

  if (parsed_srid > 0)
  {
    if (m_srid_found_in_document > 0 && parsed_srid != m_srid_found_in_document)
    {
      // A SRID has already been found, which had a different value.
      my_error(ER_INVALID_GEOJSON_UNSPECIFIED, MYF(0), func_name());
      return true;
    }
    else
    {
      m_srid_found_in_document= parsed_srid;
    }
  }
  return false;
}


/**
  Checks if a JSON member is valid based on input criteria.

  This function checks if the provided member exists, and if it's of the
  expected type. If it fails ome of the test, my_error() is called and false is
  returned from the function.

  @param member The member to validate.
  @param member_name Name of the member we are validating, so that the error
         returned to the user is more informative.
  @param expected_type Expected type of the member.
  @param allow_null If we shold allow the member to have JSON null value.
  @param[out] was_null This will be set to true if the provided member had a
              JSON null value. Is only affected if allow_null is set to true.

  @return true if the member is valid, false otherwise.
*/
bool Item_func_geomfromgeojson::
is_member_valid(const rapidjson::Value::Member *member, const char *member_name,
                rapidjson::Type expected_type, bool allow_null, bool *was_null)
{
  if (member == NULL)
  {
    my_error(ER_INVALID_GEOJSON_MISSING_MEMBER, MYF(0), func_name(),
             member_name);
    return false;
  }

  if (allow_null)
  {
    DBUG_ASSERT(was_null != NULL);
    *was_null= member->value.IsNull();
    if (*was_null)
      return true;
  }

  bool fail;
  const char *type_name;
  switch (expected_type)
  {
  case rapidjson::kObjectType:
    type_name= "object";
    fail= !member->value.IsObject();
    break;
  case rapidjson::kArrayType:
    type_name= "array";
    fail= !member->value.IsArray();
    break;
  case rapidjson::kStringType:
    type_name= "string";
    fail= !member->value.IsString();
    break;
  default:
    DBUG_ASSERT(false);
    return false;
  }

  if (fail)
  {
    my_error(ER_INVALID_GEOJSON_WRONG_TYPE, MYF(0), func_name(), member_name,
             type_name);
    return false;
  }
  return true;
}


void Item_func_geomfromgeojson::fix_length_and_dec()
{
  Item_geometry_func::fix_length_and_dec();
}


/**
  Checks if the supplied argument is a valid integer type.

  The function will fail if the supplied data is binary data. It will accept
  strings as integer type. Used for checking SRID and OPTIONS argument.

  @param argument The argument to check.

  @return true if the argument is a valid integer type, false otherwise.
*/
bool Item_func_geomfromgeojson::check_argument_valid_integer(Item *argument)
{
  bool is_binary_charset= (argument->collation.collation == &my_charset_bin);
  bool is_parameter_marker= (argument->type() == PARAM_ITEM);

  switch (argument->field_type())
  {
  case MYSQL_TYPE_NULL:
    return true;
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_VAR_STRING:
    return (!is_binary_charset || is_parameter_marker);
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_TINY:
    return true;
  default:
    return false;
  }
}


/**
  Do type checking on all provided arguments, as well as settings maybe_null to
  the appropriate value.
*/
bool Item_func_geomfromgeojson::fix_fields(THD *thd, Item **ref)
{
  if (Item_geometry_func::fix_fields(thd, ref))
    return true;

  switch (arg_count)
  {
  case 3:
    {
      // Validate SRID argument
      if (!check_argument_valid_integer(args[2]))
      {
        my_error(ER_INCORRECT_TYPE, MYF(0), "SRID", func_name());
        return true;
      }
      maybe_null= (args[0]->maybe_null || args[1]->maybe_null ||
                   args[2]->maybe_null);
    }
  case 2:
    {
      // Validate options argument
      if (!check_argument_valid_integer(args[1]))
      {
        my_error(ER_INCORRECT_TYPE, MYF(0), "options", func_name());
        return true;
      }
      maybe_null= (args[0]->maybe_null || args[1]->maybe_null);
    }
  case 1:
    {
      /*
        Validate GeoJSON argument type. We do not allow binary data as GeoJSON
        argument.
      */
      bool is_binary_charset= (args[0]->collation.collation == &my_charset_bin);
      bool is_parameter_marker= (args[0]->type() == PARAM_ITEM);
      switch (args[0]->field_type())
      {
      case MYSQL_TYPE_NULL:
        break;
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
        if (is_binary_charset && !is_parameter_marker)
        {
          my_error(ER_INCORRECT_TYPE, MYF(0), "geojson", func_name());
          return true;
        }
        break;
      default:
        my_error(ER_INCORRECT_TYPE, MYF(0), "geojson", func_name());
        return true;
      }
      maybe_null= args[0]->maybe_null;
      break;
    }
  }
  return false;
}


String *Item_func_as_wkt::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0;

  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  str->length(0);
  if ((null_value= geom->as_wkt(str)))
    return 0;

  return str;
}


void Item_func_as_wkt::fix_length_and_dec()
{
  collation.set(default_charset(), DERIVATION_COERCIBLE, MY_REPERTOIRE_ASCII);
  max_length=MAX_BLOB_WIDTH;
  maybe_null= 1;
}


String *Item_func_as_wkb::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0;

  if (!(Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  str->copy(swkb->ptr() + SRID_SIZE, swkb->length() - SRID_SIZE,
	    &my_charset_bin);
  return str;
}


String *Item_func_geometry_type::val_str_ascii(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String *swkb= args[0]->val_str(str);
  Geometry_buffer buffer;
  Geometry *geom= NULL;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0;

  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }
  /* String will not move */
  str->copy(geom->get_class_info()->m_name.str,
	    geom->get_class_info()->m_name.length,
	    default_charset());
  return str;
}


Field::geometry_type Item_func_envelope::get_geometry_type() const
{
  return Field::GEOM_POLYGON;
}


String *Item_func_envelope::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  uint32 srid;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  if (geom->get_geotype() != Geometry::wkb_geometrycollection &&
      geom->normalize_ring_order() == NULL)
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  srid= uint4korr(swkb->ptr());
  str->set_charset(&my_charset_bin);
  str->length(0);
  if (str->reserve(SRID_SIZE, 512))
    return 0;
  str->q_append(srid);
  return (null_value= geom->envelope(str)) ? 0 : str;
}


Field::geometry_type Item_func_centroid::get_geometry_type() const
{
  return Field::GEOM_POINT;
}


String *Item_func_centroid::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;

  if ((null_value= (!swkb || args[0]->null_value)))
    return NULL;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  str->set_charset(&my_charset_bin);

  if (geom->get_geotype() != Geometry::wkb_geometrycollection &&
      geom->normalize_ring_order() == NULL)
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  null_value= bg_centroid<bgcs::cartesian>(geom, str);
  if (null_value)
    return error_str();
  return str;
}


#define CATCH_ALL(funcname, expr) \
  catch (const boost::geometry::centroid_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_CENTROID_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const boost::geometry::overlay_invalid_input_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_OVERLAY_INVALID_INPUT_EXCEPTION, MYF(0),\
             (funcname));\
  }\
  catch (const boost::geometry::turn_info_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_TURN_INFO_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const boost::geometry::detail::self_get_turn_points::self_ip_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_SELF_INTERSECTION_POINT_EXCEPTION, MYF(0),\
             (funcname));\
  }\
  catch (const boost::geometry::empty_input_exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_EMPTY_INPUT_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const boost::geometry::exception &)\
  {\
    expr;\
    my_error(ER_BOOST_GEOMETRY_UNKNOWN_EXCEPTION, MYF(0), (funcname));\
  }\
  catch (const std::bad_alloc &e)\
  {\
    expr;\
    my_error(ER_STD_BAD_ALLOC_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::domain_error &e)\
  {\
    expr;\
    my_error(ER_STD_DOMAIN_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::length_error &e)\
  {\
    expr;\
    my_error(ER_STD_LENGTH_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::invalid_argument &e)\
  {\
    expr;\
    my_error(ER_STD_INVALID_ARGUMENT, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::out_of_range &e)\
  {\
    expr;\
    my_error(ER_STD_OUT_OF_RANGE_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::overflow_error &e)\
  {\
    expr;\
    my_error(ER_STD_OVERFLOW_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::range_error &e)\
  {\
    expr;\
    my_error(ER_STD_RANGE_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::underflow_error &e)\
  {\
    expr;\
    my_error(ER_STD_UNDERFLOW_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::logic_error &e)\
  {\
    expr;\
    my_error(ER_STD_LOGIC_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::runtime_error &e)\
  {\
    expr;\
    my_error(ER_STD_RUNTIME_ERROR, MYF(0), e.what(), (funcname));\
  }\
  catch (const std::exception &e)\
  {\
    expr;\
    my_error(ER_STD_UNKNOWN_EXCEPTION, MYF(0), e.what(), (funcname));\
  }\
  catch (...)\
  {\
    expr;\
    my_error(ER_GIS_UNKNOWN_EXCEPTION, MYF(0), (funcname));\
  }


/**
   Accumulate a geometry's all vertex points into a multipoint.
   It implements the WKB_scanner_event_handler interface so as to be registered
   into wkb_scanner and be notified of WKB data events.
 */
class Point_accumulator : public WKB_scanner_event_handler
{
  Gis_multi_point *m_mpts;
  const void *pt_start;
public:
  explicit Point_accumulator(Gis_multi_point *mpts)
    :m_mpts(mpts), pt_start(NULL)
  {
  }

  virtual void on_wkb_start(Geometry::wkbByteOrder bo,
                            Geometry::wkbType geotype,
                            const void *wkb, uint32 len, bool has_hdr)
  {
    if (geotype == Geometry::wkb_point)
    {
      Gis_point pt(wkb, POINT_DATA_SIZE,
                   Geometry::Flags_t(Geometry::wkb_point, len),
                   m_mpts->get_srid());
      m_mpts->push_back(pt);
      pt_start= wkb;
    }
  }


  virtual void on_wkb_end(const void *wkb)
  {
    if (pt_start)
      DBUG_ASSERT(static_cast<const char *>(pt_start) + POINT_DATA_SIZE == wkb);

    pt_start= NULL;
  }
};


/**
  Retrieve from a geometry collection geometries of the same base type into
  a multi-xxx geometry object. For example, group all points and multipoints
  into a single multipoint object, where the base type is point.

  @tparam Base_type the base type to group.
*/
template <typename Base_type>
class Geometry_grouper : public WKB_scanner_event_handler
{
  std::vector<Geometry::wkbType> m_types;
  std::vector<const void *> m_ptrs;

  typedef Gis_wkb_vector<Base_type> Group_type;
  Group_type *m_group;
  Gis_geometry_collection *m_collection;
  String *m_gcbuf;
  Geometry::wkbType m_target_type;

public:
  explicit Geometry_grouper(Group_type *out)
    :m_group(out), m_collection(NULL), m_gcbuf(NULL)
  {
    switch (out->get_type())
    {
    case Geometry::wkb_multipoint:
      m_target_type= Geometry::wkb_point;
      break;
    case Geometry::wkb_multilinestring:
      m_target_type= Geometry::wkb_linestring;
      break;
    case Geometry::wkb_multipolygon:
      m_target_type= Geometry::wkb_polygon;
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }
  }

  /*
    Group polygons and multipolygons into a geometry collection. 
  */ 
  Geometry_grouper(Gis_geometry_collection *out, String *gcbuf)
    :m_group(NULL), m_collection(out), m_gcbuf(gcbuf)
  {
    m_target_type= Geometry::wkb_polygon;
    DBUG_ASSERT(out != NULL && gcbuf != NULL);
  }


  virtual void on_wkb_start(Geometry::wkbByteOrder bo,
                            Geometry::wkbType geotype,
                            const void *wkb, uint32 len, bool has_hdr)
  {
    m_types.push_back(geotype);
    m_ptrs.push_back(wkb);

    if (m_types.size() == 1)
      DBUG_ASSERT(geotype == Geometry::wkb_geometrycollection);
  }


  virtual void on_wkb_end(const void *wkb_end)
  {
    Geometry::wkbType geotype= m_types.back();
    m_types.pop_back();

    const void *wkb_start= m_ptrs.back();
    m_ptrs.pop_back();

    if (geotype != m_target_type || m_types.size() == 0)
      return;

    Geometry::wkbType ptype= m_types.back();
    size_t len= static_cast<const char *>(wkb_end) -
      static_cast<const char *>(wkb_start);

    /*
      We only group independent geometries, points in linestrings or polygons
      are not independent, nor are linestrings in polygons.
     */
    if (m_target_type == geotype && m_group != NULL &&
        ((m_target_type == Geometry::wkb_point &&
          (ptype == Geometry::wkb_geometrycollection ||
           ptype == Geometry::wkb_multipoint)) ||
         (m_target_type == Geometry::wkb_linestring &&
          (ptype == Geometry::wkb_geometrycollection ||
           ptype == Geometry::wkb_multilinestring)) ||
         (m_target_type == Geometry::wkb_polygon &&
          (ptype == Geometry::wkb_geometrycollection ||
           ptype == Geometry::wkb_multipolygon))))
    {
      Base_type g(wkb_start, len, Geometry::Flags_t(m_target_type, 0), 0);
      m_group->push_back(g);
      DBUG_ASSERT(m_collection == NULL && m_gcbuf == NULL);
    }

    if (m_collection != NULL && (geotype == Geometry::wkb_polygon ||
                                 geotype == Geometry::wkb_multipolygon))
    {
      DBUG_ASSERT(m_group == NULL && m_gcbuf != NULL);
      String str(static_cast<const char *>(wkb_start), len, &my_charset_bin);
      m_collection->append_geometry(m_collection->get_srid(), geotype,
                                    &str, m_gcbuf);
    }
  }
};


/*
  Compute a geometry collection's centroid in demension decreasing order:
  If it has polygons, make them a multipolygon and compute its centroid as the
  result; otherwise compose a multilinestring and compute its centroid as the
  result; otherwise compose a multipoint and compute its centroid as the result.
  @param geom the geometry collection.
  @param[out] respt takes out the centroid point result.
  @param[out] null_value returns whether the result is NULL.
  @return whether got error, true if got error and false if successful.
*/
template <typename Coordsys>
bool geometry_collection_centroid(const Geometry *geom,
                                  typename BG_models<double, Coordsys>::
                                  Point *respt, my_bool *null_value)
{
  typename BG_models<double, Coordsys>::Multipolygon mplgn;
  Geometry_grouper<typename BG_models<double, Coordsys>::Polygon>
    plgn_grouper(&mplgn);

  const char *wkb_start= geom->get_cptr();
  uint32 wkb_len0, wkb_len= geom->get_data_size();
  *null_value= false;

  /*
    The geometries with largest dimension determine the centroid, because
    components of lower dimensions weighs nothing in comparison.
   */
  wkb_len0= wkb_len;
  wkb_scanner(wkb_start, &wkb_len,
              Geometry::wkb_geometrycollection, false, &plgn_grouper);
  if (mplgn.size() > 0)
  {
    if (mplgn.normalize_ring_order() == NULL)
      return true;

    boost::geometry::centroid(mplgn, *respt);
  }
  else
  {
    typename BG_models<double, Coordsys>::Multilinestring mls;
    wkb_len= wkb_len0;
    Geometry_grouper<typename BG_models<double, Coordsys>::Linestring>
      ls_grouper(&mls);
    wkb_scanner(wkb_start, &wkb_len,
                Geometry::wkb_geometrycollection, false, &ls_grouper);
    if (mls.size() > 0)
      boost::geometry::centroid(mls, *respt);
    else
    {
      typename BG_models<double, Coordsys>::Multipoint mpts;
      wkb_len= wkb_len0;
      Geometry_grouper<typename BG_models<double, Coordsys>::Point>
        pt_grouper(&mpts);
      wkb_scanner(wkb_start, &wkb_len,
                  Geometry::wkb_geometrycollection, false, &pt_grouper);
      if (mpts.size() > 0)
        boost::geometry::centroid(mpts, *respt);
      else
        *null_value= true;
    }
  }

  return false;
}


template <typename Coordsys>
bool Item_func_centroid::bg_centroid(const Geometry *geom, String *ptwkb)
{
  typename BG_models<double, Coordsys>::Point respt;

  // Release last call's result buffer.
  bg_resbuf_mgr.free_result_buffer();

  try
  {
    switch (geom->get_type())
    {
    case Geometry::wkb_point:
      {
        typename BG_models<double, Coordsys>::Point
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::centroid(geo, respt);
      }
      break;
    case Geometry::wkb_multipoint:
      {
        typename BG_models<double, Coordsys>::Multipoint
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::centroid(geo, respt);
      }
      break;
    case Geometry::wkb_linestring:
      {
        typename BG_models<double, Coordsys>::Linestring
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::centroid(geo, respt);
      }
      break;
    case Geometry::wkb_multilinestring:
      {
        typename BG_models<double, Coordsys>::Multilinestring
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::centroid(geo, respt);
      }
      break;
    case Geometry::wkb_polygon:
      {
        typename BG_models<double, Coordsys>::Polygon
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::centroid(geo, respt);
      }
      break;
    case Geometry::wkb_multipolygon:
      {
        typename BG_models<double, Coordsys>::Multipolygon
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::centroid(geo, respt);
      }
      break;
    case Geometry::wkb_geometrycollection:
      if (geometry_collection_centroid<Coordsys>(geom, &respt, &null_value))
      {
        my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
        null_value= true;
      }
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }

    respt.set_srid(geom->get_srid());
    if (!null_value)
      null_value= post_fix_result(&bg_resbuf_mgr, respt, ptwkb);

    bg_resbuf_mgr.set_result_buffer(const_cast<char *>(ptwkb->ptr()));
  }
  CATCH_ALL("st_centroid", null_value= true)

  return null_value;
}

Field::geometry_type Item_func_convex_hull::get_geometry_type() const
{
  return Field::GEOM_POLYGON;
}

String *Item_func_convex_hull::val_str(String *str)
{
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;

  if ((null_value= (!swkb || args[0]->null_value)))
    return NULL;

  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  DBUG_ASSERT(geom->get_coordsys() == Geometry::cartesian);
  str->set_charset(&my_charset_bin);
  str->length(0);

  if (geom->get_geotype() != Geometry::wkb_geometrycollection &&
      geom->normalize_ring_order() == NULL)
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  null_value= bg_convex_hull<bgcs::cartesian>(geom, str);
  if (null_value)
    return error_str();
  if (geom->get_type() == Geometry::wkb_point)
    str->takeover(*swkb);

  return str;
}


template <typename Coordsys>
bool Item_func_convex_hull::bg_convex_hull(const Geometry *geom,
                                           String *res_hull)
{
  typename BG_models<double, Coordsys>::Polygon hull;
  typename BG_models<double, Coordsys>::Linestring line_hull;
  Geometry::wkbType geotype= geom->get_type();

  // Release last call's result buffer.
  bg_resbuf_mgr.free_result_buffer();

  try
  {
    if (geotype == Geometry::wkb_multipoint ||
        geotype == Geometry::wkb_linestring ||
        geotype == Geometry::wkb_multilinestring ||
        geotype == Geometry::wkb_geometrycollection)
    {
      /*
        It's likely that the multilinestring, linestring, geometry collection
        and multipoint have all colinear points so the final hull is a
        linear hull. If so we must get the linear hull otherwise we will get
        an invalid polygon hull.
       */
      typename BG_models<double, Coordsys>::Multipoint mpts;
      Point_accumulator pt_acc(&mpts);
      const char *wkb_start= geom->get_cptr();
      uint32 wkb_len= geom->get_data_size();
      wkb_scanner(wkb_start, &wkb_len, geotype, false, &pt_acc);
      bool isdone= true;
      if (mpts.size() == 0)
        return (null_value= true);

      if (is_colinear(mpts))
      {
        boost::geometry::convex_hull(mpts, line_hull);
        line_hull.set_srid(geom->get_srid());
        null_value= post_fix_result(&bg_resbuf_mgr, line_hull, res_hull);
      }
      else if (geotype == Geometry::wkb_geometrycollection)
      {
        boost::geometry::convex_hull(mpts, hull);
        hull.set_srid(geom->get_srid());
        null_value= post_fix_result(&bg_resbuf_mgr, hull, res_hull);
      }
      else
        isdone= false;

      if (isdone)
      {
        bg_resbuf_mgr.set_result_buffer(const_cast<char *>(res_hull->ptr()));
        return false;
      }
    }

    /*
      From here on we don't have to consider linear hulls, it's impossible.

      In theory we can use above multipoint to get convex hull for all 7 types
      of geometries, however we'd better use BG standard logic for each type,
      a tricky example would be: imagine an invalid polygon whose inner ring is
      completely contains its outer ring inside, BG might return the outer ring
      but if using the multipoint to get convexhull, we would get the
      inner ring as result instead.
    */
    switch (geotype)
    {
    case Geometry::wkb_point:
      {
        /*
          A point's convex hull is the point itself, directly use the point's
          WKB buffer, set its header info correctly. 
        */
        DBUG_ASSERT(geom->get_ownmem() == false &&
                    geom->has_geom_header_space());
        char *p= geom->get_cptr() - GEOM_HEADER_SIZE;
        write_geometry_header(p, geom->get_srid(), geom->get_geotype());
        return false;
      }
      break;
    case Geometry::wkb_multipoint:
      {
        typename BG_models<double, Coordsys>::Multipoint
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::convex_hull(geo, hull);
      }
      break;
    case Geometry::wkb_linestring:
      {
        typename BG_models<double, Coordsys>::Linestring
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::convex_hull(geo, hull);
      }
      break;
    case Geometry::wkb_multilinestring:
      {
        typename BG_models<double, Coordsys>::Multilinestring
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::convex_hull(geo, hull);
      }
      break;
    case Geometry::wkb_polygon:
      {
        typename BG_models<double, Coordsys>::Polygon
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::convex_hull(geo, hull);
      }
      break;
    case Geometry::wkb_multipolygon:
      {
        typename BG_models<double, Coordsys>::Multipolygon
          geo(geom->get_data_ptr(), geom->get_data_size(),
              geom->get_flags(), geom->get_srid());
        boost::geometry::convex_hull(geo, hull);
      }
      break;
    case Geometry::wkb_geometrycollection:
      // Handled above.
      DBUG_ASSERT(false);
      break;
    default:
      break;
    }

    hull.set_srid(geom->get_srid());
    null_value= post_fix_result(&bg_resbuf_mgr, hull, res_hull);
    bg_resbuf_mgr.set_result_buffer(const_cast<char *>(res_hull->ptr()));
  }
  CATCH_ALL("st_convexhull", null_value= true)

  return null_value;
}


/*
  Spatial decomposition functions
*/

String *Item_func_spatial_decomp::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  uint32 srid;

  if ((null_value= (!swkb || args[0]->null_value)))
    return NULL;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  srid= uint4korr(swkb->ptr());
  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
    goto err;
  str->length(0);
  str->q_append(srid);
  switch (decomp_func) {
    case SP_STARTPOINT:
      if (geom->start_point(str))
        goto err;
      break;

    case SP_ENDPOINT:
      if (geom->end_point(str))
        goto err;
      break;

    case SP_EXTERIORRING:
      if (geom->exterior_ring(str))
        goto err;
      break;

    default:
      goto err;
  }
  return str;

err:
  null_value= 1;
  return 0;
}


String *Item_func_spatial_decomp_n::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_val;
  String *swkb= args[0]->val_str(&arg_val);
  long n= (long) args[1]->val_int();
  Geometry_buffer buffer;
  Geometry *geom= NULL;
  uint32 srid;

  if ((null_value= (!swkb || args[0]->null_value || args[1]->null_value)))
    return NULL;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_str();
  }

  str->set_charset(&my_charset_bin);
  if (str->reserve(SRID_SIZE, 512))
    goto err;
  srid= uint4korr(swkb->ptr());
  str->length(0);
  str->q_append(srid);
  switch (decomp_func_n)
  {
    case SP_POINTN:
      if (geom->point_n(n,str))
        goto err;
      break;

    case SP_GEOMETRYN:
      if (geom->geometry_n(n,str))
        goto err;
      break;

    case SP_INTERIORRINGN:
      if (geom->interior_ring_n(n,str))
        goto err;
      break;

    default:
      goto err;
  }
  return str;

err:
  null_value=1;
  return 0;
}


/*
  Functions to concatenate various spatial objects
*/


/*
*  Concatenate doubles into Point
*/


Field::geometry_type Item_func_point::get_geometry_type() const
{
  return Field::GEOM_POINT;
}


String *Item_func_point::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  double x= args[0]->val_real();
  double y= args[1]->val_real();
  uint32 srid= 0;

  if ((null_value= (args[0]->null_value ||
                    args[1]->null_value ||
                    str->mem_realloc(4/*SRID*/ + 1 + 4 + SIZEOF_STORED_DOUBLE * 2))))
    return 0;

  str->set_charset(&my_charset_bin);
  str->length(0);
  str->q_append(srid);
  str->q_append((char)Geometry::wkb_ndr);
  str->q_append((uint32)Geometry::wkb_point);
  str->q_append(x);
  str->q_append(y);
  return str;
}


/// This will check if arguments passed (geohash and SRID) are of valid types.
bool Item_func_pointfromgeohash::fix_fields(THD *thd, Item **ref)
{
  if (Item_geometry_func::fix_fields(thd, ref))
    return true;

  maybe_null= (args[0]->maybe_null || args[1]->maybe_null);

  // Check for valid type in geohash argument.
  if (!Item_func_latlongfromgeohash::check_geohash_argument_valid_type(args[0]))
  {
    my_error(ER_INCORRECT_TYPE, MYF(0), "geohash", func_name());
    return true;
  }

  /*
    Check for valid type in SRID argument.

    We will allow all integer types, and strings since some connectors will
    covert integers to strings. Binary data is not allowed. Note that when
    calling e.g ST_POINTFROMGEOHASH("bb", NULL), the second argument is reported
    to have binary charset, and we thus have to check field_type().
  */
  if (args[1]->collation.collation == &my_charset_bin &&
      args[1]->field_type() != MYSQL_TYPE_NULL)
  {
    my_error(ER_INCORRECT_TYPE, MYF(0), "SRID", func_name());
    return true;
  }

  switch (args[1]->field_type())
  {
  case MYSQL_TYPE_NULL:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_VARCHAR:
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_INT24:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_TINY:
    break;
  default:
    my_error(ER_INCORRECT_TYPE, MYF(0), "SRID", func_name());
    return true;
  }
  return false;
}


String *Item_func_pointfromgeohash::val_str(String *str)
{
  DBUG_ASSERT(fixed == TRUE);

  String argument_value;
  String *geohash= args[0]->val_str_ascii(&argument_value);
  longlong srid_input= args[1]->val_int();

  // Return null if one or more of the input arguments is null.
  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return NULL;

  // Only allow unsigned 32 bits integer as SRID.
  if (srid_input < 0 || srid_input > UINT_MAX32)
  {
    char srid_string[MAX_BIGINT_WIDTH + 1];
    llstr(srid_input, srid_string);
    my_error(ER_WRONG_VALUE_FOR_TYPE, MYF(0), "SRID", srid_string, func_name());
    return error_str();
  }

  if (str->mem_realloc(GEOM_HEADER_SIZE + POINT_DATA_SIZE))
    return make_empty_result();
  
  if (geohash->length() == 0)
  {
    my_error(ER_WRONG_VALUE_FOR_TYPE, MYF(0), "geohash", geohash->c_ptr(),
             func_name());
    return error_str();
  }

  double latitude= 0.0;
  double longitude= 0.0;
  uint32 srid= static_cast<uint32>(srid_input);
  if (Item_func_latlongfromgeohash::decode_geohash(geohash, upper_latitude,
                                                   lower_latitude,
                                                   upper_longitude,
                                                   lower_longitude, &latitude,
                                                   &longitude))
  {
    my_error(ER_WRONG_VALUE_FOR_TYPE, MYF(0), "geohash", geohash->c_ptr(),
             func_name());
    return error_str();
  }

  str->set_charset(&my_charset_bin);
  str->length(0);
  write_geometry_header(str, srid, Geometry::wkb_point);
  str->q_append(longitude);
  str->q_append(latitude);
  return str;
}


const char *Item_func_spatial_collection::func_name() const
{
  const char *str= NULL;

  switch (coll_type)
  {
  case Geometry::wkb_multipoint:
    str= "multipoint";
    break;
  case Geometry::wkb_multilinestring:
    str= "multilinestring";
    break;
  case Geometry::wkb_multipolygon:
    str= "multipolygon";
    break;
  case Geometry::wkb_linestring:
    str= "linestring";
    break;
  case Geometry::wkb_polygon:
    str= "polygon";
    break;
  case Geometry::wkb_geometrycollection:
    str= "geometrycollection";
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return str;
}


/**
  Concatenates various items into various collections
  with checkings for valid wkb type of items.
  For example, multipoint can be a collection of points only.
  coll_type contains wkb type of target collection.
  item_type contains a valid wkb type of items.
  In the case when coll_type is wkbGeometryCollection,
  we do not check wkb type of items, any is valid.
*/

String *Item_func_spatial_collection::val_str(String *str)
{
  DBUG_ASSERT(fixed == 1);
  String arg_value;
  uint i;
  uint32 srid= 0;

  str->set_charset(&my_charset_bin);
  str->length(0);
  if (str->reserve(4/*SRID*/ + 1 + 4 + 4, 512))
    goto err;

  str->q_append(srid);
  str->q_append((char) Geometry::wkb_ndr);
  str->q_append((uint32) coll_type);
  str->q_append((uint32) arg_count);

  for (i= 0; i < arg_count; ++i)
  {
    String *res= args[i]->val_str(&arg_value);
    size_t len;
    if (args[i]->null_value || ((len= res->length()) < WKB_HEADER_SIZE))
      goto err;

    if (coll_type == Geometry::wkb_geometrycollection)
    {
      /*
	In the case of GeometryCollection we don't need any checkings
	for item types, so just copy them into target collection
      */
      if (str->append(res->ptr() + 4/*SRID*/, len - 4/*SRID*/, (uint32) 512))
        goto err;
    }
    else
    {
      enum Geometry::wkbType wkb_type;
      const uint data_offset= 4/*SRID*/ + 1;
      if (res->length() < data_offset + sizeof(uint32))
        goto err;
      const char *data= res->ptr() + data_offset;

      /*
	In the case of named collection we must check that items
	are of specific type, let's do this checking now
      */

      wkb_type= get_wkb_geotype(data);
      data+= 4;
      len-= 5 + 4/*SRID*/;
      if (wkb_type != item_type)
        goto err;

      switch (coll_type) {
      case Geometry::wkb_multipoint:
      case Geometry::wkb_multilinestring:
      case Geometry::wkb_multipolygon:
	if (len < WKB_HEADER_SIZE ||
	    str->append(data-WKB_HEADER_SIZE, len+WKB_HEADER_SIZE, 512))
	  goto err;
	break;

      case Geometry::wkb_linestring:
	if (len < POINT_DATA_SIZE || str->append(data, POINT_DATA_SIZE, 512))
	  goto err;
	break;
      case Geometry::wkb_polygon:
      {
	uint32 n_points;
	double x1, y1, x2, y2;
	const char *org_data= data;
        const char *firstpt= NULL;
        char *p_npts= NULL;

	if (len < 4)
	  goto err;

	n_points= uint4korr(data);
        p_npts= const_cast<char *>(data);
	data+= 4;

        if (n_points < 2 || len < 4 + n_points * POINT_DATA_SIZE)
          goto err;

        firstpt= data;
	float8get(&x1, data);
	data+= SIZEOF_STORED_DOUBLE;
	float8get(&y1, data);
	data+= SIZEOF_STORED_DOUBLE;

	data+= (n_points - 2) * POINT_DATA_SIZE;

	float8get(&x2, data);
	float8get(&y2, data + SIZEOF_STORED_DOUBLE);

        if ((x1 != x2) || (y1 != y2))
        {
          n_points++;
          int4store(p_npts, n_points);
        }

	if (str->append(org_data, len, 512) ||
            (((x1 != x2) || (y1 != y2)) &&
             str->append(firstpt, POINT_DATA_SIZE, 512)))
	  goto err;
      }
      break;

      default:
	goto err;
      }
    }
  }
  if (str->length() > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, Sql_condition::SL_WARNING,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }

  null_value= 0;
  return str;

err:
  null_value= 1;
  return 0;
}


/*
  Functions for spatial relations
*/

const char *Item_func_spatial_mbr_rel::func_name() const
{
  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return "mbrcontains";
    case SP_WITHIN_FUNC:
      return "mbrwithin";
    case SP_EQUALS_FUNC:
      return "mbrequals";
    case SP_DISJOINT_FUNC:
      return "mbrdisjoint";
    case SP_INTERSECTS_FUNC:
      return "mbrintersects";
    case SP_TOUCHES_FUNC:
      return "mbrtouches";
    case SP_CROSSES_FUNC:
      return "mbrcrosses";
    case SP_OVERLAPS_FUNC:
      return "mbroverlaps";
    default:
      DBUG_ASSERT(0);  // Should never happened
      return "mbrsp_unknown";
  }
}


longlong Item_func_spatial_mbr_rel::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *res1= args[0]->val_str(&cmp.value1);
  String *res2= args[1]->val_str(&cmp.value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;
  MBR mbr1, mbr2;

  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    return 0;
  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }
  if ((null_value= (g1->get_mbr(&mbr1) || g2->get_mbr(&mbr2))))
    return 0;

  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return mbr1.contains(&mbr2);
    case SP_WITHIN_FUNC:
      return mbr1.within(&mbr2);
    case SP_EQUALS_FUNC:
      return mbr1.equals(&mbr2);
    case SP_DISJOINT_FUNC:
      return mbr1.disjoint(&mbr2);
    case SP_INTERSECTS_FUNC:
      return mbr1.intersects(&mbr2);
    case SP_TOUCHES_FUNC:
      return mbr1.touches(&mbr2);
    case SP_OVERLAPS_FUNC:
      return mbr1.overlaps(&mbr2);
    case SP_CROSSES_FUNC:
      return 0;
    default:
      break;
  }

  null_value=1;
  return 0;
}


Item_func_spatial_rel::Item_func_spatial_rel(const POS &pos, Item *a,Item *b,
                                             enum Functype sp_rel) :
    Item_bool_func2(pos, a,b), collector()
{
  spatial_rel= sp_rel;
}


Item_func_spatial_rel::~Item_func_spatial_rel()
{
}


const char *Item_func_spatial_rel::func_name() const
{
  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      return "st_contains";
    case SP_WITHIN_FUNC:
      return "st_within";
    case SP_EQUALS_FUNC:
      return "st_equals";
    case SP_DISJOINT_FUNC:
      return "st_disjoint";
    case SP_INTERSECTS_FUNC:
      return "st_intersects";
    case SP_TOUCHES_FUNC:
      return "st_touches";
    case SP_CROSSES_FUNC:
      return "st_crosses";
    case SP_OVERLAPS_FUNC:
      return "st_overlaps";
    default:
      DBUG_ASSERT(0);  // Should never happened
      return "sp_unknown";
  }
}


static double count_edge_t(const Gcalc_heap::Info *ea,
                           const Gcalc_heap::Info *eb,
                           const Gcalc_heap::Info *v,
                           double &ex, double &ey, double &vx, double &vy,
                           double &e_sqrlen)
{
  ex= eb->x - ea->x;
  ey= eb->y - ea->y;
  vx= v->x - ea->x;
  vy= v->y - ea->y;
  e_sqrlen= ex * ex + ey * ey;
  return (ex * vx + ey * vy) / e_sqrlen;
}


static double distance_to_line(double ex, double ey, double vx, double vy,
                               double e_sqrlen)
{
  return fabs(vx * ey - vy * ex) / sqrt(e_sqrlen);
}


static double distance_points(const Gcalc_heap::Info *a,
                              const Gcalc_heap::Info *b)
{
  double x= a->x - b->x;
  double y= a->y - b->y;
  return sqrt(x * x + y * y);
}


/*
  Calculates the distance between objects.
*/

static int calc_distance(double *result, Gcalc_heap *collector, uint obj2_si,
                         Gcalc_function *func, Gcalc_scan_iterator *scan_it)
{
  bool cur_point_edge;
  const Gcalc_scan_iterator::point *evpos;
  const Gcalc_heap::Info *cur_point, *dist_point;
  Gcalc_scan_events ev;
  double t, distance, cur_distance;
  double ex, ey, vx, vy, e_sqrlen;

  DBUG_ENTER("calc_distance");

  distance= DBL_MAX;

  while (scan_it->more_points())
  {
    if (scan_it->step())
      goto mem_error;
    evpos= scan_it->get_event_position();
    ev= scan_it->get_event();
    cur_point= evpos->pi;

    /*
       handling intersection we only need to check if it's the intersecion
       of objects 1 and 2. In this case distance is 0
    */
    if (ev == scev_intersection)
    {
      if ((evpos->get_next()->pi->shape >= obj2_si) !=
            (cur_point->shape >= obj2_si))
      {
        distance= 0;
        goto exit;
      }
      continue;
    }

    /*
       if we get 'scev_point | scev_end | scev_two_ends' we don't need
       to check for intersection of objects.
       Though we need to calculate distances.
    */
    if (ev & (scev_point | scev_end | scev_two_ends))
      goto calculate_distance;

    goto calculate_distance;
    /*
       having these events we need to check for possible intersection
       of objects
       scev_thread | scev_two_threads | scev_single_point
    */
    DBUG_ASSERT(ev & (scev_thread | scev_two_threads | scev_single_point));

    func->clear_state();
    for (Gcalc_point_iterator pit(scan_it); pit.point() != evpos; ++pit)
    {
      gcalc_shape_info si= pit.point()->get_shape();
      if ((func->get_shape_kind(si) == Gcalc_function::shape_polygon))
        func->invert_state(si);
    }
    func->invert_state(evpos->get_shape());
    if (func->count())
    {
      /* Point of one object is inside the other - intersection found */
      distance= 0;
      goto exit;
    }


calculate_distance:
    if (cur_point->shape >= obj2_si)
      continue;
    cur_point_edge= !cur_point->is_bottom();

    for (dist_point= collector->get_first(); dist_point;
         dist_point= dist_point->get_next())
    {
      /* We only check vertices of object 2 */
      if (dist_point->shape < obj2_si)
        continue;

      /* if we have an edge to check */
      if (dist_point->left)
      {
        t= count_edge_t(dist_point, dist_point->left, cur_point,
                        ex, ey, vx, vy, e_sqrlen);
        if ((t > 0.0) && (t < 1.0))
        {
          cur_distance= distance_to_line(ex, ey, vx, vy, e_sqrlen);
          if (distance > cur_distance)
            distance= cur_distance;
        }
      }
      if (cur_point_edge)
      {
        t= count_edge_t(cur_point, cur_point->left, dist_point,
                        ex, ey, vx, vy, e_sqrlen);
        if ((t > 0.0) && (t < 1.0))
        {
          cur_distance= distance_to_line(ex, ey, vx, vy, e_sqrlen);
          if (distance > cur_distance)
            distance= cur_distance;
        }
      }
      cur_distance= distance_points(cur_point, dist_point);
      if (distance > cur_distance)
        distance= cur_distance;
    }
  }

exit:
  *result= distance;
  DBUG_RETURN(0);

mem_error:
  DBUG_RETURN(1);
}


#define GIS_ZERO 0.00000000001

int Item_func_spatial_rel::func_touches()
{
  double distance= GIS_ZERO;
  int result= 0;
  int cur_func= 0;

  Gcalc_operation_transporter trn(&func, &collector);

  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;
  int obj2_si;

  DBUG_ENTER("Item_func_spatial_rel::func_touches");
  DBUG_ASSERT(fixed == 1);

  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    goto mem_error;
  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    result= error_int();
    goto exit;
  }

  if ((g1->get_class_info()->m_type_id == Geometry::wkb_point) &&
      (g2->get_class_info()->m_type_id == Geometry::wkb_point))
  {
    point_xy p1, p2, e;
    if (((Gis_point *) g1)->get_xy(&p1) ||
        ((Gis_point *) g2)->get_xy(&p2))
      goto mem_error;
    e.x= p2.x - p1.x;
    e.y= p2.y - p1.y;
    DBUG_RETURN((e.x * e.x + e.y * e.y) < GIS_ZERO);
  }

  if (func.reserve_op_buffer(1))
    goto mem_error;
  func.add_operation(Gcalc_function::op_intersection, 2);

  if (g1->store_shapes(&trn))
    goto mem_error;
  obj2_si= func.get_nshapes();

  if (g2->store_shapes(&trn) || func.alloc_states())
    goto mem_error;

#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  collector.prepare_operation();
  scan_it.init(&collector);

  if (calc_distance(&distance, &collector, obj2_si, &func, &scan_it))
    goto mem_error;
  if (distance > GIS_ZERO)
    goto exit;

  scan_it.reset();
  scan_it.init(&collector);

  distance= DBL_MAX;

  while (scan_it.more_trapezoids())
  {
    if (scan_it.step())
      goto mem_error;

    func.clear_state();
    for (Gcalc_trapezoid_iterator ti(&scan_it); ti.more(); ++ti)
    {
      gcalc_shape_info si= ti.lb()->get_shape();
      if ((func.get_shape_kind(si) == Gcalc_function::shape_polygon))
      {
        func.invert_state(si);
        cur_func= func.count();
      }
      if (cur_func)
      {
        double area= scan_it.get_h() *
              ((ti.rb()->x - ti.lb()->x) + (ti.rt()->x - ti.lt()->x));
        if (area > GIS_ZERO)
        {
          result= 0;
          goto exit;
        }
      }
    }
  }
  result= 1;

exit:
  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(result);
mem_error:
  null_value= 1;
  DBUG_RETURN(0);
}


int Item_func_spatial_rel::func_equals()
{
  Gcalc_heap::Info *pi_s1, *pi_s2;
  Gcalc_heap::Info *cur_pi= collector.get_first();
  double d;

  if (!cur_pi)
    return 1;

  do {
    pi_s1= cur_pi;
    pi_s2= 0;
    while ((cur_pi= cur_pi->get_next()))
    {
      d= fabs(pi_s1->x - cur_pi->x) + fabs(pi_s1->y - cur_pi->y);
      if (d > GIS_ZERO)
        break;
      if (!pi_s2 && pi_s1->shape != cur_pi->shape)
        pi_s2= cur_pi;
    }

    if (!pi_s2)
      return 0;
  } while (cur_pi);

  return 1;
}


/**
   Less than comparator for points used by BG.
 */
struct bgpt_lt
{
  template <typename Point>
  bool operator ()(const Point &p1, const Point &p2) const
  {
    if (p1.template get<0>() != p2.template get<0>())
      return p1.template get<0>() < p2.template get<0>();
    else
      return p1.template get<1>() < p2.template get<1>();
  }
};


/**
   Equals comparator for points used by BG.
 */
struct bgpt_eq
{
  template <typename Point>
  bool operator ()(const Point &p1, const Point &p2) const
  {
    return p1.template get<0>() == p2.template get<0>() &&
      p1.template get<1>() == p2.template get<1>();
  }
};



/**
  Convert this into a Gis_geometry_collection object.
  @param geodata Stores the result object's WKB data.
  @return The Gis_geometry_collection object created from this object.
 */
Gis_geometry_collection *
BG_geometry_collection::as_geometry_collection(String *geodata) const
{
  if (m_geos.size() == 0)
    return NULL;

  Gis_geometry_collection *gc= NULL;

  for (Geometry_list::const_iterator i= m_geos.begin();
       i != m_geos.end(); ++i)
  {
    if (gc == NULL)
      gc= new Gis_geometry_collection(*i, geodata);
    else
      gc->append_geometry(*i, geodata);
  }

  return gc;
}


/**
  Store a Geometry object into this collection. If it's a geometry collection,
  flatten it and store its components into this collection, so that no
  component is a geometry collection.
  @param geo The Geometry object to put into this collection. We duplicate
         geo's data rather than directly using it.
  @return true if error occured, false if no error(successful).
 */
bool BG_geometry_collection::store_geometry(const Geometry *geo)
{
  if (geo->get_type() == Geometry::wkb_geometrycollection)
  {
    uint32 ngeom= 0;

    if (geo->num_geometries(&ngeom))
      return true;

    /*
      Get its components and store each of them separately, if a component
      is also a collection, recursively disintegrate and store its
      components in the same way.
     */
    for (uint32 i= 1; i <= ngeom; i++)
    {
      String *pres= m_geosdata.append_object();
      if (pres == NULL || pres->reserve(GEOM_HEADER_SIZE, 512))
        return true;

      pres->q_append(geo->get_srid());
      if (geo->geometry_n(i, pres))
        return true;

      Geometry_buffer *pgeobuf= m_geobufs.append_object();
      if (pgeobuf == NULL)
        return true;
      Geometry *geo2= Geometry::construct(pgeobuf, pres->ptr(),
                                          pres->length());
      if (geo2 == NULL)
      {
        // The geometry data already pass such checks, it's always valid here.
        DBUG_ASSERT(false);
        return true;
      }
      else if (geo2->get_type() == Geometry::wkb_geometrycollection)
      {
        if (store_geometry(geo2))
          return true;
      }
      else
        m_geos.push_back(geo2);
    }
  }
  else if (store(geo) == NULL)
    return true;

  return false;
}


/**
  Store a geometry of GEOMETRY format into this collection.
  @param geo a geometry object whose data of GEOMETRY format is to be duplicated
         and stored into this collection. It's not a geometry collection.
  @return a duplicated Geometry object created from geo.
 */
Geometry *BG_geometry_collection::store(const Geometry *geo)
{
  String *pres= NULL;
  Geometry *geo2= NULL;
  Geometry_buffer *pgeobuf= NULL;
  size_t geosize= geo->get_data_size();

  DBUG_ASSERT(geo->get_type() != Geometry::wkb_geometrycollection);
  pres= m_geosdata.append_object();
  if (pres == NULL || pres->reserve(GEOM_HEADER_SIZE + geosize))
    return NULL;
  write_geometry_header(pres, geo->get_srid(), geo->get_type());
  pres->q_append(geo->get_cptr(), geosize);

  pgeobuf= m_geobufs.append_object();
  if (pgeobuf == NULL)
    return NULL;
  geo2= Geometry::construct(pgeobuf, pres->ptr(), pres->length());
  // The geometry data already pass such checks, it's always valid here.
  DBUG_ASSERT(geo2 != NULL);

  if (geo2 != NULL && geo2->get_type() != Geometry::wkb_geometrycollection)
    m_geos.push_back(geo2);

  return geo2;
}


longlong Item_func_spatial_rel::val_int()
{
  DBUG_ENTER("Item_func_spatial_rel::val_int");
  DBUG_ASSERT(fixed == 1);
  String *res1= NULL;
  String *res2= NULL;
  Geometry_buffer buffer1, buffer2;
  Geometry *g1= NULL, *g2= NULL;
  int result= 0;
  int mask= 0;
  int tres= 0;
  bool bgdone= false;
  bool had_except= false;
  my_bool had_error= false;
  String wkt1, wkt2;
  Gcalc_operation_transporter trn(&func, &collector);

  res1= args[0]->val_str(&tmp_value1);
  res2= args[1]->val_str(&tmp_value2);
  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    goto exit;
  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    tres= error_int();
    goto exit;
  }

  // The two geometry operands must be in the same coordinate system.
  if (g1->get_srid() != g2->get_srid())
  {
    my_error(ER_GIS_DIFFERENT_SRIDS, MYF(0), func_name(),
             g1->get_srid(), g2->get_srid());
    tres= error_int();
    goto exit;
  }

  /*
    Catch all exceptions to make sure no exception can be thrown out of
    current function. Put all and any code that calls Boost.Geometry functions,
    STL functions into this try block. Code out of the try block should never
    throw any exception.
  */
  try
  {
    if (g1->get_type() != Geometry::wkb_geometrycollection &&
        g2->get_type() != Geometry::wkb_geometrycollection)
    {
      // Must use double, otherwise may lose valid result, not only precision.
      tres= bg_geo_relation_check<double, bgcs::cartesian>
        (g1, g2, &bgdone, spatial_rel, &had_error);
    }
    else
      tres= geocol_relation_check<double, bgcs::cartesian>(g1, g2, &bgdone);
  }
  CATCH_ALL(func_name(), { had_except= true; })

  if (had_except || had_error || null_value)
  {
    bgdone= false;
    DBUG_RETURN(error_int());
  }

  if (bgdone)
    DBUG_RETURN(tres);

  // Start of old GIS algorithms for geometry relationship checks.
  if (spatial_rel == SP_TOUCHES_FUNC)
    DBUG_RETURN(func_touches());

  if (func.reserve_op_buffer(1))
    DBUG_RETURN(0);

  switch (spatial_rel) {
    case SP_CONTAINS_FUNC:
      mask= 1;
      func.add_operation(Gcalc_function::op_backdifference, 2);
      break;
    case SP_WITHIN_FUNC:
      mask= 1;
      func.add_operation(Gcalc_function::op_difference, 2);
      break;
    case SP_EQUALS_FUNC:
      break;
    case SP_DISJOINT_FUNC:
      mask= 1;
      func.add_operation(Gcalc_function::op_intersection, 2);
      break;
    case SP_INTERSECTS_FUNC:
      func.add_operation(Gcalc_function::op_intersection, 2);
      break;
    case SP_OVERLAPS_FUNC:
      func.add_operation(Gcalc_function::op_backdifference, 2);
      break;
    case SP_CROSSES_FUNC:
      func.add_operation(Gcalc_function::op_intersection, 2);
      break;
    default:
      DBUG_ASSERT(FALSE);
      break;
  }
  if ((null_value= (g1->store_shapes(&trn) || g2->store_shapes(&trn))))
    goto exit;

#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  collector.prepare_operation();
  scan_it.init(&collector);
  /* Note: other functions might be checked here as well. */
  if (spatial_rel == SP_EQUALS_FUNC ||
      spatial_rel == SP_WITHIN_FUNC ||
      spatial_rel == SP_CONTAINS_FUNC)
  {
    result= (g1->get_class_info()->m_type_id ==
             g1->get_class_info()->m_type_id) && func_equals();
    if (spatial_rel == SP_EQUALS_FUNC ||
        result) // for SP_WITHIN_FUNC and SP_CONTAINS_FUNC
      goto exit;
  }

  if (func.alloc_states())
    goto exit;

  result= func.find_function(scan_it) ^ mask;

exit:
  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(result);
}


/*
  Check whether g is an empty geometry collection.
*/
static inline bool is_empty_geocollection(const Geometry *g)
{
  if (g->get_geotype() != Geometry::wkb_geometrycollection)
    return false;

  uint32 num= uint4korr(g->get_cptr());
  return num == 0;
}


/**
  Do geometry collection relation check. Boost geometry doesn't support
  geometry collections directly, we have to treat them as a collection of basic
  geometries and use BG features to compute.
  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::geocol_relation_check(Geometry *g1, Geometry *g2,
                                                 bool *pbgdone)
{
  String gcbuf;
  Geometry *tmpg= NULL;
  int tres= 0;
  *pbgdone= false;
  const typename BG_geometry_collection::Geometry_list *gv1= NULL, *gv2= NULL;
  BG_geometry_collection bggc1, bggc2;

  bool empty1= is_empty_geocollection(g1);
  bool empty2= is_empty_geocollection(g2);

  /*
    An empty geometry collection is an empty point set, according to OGC
    specifications and set theory we make below conclusion.
   */
  if (empty1 || empty2)
  {
    if (spatial_rel == SP_DISJOINT_FUNC)
      tres= 1;
    else if (empty1 && empty2 && spatial_rel == SP_EQUALS_FUNC)
      tres= 1;
    *pbgdone= true;
    return tres;
  }

  if (spatial_rel == SP_CONTAINS_FUNC)
  {
    tmpg= g2;
    g2= g1;
    g1= tmpg;
    spatial_rel= SP_WITHIN_FUNC;
  }
  else if (spatial_rel == SP_OVERLAPS_FUNC ||
           spatial_rel == SP_CROSSES_FUNC || spatial_rel == SP_TOUCHES_FUNC)
  {
    // Note: below algo may not work for overlap/cross because they have
    // dimensional requirement, not sure how to apply to geo collection.
    // Old algorithm is able to compute some of this, so don't error out yet.
    // my_error(ER_GIS_UNSUPPORTED_ARGUMENT, MYF(0), func_name());
    *pbgdone= false;
    return tres;
  }

  bggc1.fill(g1);
  bggc2.fill(g2);

  gv1= &(bggc1.get_geometries());
  gv2= &(bggc2.get_geometries());

  if (gv1->size() == 0 || gv2->size() == 0)
  {
    null_value= true;
    *pbgdone= true;
    return tres;
  }

  if (spatial_rel == SP_DISJOINT_FUNC || spatial_rel == SP_INTERSECTS_FUNC)
    tres= geocol_relcheck_intersect_disjoint<Coord_type, Coordsys>
      (gv1, gv2, pbgdone);
  else if (spatial_rel == SP_WITHIN_FUNC)
    tres= geocol_relcheck_within<Coord_type, Coordsys>(gv1, gv2, pbgdone);
  else if (spatial_rel == SP_EQUALS_FUNC)
    tres= geocol_equals_check<Coord_type, Coordsys>(gv1, gv2, pbgdone);
  else
    DBUG_ASSERT(false);

  /* If doing contains check, need to switch back the two operands. */
  if (tmpg)
  {
    DBUG_ASSERT(spatial_rel == SP_WITHIN_FUNC);
    spatial_rel= SP_CONTAINS_FUNC;
    tmpg= g2;
    g2= g1;
    g1= tmpg;
  }

  return tres;
}


/**
  Geometry collection relation checks for disjoint and intersects operations.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_relcheck_intersect_disjoint(const typename BG_geometry_collection::
                                   Geometry_list *gv1,
                                   const typename BG_geometry_collection::
                                   Geometry_list *gv2,
                                   bool *pbgdone)
{
  int tres= 0;
  *pbgdone= false;

  DBUG_ASSERT(spatial_rel == SP_DISJOINT_FUNC ||
              spatial_rel == SP_INTERSECTS_FUNC);

  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv1->begin();
       i != gv1->end(); ++i)
  {
    for (BG_geometry_collection::
         Geometry_list::const_iterator j= gv2->begin();
         j != gv2->end(); ++j)
    {
      bool had_except= false;
      my_bool had_error= false;

      try
      {
        tres= bg_geo_relation_check<Coord_type, Coordsys>
          (*i, *j, pbgdone, spatial_rel, &had_error);
      }
      CATCH_ALL(func_name(), {had_except= true;})

      if (had_except || had_error)
      {
        *pbgdone= false;
        return error_int();
      }

      if (!*pbgdone || null_value)
        return tres;

      /*
        If a pair of geometry intersect or don't disjoint, the two
        geometry collections intersect or don't disjoint, in both cases the
        check is completed.
       */
      if ((spatial_rel == SP_INTERSECTS_FUNC && tres) ||
          (spatial_rel == SP_DISJOINT_FUNC && !tres))
      {
        *pbgdone= true;
        return tres;
      }
    }
  }

  /*
    When we arrive here, the disjoint check must have succeeded and
    intersects check must have failed, otherwise control would
    have gone out of this function.

    The reason we can derive the relation check result is that if
    any two geometries from the two collections intersect, the two
    geometry collections intersect; and disjoint is true
    only when any(and every) combination of geometries from
    the two collections are disjoint.
   */
  DBUG_ASSERT((tres && spatial_rel == SP_DISJOINT_FUNC) ||
              (!tres && spatial_rel == SP_INTERSECTS_FUNC));
  *pbgdone= true;
  return tres;
}


/**
  Geometry collection relation checks for within and equals(half) checks.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_relcheck_within(const typename BG_geometry_collection::
                       Geometry_list *gv1,
                       const typename BG_geometry_collection::
                       Geometry_list *gv2,
                       bool *pbgdone)
{
  int tres= 0;
  *pbgdone= false;
  DBUG_ASSERT(spatial_rel == SP_WITHIN_FUNC || spatial_rel == SP_EQUALS_FUNC);

  for (BG_geometry_collection::
       Geometry_list::const_iterator i= gv1->begin();
       i != gv1->end(); ++i)
  {
    bool innerOK= false;

    for (BG_geometry_collection::
         Geometry_list::const_iterator j= gv2->begin();
         j != gv2->end(); ++j)
    {
      bool had_except= false;
      my_bool had_error= false;

      try
      {
        tres= bg_geo_relation_check<Coord_type, Coordsys>
          (*i, *j, pbgdone, spatial_rel, &had_error);
      }
      CATCH_ALL(func_name(), {had_except= true;})

      if (had_except || had_error || null_value)
      {
        *pbgdone= false;
        return error_int();
      }

      if (!*pbgdone)
        return tres;

      /*
        We've found a geometry j in gv2 so that current geometry element i
        in gv1 is within j, or i is equal to j. This means i in gv1
        passes the test, proceed to next geometry in gv1.
       */
      if ((spatial_rel == SP_WITHIN_FUNC ||
           spatial_rel == SP_EQUALS_FUNC) && tres)
      {
        innerOK= true;
        break;
      }
    }

    /*
      For within and equals check, if we can't find a geometry j in gv2
      so that current geometry element i in gv1 is with j or i is equal to j,
      gv1 is not within or equal to gv2.
     */
    if (!innerOK)
    {
      *pbgdone= true;
      DBUG_ASSERT(tres == false);
      return tres;
    }
  }

  /*
    When we arrive here, within or equals checks must have
    succeeded, otherwise control would go out of this function.
    The reason we can derive the relation check result is that
    within and equals are true only when any(and every) combination of
    geometries from the two collections are true for the relation check.
   */
  DBUG_ASSERT(tres);
  *pbgdone= true;

  return tres;
}

/**
  Geometry collection equality check.
  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 the 1st geometry collection parameter.
  @param g2 the 2nd geometry collection parameter.
  @param[out] pbgdone Whether the operation is successfully performed by
  Boost Geometry. Note that BG doesn't support many type combinations so far,
  in case not, the operation is to be done by old GIS algorithm instead.
  @return whether g1 and g2 satisfy the specified relation, 0 for negative,
                none 0 for positive.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::
geocol_equals_check(const typename BG_geometry_collection::Geometry_list *gv1,
                    const typename BG_geometry_collection::Geometry_list *gv2,
                    bool *pbgdone)
{
  int tres= 0, num_try= 0;
  *pbgdone= false;
  DBUG_ASSERT(spatial_rel == SP_EQUALS_FUNC);

  do
  {
    tres= geocol_relcheck_within<Coord_type, Coordsys>(gv1, gv2, pbgdone);
    if (!tres || !*pbgdone || null_value)
      return tres;
    /*
      Two sets A and B are equal means A is a subset of B and B is a
      subset of A. Thus we need to check twice, each successful check
      means half truth. Switch gv1 and gv2 for 2nd check.
     */
    std::swap(gv1, gv2);
    num_try++;
  }
  while (num_try < 2);

  return tres;
}


/**
  Wraps and dispatches type specific BG function calls according to operation
  type and both operands' types.

  We want to isolate boost header file inclusion only inside this file, so we
  can't put this class declaration in any header file. And we want to make the
  methods static since no state is needed here.

  @tparam Geom_types Geometry types definitions.
*/
template<typename Geom_types>
class BG_wrap {
public:

  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Multilinestring Multilinestring;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  // For abbrievation.
  typedef Item_func_spatial_rel Ifsr;
  typedef std::set<Point, bgpt_lt> Point_set;
  typedef std::vector<Point> Point_vector;

  static int point_within_geometry(Geometry *g1, Geometry *g2,
                                   bool *pbgdone, my_bool *pnull_value);

  static int multipoint_within_geometry(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value);

  static int multipoint_equals_geometry(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value);

  static int point_disjoint_geometry(Geometry *g1, Geometry *g2,
                                     bool *pbgdone, my_bool *pnull_value);
  static int multipoint_disjoint_geometry(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value);

  static int linestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value);
  static int multilinestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                               bool *pbgdone,
                                               my_bool *pnull_value);
  static int polygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                                       bool *pbgdone, my_bool *pnull_value);
  static int multipolygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
  static int point_intersects_geometry(Geometry *g1, Geometry *g2,
                                       bool *pbgdone, my_bool *pnull_value);
  static int multipoint_intersects_geometry(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
  static int linestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
  static int multilinestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                                 bool *pbgdone,
                                                 my_bool *pnull_value);
  static int polygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value);
  static int multipolygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                              bool *pbgdone,
                                              my_bool *pnull_value);
  static int multipoint_crosses_geometry(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value);
  static int multipoint_overlaps_multipoint(Geometry *g1, Geometry *g2,
                                            bool *pbgdone,
                                            my_bool *pnull_value);
};// bg_wrapper


/*
  Call a BG function with specified types of operands. We have to create
  geo1 and geo2 because operands g1 and g2 are created without their WKB data
  parsed, so not suitable for BG to use. geo1 will share the same copy of WKB
  data with g1, also true for geo2.
 */
#define BGCALL(res, bgfunc, GeoType1, g1, GeoType2, g2, pnullval) do {  \
  const void *pg1= g1->normalize_ring_order();                          \
  const void *pg2= g2->normalize_ring_order();                          \
  if (pg1 != NULL && pg2 != NULL)                                       \
  {                                                                     \
    GeoType1 geo1(pg1, g1->get_data_size(), g1->get_flags(),            \
                  g1->get_srid());                                      \
    GeoType2 geo2(pg2, g2->get_data_size(), g2->get_flags(),            \
                  g2->get_srid());                                      \
    res= boost::geometry::bgfunc(geo1, geo2);                           \
  }                                                                     \
  else                                                                  \
  {                                                                     \
    my_error(ER_GIS_INVALID_DATA, MYF(0), "st_" #bgfunc);               \
    (*(pnullval))= 1;                                                   \
  }                                                                     \
} while (0)


/**
  Dispatcher for 'point WITHIN xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::point_within_geometry(Geometry *g1, Geometry *g2,
                                               bool *pbgdone,
                                               my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  if (gt2 == Geometry::wkb_polygon)
  {
    BGCALL(result, within, Point, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multipolygon)
  {
    BGCALL(result, within, Point, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_point)
  {
    BGCALL(result, equals, Point, g1, Point, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multipoint)
  {
    Multipoint mpts(g2->get_data_ptr(),
                    g2->get_data_size(), g2->get_flags(), g2->get_srid());
    Point pt(g1->get_data_ptr(),
             g1->get_data_size(), g1->get_flags(), g1->get_srid());

    Point_set ptset(mpts.begin(), mpts.end());
    result= ((ptset.find(pt) != ptset.end()));
    *pbgdone= true;
  }
  return result;
}


/**
  Dispatcher for 'multipoint WITHIN xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::multipoint_within_geometry(Geometry *g1, Geometry *g2,
                                                    bool *pbgdone,
                                                    my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();
  const void *data_ptr= NULL;

  *pbgdone= false;

  Multipoint mpts(g1->get_data_ptr(), g1->get_data_size(),
                  g1->get_flags(), g1->get_srid());
  if (gt2 == Geometry::wkb_polygon)
  {
    data_ptr= g2->normalize_ring_order();
    if (data_ptr == NULL)
    {
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_within");
      *pnull_value= true;
      return result;
    }

    Polygon plg(data_ptr, g2->get_data_size(),
                g2->get_flags(), g2->get_srid());

    for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
    {
      result= boost::geometry::within(*i, plg);
      if (result == 0)
        break;
    }
    *pbgdone= true;

  }
  else if (gt2 == Geometry::wkb_multipolygon)
  {
    data_ptr= g2->normalize_ring_order();
    if (data_ptr == NULL)
    {
      *pnull_value= true;
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_within");
      return result;
    }

    Multipolygon mplg(data_ptr, g2->get_data_size(),
                      g2->get_flags(), g2->get_srid());
    for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end(); ++i)
    {
      result= boost::geometry::within(*i, mplg);
      if (result == 0)
        break;
    }
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_point)
  {
    /* There may be duplicate Points, thus use a set to make them unique*/
    Point_set ptset1(mpts.begin(), mpts.end());
    Point pt(g2->get_data_ptr(),
             g2->get_data_size(), g2->get_flags(), g2->get_srid());
    result= ((ptset1.size() == 1) &&
             boost::geometry::equals(*ptset1.begin(), pt));
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multipoint)
  {
    /* There may be duplicate Points, thus use a set to make them unique*/
    Point_set ptset1(mpts.begin(), mpts.end());
    Multipoint mpts2(g2->get_data_ptr(),
                     g2->get_data_size(), g2->get_flags(), g2->get_srid());
    Point_set ptset2(mpts2.begin(), mpts2.end());
    Point_vector respts;
    TYPENAME Point_vector::iterator endpos;
    respts.resize(std::max(ptset1.size(), ptset2.size()));
    endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                  ptset2.begin(), ptset2.end(),
                                  respts.begin(), bgpt_lt());
    result= (ptset1.size() == static_cast<size_t>(endpos - respts.begin()));
    *pbgdone= true;
  }
  return result;
}


/**
  Dispatcher for 'multipoint EQUALS xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::multipoint_equals_geometry(Geometry *g1, Geometry *g2,
                                                    bool *pbgdone,
                                                    my_bool *pnull_value)
{
  *pbgdone= false;
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    result= Ifsr::equals_check<Geom_types>(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      Multipoint mpts2(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());

      Point_set ptset1(mpts1.begin(), mpts1.end());
      Point_set ptset2(mpts2.begin(), mpts2.end());
      result= (ptset1.size() == ptset2.size() &&
               std::equal(ptset1.begin(), ptset1.end(),
                          ptset2.begin(), bgpt_eq()));
    }
    break;
  default:
    result= 0;
    break;
  }
  *pbgdone= true;
  return result;
}


/**
  Dispatcher for 'multipoint disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_disjoint_geometry(Geometry *g1, Geometry *g2,
                             bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();
  const void *data_ptr= NULL;

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    result= point_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      Multipoint mpts2(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());
      Point_set ptset1(mpts1.begin(), mpts1.end());
      Point_set ptset2(mpts2.begin(), mpts2.end());
      Point_vector respts;
      TYPENAME Point_vector::iterator endpos;
      size_t ptset1sz= ptset1.size(), ptset2sz= ptset2.size();

      respts.resize(ptset1sz > ptset2sz ? ptset1sz : ptset2sz);
      endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                    ptset2.begin(), ptset2.end(),
                                    respts.begin(), bgpt_lt());
      result= (endpos == respts.begin());
      *pbgdone= true;
    }
    break;
  case Geometry::wkb_polygon:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      data_ptr= g2->normalize_ring_order();
      if (data_ptr == NULL)
      {
        *pnull_value= true;
        my_error(ER_GIS_INVALID_DATA, MYF(0), "st_disjoint");
        return result;
      }

      Polygon plg(data_ptr, g2->get_data_size(),
                  g2->get_flags(), g2->get_srid());

      for (TYPENAME Multipoint::iterator i= mpts1.begin();
           i != mpts1.end(); ++i)
      {
        result= boost::geometry::disjoint(*i, plg);

        if (!result)
          break;
      }

      *pbgdone= true;
    }
    break;
  case Geometry::wkb_multipolygon:
    {
      Multipoint mpts1(g1->get_data_ptr(),
                       g1->get_data_size(), g1->get_flags(), g1->get_srid());
      data_ptr= g2->normalize_ring_order();
      if (data_ptr == NULL)
      {
        *pnull_value= true;
        my_error(ER_GIS_INVALID_DATA, MYF(0), "st_disjoint");
        return result;
      }

      Multipolygon mplg(data_ptr, g2->get_data_size(),
                        g2->get_flags(), g2->get_srid());

      for (TYPENAME Multipoint::iterator i= mpts1.begin();
           i != mpts1.end(); ++i)
      {
        result= boost::geometry::disjoint(*i, mplg);

        if (!result)
          break;
      }

      *pbgdone= true;
    }
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'linestring disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                             bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring)
  {
    BGCALL(result, disjoint, Linestring, g1, Linestring, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multilinestring)
  {
    Multilinestring mls(g2->get_data_ptr(), g2->get_data_size(),
                        g2->get_flags(), g2->get_srid());
    Linestring ls(g1->get_data_ptr(),
                  g1->get_data_size(), g1->get_flags(), g1->get_srid());

    for (TYPENAME Multilinestring::iterator i= mls.begin();
         i != mls.end(); ++i)
    {
      result= boost::geometry::disjoint(ls, *i);

      if (!result)
        break;
    }
    *pbgdone= true;

  }
  else
    *pbgdone= false;

  return result;
}


/**
  Dispatcher for 'multilinestring disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_disjoint_geometry(Geometry *g1, Geometry *g2,
                                  bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring)
    result= BG_wrap<Geom_types>::
      linestring_disjoint_geometry(g2, g1, pbgdone, pnull_value);
  else if (gt2 == Geometry::wkb_multilinestring)
  {
    Multilinestring mls1(g1->get_data_ptr(), g1->get_data_size(),
                         g1->get_flags(), g1->get_srid());
    Multilinestring mls2(g2->get_data_ptr(), g2->get_data_size(),
                         g2->get_flags(), g2->get_srid());

    for (TYPENAME Multilinestring::iterator i= mls1.begin();
         i != mls1.end(); ++i)
    {
      for (TYPENAME Multilinestring::iterator j= mls2.begin();
           j != mls2.end(); ++j)
      {
        result= boost::geometry::disjoint(*i, *j);
        if (!result)
          break;
      }

      if (!result)
        break;
    }

    *pbgdone= true;
  }
  else
    *pbgdone= false;

  return result;
}


/**
  Dispatcher for 'point disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
point_disjoint_geometry(Geometry *g1, Geometry *g2,
                        bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt2= g2->get_type();

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Point, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Point, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Point, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    {
      Multipoint mpts(g2->get_data_ptr(),
                      g2->get_data_size(), g2->get_flags(), g2->get_srid());
      Point pt(g1->get_data_ptr(),
               g1->get_data_size(), g1->get_flags(), g1->get_srid());

      Point_set ptset(mpts.begin(), mpts.end());
      result= (ptset.find(pt) == ptset.end());
      *pbgdone= true;
    }
    break;
  default:
    *pbgdone= false;
    break;
  }
  return result;
}


/**
  Dispatcher for 'polygon disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                          bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Polygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Polygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Polygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    *pbgdone= false;
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipolygon disjoint xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_disjoint_geometry(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, disjoint, Multipolygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, disjoint, Multipolygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, disjoint, Multipolygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    *pbgdone= false;
    break;
  }

  return result;
}


/**
  Dispatcher for 'point intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
point_intersects_geometry(Geometry *g1, Geometry *g2,
                          bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Point, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= !point_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Point, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Point, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipoint intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_intersects_geometry(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch(gt2)
  {
  case Geometry::wkb_point:
  case Geometry::wkb_multipoint:
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    result= !multipoint_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'linestring intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
linestring_intersects_geometry(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  if (gt2 == Geometry::wkb_linestring)
  {
    BGCALL(result, intersects, Linestring, g1, Linestring, g2, pnull_value);
    *pbgdone= true;
  }
  else if (gt2 == Geometry::wkb_multilinestring)
  {
    result= !linestring_disjoint_geometry(g1, g2, pbgdone, pnull_value);
  }

  return result;
}


/**
  Dispatcher for 'multilinestring intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multilinestring_intersects_geometry(Geometry *g1, Geometry *g2,
                                    bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  if (gt2 == Geometry::wkb_linestring ||
      gt2 == Geometry::wkb_multilinestring)
    result= (!BG_wrap<Geom_types>::
             multilinestring_disjoint_geometry(g1, g2,
                                               pbgdone, pnull_value) ? 1 : 0);
  else
    *pbgdone= false;

  return result;
}


/**
  Dispatcher for 'polygon intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
polygon_intersects_geometry(Geometry *g1, Geometry *g2,
                            bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Polygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= !multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Polygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Polygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    break;
  }

  return result;
}


/**
  Dispatcher for 'multipolygon intersects xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipolygon_intersects_geometry(Geometry *g1, Geometry *g2,
                                 bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_point:
    BGCALL(result, intersects, Multipolygon, g1, Point, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipoint:
    result= !multipoint_disjoint_geometry(g2, g1, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    BGCALL(result, intersects, Multipolygon, g1, Polygon, g2, pnull_value);
    *pbgdone= true;
    break;
  case Geometry::wkb_multipolygon:
    BGCALL(result, intersects, Multipolygon, g1, Multipolygon, g2, pnull_value);
    *pbgdone= true;
    break;
  default:
    break;
  }
  return result;
}


/**
  Dispatcher for 'multipoint crosses xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_crosses_geometry(Geometry *g1, Geometry *g2,
                            bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  Geometry::wkbType gt2= g2->get_type();

  *pbgdone= false;

  switch (gt2)
  {
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    {
      bool isdone= false, has_in= false, has_out= false;
      int res= 0;

      Multipoint mpts(g1->get_data_ptr(),
                      g1->get_data_size(), g1->get_flags(), g1->get_srid());
      /*
        According to OGC's definition to crosses, if some Points of
        g1 is in g2 and some are not, g1 crosses g2, otherwise not.
       */
      for (TYPENAME Multipoint::iterator i= mpts.begin(); i != mpts.end() &&
           !(has_in && has_out); ++i)
      {
        res= point_disjoint_geometry(&(*i), g2, &isdone, pnull_value);

        if (isdone && !*pnull_value)
        {
          if (!res)
            has_in= true;
          else
            has_out= true;
        }
        else
        {
          *pbgdone= false;
          return 0;
        }
      }

      *pbgdone= true;

      if (has_in && has_out)
        result= 1;
      else
        result= 0;
    }
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return result;
}


/**
  Dispatcher for 'multipoint crosses xxx'.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, a Point.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Geom_types>
int BG_wrap<Geom_types>::
multipoint_overlaps_multipoint(Geometry *g1, Geometry *g2,
                               bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;

  *pbgdone= false;

  Multipoint mpts1(g1->get_data_ptr(),
                   g1->get_data_size(), g1->get_flags(), g1->get_srid());
  Multipoint mpts2(g2->get_data_ptr(),
                   g2->get_data_size(), g2->get_flags(), g2->get_srid());
  Point_set ptset1, ptset2;

  ptset1.insert(mpts1.begin(), mpts1.end());
  ptset2.insert(mpts2.begin(), mpts2.end());

  // They overlap if they intersect and also each has some points that the other
  // one doesn't have.
  Point_vector respts;
  TYPENAME Point_vector::iterator endpos;
  size_t ptset1sz= ptset1.size(), ptset2sz= ptset2.size(), resptssz;

  respts.resize(ptset1sz > ptset2sz ? ptset1sz : ptset2sz);
  endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                ptset2.begin(), ptset2.end(),
                                respts.begin(), bgpt_lt());
  resptssz= endpos - respts.begin();
  if (resptssz > 0 && resptssz < ptset1.size() &&
      resptssz < ptset2.size())
    result= 1;
  else
    result= 0;

  *pbgdone= true;

  return result;
}


/**
  Do within relation check of two geometries.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::within_check(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  int result= 0;

  gt1= g1->get_type();

  if (gt1 == Geometry::wkb_point)
    result= BG_wrap<Geom_types>::point_within_geometry(g1, g2,
                                                       pbgdone, pnull_value);
  else if (gt1 == Geometry::wkb_multipoint)
    result= BG_wrap<Geom_types>::
      multipoint_within_geometry(g1, g2, pbgdone, pnull_value);
  /*
    Can't do above if gt1 is Linestring or Polygon, because g2 can be
    an concave Polygon.
    Note: need within(lstr, plgn), within(pnt, lstr), within(lstr, lstr),
    within(plgn, plgn), (lstr, multiplgn), (lstr, multilstr),
    (multilstr, multilstr), (multilstr, multiplgn), (multiplgn, multiplgn),
    (plgn, multiplgn).

    Note that we can't iterate geometries in multiplgn, multilstr one by one
    and use within(lstr, plgn)(plgn, plgn) to do within computation for them
    because it's possible for a lstr to be not in any member plgn but in the
    multiplgn.
   */
  return result;
}


/**
  Do equals relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::equals_check(Geometry *g1, Geometry *g2,
                                        bool *pbgdone, my_bool *pnull_value)
{
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef std::set<Point, bgpt_lt> Point_set;

  *pbgdone= false;
  int result= 0;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  /*
    Only geometries of the same base type can be equal, any other
    combinations always result as false. This is different from all other types
    of geometry relation checks.
   */
  *pbgdone= true;
  if (gt1 == Geometry::wkb_point)
  {
    if (gt2 == Geometry::wkb_point)
      BGCALL(result, equals, Point, g1, Point, g2, pnull_value);
    else if (gt2 == Geometry::wkb_multipoint)
    {
      Point pt(g1->get_data_ptr(),
               g1->get_data_size(), g1->get_flags(), g1->get_srid());
      Multipoint mpts(g2->get_data_ptr(),
                      g2->get_data_size(), g2->get_flags(), g2->get_srid());

      Point_set ptset(mpts.begin(), mpts.end());

      result= (ptset.size() == 1 &&
               boost::geometry::equals(pt, *ptset.begin()));
    }
    else
      result= 0;
  }
  else if (gt1 == Geometry::wkb_multipoint)
    result= BG_wrap<Geom_types>::
      multipoint_equals_geometry(g1, g2, pbgdone, pnull_value);
  else if (gt1 == Geometry::wkb_linestring &&
           gt2 == Geometry::wkb_linestring)
    BGCALL(result, equals, Linestring, g1, Linestring, g2, pnull_value);
  else if ((gt1 == Geometry::wkb_linestring &&
            gt2 == Geometry::wkb_multilinestring) ||
           (gt2 == Geometry::wkb_linestring &&
            gt1 == Geometry::wkb_multilinestring) ||
           (gt2 == Geometry::wkb_multilinestring &&
            gt1 == Geometry::wkb_multilinestring))
  {
    *pbgdone= false;
    /*
      Note: can't handle this case simply like Multipoint&point above,
      because multiple line segments can form a longer linesegment equal
      to a single line segment.
     */
  }
  else if (gt1 == Geometry::wkb_polygon && gt2 == Geometry::wkb_polygon)
    BGCALL(result, equals, Polygon, g1, Polygon, g2, pnull_value);
  else if (gt1 == Geometry::wkb_polygon && gt2 ==Geometry::wkb_multipolygon)
    BGCALL(result, equals, Polygon, g1, Multipolygon, g2, pnull_value);
  else if (gt1 == Geometry::wkb_multipolygon && gt2 ==Geometry::wkb_polygon)
    BGCALL(result, equals, Multipolygon, g1, Polygon, g2, pnull_value);
  else if (gt1 == Geometry::wkb_multipolygon &&
           gt2 == Geometry::wkb_multipolygon)
    BGCALL(result, equals, Multipolygon, g1, Multipolygon, g2, pnull_value);
  else
    result= 0;
  return result;
}


/**
  Do disjoint relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::disjoint_check(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  int result= 0;

  *pbgdone= false;
  gt1= g1->get_type();

  switch (gt1)
  {
  case Geometry::wkb_point:
    result= BG_wrap<Geom_types>::
      point_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      polygon_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    result= BG_wrap<Geom_types>::
      multipolygon_disjoint_geometry(g1, g2, pbgdone, pnull_value);
    break;
  default:
    break;
  }

  /*
    Note: need disjoint(point, Linestring) and disjoint(linestring, Polygon)
   */
  return result;
}


/**
  Do interesects relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::intersects_check(Geometry *g1, Geometry *g2,
                                            bool *pbgdone, my_bool *pnull_value)
{
  Geometry::wkbType gt1;
  *pbgdone= false;
  int result= 0;

  gt1= g1->get_type();
  /*
    According to OGC SFA, intersects is identical to !disjoint, but
    boost geometry has functions to compute intersects, so we still call
    them.
   */
  switch (gt1)
  {
  case Geometry::wkb_point:
    result= BG_wrap<Geom_types>::
      point_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_linestring:
    result= BG_wrap<Geom_types>::
      linestring_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multilinestring:
    result= BG_wrap<Geom_types>::
      multilinestring_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_polygon:
    result= BG_wrap<Geom_types>::
      polygon_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_multipolygon:
    result= BG_wrap<Geom_types>::
      multipolygon_intersects_geometry(g1, g2, pbgdone, pnull_value);
    break;
  default:
    *pbgdone= false;
    break;
  }
  /*
    Note: need intersects(pnt, lstr), (lstr, plgn)
   */
  return result;
}


/**
  Do overlaps relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::overlaps_check(Geometry *g1, Geometry *g2,
                                          bool *pbgdone, my_bool *pnull_value)
{
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef std::set<Point, bgpt_lt> Point_set;
  typedef std::vector<Point> Point_vector;

  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if (g1->feature_dimension() != g2->feature_dimension())
  {
    *pbgdone= true;
    // OGC says this is not applicable, but PostGIS doesn't errout but
    // returns false.
    //null_value= true;
    //my_error(ER_GIS_UNSUPPORTED_ARGUMENT, MYF(0), "st_overlaps");
    return 0;
  }

  if (gt1 == Geometry::wkb_point || gt2 == Geometry::wkb_point)
  {
    *pbgdone= true;
    result= 0;
  }

  if (gt1 == Geometry::wkb_multipoint && gt2 == Geometry::wkb_multipoint)
    result= BG_wrap<Geom_types>::
      multipoint_overlaps_multipoint(g1, g2, pbgdone, pnull_value);

  /*
    Note: Need overlaps([m]ls, [m]ls), overlaps([m]plgn, [m]plgn).
   */
  return result;
}


/**
  Do touches relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::touches_check(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value)
{
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipolygon Multipolygon;

  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if ((gt1 == Geometry::wkb_point || gt1 == Geometry::wkb_multipoint) &&
      (gt2 == Geometry::wkb_point || gt2 == Geometry::wkb_multipoint))
  {
    *pbgdone= true;
    // OGC says this is not applicable, but PostGIS doesn't errout but
    // returns false.
    //null_value= true;
    //my_error(ER_GIS_UNSUPPORTED_ARGUMENT, MYF(0), "st_touches");
    return 0;
  }
  /*
    Touches is symetric, and one argument is allowed to be a Point/multipoint.
   */
  switch (gt1)
  {
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      BGCALL(result, touches, Polygon, g1, Polygon, g2, pnull_value);
      *pbgdone= true;
      break;
    case Geometry::wkb_multipolygon:
      BGCALL(result, touches, Polygon, g1, Multipolygon, g2, pnull_value);
      *pbgdone= true;
      break;
    default:
      *pbgdone= false;
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      BGCALL(result, touches, Multipolygon, g1, Polygon, g2, pnull_value);
      *pbgdone= true;
      break;
    case Geometry::wkb_multipolygon:
      BGCALL(result, touches, Multipolygon, g1, Multipolygon, g2, pnull_value);
      *pbgdone= true;
      break;
    default:
      *pbgdone= false;
      break;
    }
    break;
  default:
    *pbgdone= false;
    break;
  }
  /*
    Note: need touches(pnt, lstr), (pnt, plgn), (lstr, lstr), (lstr, plgn).
    for multi geometry, can iterate geos in it and compute for
    each geo separately.
   */
  return result;
}


/**
  Do crosses relation check of two geometries.
  Dispatch to specific BG functions according to operation type, and 1st or
  both operand types.

  @tparam Geom_types Geometry types definitions.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pbgdone Returns whether the specified relation check operation is
        performed. For now BG doesn't support many type combinatioons
        for each type of relation check. We have implemented some of the
        checks for some type combinations, which are not supported by BG,
        bgdone will also be set to true for such checks.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
*/
template<typename Geom_types>
int Item_func_spatial_rel::crosses_check(Geometry *g1, Geometry *g2,
                                         bool *pbgdone, my_bool *pnull_value)
{
  int result= 0;
  *pbgdone= false;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  if (gt1 == Geometry::wkb_polygon || gt2 == Geometry::wkb_point ||
      (gt1 == Geometry::wkb_multipolygon || gt2 == Geometry::wkb_multipoint))
  {
    *pbgdone= true;
    // OGC says this is not applicable, but PostGIS doesn't errout but
    // returns false.
    //null_value= true;
    //my_error(ER_GIS_UNSUPPORTED_ARGUMENT, MYF(0), "st_crosses");
    return 0;
  }

  if (gt1 == Geometry::wkb_point)
  {
    *pbgdone= true;
    result= 0;
    return result;
  }

  switch (gt1)
  {
  case Geometry::wkb_multipoint:
    result= BG_wrap<Geom_types>::
      multipoint_crosses_geometry(g1, g2, pbgdone, pnull_value);
    break;
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  /*
    Note: needs crosses([m]ls, [m]ls), crosses([m]ls, [m]plgn).
   */
  return result;
}


/**
  Entry point to call Boost Geometry functions to check geometry relations.
  This function is static so that it can be called without the
  Item_func_spatial_rel object --- we do so to implement a few functionality
  for other classes in this file, e.g. Item_func_spatial_operation::val_str.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] pisdone Returns whether the specified relation check operation is
        performed by BG. For now BG doesn't support many type combinatioons
        for each type of relation check. If isdone returns false, old GIS
        algorithms will be called to do the check.
  @param relchk_type The type of relation check.
  @param[out] pnull_value Returns whether error occured duirng the computation.
  @return 0 if specified relation doesn't hold for the given operands,
                otherwise returns none 0.
 */
template<typename Coord_type, typename Coordsys>
int Item_func_spatial_rel::bg_geo_relation_check(Geometry *g1, Geometry *g2,
                                                 bool *pisdone,
                                                 Functype relchk_type,
                                                 my_bool *pnull_value)
{
  int result= 0;
  bool bgdone= false;

  typedef BG_models<Coord_type, Coordsys> Geom_types;

  *pisdone= false;
  /*
    Dispatch calls to all specific type combinations for each relation check
    function.

    Boost.Geometry doesn't have dynamic polymorphism,
    e.g. the above Point, Linestring, and Polygon templates don't have a common
    base class template, so we have to dispatch by types.

    The checking functions should set bgdone to true if the relation check is
    performed, they should also set null_value to true if there is error.
   */

  switch (relchk_type) {
  case SP_CONTAINS_FUNC:
    result= within_check<Geom_types>(g2, g1, &bgdone, pnull_value);
    break;
  case SP_WITHIN_FUNC:
    result= within_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_EQUALS_FUNC:
    result= equals_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_DISJOINT_FUNC:
    result= disjoint_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_INTERSECTS_FUNC:
    result= intersects_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_OVERLAPS_FUNC:
    result= overlaps_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_TOUCHES_FUNC:
    result= touches_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  case SP_CROSSES_FUNC:
    result= crosses_check<Geom_types>(g1, g2, &bgdone, pnull_value);
    break;
  default:
    DBUG_ASSERT(FALSE);
    break;
  }

  *pisdone= bgdone;
  return result;
}

Item_func_spatial_operation::~Item_func_spatial_operation()
{
}

using std::auto_ptr;

inline static void reassemble_geometry(Geometry *g)
{
  Geometry::wkbType gtype= g->get_geotype();
  if (gtype == Geometry::wkb_polygon)
    down_cast<Gis_polygon *>(g)->to_wkb_unparsed();
  else if (gtype == Geometry::wkb_multilinestring)
    down_cast<Gis_multi_line_string *>(g)->reassemble();
  else if (gtype == Geometry::wkb_multipolygon)
    down_cast<Gis_multi_polygon *>(g)->reassemble();
}

/**
  For every Geometry object write-accessed by a boost geometry function, i.e.
  those passed as out parameter into set operation functions, call this
  function before using the result object's data.

  @param resbuf_mgr tracks the result buffer
  @return true if got error; false if no error occured.
*/
template <typename BG_geotype>
bool post_fix_result(BG_result_buf_mgr *resbuf_mgr,
                     BG_geotype &geout, String *res)
{
  DBUG_ASSERT(geout.has_geom_header_space());
  reassemble_geometry(&geout);
  if (geout.get_ptr() == NULL)
    return true;
  if (res)
  {
    char *resptr= geout.get_cptr() - GEOM_HEADER_SIZE;
    uint32 len= static_cast<uint32>(geout.get_nbytes());

    /*
      The resptr buffer is now owned by resbuf_mgr and used by res, resptr
      will be released properly by resbuf_mgr.
     */
    resbuf_mgr->add_buffer(resptr);
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


#define BGOPCALL(GeoOutType, geom_out, bgop,                            \
                 GeoType1, g1, GeoType2, g2, wkbres, nullval)           \
do                                                                      \
{                                                                       \
  const void *pg1= g1->normalize_ring_order();                          \
  const void *pg2= g2->normalize_ring_order();                          \
  geom_out= NULL;                                                       \
  if (pg1 != NULL && pg2 != NULL)                                       \
  {                                                                     \
    GeoType1 geo1(pg1, g1->get_data_size(), g1->get_flags(),            \
                  g1->get_srid());                                      \
    GeoType2 geo2(pg2, g2->get_data_size(), g2->get_flags(),            \
                  g2->get_srid());                                      \
    auto_ptr<GeoOutType>geout(new GeoOutType());                        \
    geout->set_srid(g1->get_srid());                                    \
    boost::geometry::bgop(geo1, geo2, *geout);                          \
    (nullval)= false;                                                   \
    if (geout->size() == 0 ||                                           \
        (nullval= post_fix_result(&(m_ifso->bg_resbuf_mgr),             \
                                  *geout, wkbres)))                     \
    {                                                                   \
      if (nullval)                                                      \
        return NULL;                                                    \
    }                                                                   \
    else                                                                \
      geom_out= geout.release();                                        \
  }                                                                     \
  else                                                                  \
  {                                                                     \
    (nullval)= true;                                                    \
    my_error(ER_GIS_INVALID_DATA, MYF(0), "st_" #bgop);                 \
    return NULL;                                                        \
  }                                                                     \
} while (0)


/*
  Write an empty geometry collection's wkb encoding into str, and create a
  geometry object for this empty geometry colletion.
 */
Geometry *Item_func_spatial_operation::empty_result(String *str, uint32 srid)
{
  if ((null_value= str->reserve(GEOM_HEADER_SIZE + 4 + 16, 256)))
    return 0;

  write_geometry_header(str, srid, Geometry::wkb_geometrycollection, 0);
  Gis_geometry_collection *gcol= new Gis_geometry_collection();
  gcol->set_data_ptr(str->ptr() + GEOM_HEADER_SIZE, 4);
  gcol->has_geom_header_space(true);
  return gcol;
}


/**
  Wraps and dispatches type specific BG function calls according to operation
  type and the 1st or both operand type(s), depending on code complexity.

  We want to isolate boost header file inclusion only inside this file, so we
  can't put this class declaration in any header file. And we want to make the
  methods static since no state is needed here.
  @tparam Geom_types A wrapper for all geometry types.
*/
template<typename Geom_types>
class BG_setop_wrapper
{
  // Some computation in this class may rely on functions in
  // Item_func_spatial_operation.
  Item_func_spatial_operation *m_ifso;
  my_bool null_value; // Whether computation has error.

  // Some computation in this class may rely on functions in
  // Item_func_spatial_operation, after each call of its functions, copy its
  // null_value, we don't want to miss errors.
  void copy_ifso_state()
  {
    null_value= m_ifso->null_value;
  }

public:
  typedef typename Geom_types::Point Point;
  typedef typename Geom_types::Linestring Linestring;
  typedef typename Geom_types::Polygon Polygon;
  typedef typename Geom_types::Multipoint Multipoint;
  typedef typename Geom_types::Multilinestring Multilinestring;
  typedef typename Geom_types::Multipolygon Multipolygon;
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;
  typedef Item_func_spatial_rel Ifsr;
  typedef Item_func_spatial_operation Ifso;
  typedef std::set<Point, bgpt_lt> Point_set;
  typedef std::vector<Point> Point_vector;

  BG_setop_wrapper(Item_func_spatial_operation *ifso)
  {
    m_ifso= ifso;
    null_value= 0;
  }


  my_bool get_null_value() const
  {
    return null_value;
  }


  /**
    Do point insersection point operation.
    @param g1 First Geometry operand, must be a Point.
    @param g2 Second Geometry operand, must be a Point.
    @param[out] result Holds WKB data of the result.
    @param[out] pdone Whether the operation is performed successfully.
    @return the result Geometry whose WKB data is in result.
    */
  Geometry *point_intersection_point(Geometry *g1, Geometry *g2,
                                     String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;

    *pdone= false;
    Point pt1(g1->get_data_ptr(),
              g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Point pt2(g2->get_data_ptr(),
              g2->get_data_size(), g2->get_flags(), g2->get_srid());

    if (bgpt_eq()(pt1, pt2))
    {
      retgeo= g1;
      null_value= retgeo->as_geometry(result, true);
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  /*
    Do point intersection Multipoint operation.
    The parameters and return value has identical/similar meaning as to above
    function, which can be inferred from the function name, we won't repeat
    here or for the rest of the functions in this class.
  */
  Geometry *point_intersection_multipoint(Geometry *g1, Geometry *g2,
                                          String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;

    *pdone= false;
    Point pt(g1->get_data_ptr(),
             g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint mpts(g2->get_data_ptr(),
                    g2->get_data_size(), g2->get_flags(), g2->get_srid());
    Point_set ptset(mpts.begin(), mpts.end());

    if (ptset.find(pt) != ptset.end())
    {
      retgeo= g1;
      null_value= retgeo->as_geometry(result, true);
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_intersection_geometry(Geometry *g1, Geometry *g2,
                                        String *result, bool *pdone)
  {
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    Geometry *retgeo= NULL;
    *pdone= false;
    /*
      Whether Ifsr::bg_geo_relation_check or this function is
      completed. Only check this variable immediately after calling the two
      functions. If !isdone, unable to proceed, simply return 0.

      This is also true for other uses of this variable in other member
      functions of this class.
     */
    bool isdone= false;

    bool is_out= !Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
      (g1, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value);

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    if (isdone && !null_value)
    {
      if (is_out)
      {
        null_value= g1->as_geometry(result, true);
        retgeo= g1;
      }
      else
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
      *pdone= true;
    }
    return retgeo;
  }


  Geometry *multipoint_intersection_multipoint(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;
    Point_set ptset1, ptset2;
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    *pdone= false;
    mpts->set_srid(g1->get_srid());

    Multipoint mpts1(g1->get_data_ptr(),
                     g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint mpts2(g2->get_data_ptr(),
                     g2->get_data_size(), g2->get_flags(), g2->get_srid());

    ptset1.insert(mpts1.begin(), mpts1.end());
    ptset2.insert(mpts2.begin(), mpts2.end());

    Point_vector respts;
    TYPENAME Point_vector::iterator endpos;
    size_t ptset1sz= ptset1.size(), ptset2sz= ptset2.size();
    respts.resize(ptset1sz > ptset2sz ? ptset1sz : ptset2sz);

    endpos= std::set_intersection(ptset1.begin(), ptset1.end(),
                                  ptset2.begin(), ptset2.end(),
                                  respts.begin(), bgpt_lt());
    std::copy(respts.begin(), endpos, std::back_inserter(*mpts));
    if (mpts->size() > 0)
    {
      null_value= m_ifso->assign_result(mpts, result);
      retgeo= mpts;
      guard.release();
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipoint_intersection_geometry(Geometry *g1, Geometry *g2,
                                             String *result, bool *pdone)
  {
    Geometry *retgeo= NULL;
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    Point_set ptset;
    Multipoint mpts(g1->get_data_ptr(),
                    g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint *mpts2= new Multipoint();
    auto_ptr<Multipoint> guard(mpts2);
    bool isdone= false;

    *pdone= false;
    mpts2->set_srid(g1->get_srid());

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    ptset.insert(mpts.begin(), mpts.end());

    for (TYPENAME Point_set::iterator i= ptset.begin(); i != ptset.end(); ++i)
    {
      Point &pt= const_cast<Point&>(*i);
      if (!Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
          (&pt, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) &&
          isdone && !null_value)
      {
        mpts2->push_back(pt);
      }

      if (null_value || !isdone)
        return 0;
    }

    if (mpts2->size() > 0)
    {
      null_value= m_ifso->assign_result(mpts2, result);
      retgeo= mpts2;
      guard.release();
    }
    else
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *linestring_intersection_polygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    Geometry::wkbType gt2= g2->get_type();
    Geometry *retgeo= NULL, *tmp1= NULL, *tmp2= NULL;
    *pdone= false;
    // It is likely for there to be discrete intersection Points.
    if (gt2 == Geometry::wkb_multipolygon)
    {
      BGOPCALL(Multilinestring, tmp1, intersection,
               Linestring, g1, Multipolygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Linestring, g1, Multipolygon, g2, NULL, null_value);
    }
    else
    {
      BGOPCALL(Multilinestring, tmp1, intersection,
               Linestring, g1, Polygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Linestring, g1, Polygon, g2, NULL, null_value);
    }

    // Need merge, exclude Points that are on the result Linestring.
    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (tmp1, tmp2, result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_intersection_multilinestring(Geometry *g1, Geometry *g2,
                                                 String *result, bool *pdone)
  {
    Geometry *retgeo= NULL, *tmp1= NULL;
    Multipoint *tmp2= NULL;
    auto_ptr<Geometry> guard1;

    *pdone= false;

    BGOPCALL(Multilinestring, tmp1, intersection,
             Polygon, g1, Multilinestring, g2, NULL, null_value);
    guard1.reset(tmp1);

    Multilinestring mlstr(g2->get_data_ptr(), g2->get_data_size(),
                          g2->get_flags(), g2->get_srid());
    Multipoint mpts;
    Point_set ptset;

    const void *data_ptr= g1->normalize_ring_order();
    if (data_ptr == NULL)
    {
      null_value= true;
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_intersection");
      return NULL;
    }

    Polygon plgn(data_ptr, g1->get_data_size(),
                 g1->get_flags(), g1->get_srid());

    for (TYPENAME Multilinestring::iterator i= mlstr.begin();
         i != mlstr.end(); ++i)
    {
      boost::geometry::intersection(plgn, *i, mpts);
      if (mpts.size() > 0)
      {
        ptset.insert(mpts.begin(), mpts.end());
        mpts.clear();
      }
    }

    auto_ptr<Multipoint> guard2;
    if (ptset.size() > 0)
    {
      tmp2= new Multipoint;
      tmp2->set_srid(g1->get_srid());
      guard2.reset(tmp2);
      std::copy(ptset.begin(), ptset.end(), std::back_inserter(*tmp2));
    }

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (guard1.release(), guard2.release(), result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_intersection_polygon(Geometry *g1, Geometry *g2,
                                         String *result, bool *pdone)
  {
    *pdone= false;
    Geometry::wkbType gt2= g2->get_type();
    Geometry *retgeo= NULL, *tmp1= NULL, *tmp2= NULL;

    if (gt2 == Geometry::wkb_polygon)
    {
      BGOPCALL(Multipolygon, tmp1, intersection,
               Polygon, g1, Polygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Polygon, g1, Polygon, g2, NULL, null_value);
    }
    else
    {
      BGOPCALL(Multipolygon, tmp1, intersection,
               Polygon, g1, Multipolygon, g2, NULL, null_value);
      BGOPCALL(Multipoint, tmp2, intersection,
               Polygon, g1, Multipolygon, g2, NULL, null_value);
    }

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (tmp1, tmp2, result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multilinestring_intersection_multipolygon(Geometry *g1,
                                                      Geometry *g2,
                                                      String *result,
                                                      bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL, *tmp1= NULL;
    Multipoint *tmp2= NULL;

    auto_ptr<Geometry> guard1;
    BGOPCALL(Multilinestring, tmp1, intersection,
             Multilinestring, g1, Multipolygon, g2,
             NULL, null_value);
    guard1.reset(tmp1);

    Multilinestring mlstr(g1->get_data_ptr(), g1->get_data_size(),
                          g1->get_flags(), g1->get_srid());
    Multipoint mpts;

    const void *data_ptr= g2->normalize_ring_order();
    if (data_ptr == NULL)
    {
      null_value= true;
      my_error(ER_GIS_INVALID_DATA, MYF(0), "st_intersection");
      return NULL;
    }

    Multipolygon mplgn(data_ptr, g2->get_data_size(),
                       g2->get_flags(), g2->get_srid());
    Point_set ptset;

    for (TYPENAME Multilinestring::iterator i= mlstr.begin();
         i != mlstr.end(); ++i)
    {
      boost::geometry::intersection(*i, mplgn, mpts);
      if (mpts.size() > 0)
      {
        ptset.insert(mpts.begin(), mpts.end());
        mpts.clear();
      }
    }

    auto_ptr<Multipoint> guard2;
    if (ptset.empty() == false)
    {
      tmp2= new Multipoint;
      tmp2->set_srid(g1->get_srid());
      guard2.reset(tmp2);
      std::copy(ptset.begin(), ptset.end(), std::back_inserter(*tmp2));
    }

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (guard1.release(), guard2.release(), result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_intersection_multipolygon(Geometry *g1, Geometry *g2,
                                                   String *result,
                                                   bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL, *tmp1= NULL, *tmp2= NULL;
    BGOPCALL(Multipolygon, tmp1, intersection,
             Multipolygon, g1, Multipolygon, g2, NULL, null_value);

    BGOPCALL(Multipoint, tmp2, intersection,
             Multipolygon, g1, Multipolygon, g2, NULL, null_value);

    retgeo= m_ifso->combine_sub_results<Coord_type, Coordsys>
      (tmp1, tmp2, result);
    copy_ifso_state();

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_union_point(Geometry *g1, Geometry *g2,
                              String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    Geometry::wkbType gt2= g2->get_type();
    Point_set ptset;// Use set to make Points unique.

    Point pt1(g1->get_data_ptr(),
              g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    mpts->set_srid(g1->get_srid());
    ptset.insert(pt1);
    if (gt2 == Geometry::wkb_point)
    {
      Point pt2(g2->get_data_ptr(),
                g2->get_data_size(), g2->get_flags(), g2->get_srid());
      ptset.insert(pt2);
    }
    else
    {
      Multipoint mpts2(g2->get_data_ptr(),
                       g2->get_data_size(), g2->get_flags(), g2->get_srid());
      ptset.insert(mpts2.begin(), mpts2.end());
    }

    std::copy(ptset.begin(), ptset.end(), std::back_inserter(*mpts));
    if (mpts->size() > 0)
    {
      retgeo= mpts;
      null_value= m_ifso->assign_result(mpts, result);
      guard.release();
    }
    else
    {
      if (!null_value)
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_union_geometry(Geometry *g1, Geometry *g2,
                                 String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    bool isdone= false;

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    if (Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
        (g1, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) &&
        isdone && !null_value)
    {
      Gis_geometry_collection *geocol= new Gis_geometry_collection(g2, result);
      null_value= (geocol == NULL || geocol->append_geometry(g1, result));
      retgeo= geocol;
    }
    else if (!isdone || null_value)
    {
      retgeo= NULL;
      return 0;
    }
    else
    {
      retgeo= g2;
      null_value= retgeo->as_geometry(result, true);
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipoint_union_multipoint(Geometry *g1, Geometry *g2,
                                        String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    Point_set ptset;
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    mpts->set_srid(g1->get_srid());
    Multipoint mpts1(g1->get_data_ptr(),
                     g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Multipoint mpts2(g2->get_data_ptr(),
                     g2->get_data_size(), g2->get_flags(), g2->get_srid());

    ptset.insert(mpts1.begin(), mpts1.end());
    ptset.insert(mpts2.begin(), mpts2.end());
    std::copy(ptset.begin(), ptset.end(), std::back_inserter(*mpts));

    if (mpts->size() > 0)
    {
      retgeo= mpts;
      null_value= m_ifso->assign_result(mpts, result);
      guard.release();
    }
    else
    {
      if (!null_value)
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipoint_union_geometry(Geometry *g1, Geometry *g2,
                                      String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
#if !defined(DBUG_OFF)
    Geometry::wkbType gt2= g2->get_type();
#endif
    Point_set ptset;
    Multipoint mpts(g1->get_data_ptr(),
                    g1->get_data_size(), g1->get_flags(), g1->get_srid());
    bool isdone= false;

    DBUG_ASSERT(gt2 == Geometry::wkb_linestring ||
                gt2 == Geometry::wkb_polygon ||
                gt2 == Geometry::wkb_multilinestring ||
                gt2 == Geometry::wkb_multipolygon);
    ptset.insert(mpts.begin(), mpts.end());

    Gis_geometry_collection *geocol= new Gis_geometry_collection(g2, result);
    auto_ptr<Gis_geometry_collection> guard(geocol);
    bool added= false;

    for (TYPENAME Point_set::iterator i= ptset.begin(); i != ptset.end(); ++i)
    {
      Point &pt= const_cast<Point&>(*i);
      if (Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
          (&pt, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) &&
          isdone)
      {
        if (null_value || (null_value= geocol->append_geometry(&pt, result)))
          break;
        added= true;
      }

      if (!isdone)
        break;
    }

    if (null_value || !isdone)
      return 0;

    if (added)
    {
      // Result is already filled above.
      retgeo= geocol;
      guard.release();
    }
    else
    {
      retgeo= g2;
      null_value= g2->as_geometry(result, true);
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_union_polygon(Geometry *g1, Geometry *g2,
                                  String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, union_, Polygon, g1, Polygon, g2,
             result, null_value);
    if (retgeo && !null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_union_multipolygon(Geometry *g1, Geometry *g2,
                                       String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, union_,
             Polygon, g1, Multipolygon, g2, result, null_value);
    if (retgeo && !null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_union_multipolygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, union_,
             Multipolygon, g1, Multipolygon, g2, result, null_value);

    if (retgeo && !null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *point_difference_geometry(Geometry *g1, Geometry *g2,
                                      String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    bool isdone= false;
    bool is_out= Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
      (g1, g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value);

    if (isdone && !null_value)
    {
      if (is_out)
      {
        retgeo= g1;
        null_value= retgeo->as_geometry(result, true);
      }
      else
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
      if (!null_value)
        *pdone= true;
    }
    return retgeo;
  }


  Geometry *multipoint_difference_geometry(Geometry *g1, Geometry *g2,
                                           String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;
    Multipoint *mpts= new Multipoint();
    auto_ptr<Multipoint> guard(mpts);

    mpts->set_srid(g1->get_srid());
    Multipoint mpts1(g1->get_data_ptr(),
                     g1->get_data_size(), g1->get_flags(), g1->get_srid());
    Point_set ptset;
    bool isdone= false;

    for (TYPENAME Multipoint::iterator i= mpts1.begin();
         i != mpts1.end(); ++i)
    {
      if (Ifsr::bg_geo_relation_check<Coord_type, Coordsys>
          (&(*i), g2, &isdone, Ifsr::SP_DISJOINT_FUNC, &null_value) && isdone)
      {
        if (null_value)
          return 0;
        ptset.insert(*i);
      }

      if (!isdone)
        return 0;
    }

    if (ptset.empty() == false)
    {
      std::copy(ptset.begin(), ptset.end(), std::back_inserter(*mpts));
      null_value= m_ifso->assign_result(mpts, result);
      retgeo= mpts;
      guard.release();
    }
    else
    {
      if (!null_value)
      {
        retgeo= m_ifso->empty_result(result, g1->get_srid());
        copy_ifso_state();
      }
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *linestring_difference_polygon(Geometry *g1, Geometry *g2,
                                          String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Linestring, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }

    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *linestring_difference_multipolygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Linestring, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_difference_polygon(Geometry *g1, Geometry *g2,
                                       String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Polygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_difference_multipolygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Polygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multilinestring_difference_polygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Multilinestring, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multilinestring_difference_multipolygon(Geometry *g1, Geometry *g2,
                                                    String *result,
                                                    bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multilinestring, retgeo, difference,
             Multilinestring, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_difference_polygon(Geometry *g1, Geometry *g2,
                                            String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Multipolygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_difference_multipolygon(Geometry *g1, Geometry *g2,
                                                 String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, difference,
             Multipolygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_symdifference_polygon(Geometry *g1, Geometry *g2,
                                          String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Polygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *polygon_symdifference_multipolygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Polygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_symdifference_polygon(Geometry *g1, Geometry *g2,
                                               String *result, bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Multipolygon, g1, Polygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }


  Geometry *multipolygon_symdifference_multipolygon(Geometry *g1, Geometry *g2,
                                                    String *result,
                                                    bool *pdone)
  {
    *pdone= false;
    Geometry *retgeo= NULL;

    BGOPCALL(Multipolygon, retgeo, sym_difference,
             Multipolygon, g1, Multipolygon, g2, result, null_value);

    if (!retgeo && !null_value)
    {
      retgeo= m_ifso->empty_result(result, g1->get_srid());
      copy_ifso_state();
    }
    if (!null_value)
      *pdone= true;
    return retgeo;
  }
};


/**
  Do intersection operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
intersection_operation(Geometry *g1, Geometry *g2,
                       String *result, bool *pdone)
{
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();
  *pdone= false;

  switch (gt1)
  {
  case Geometry::wkb_point:
    switch (gt2)
    {
    case Geometry::wkb_point:
      retgeo= wrap.point_intersection_point(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipoint:
      retgeo= wrap.point_intersection_multipoint(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.point_intersection_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }

    break;
  case Geometry::wkb_multipoint:
    switch (gt2)
    {
    case Geometry::wkb_point:
      retgeo= wrap.point_intersection_multipoint(g2, g1, result, pdone);
      break;

    case Geometry::wkb_multipoint:
      retgeo= wrap.multipoint_intersection_multipoint(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipoint_intersection_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_linestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
      /*
        The Multilinestring call isn't supported for these combinations,
        but such a result is quite likely, thus can't use bg for
        this combination.
       */
      break;
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.linestring_intersection_polygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      retgeo= wrap.polygon_intersection_multilinestring(g1, g2,
                                                        result, pdone);
      break;
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      // Note: for now BG's set operations don't allow returning a
      // Multilinestring, thus this result isn't complete.
      retgeo= wrap.polygon_intersection_polygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multilinestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      /*
        The Multilinestring call isn't supported for these combinations,
        but such a result is quite likely, thus can't use bg for
        this combination.
       */
      break;

    case Geometry::wkb_multipolygon:
      retgeo= wrap.multilinestring_intersection_multipolygon(g1, g2,
                                                             result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
      retgeo= intersection_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_intersection_multipolygon(g1, g2,
                                                          result, pdone);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Do union operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
union_operation(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();
  *pdone= false;

  // Note that union can't produce empty point set unless given two empty
  // point set arguments.
  switch (gt1)
  {
  case Geometry::wkb_point:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= wrap.point_union_point(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.point_union_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipoint:
    switch (gt2)
    {
    case Geometry::wkb_point:
      retgeo= wrap.point_union_point(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multipoint:
      retgeo= wrap.multipoint_union_multipoint(g1, g2, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipoint_union_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_linestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multipolygon:
    /*
      boost geometry doesn't support union with either parameter being
      Linestring or Multilinestring, and we can't do simple calculation
      to Linestring as Points above. In following code this is denoted
      as NOT_SUPPORTED_BY_BG.

      Note: Also, current bg::union functions don't allow result being
      Multilinestring, thus these calculation isn't possible.
     */
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      // NOT_SUPPORTED_BY_BG
      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.polygon_union_polygon(g1, g2, result, pdone);
      // Union can't produce empty point set.
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.polygon_union_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multilinestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
      break;
    case Geometry::wkb_multilinestring:
      // NOT_SUPPORTED_BY_BG
      break;
    case Geometry::wkb_multipolygon:
      // NOT_SUPPORTED_BY_BG
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
      retgeo= union_operation<Geom_types>(g2, g1, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_union_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Do difference operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
difference_operation(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();
  *pdone= false;

  /*
    Given two geometries g1 and g2, where g1.dimension < g2.dimension, then
    g2 - g1 is equal to g2, this is always true. This is how postgis works.
    Below implementation uses this fact.
   */
  switch (gt1)
  {
  case Geometry::wkb_point:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.point_difference_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipoint:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_polygon:
    case Geometry::wkb_multilinestring:
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipoint_difference_geometry(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_linestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);
      if (!null_value)
        *pdone= true;
      break;
    case Geometry::wkb_linestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.linestring_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.linestring_difference_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_polygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);

      if (!null_value)
        *pdone= true;

      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.polygon_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.polygon_difference_multipolygon(g1, g2, result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multilinestring:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);

      if (!null_value)
        *pdone= true;

      break;
    case Geometry::wkb_linestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.multilinestring_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multilinestring:
      /*
        The result from boost geometry is wrong for this combination.
        NOT_SUPPORTED_BY_BG
       */
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multilinestring_difference_multipolygon(g1, g2,
                                                           result, pdone);
      break;
    default:
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
      retgeo= g1;
      null_value= g1->as_geometry(result, true);

      if (!null_value)
        *pdone= true;

      break;
    case Geometry::wkb_polygon:
      retgeo= wrap.multipolygon_difference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_difference_multipolygon(g1, g2,
                                                        result, pdone);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Do symdifference operation for two geometries, dispatch to specific BG
  function wrapper calls according to set operation type, and the 1st or
  both operand types.

  @tparam Geom_types A wrapper for all geometry types.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param[out] result Holds WKB data of the result.
  @param[out] pdone Whether the operation is performed successfully.
  @return The result geometry whose WKB data is held in result.
 */
template <typename Geom_types>
Geometry *Item_func_spatial_operation::
symdifference_operation(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  typedef typename Geom_types::Coord_type Coord_type;
  typedef typename Geom_types::Coordsys Coordsys;

  BG_setop_wrapper<Geom_types> wrap(this);
  Geometry *retgeo= NULL;
  Geometry::wkbType gt1= g1->get_type();
  Geometry::wkbType gt2= g2->get_type();

  /*
    Note: g1 sym-dif g2 <==> (g1 union g2) dif (g1 intersection g2), so
    theoretically we can compute symdifference results for any type
    combination using the other 3 kinds of set operations. We need to use
    geometry collection set operations to implement symdifference of any
    two geometry, because the return values of them may be
    geometry-collections.

    Boost geometry explicitly and correctly supports symdifference for the
    following four type combinations.
   */
  bool do_geocol_setop= false;

  switch (gt1)
  {
  case Geometry::wkb_polygon:

    switch (gt2)
    {
    case Geometry::wkb_polygon:
      retgeo= wrap.polygon_symdifference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.polygon_symdifference_multipolygon(g1, g2, result, pdone);
      break;
    default:
      do_geocol_setop= true;
      break;
    }
    break;
  case Geometry::wkb_multipolygon:
    switch (gt2)
    {
    case Geometry::wkb_polygon:
      retgeo= wrap.multipolygon_symdifference_polygon(g1, g2, result, pdone);
      break;
    case Geometry::wkb_multipolygon:
      retgeo= wrap.multipolygon_symdifference_multipolygon(g1, g2,
                                                           result, pdone);
      break;
    default:
      do_geocol_setop= true;
      break;
    }
    break;
  default:
    do_geocol_setop= true;
    break;
  }

  if (do_geocol_setop)
    retgeo= geometry_collection_set_operation<Coord_type,
      Coordsys>(g1, g2, result, pdone);
  else
    null_value= wrap.get_null_value();
  return retgeo;
}


/**
  Call boost geometry set operations to compute set operation result, and
  returns the result as a Geometry object.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First Geometry operand, not a geometry collection.
  @param g2 Second Geometry operand, not a geometry collection.
  @param opdone takes back whether the set operation is successfully completed,
  failures include:
    1. boost geometry doesn't support a type combination for a set operation.
    2. gis computation(not mysql code) got error, null_value isn't set to true.
    3. the relation check called isn't completed successfully, unable to proceed
       the set operation, and null_value isn't true.
  It is used in order to distinguish the types of errors above. And when caller
  got an false 'opdone', it should fallback to old gis set operation.

  @param[out] result buffer containing the GEOMETRY byte string of
  the returned geometry.

  @return If the set operation results in an empty point set, return a
  geometry collection containing 0 objects. If opdone or null_value is set to
  true, always returns 0. The returned geometry object can be used in the same
  val_str call.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
bg_geo_set_op(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  typedef BG_models<Coord_type, Coordsys> Geom_types;

  Geometry *retgeo= NULL;

  if (g1->get_coordsys() != g2->get_coordsys())
    return 0;

  *pdone= false;

  switch (spatial_op)
  {
  case Gcalc_function::op_intersection:
    retgeo= intersection_operation<Geom_types>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_union:
    retgeo= union_operation<Geom_types>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_difference:
    retgeo= difference_operation<Geom_types>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_symdifference:
    retgeo= symdifference_operation<Geom_types>(g1, g2, result, pdone);
    break;
  default:
    // Other operations are not set operations.
    DBUG_ASSERT(false);
    break;
  }

  /*
    null_value is set in above xxx_operatoin calls if error occured.
  */
  if (null_value)
  {
    error_str();
    *pdone= false;
    DBUG_ASSERT(retgeo == NULL);
  }

  // If we got effective result, the wkb encoding is written to 'result', and
  // the retgeo is effective Geometry object whose data Points into
  // 'result''s data.
  return retgeo;

}


/*
  Here wkbres is a piece of geometry data of GEOMETRY format,
  i.e. an SRID prefixing a WKB.
  Check if wkbres is the data of an empty geometry collection.
 */
static inline bool is_empty_geocollection(const String &wkbres)
{
  if (wkbres.ptr() == NULL)
    return true;

  uint32 geotype= uint4korr(wkbres.ptr() + SRID_SIZE + 1);

  return (geotype == static_cast<uint32>(Geometry::wkb_geometrycollection) &&
          uint4korr(wkbres.ptr() + SRID_SIZE + WKB_HEADER_SIZE) == 0);
}


/**
  Combine sub-results of set operation into a geometry collection.
  This function eliminates points in geo2 that are within
  geo1(polygons or linestrings). We have to do so
  because BG set operations return results in 3 forms --- multipolygon,
  multilinestring and multipoint, however given a type of set operation and
  the operands, the returned 3 types of results may intersect, and we want to
  eliminate the points already in the polygons/linestrings. And in future we
  also need to remove the linestrings that are already in the polygons, this
  isn't done now because there are no such set operation results to combine.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param geo1 First operand, a Multipolygon or Multilinestring object
              computed by BG set operation.
  @param geo2 Second operand, a Multipoint object
              computed by BG set operation.
  @param result Holds result geometry's WKB data in GEOMETRY format.
  @return A geometry combined from geo1 and geo2. Either or both of
  geo1 and geo2 can be NULL, so we may end up with a multipoint,
  a multipolygon/multilinestring, a geometry collection, or an
  empty geometry collection.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
combine_sub_results(Geometry *geo1, Geometry *geo2, String *result)
{
  typedef BG_models<Coord_type, Coordsys> Geom_types;
  typedef typename Geom_types::Multipoint Multipoint;
  Geometry *retgeo= NULL;
  bool isin= false, isdone= false, added= false;

  if (null_value)
  {
    delete geo1;
    delete geo2;
    return NULL;
  }

  auto_ptr<Geometry> guard1(geo1), guard2(geo2);

  Gis_geometry_collection *geocol= NULL;
  if (geo1 == NULL && geo2 == NULL)
    retgeo= empty_result(result, Geometry::default_srid);
  else if (geo1 != NULL && geo2 == NULL)
  {
    retgeo= geo1;
    null_value= assign_result(geo1, result);
    guard1.release();
  }
  else if (geo1 == NULL && geo2 != NULL)
  {
    retgeo= geo2;
    null_value= assign_result(geo2, result);
    guard2.release();
  }

  if (geo1 == NULL || geo2 == NULL)
  {
    if (null_value)
      retgeo= NULL;
    return retgeo;
  }

  DBUG_ASSERT((geo1->get_type() == Geometry::wkb_multilinestring ||
               geo1->get_type() == Geometry::wkb_multipolygon) &&
              geo2->get_type() == Geometry::wkb_multipoint);
  Multipoint mpts(geo2->get_data_ptr(), geo2->get_data_size(),
                  geo2->get_flags(), geo2->get_srid());
  geocol= new Gis_geometry_collection(geo1, result);
  auto_ptr<Gis_geometry_collection> guard3(geocol);
  my_bool had_error= false;

  for (TYPENAME Multipoint::iterator i= mpts.begin();
       i != mpts.end(); ++i)
  {
    isin= !Item_func_spatial_rel::bg_geo_relation_check<Coord_type,
      Coordsys>(&(*i), geo1, &isdone, SP_DISJOINT_FUNC, &had_error);

    // The bg_geo_relation_check can't handle pt intersects/within/disjoint ls
    // for now(isdone == false), so we have no points in mpts. When BG's
    // missing feature is completed, we will work correctly here.
    if (had_error)
    {
      error_str();
      return NULL;
    }

    if (!isin)
    {
      geocol->append_geometry(&(*i), result);
      added= true;
    }
  }

  if (added)
  {
    retgeo= geocol;
    guard3.release();
  }
  else
  {
    retgeo= geo1;
    guard1.release();
    null_value= assign_result(geo1, result);
  }

  if (null_value)
    error_str();

  return retgeo;
}


/*
  Do set operations on geometries.
  Writes geometry set operation result into str_value_arg in wkb format.
 */
String *Item_func_spatial_operation::val_str(String *str_value_arg)
{
  DBUG_ENTER("Item_func_spatial_operation::val_str");
  DBUG_ASSERT(fixed == 1);
  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1= NULL, *g2= NULL, *gres= NULL;
  uint32 srid= 0;
  Gcalc_operation_transporter trn(&func, &collector);
  bool opdone= false;
  bool had_except1= false, had_except2= false;

  // Release last call's result buffer.
  bg_resbuf_mgr.free_result_buffer();

  // Clean up the result first, since caller may give us one with non-NULL
  // buffer, we don't need it here.
  str_value_arg->set(NullS, 0, &my_charset_bin);

  if (func.reserve_op_buffer(1))
    DBUG_RETURN(0);
  func.add_operation(spatial_op, 2);

  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    goto exit;
  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    DBUG_RETURN(error_str());
  }

  // The two geometry operand must be in the same coordinate system.
  if (g1->get_srid() != g2->get_srid())
  {
    my_error(ER_GIS_DIFFERENT_SRIDS, MYF(0), func_name(),
             g1->get_srid(), g2->get_srid());
    DBUG_RETURN(error_str());
  }

  str_value_arg->set_charset(&my_charset_bin);
  str_value_arg->length(0);


  /*
    Catch all exceptions to make sure no exception can be thrown out of
    current function. Put all and any code that calls Boost.Geometry functions,
    STL functions into this try block. Code out of the try block should never
    throw any exception.
  */
  try
  {
    if (g1->get_type() != Geometry::wkb_geometrycollection &&
        g2->get_type() != Geometry::wkb_geometrycollection)
      gres= bg_geo_set_op<double, bgcs::cartesian>(g1, g2, str_value_arg,
                                                   &opdone);
    else
      gres= geometry_collection_set_operation<double, bgcs::cartesian>
        (g1, g2, str_value_arg, &opdone);

  }
  CATCH_ALL(func_name(), had_except1= true)

  try
  {
    /*
      Release intermediate geometry data buffers accumulated during execution
      of this set operation.
    */
    if (!str_value_arg->is_alloced() && gres != g1 && gres != g2)
      bg_resbuf_mgr.set_result_buffer(const_cast<char *>(str_value_arg->ptr()));
    bg_resbuf_mgr.free_intermediate_result_buffers();
  }
  CATCH_ALL(func_name(), had_except2= true)

  if (had_except1 || had_except2 || null_value)
  {
    opdone= false;
    if (gres != NULL && gres != g1 && gres != g2)
    {
      delete gres;
      gres= NULL;
    }
    DBUG_RETURN(error_str());
  }

  if (gres != NULL)
  {
    DBUG_ASSERT(!null_value && opdone && str_value_arg->length() > 0);

    /*
      There are 3 ways to create the result geometry object and allocate
      memory for the result String object:
      1. Created in BGOPCALL and allocated by BG code using gis_wkb_alloc
         functions; The geometry result object's memory is took over by
         str_value_arg, thus not allocated by str_value_arg.
      2. Created as a Gis_geometry_collection object and allocated by
         str_value_arg's String member functions.
      3. One of g1 or g2 used as result and g1/g2's String object is used as
         final result without duplicating their byte strings. Also, g1 and/or
         g2 may be used as intermediate result and their byte strings are
         assigned to intermediate String objects without giving the ownerships
         to them, so they are always owned by tmp_value1 and/or tmp_value2.

      Among above 3 ways, #1 and #2 write the byte string only once without
      any data copying, #3 doesn't write any byte strings.
     */
    if (!str_value_arg->is_alloced() && gres != g1 && gres != g2)
      DBUG_ASSERT(gres->has_geom_header_space() && gres->is_bg_adapter());
    else
    {
      DBUG_ASSERT((gres->has_geom_header_space() &&
                   gres->get_geotype() == Geometry::wkb_geometrycollection) ||
                  (gres == g1 || gres == g2));
      if (gres == g1)
        str_value_arg= res1;
      else if (gres == g2)
        str_value_arg= res2;
    }

    goto exit;
  }
  else if (opdone)
  {
    /*
      It's impossible to arrive here because the code calling BG features only
      returns NULL if not done, otherwise if result is empty, it returns an
      empty geometry collection whose pointer isn't NULL.
     */
    DBUG_ASSERT(false);
    goto exit;
  }

  DBUG_ASSERT(!opdone && gres == NULL);
  // We caught error, don't proceed with old GIS algorithm but error out.
  if (null_value)
    goto exit;

  /* Fall back to old GIS algorithm. */
  null_value= true;

  str_value_arg->set(NullS, 0U, &my_charset_bin);
  if (str_value_arg->reserve(SRID_SIZE, 512))
    goto exit;
  str_value_arg->q_append(srid);

  if (g1->store_shapes(&trn) || g2->store_shapes(&trn))
    goto exit;
#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  collector.prepare_operation();
  if (func.alloc_states())
    goto exit;

  operation.init(&func);

  if (operation.count_all(&collector) ||
      operation.get_result(&res_receiver))
    goto exit;


  if (!Geometry::create_from_opresult(&buffer1, str_value_arg, res_receiver))
    goto exit;

  /*
    If got some result, it's not NULL, note that we prepended an
    srid above(4 bytes).
  */
  if (str_value_arg->length() > 4)
    null_value= false;

exit:
  collector.reset();
  func.reset();
  res_receiver.reset();
  if (gres != g1 && gres != g2 && gres != NULL)
    delete gres;
  DBUG_RETURN(null_value ? NULL : str_value_arg);
}


/**
  Utility class, reset specified variable 'valref' to specified 'oldval' when
  val_resetter<valtype> instance is destroyed.
  @tparam Valtype Variable type to reset.
 */
template <typename Valtype>
class Var_resetter
{
private:
  Valtype *valref;
  Valtype oldval;

  // Forbid use, to eliminate a warning: oldval may be used uninitialized.
  Var_resetter();
  Var_resetter(const Var_resetter &o);
  Var_resetter &operator=(const Var_resetter&);
public:
  Var_resetter(Valtype *v, Valtype oldval) : valref(v)
  {
    this->oldval= oldval;
  }

  ~Var_resetter() { *valref= oldval; }
};


/**
  Do set operation on geometry collections.
  BG doesn't directly support geometry collections in any function, so we
  have to do so by computing the set operation result of all two operands'
  components, which must be the 6 basic types of geometries, and then we
  combine the sub-results.

  This function dispatches to specific set operation types.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
  two geometry collections. We rely on some Boost Geometry functions to do
  geometry relation checks and set operations to the components of g1 and g2,
  and since BG now doesn't support some type combinations for each type of
  operaton, we may not be able to perform the operation. If so, old GIS
  algorithm is called to do so.
  @return The set operation result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geometry_collection_set_operation(Geometry *g1, Geometry *g2,
                                  String *result, bool *pdone)
{
  Geometry *gres= NULL;
  *pdone= false;

  switch (this->spatial_op)
  {
  case Gcalc_function::op_intersection:
    gres= geocol_intersection<Coord_type, Coordsys>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_union:
    gres= geocol_union<Coord_type, Coordsys>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_difference:
    gres= geocol_difference<Coord_type, Coordsys>(g1, g2, result, pdone);
    break;
  case Gcalc_function::op_symdifference:
    gres= geocol_symdifference<Coord_type, Coordsys>(g1, g2, result, pdone);
    break;
  default:
    DBUG_ASSERT(false);                              /* Only above four supported. */
    break;
  }

  if (gres == NULL && *pdone && !null_value)
    gres= empty_result(result, g1->get_srid());
  return gres;
}


/**
  Do intersection operation on geometry collections. We do intersection for
  all pairs of components in g1 and g2, put the results in a geometry
  collection. If all subresults can be computed successfully, the geometry
  collection is our result.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The intersection result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_intersection(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  Geometry *gres= NULL;
  String wkbres;
  bool opdone= false;
  Geometry *g0= NULL;
  BG_geometry_collection bggc1, bggc2, bggc;

  bggc1.fill(g1);
  bggc2.fill(g2);
  *pdone= false;

  for (BG_geometry_collection::Geometry_list::iterator i=
       bggc1.get_geometries().begin(); i != bggc1.get_geometries().end(); ++i)
  {
    for (BG_geometry_collection::Geometry_list::iterator
         j= bggc2.get_geometries().begin();
         j != bggc2.get_geometries().end(); ++j)
    {
      // Free before using it, wkbres may have WKB data from last execution.
      wkbres.mem_free();
      opdone= false;
      g0= bg_geo_set_op<Coord_type, Coordsys>(*i, *j, &wkbres, &opdone);

      if (!opdone || null_value)
      {
        if (g0 != NULL && g0 != *i && g0 != *j)
          delete g0;
        return 0;
      }

      if (g0 && !is_empty_geocollection(wkbres))
        bggc.fill(g0);
      if (g0 != NULL && g0 != *i && g0 != *j)
      {
        delete g0;
        g0= NULL;
      }
    }
  }
  /*
    Note: result unify and merge

    The result may have geometry elements that overlap, caused by overlap
    geos in either or both gc1 and/or gc2. Also, there may be geometries
    that can be merged into a larger one of the same type in the result.
    We will need to figure out how to make such enhancements.
   */
  gres= bggc.as_geometry_collection(result);
  if (!null_value)
    *pdone= true;

  return gres;
}


/**
  Merge all components as appropriate so that the object contains only
  components that don't overlap.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param ifso the Item_func_spatial_operation object, we here rely on it to
         do union operation.
  @param[out] pdone takes back whether the merge operation is completed.
  @param[out] pnull_value takes back null_value set during the operation.
 */
template<typename Coord_type, typename Coordsys>
void BG_geometry_collection::
merge_components(Item_func_spatial_operation *ifso,
                 bool *pdone, my_bool *pnull_value)
{

  while (merge_one_run<Coord_type, Coordsys>(ifso, pdone, pnull_value))
    ;
}


/**
  One run of merging components.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param ifso the Item_func_spatial_operation object, we here rely on it to
         do union operation.
  @param[out] pdone takes back whether the merge operation is completed.
  @param[out] pnull_value takes back null_value set during the operation.
 */
template<typename Coord_type, typename Coordsys>
bool BG_geometry_collection::merge_one_run(Item_func_spatial_operation *ifso,
                                           bool *pdone, my_bool *pnull_value)
{
  Geometry *gres= NULL;
  Geometry *gi= NULL, *gj= NULL;
  bool isdone= false;
  bool has_overlap= false;
  bool opdone= false;
  my_bool &null_value= *pnull_value;
  size_t idx= 0, idx2= 0, ngeos= m_geos.size();
  BG_geometry_collection &bggc= *this;
  String wkbres;

  *pdone= false;

  for (Geometry_list::iterator i= m_geos.begin();
       i != m_geos.end() && idx < ngeos; ++i, ++idx)
  {
    // Move i after already isolated geometries.
    if (idx < m_num_isolated)
      continue;

    idx2= 0;
    for (Geometry_list::iterator
         j= m_geos.begin(); idx2 < ngeos && j != m_geos.end(); ++j, ++idx2)
    {
      if (idx2 < m_num_isolated + 1)
        continue;

      isdone= false;
      opdone= false;

      if (Item_func_spatial_rel::bg_geo_relation_check<Coord_type, Coordsys>
          (*i, *j, &isdone, Item_func::SP_OVERLAPS_FUNC, &null_value) &&
          isdone && !null_value)
      {
        // Free before using it, wkbres may have WKB data from last execution.
        wkbres.mem_free();
        gres= ifso->bg_geo_set_op<Coord_type, Coordsys>(*i, *j,
                                                        &wkbres, &opdone);
        null_value= ifso->null_value;

        if (!opdone || null_value)
        {
          if (gres != NULL && gres != *i && gres != *j)
            delete gres;
          return false;
        }

        gi= *i;
        gj= *j;

        /*
          Only std::list has the iterator validity on erase operation,
          vector and deque don't, and vector has expensive erase cost,
          that's why we are using std::list for m_geos.
         */
        m_geos.erase(i);
        m_geos.erase(j);

        // The union result is appended at end of the geometry list.
        bggc.fill(gres);
        if (gres != NULL && gres != gi && gres != gj)
        {
          delete gres;
          gres= NULL;
        }
        has_overlap= true;

        /*
          When we erase i and j, the two iterations are both invalid,
          so we have to start new iterations.
         */
        break;
      }

      /*
        Proceed if !isdone to leave the two geometries as is, this is OK.
       */
      if (null_value)
        return false;

    }

    // Isolated ones are part of all geometries.
    DBUG_ASSERT(m_num_isolated <= idx);

    /*
      *i doesn't overlap with anyone after it, it's isolated, and
      because of this it won't overlap with any following union result
      which are always appended at end of the list(after current *i), and
      thus we can skip them and start new iterations from m_num_isolated'th
      geometry.
     */
    if (!has_overlap)
      m_num_isolated++;
    else
      break;
  }

  *pdone= true;
  return has_overlap;
}


/**
  Do union operation on geometry collections. We do union for
  all pairs of components in g1 and g2, whenever a union can be done, we do
  so and put the results in a geometry collection GC and remove the two
  components from g1 and g2 respectively. Finally no components in g1 and g2
  overlap and GC is our result.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The union result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_union(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  Geometry *gres= NULL;
  BG_geometry_collection bggc;

  bggc.fill(g1);
  bggc.fill(g2);
  *pdone= false;

  bggc.merge_components<Coord_type, Coordsys>(this, pdone, &null_value);
  if (!null_value && *pdone)
    gres= bggc.as_geometry_collection(result);

  return gres;
}


/**
  Do difference operation on geometry collections. For each component CX in g1,
  we do CX:= CX difference CY for all components CY in g2. When at last CX isn't
  empty, it's put into result geometry collection GC.
  If all subresults can be computed successfully, the geometry
  collection GC is our result.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The difference result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_difference(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  Geometry *gres= NULL;
  bool opdone= false;
  String *wkbres= NULL;
  BG_geometry_collection bggc1, bggc2, bggc;

  bggc1.fill(g1);
  bggc2.fill(g2);
  *pdone= false;

  for (BG_geometry_collection::Geometry_list::iterator
       i= bggc1.get_geometries().begin();
       i != bggc1.get_geometries().end(); ++i)
  {
    bool g11_isempty= false;
    auto_ptr<Geometry> guard11;
    Geometry *g11= NULL;
    g11= *i;
    Inplace_vector<String> wkbstrs(PSI_INSTRUMENT_ME);

    for (BG_geometry_collection::Geometry_list::iterator
         j= bggc2.get_geometries().begin();
         j != bggc2.get_geometries().end(); ++j)
    {
      wkbres= wkbstrs.append_object();
      if (wkbres == NULL)
        return NULL;
      opdone= false;
      Geometry *g0= bg_geo_set_op<Coord_type, Coordsys>(g11, *j, wkbres,
                                                        &opdone);
      auto_ptr<Geometry> guard0(g0);

      if (!opdone || null_value)
      {
        if (!(g0 != NULL && g0 != *i && g0 != *j))
          guard0.release();
        if (!(g11 != NULL && g11 != g0 && g11 != *i && g11 != *j))
          guard11.release();
        return NULL;
      }

      if (g0 != NULL && !is_empty_geocollection(*wkbres))
      {
        if (g11 != NULL && g11 != *i && g11 != *j && g11 != g0)
          delete guard11.release();
        else
          guard11.release();
        guard0.release();
        g11= g0;
        if (g0 != NULL && g0 != *i && g0 != *j)
          guard11.reset(g11);
      }
      else
      {
        g11_isempty= true;
        if (!(g0 != NULL && g0 != *i && g0 != *j && g0 != g11))
          guard0.release();
        break;
      }
    }

    if (!g11_isempty)
      bggc.fill(g11);
    if (!(g11 != NULL && g11 != *i))
      guard11.release();
    else
      guard11.reset(NULL);
  }

  gres= bggc.as_geometry_collection(result);
  if (!null_value)
    *pdone= true;

  return gres;
}


/**
  Do symdifference operation on geometry collections. We do so according to
  this formula:
  g1 symdifference g2 <==> (g1 union g2) difference (g1 intersection g2).
  Since we've implemented the other 3 types of set operations for geometry
  collections, we can do so.

  @tparam Coord_type The numeric type for a coordinate value, most often
          it's double.
  @tparam Coordsys Coordinate system type, specified using those defined in
          boost::geometry::cs.
  @param g1 First geometry operand, a geometry collection.
  @param g2 Second geometry operand, a geometry collection.
  @param[out] result Holds WKB data of the result, which must be a
                geometry collection.
  @param[out] pdone Returns whether the set operation is performed for the
                two geometry collections.
  @return The symdifference result, whose WKB data is stored in 'result'.
 */
template<typename Coord_type, typename Coordsys>
Geometry *Item_func_spatial_operation::
geocol_symdifference(Geometry *g1, Geometry *g2, String *result, bool *pdone)
{
  Geometry *gres= NULL;
  String wkbres;
  bool isdone1= false, isdone2= false, isdone3= false;
  String union_res, dif_res, isct_res;
  Geometry *gc_union= NULL, *gc_isct= NULL;

  *pdone= false;
  Var_resetter<Gcalc_function::op_type>
    var_reset(&spatial_op, Gcalc_function::op_symdifference);

  spatial_op= Gcalc_function::op_union;
  gc_union= geometry_collection_set_operation<Coord_type,
    Coordsys>(g1, g2, &union_res, &isdone1);
  auto_ptr<Geometry> guard_union(gc_union);

  if (!isdone1 || null_value)
    return NULL;
  DBUG_ASSERT(gc_union != NULL);

  spatial_op= Gcalc_function::op_intersection;
  gc_isct= geometry_collection_set_operation<Coord_type, Coordsys>
    (g1, g2, &isct_res, &isdone2);
  auto_ptr<Geometry> guard_isct(gc_isct);

  if (!isdone2 || null_value)
    return NULL;

  auto_ptr<Geometry> guard_dif;
  if (gc_isct != NULL)
  {
    spatial_op= Gcalc_function::op_difference;
    gres= geometry_collection_set_operation<Coord_type, Coordsys>
      (gc_union, gc_isct, result, &isdone3);
    guard_dif.reset(gres);

    if (!isdone3 || null_value)
      return NULL;
  }
  else
  {
    gres= gc_union;
    result->takeover(union_res);
    guard_union.release();
  }

  *pdone= true;
  guard_dif.release();
  return gres;
}


bool Item_func_spatial_operation::assign_result(Geometry *geo, String *result)
{
  DBUG_ASSERT(geo->has_geom_header_space());
  char *p= geo->get_cptr() - GEOM_HEADER_SIZE;
  write_geometry_header(p, geo->get_srid(), geo->get_geotype());
  result->set(p, GEOM_HEADER_SIZE + geo->get_nbytes(), &my_charset_bin);
  bg_resbuf_mgr.add_buffer(p);
  geo->set_ownmem(false);

  return false;
}


const char *Item_func_spatial_operation::func_name() const
{
  switch (spatial_op) {
    case Gcalc_function::op_intersection:
      return "st_intersection";
    case Gcalc_function::op_difference:
      return "st_difference";
    case Gcalc_function::op_union:
      return "st_union";
    case Gcalc_function::op_symdifference:
      return "st_symdifference";
    default:
      DBUG_ASSERT(0);  // Should never happen
      return "sp_unknown";
  }
}

static const int SINUSES_CALCULATED= 32;
static double n_sinus[SINUSES_CALCULATED+1]=
{
  0,
  0.04906767432741802,
  0.0980171403295606,
  0.1467304744553618,
  0.1950903220161283,
  0.2429801799032639,
  0.2902846772544623,
  0.3368898533922201,
  0.3826834323650898,
  0.4275550934302821,
  0.4713967368259976,
  0.5141027441932217,
  0.5555702330196022,
  0.5956993044924334,
  0.6343932841636455,
  0.6715589548470183,
  0.7071067811865475,
  0.7409511253549591,
  0.773010453362737,
  0.8032075314806448,
  0.8314696123025452,
  0.8577286100002721,
  0.8819212643483549,
  0.9039892931234433,
  0.9238795325112867,
  0.9415440651830208,
  0.9569403357322089,
  0.970031253194544,
  0.9807852804032304,
  0.989176509964781,
  0.9951847266721968,
  0.9987954562051724,
  1
};


static void get_n_sincos(int n, double *sinus, double *cosinus)
{
  DBUG_ASSERT(n > 0 && n < SINUSES_CALCULATED*2+1);
  if (n < (SINUSES_CALCULATED + 1))
  {
    *sinus= n_sinus[n];
    *cosinus= n_sinus[SINUSES_CALCULATED - n];
  }
  else
  {
    n-= SINUSES_CALCULATED;
    *sinus= n_sinus[SINUSES_CALCULATED - n];
    *cosinus= -n_sinus[n];
  }
}


static int fill_half_circle(Gcalc_shape_transporter *trn,
                            Gcalc_shape_status *st,
                            double x, double y,
                            double ax, double ay)
{
  double n_sin, n_cos;
  double x_n, y_n;
  for (int n= 1; n < (SINUSES_CALCULATED * 2 - 1); n++)
  {
    get_n_sincos(n, &n_sin, &n_cos);
    x_n= ax * n_cos - ay * n_sin;
    y_n= ax * n_sin + ay * n_cos;
    if (trn->add_point(st, x_n + x, y_n + y))
      return 1;
  }
  return 0;
}


static int fill_gap(Gcalc_shape_transporter *trn,
                    Gcalc_shape_status *st,
                    double x, double y,
                    double ax, double ay, double bx, double by, double d,
                    bool *empty_gap)
{
  double ab= ax * bx + ay * by;
  double cosab= ab / (d * d) + GIS_ZERO;
  double n_sin, n_cos;
  double x_n, y_n;
  int n=1;

  *empty_gap= true;
  for (;;)
  {
    get_n_sincos(n++, &n_sin, &n_cos);
    if (n_cos <= cosab)
      break;
    *empty_gap= false;
    x_n= ax * n_cos - ay * n_sin;
    y_n= ax * n_sin + ay * n_cos;
    if (trn->add_point(st, x_n + x, y_n + y))
      return 1;
  }
  return 0;
}


/*
  Calculates the vector (p2,p1) and
  negatively orthogonal to it with the length of d.
  The result is (ex,ey) - the vector, (px,py) - the orthogonal.
*/

static void calculate_perpendicular(
    double x1, double y1, double x2, double y2, double d,
    double *ex, double *ey,
    double *px, double *py)
{
  double q;
  *ex= x1 - x2;
  *ey= y1 - y2;
  q= d / sqrt((*ex) * (*ex) + (*ey) * (*ey));
  *px= (*ey) * q;
  *py= -(*ex) * q;
}


int Item_func_buffer::Transporter::single_point(Gcalc_shape_status *st,
                                                double x, double y)
{
  return add_point_buffer(st, x, y);
}


int Item_func_buffer::Transporter::add_edge_buffer(Gcalc_shape_status *st,
  double x3, double y3, bool round_p1, bool round_p2)
{
  DBUG_PRINT("info", ("Item_func_buffer::Transporter::add_edge_buffer: "
             "(%g,%g)(%g,%g)(%g,%g) p1=%d p2=%d",
             x1, y1, x2, y2, x3, y3, (int) round_p1, (int) round_p2));

  Gcalc_operation_transporter trn(m_fn, m_heap);
  double e1_x, e1_y, e2_x, e2_y, p1_x, p1_y, p2_x, p2_y;
  double e1e2;
  double sin1, cos1;
  double x_n, y_n;
  bool empty_gap1, empty_gap2;

  st->m_nshapes++;
  Gcalc_shape_status dummy;
  if (trn.start_simple_poly(&dummy))
    return 1;

  calculate_perpendicular(x1, y1, x2, y2, m_d, &e1_x, &e1_y, &p1_x, &p1_y);
  calculate_perpendicular(x3, y3, x2, y2, m_d, &e2_x, &e2_y, &p2_x, &p2_y);

  e1e2= e1_x * e2_y - e2_x * e1_y;
  sin1= n_sinus[1];
  cos1= n_sinus[31];
  if (e1e2 < 0)
  {
    empty_gap2= false;
    x_n= x2 + p2_x * cos1 - p2_y * sin1;
    y_n= y2 + p2_y * cos1 + p2_x * sin1;
    if (fill_gap(&trn, &dummy, x2, y2, -p1_x,-p1_y,
                 p2_x,p2_y, m_d, &empty_gap1) ||
        trn.add_point(&dummy, x2 + p2_x, y2 + p2_y) ||
        trn.add_point(&dummy, x_n, y_n))
      return 1;
  }
  else
  {
    x_n= x2 - p2_x * cos1 - p2_y * sin1;
    y_n= y2 - p2_y * cos1 + p2_x * sin1;
    if (trn.add_point(&dummy, x_n, y_n) ||
        trn.add_point(&dummy, x2 - p2_x, y2 - p2_y) ||
        fill_gap(&trn, &dummy, x2, y2, -p2_x, -p2_y,
                 p1_x, p1_y, m_d, &empty_gap2))
      return 1;
    empty_gap1= false;
  }
  if ((!empty_gap2 && trn.add_point(&dummy, x2 + p1_x, y2 + p1_y)) ||
      trn.add_point(&dummy, x1 + p1_x, y1 + p1_y))
    return 1;

  if (round_p1 && fill_half_circle(&trn, &dummy, x1, y1, p1_x, p1_y))
    return 1;

  if (trn.add_point(&dummy, x1 - p1_x, y1 - p1_y) ||
      (!empty_gap1 && trn.add_point(&dummy, x2 - p1_x, y2 - p1_y)))
    return 1;
  return trn.complete_simple_poly(&dummy);
}


int Item_func_buffer::Transporter::add_last_edge_buffer(Gcalc_shape_status *st)
{
  Gcalc_operation_transporter trn(m_fn, m_heap);
  Gcalc_shape_status dummy;
  double e1_x, e1_y, p1_x, p1_y;

  st->m_nshapes++;
  if (trn.start_simple_poly(&dummy))
    return 1;

  calculate_perpendicular(x1, y1, x2, y2, m_d, &e1_x, &e1_y, &p1_x, &p1_y);

  if (trn.add_point(&dummy, x1 + p1_x, y1 + p1_y) ||
      trn.add_point(&dummy, x1 - p1_x, y1 - p1_y) ||
      trn.add_point(&dummy, x2 - p1_x, y2 - p1_y) ||
      fill_half_circle(&trn, &dummy, x2, y2, -p1_x, -p1_y) ||
      trn.add_point(&dummy, x2 + p1_x, y2 + p1_y))
    return 1;
  return trn.complete_simple_poly(&dummy);
}


int Item_func_buffer::Transporter::add_point_buffer(Gcalc_shape_status *st,
                                                    double x, double y)
{
  Gcalc_operation_transporter trn(m_fn, m_heap);
  Gcalc_shape_status dummy;

  st->m_nshapes++;
  if (trn.start_simple_poly(&dummy))
    return 1;
  if (trn.add_point(&dummy, x - m_d, y) ||
      fill_half_circle(&trn, &dummy, x, y, -m_d, 0.0) ||
      trn.add_point(&dummy, x + m_d, y) ||
      fill_half_circle(&trn, &dummy, x, y, m_d, 0.0))
    return 1;
  return trn.complete_simple_poly(&dummy);
}


int Item_func_buffer::Transporter::start_line(Gcalc_shape_status *st)
{
  st->m_nshapes= 0;
  if (m_fn->reserve_op_buffer(2))
    return 1;
  st->m_last_shape_pos= m_fn->get_next_operation_pos();
  m_fn->add_operation(m_buffer_op, 0); // Will be set in complete_line()
  m_npoints= 0;
  int_start_line();
  return 0;
}


int Item_func_buffer::Transporter::start_poly(Gcalc_shape_status *st)
{
  st->m_nshapes= 1;
  if (m_fn->reserve_op_buffer(2))
    return 1;
  st->m_last_shape_pos= m_fn->get_next_operation_pos();
  m_fn->add_operation(m_buffer_op, 0); // Will be set in complete_poly()
  return Gcalc_operation_transporter::start_poly(st);
}


int Item_func_buffer::Transporter::complete_poly(Gcalc_shape_status *st)
{
  if (Gcalc_operation_transporter::complete_poly(st))
    return 1;
  m_fn->add_operands_to_op(st->m_last_shape_pos, st->m_nshapes);
  return 0;
}


int Item_func_buffer::Transporter::start_ring(Gcalc_shape_status *st)
{
  m_npoints= 0;
  return Gcalc_operation_transporter::start_ring(st);
}


int Item_func_buffer::Transporter::add_point(Gcalc_shape_status *st,
                                             double x, double y)
{
  if (m_npoints && x == x2 && y == y2)
    return 0;

  ++m_npoints;

  if (m_npoints == 1)
  {
    x00= x;
    y00= y;
  }
  else if (m_npoints == 2)
  {
    x01= x;
    y01= y;
  }
  else if (add_edge_buffer(st, x, y, (m_npoints == 3) && line_started(), false))
    return 1;

  x1= x2;
  y1= y2;
  x2= x;
  y2= y;

  return line_started() ? 0 : Gcalc_operation_transporter::add_point(st, x, y);
}


int Item_func_buffer::Transporter::complete(Gcalc_shape_status *st)
{
  if (m_npoints)
  {
    if (m_npoints == 1)
    {
      if (add_point_buffer(st, x2, y2))
        return 1;
    }
    else if (m_npoints == 2)
    {
      if (add_edge_buffer(st, x1, y1, true, true))
        return 1;
    }
    else if (line_started())
    {
      if (add_last_edge_buffer(st))
        return 1;
    }
    else
    {
      /*
        Add edge only the the most recent coordinate is not
        the same to the very first one.
      */
      if (x2 != x00 || y2 != y00)
      {
        if (add_edge_buffer(st, x00, y00, false, false))
          return 1;
        x1= x2;
        y1= y2;
        x2= x00;
        y2= y00;
      }
      if (add_edge_buffer(st, x01, y01, false, false))
        return 1;
    }
  }

  return 0;
}


int Item_func_buffer::Transporter::complete_line(Gcalc_shape_status *st)
{
  if (complete(st))
    return 1;
  int_complete_line();
  // Set real number of operands (points) to the operation.
  m_fn->add_operands_to_op(st->m_last_shape_pos, st->m_nshapes);
  return 0;
}


int Item_func_buffer::Transporter::complete_ring(Gcalc_shape_status *st)
{
  return complete(st) ||
         Gcalc_operation_transporter::complete_ring(st);
}


int Item_func_buffer::Transporter::start_collection(Gcalc_shape_status *st,
                                                    int n_objects)
{
  st->m_nshapes= 0;
  st->m_last_shape_pos= m_fn->get_next_operation_pos();
  return Gcalc_operation_transporter::start_collection(st, n_objects);
}


int Item_func_buffer::Transporter::complete_collection(Gcalc_shape_status *st)
{
  Gcalc_operation_transporter::complete_collection(st);
  m_fn->set_operands_to_op(st->m_last_shape_pos, st->m_nshapes);
  return 0;
}


int Item_func_buffer::Transporter::collection_add_item(Gcalc_shape_status
                                                       *st_collection,
                                                       Gcalc_shape_status
                                                       *st_item)
{
  /*
    If some collection item created no shapes,
    it means it was skipped during transformation by filters
    skip_point(), skip_line(), skip_poly().
    In this case nothing was added into function_buffer by the item,
    so we don't increment shape counter of the owning collection.
  */
  if (st_item->m_nshapes)
    st_collection->m_nshapes++;
  return 0;
}

// Boost.Geometry doesn't support buffer for any type of geometry
// defined by OGC for now.
String *Item_func_buffer::val_str(String *str_value_arg)
{
  DBUG_ENTER("Item_func_buffer::val_str");
  DBUG_ASSERT(fixed == 1);
  String *obj= args[0]->val_str(&tmp_value);
  double dist= args[1]->val_real();
  Geometry_buffer buffer;
  Geometry *g;
  uint32 srid= 0;
  String *str_result= NULL;
  Transporter trn(&func, &collector, dist);
  Gcalc_shape_status st;

  null_value= 1;
  if (!obj || args[0]->null_value || args[1]->null_value)
    goto mem_error;
  if (!(g= Geometry::construct(&buffer, obj)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    DBUG_RETURN(error_str());
  }

  srid= g->get_srid();

  /*
    If distance passed to ST_Buffer is too small, then we return the
    original geometry as its buffer. This is needed to avoid division
    overflow in buffer calculation, as well as for performance purposes.
  */
  if (fabs(dist) < GIS_ZERO)
  {
    null_value= 0;
    str_result= obj;
    goto mem_error;
  }

  if (g->store_shapes(&trn, &st))
    goto mem_error;

#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  if (st.m_nshapes == 0)
  {
    /*
      Buffer transformation returned empty set.
      This is possible with negative buffer distance
      if the original geometry consisted of only Points and lines
      and did not have any Polygons.
    */
    str_value_arg->length(0);
    goto mem_error;
  }

  collector.prepare_operation();
  if (func.alloc_states())
    goto mem_error;
  operation.init(&func);

  if (operation.count_all(&collector) ||
      operation.get_result(&res_receiver))
    goto mem_error;

  str_value_arg->set_charset(&my_charset_bin);
  if (str_value_arg->reserve(SRID_SIZE, 512))
    goto mem_error;
  str_value_arg->length(0);
  str_value_arg->q_append(srid);

  if (!Geometry::create_from_opresult(&buffer, str_value_arg, res_receiver))
    goto mem_error;

  null_value= 0;
  str_result= str_value_arg;
mem_error:
  collector.reset();
  func.reset();
  res_receiver.reset();
  DBUG_RETURN(str_result);
}


longlong Item_func_isempty::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String tmp;
  String *swkb= args[0]->val_str(&tmp);
  Geometry_buffer buffer;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0;
  if (!(Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }

  return null_value ? 1 : 0;
}


longlong Item_func_issimple::val_int()
{
  String *swkb= args[0]->val_str(&tmp);
  Geometry_buffer buffer;
  Gcalc_operation_transporter trn(&func, &collector);
  Geometry *g;
  int result= 1;

  DBUG_ENTER("Item_func_issimple::val_int");
  DBUG_ASSERT(fixed == 1);

  if ((null_value= (!swkb || args[0]->null_value)))
    DBUG_RETURN(0);
  if (!(g= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    DBUG_RETURN(error_int());
  }

  if (g->get_class_info()->m_type_id == Geometry::wkb_point)
    DBUG_RETURN(1);

  if (g->store_shapes(&trn))
    goto mem_error;

#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  collector.prepare_operation();
  scan_it.init(&collector);

  while (scan_it.more_points())
  {
    if (scan_it.step())
      goto mem_error;

    if (scan_it.get_event() == scev_intersection)
    {
      result= 0;
      break;
    }
  }

  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(result);
mem_error:
  null_value= 1;
  DBUG_RETURN(0);
  return 0;
}


longlong Item_func_isclosed::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String tmp;
  String *swkb= args[0]->val_str(&tmp);
  Geometry_buffer buffer;
  Geometry *geom;
  int isclosed= 0;				// In case of error

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0L;

  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }

  null_value= geom->is_closed(&isclosed);

  return (longlong) isclosed;
}

/*
  Numerical functions
*/


longlong Item_func_dimension::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 dim= 0;				// In case of error
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }
  null_value= geom->dimension(&dim);
  return (longlong) dim;
}


longlong Item_func_numinteriorring::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 num= 0;				// In case of error
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0L;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }
  null_value= geom->num_interior_ring(&num);
  return (longlong) num;
}


longlong Item_func_numgeometries::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 num= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0L;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }
  null_value= geom->num_geometries(&num);
  return (longlong) num;
}


longlong Item_func_numpoints::val_int()
{
  DBUG_ASSERT(fixed == 1);
  uint32 num= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  if ((null_value= (!swkb || args[0]->null_value)))
    return 0L;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }
  null_value= geom->num_points(&num);
  return (longlong) num;
}


double Item_func_x::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0.0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  if ((null_value= (!swkb || args[0]->null_value)))
    return res;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_real();
  }
  null_value= geom->get_x(&res);
  return res;
}


double Item_func_y::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  if ((null_value= (!swkb || args[0]->null_value)))
    return res;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_real();
  }
  null_value= geom->get_y(&res);
  return res;
}


template <typename Coordsys>
double Item_func_area::bg_area(const Geometry *geom, bool *isdone)
{
  double res= 0;

  *isdone= false;
  try
  {
    switch (geom->get_type())
    {
    case Geometry::wkb_point:
    case Geometry::wkb_multipoint:
    case Geometry::wkb_linestring:
    case Geometry::wkb_multilinestring:
      *isdone= true;
      res= 0;
      break;
    case Geometry::wkb_polygon:
      {
        typename BG_models<double, Coordsys>::Polygon
          plgn(geom->get_data_ptr(), geom->get_data_size(),
               geom->get_flags(), geom->get_srid());

        res= boost::geometry::area(plgn);
        *isdone= true;
      }
      break;
    case Geometry::wkb_multipolygon:
      {
        typename BG_models<double, Coordsys>::Multipolygon
          mplgn(geom->get_data_ptr(), geom->get_data_size(),
                geom->get_flags(), geom->get_srid());

        res= boost::geometry::area(mplgn);
        *isdone= true;
      }
      break;
    case Geometry::wkb_geometrycollection:
      {
        BG_geometry_collection bggc;
        bool isdone2= false;

        bggc.fill(geom);

        for (BG_geometry_collection::Geometry_list::iterator
             i= bggc.get_geometries().begin();
             i != bggc.get_geometries().end(); ++i)
        {
          isdone2= false;
          if ((*i)->get_geotype() != Geometry::wkb_geometrycollection &&
              (*i)->normalize_ring_order() == NULL)
          {
            my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
            null_value= true;
            return 0;
          }

          res+= bg_area<Coordsys>(*i, &isdone2);
          if (!isdone2 || null_value)
            return res;
        }

        *isdone= true;
      }
      break;
    default:
      break;
    }
  }
  CATCH_ALL("st_area", null_value= true)

  /*
    Given a polygon whose rings' points are in counter-clockwise order,
    boost geometry computes an area of negative value. Also, the inner ring
    has to be clockwise.

    We now always make polygon rings CCW --- outer ring CCW and inner rings CW,
    thus if we get a negative value, it's because the inner ring is larger than
    the outer ring, and we should keep it negative.
   */

  return res;
}


double Item_func_area::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;
  bool isdone= false;

  if ((null_value= (!swkb || args[0]->null_value)))
    return res;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_real();
  }
  DBUG_ASSERT(geom->get_coordsys() == Geometry::cartesian);

  if (geom->get_geotype() != Geometry::wkb_geometrycollection &&
      geom->normalize_ring_order() == NULL)
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_real();
  }

  res= bg_area<bgcs::cartesian>(geom, &isdone);

  // Had error in bg_area.
  if (null_value)
    return error_real();

  if (!isdone)
    null_value= geom->area(&res);

  if (!my_isfinite(res))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_real();
  }
  return res;
}

double Item_func_glength::val_real()
{
  DBUG_ASSERT(fixed == 1);
  double res= 0;				// In case of errors
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  Geometry *geom;

  if ((null_value= (!swkb || args[0]->null_value)))
    return res;
  if (!(geom= Geometry::construct(&buffer, swkb)))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_real();
  }
  if ((null_value= geom->geom_length(&res)))
    return res;
  if (!my_isfinite(res))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_real();
  }
  return res;
}

longlong Item_func_srid::val_int()
{
  DBUG_ASSERT(fixed == 1);
  String *swkb= args[0]->val_str(&value);
  Geometry_buffer buffer;
  longlong res= 0L;

  if ((null_value= (!swkb || args[0]->null_value)))
    return res;
  if (!Geometry::construct(&buffer, swkb))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    return error_int();
  }

  return (longlong) (uint4korr(swkb->ptr()));
}


/**
  Compact a geometry collection, making all its points/multipoints into a
  single multipoint object, and all its linestrings/multilinestrings into a
  single multilinestring object; leave polygons and multipolygons as they were.

  @param g the input geometry collection.
  @param gbuf the place to create the new result geometry collection.
  @param str the String buffer to hold data of the result geometry collection.
 */
static const Geometry *
compact_collection(const Geometry *g, Geometry_buffer *gbuf, String *str)
{
  if (g->get_geotype() != Geometry::wkb_geometrycollection)
    return g;

  uint32 wkb_len, wkb_len0;
  char *wkb_start= g->get_cptr();

  wkb_len= wkb_len0= g->get_data_size();
  BG_models<double, bgcs::cartesian>::Multilinestring mls;
  Geometry_grouper<BG_models<double, bgcs::cartesian>::Linestring>
    ls_grouper(&mls);
  wkb_scanner(wkb_start, &wkb_len,
              Geometry::wkb_geometrycollection, false, &ls_grouper);

  BG_models<double, bgcs::cartesian>::Multipoint mpts;
  wkb_len= wkb_len0;
  Geometry_grouper<BG_models<double, bgcs::cartesian>::Point>
    pt_grouper(&mpts);
  wkb_scanner(wkb_start, &wkb_len,
              Geometry::wkb_geometrycollection, false, &pt_grouper);

  Gis_geometry_collection *ret= new (gbuf) Gis_geometry_collection();
  wkb_len= wkb_len0;
  Geometry_grouper<BG_models<double, bgcs::cartesian>::Polygon>
    mplgn_grouper(ret, str);
  wkb_scanner(wkb_start, &wkb_len,
              Geometry::wkb_geometrycollection, false, &mplgn_grouper);

  ret->append_geometry(&mls, str);
  ret->append_geometry(&mpts, str);

  return ret;
}


double Item_func_distance::val_real()
{
  bool cur_point_edge, isdone= false;
  const Gcalc_scan_iterator::point *evpos;
  const Gcalc_heap::Info *cur_point, *dist_point;
  Gcalc_scan_events ev;
  double t, distance= 0, cur_distance= 0;
  double ex, ey, vx, vy, e_sqrlen;
  uint obj2_si;
  Gcalc_operation_transporter trn(&func, &collector);

  DBUG_ENTER("Item_func_distance::val_real");
  DBUG_ASSERT(fixed == 1);
  String *res1= args[0]->val_str(&tmp_value1);
  String *res2= args[1]->val_str(&tmp_value2);
  Geometry_buffer buffer1, buffer2;
  Geometry *g1, *g2;

  if ((null_value= (!res1 || args[0]->null_value ||
                    !res2 || args[1]->null_value)))
    DBUG_RETURN(0.0);

  if (!(g1= Geometry::construct(&buffer1, res1)) ||
      !(g2= Geometry::construct(&buffer2, res2)))
  {
    // If construction fails, we assume invalid input data.
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    DBUG_RETURN(error_real());
  }

  // The two geometry operand must be in the same coordinate system.
  if (g1->get_srid() != g2->get_srid())
  {
    my_error(ER_GIS_DIFFERENT_SRIDS, MYF(0), func_name(),
             g1->get_srid(), g2->get_srid());
    DBUG_RETURN(error_real());
  }

  if ((g1->get_geotype() != Geometry::wkb_geometrycollection &&
       g1->normalize_ring_order() == NULL) ||
      (g2->get_geotype() != Geometry::wkb_geometrycollection &&
       g2->normalize_ring_order() == NULL))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    DBUG_RETURN(error_real());
  }

  if (g1->get_type() != Geometry::wkb_geometrycollection &&
      g2->get_type() != Geometry::wkb_geometrycollection)
    distance= bg_distance<bgcs::cartesian>(g1, g2, &isdone);
  else
  {
    /*
      Calculate the distance of two geometry collections. BG has optimized
      algorithm to calculate distance among multipoints, multilinestrings
      and polygons, so we compact the collection to make a single multipoint,
      a single multilinestring, and the rest are all polygons and multipolygons,
      and do a nested loop to calculate the minimum distances among such
      compacted components as the final result.
     */
    BG_geometry_collection bggc1, bggc2;
    bool initialized= false, isdone2= false, all_normalized= false;
    double min_distance= DBL_MAX, dist;
    String gcstr1, gcstr2;
    Geometry_buffer buf1, buf2;
    const Geometry *g11, *g22;

    g11= compact_collection(g1, &buf1, &gcstr1);
    g22= compact_collection(g2, &buf2, &gcstr2);

    bggc1.fill(g11);
    bggc2.fill(g22);
    for (BG_geometry_collection::Geometry_list::iterator
         i= bggc1.get_geometries().begin();
         i != bggc1.get_geometries().end(); ++i)
    {
      /* Normalize polygon rings, do only once for each component. */
      if ((*i)->get_geotype() != Geometry::wkb_geometrycollection &&
          (*i)->normalize_ring_order() == NULL)
      {
        my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
        DBUG_RETURN(error_real());
      }

      for (BG_geometry_collection::Geometry_list::iterator
           j= bggc2.get_geometries().begin();
           j != bggc2.get_geometries().end(); ++j)
      {
        /* Normalize polygon rings, do only once for each component. */
        if (!all_normalized &&
            (*j)->get_geotype() != Geometry::wkb_geometrycollection &&
            (*j)->normalize_ring_order() == NULL)
        {
          my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
          DBUG_RETURN(error_real());
        }

        dist= bg_distance<bgcs::cartesian>(*i, *j, &isdone2);
        if (!isdone2 && !null_value)
          goto old_algo;
        if (null_value)
          goto error;
        if (!initialized)
        {
          DBUG_ASSERT(dist <= DBL_MAX);
          min_distance= dist;
          initialized= true;
        }
        else if (min_distance > dist)
          min_distance= dist;

        isdone= true;
      }

      all_normalized= true;
      if (!initialized)
        break;                                  // bggc2 is empty.
    }

    /*
      If at least one of the collections is empty, we have NULL result. 
    */
    if (!initialized)
    {
      null_value= true;
      isdone= true;
    }
    else
    {
      if (min_distance >= DBL_MAX)
        min_distance= 0;
      distance= min_distance;
    }
  }

error:
  if (null_value)
    DBUG_RETURN(error_real());

  if (isdone && !null_value)
    goto exit;

old_algo:

  if ((g1->get_class_info()->m_type_id == Geometry::wkb_point) &&
      (g2->get_class_info()->m_type_id == Geometry::wkb_point))
  {
    point_xy p1, p2;
    if (((Gis_point *) g1)->get_xy(&p1) ||
        ((Gis_point *) g2)->get_xy(&p2))
      goto mem_error;
    ex= p2.x - p1.x;
    ey= p2.y - p1.y;
    DBUG_RETURN(sqrt(ex * ex + ey * ey));
  }

  if (func.reserve_op_buffer(1))
    goto mem_error;
  func.add_operation(Gcalc_function::op_intersection, 2);

  if (g1->store_shapes(&trn))
    goto mem_error;
  obj2_si= func.get_nshapes();
  if (g2->store_shapes(&trn) || func.alloc_states())
    goto mem_error;

#ifndef DBUG_OFF
  func.debug_print_function_buffer();
#endif

  collector.prepare_operation();
  scan_it.init(&collector);

  distance= DBL_MAX;
  while (scan_it.more_points())
  {
    if (scan_it.step())
      goto mem_error;
    evpos= scan_it.get_event_position();
    ev= scan_it.get_event();
    cur_point= evpos->pi;

    /*
       handling intersection we only need to check if it's the intersecion
       of objects 1 and 2. In this case distance is 0
    */
    if (ev == scev_intersection)
    {
      if ((evpos->get_next()->pi->shape >= obj2_si) !=
            (cur_point->shape >= obj2_si))
      {
        distance= 0;
        goto exit;
      }
      continue;
    }

    /*
       if we get 'scev_point | scev_end | scev_two_ends' we don't need
       to check for intersection of objects.
       Though we need to calculate distances.
    */
    if (ev & (scev_point | scev_end | scev_two_ends))
      goto count_distance;

    /*
       having these events we need to check for possible intersection
       of objects
       scev_thread | scev_two_threads | scev_single_point
    */
    DBUG_ASSERT(ev & (scev_thread | scev_two_threads | scev_single_point));

    func.clear_state();
    for (Gcalc_point_iterator pit(&scan_it); pit.point() != evpos; ++pit)
    {
      gcalc_shape_info si= pit.point()->get_shape();
      if ((func.get_shape_kind(si) == Gcalc_function::shape_polygon))
        func.invert_state(si);
    }
    func.invert_state(evpos->get_shape());
    if (func.count())
    {
      /* Point of one object is inside the other - intersection found */
      distance= 0;
      goto exit;
    }


count_distance:
    if (cur_point->shape >= obj2_si)
      continue;
    cur_point_edge= !cur_point->is_bottom();

    for (dist_point= collector.get_first(); dist_point;
         dist_point= dist_point->get_next())
    {
      /* We only check vertices of object 2 */
      if (dist_point->shape < obj2_si)
        continue;

      /* if we have an edge to check */
      if (dist_point->left)
      {
        t= count_edge_t(dist_point, dist_point->left, cur_point,
                        ex, ey, vx, vy, e_sqrlen);
        if ((t>0.0) && (t<1.0))
        {
          cur_distance= distance_to_line(ex, ey, vx, vy, e_sqrlen);
          if (distance > cur_distance)
            distance= cur_distance;
        }
      }
      if (cur_point_edge)
      {
        t= count_edge_t(cur_point, cur_point->left, dist_point,
                        ex, ey, vx, vy, e_sqrlen);
        if ((t>0.0) && (t<1.0))
        {
          cur_distance= distance_to_line(ex, ey, vx, vy, e_sqrlen);
          if (distance > cur_distance)
            distance= cur_distance;
        }
      }
      cur_distance= distance_points(cur_point, dist_point);
      if (distance > cur_distance)
        distance= cur_distance;
    }
  }
exit:

  if (!my_isfinite(distance))
  {
    my_error(ER_GIS_INVALID_DATA, MYF(0), func_name());
    DBUG_RETURN(error_real());
  }
  collector.reset();
  func.reset();
  scan_it.reset();
  DBUG_RETURN(distance);
mem_error:
  DBUG_RETURN(0.0);
}


template <typename Coordsys>
double Item_func_distance::
distance_point_geometry(const Geometry *g1, const Geometry *g2, bool *isdone)
{
  double res= 0;
  *isdone= false;

  typename BG_models<double, Coordsys>::Point
    bg1(g1->get_data_ptr(), g1->get_data_size(),
        g1->get_flags(), g1->get_srid());

  switch (g2->get_type())
  {
  case Geometry::wkb_point:
    {
      typename BG_models<double, Coordsys>::Point
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_multipoint:
    {
      typename BG_models<double, Coordsys>::Multipoint
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_linestring:
    {
      typename BG_models<double, Coordsys>::Linestring
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_multilinestring:
    {
      typename BG_models<double, Coordsys>::Multilinestring
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_polygon:
    {
      typename BG_models<double, Coordsys>::Polygon
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_multipolygon:
    {
      typename BG_models<double, Coordsys>::Multipolygon
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  *isdone= true;
  return res;
}


template <typename Coordsys>
double Item_func_distance::
distance_multipoint_geometry(const Geometry *g1, const Geometry *g2,
                             bool *isdone)
{
  double res= 0;
  *isdone= false;

  typename BG_models<double, Coordsys>::Multipoint
    bg1(g1->get_data_ptr(), g1->get_data_size(),
        g1->get_flags(), g1->get_srid());

  switch (g2->get_type())
  {
  case Geometry::wkb_point:
    res= bg_distance<Coordsys>(g2, g1, isdone);
    break;
  case Geometry::wkb_multipoint:
    {
      typename BG_models<double, Coordsys>::Multipoint
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_linestring:
    {
      typename BG_models<double, Coordsys>::Linestring
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_multilinestring:
    {
      typename BG_models<double, Coordsys>::Multilinestring
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_polygon:
    {
      typename BG_models<double, Coordsys>::Polygon
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  case Geometry::wkb_multipolygon:
    {
      typename BG_models<double, Coordsys>::Multipolygon
        bg2(g2->get_data_ptr(), g2->get_data_size(),
            g2->get_flags(), g2->get_srid());
      res= boost::geometry::distance(bg1, bg2);
    }
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }
  *isdone= true;

  return res;
}


template <typename Coordsys>
double Item_func_distance::
distance_linestring_geometry(const Geometry *g1, const Geometry *g2,
                             bool *isdone)
{
  double res= 0;
  *isdone= false;

  switch (g2->get_type())
  {
  case Geometry::wkb_point:
  case Geometry::wkb_multipoint:
    res= bg_distance<Coordsys>(g2, g1, isdone);
    break;
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    // Not supported yet by BG, call BG function when supported.
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return res;
}


template <typename Coordsys>
double Item_func_distance::
distance_multilinestring_geometry(const Geometry *g1, const Geometry *g2,
                                  bool *isdone)
{
  double res= 0;
  *isdone= false;

  switch (g2->get_type())
  {
  case Geometry::wkb_point:
  case Geometry::wkb_multipoint:
  case Geometry::wkb_linestring:
    res= bg_distance<Coordsys>(g2, g1, isdone);
    break;
  case Geometry::wkb_multilinestring:
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    // Not supported yet by BG, call BG function when supported.
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return res;
}


template <typename Coordsys>
double Item_func_distance::
distance_polygon_geometry(const Geometry *g1, const Geometry *g2, bool *isdone)
{
  double res= 0;
  *isdone= false;

  switch (g2->get_type())
  {
  case Geometry::wkb_point:
  case Geometry::wkb_multipoint:
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
    res= bg_distance<Coordsys>(g2, g1, isdone);
    break;
  case Geometry::wkb_polygon:
  case Geometry::wkb_multipolygon:
    // Not supported yet by BG, call BG function when supported.
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return res;
}


template <typename Coordsys>
double Item_func_distance::
distance_multipolygon_geometry(const Geometry *g1, const Geometry *g2,
                               bool *isdone)
{
  double res= 0;
  *isdone= false;

  switch (g2->get_type())
  {
  case Geometry::wkb_point:
  case Geometry::wkb_multipoint:
  case Geometry::wkb_linestring:
  case Geometry::wkb_multilinestring:
  case Geometry::wkb_polygon:
    res= bg_distance<Coordsys>(g2, g1, isdone);
    break;
  case Geometry::wkb_multipolygon:
    // Not supported yet by BG, call BG function when supported.
    break;
  default:
    DBUG_ASSERT(false);
    break;
  }

  return res;
}


/*
  Calculate distance of g1 and g2 using Boost.Geometry. We split the
  implementation into 6 smaller functions according to the type of g1, to
  make all functions smaller in size. Because distance is symmetric, we swap
  parameters if the swapped type combination is already implemented.
 */
template <typename Coordsys>
double Item_func_distance::bg_distance(const Geometry *g1,
                                       const Geometry *g2, bool *isdone)
{
  double res= 0;
  *isdone= false;

  try
  {

    switch (g1->get_type())
    {
    case Geometry::wkb_point:
      res= distance_point_geometry<Coordsys>(g1, g2, isdone);
      break;
    case Geometry::wkb_multipoint:
      res= distance_multipoint_geometry<Coordsys>(g1, g2, isdone);
      break;
    case Geometry::wkb_linestring:
      res= distance_linestring_geometry<Coordsys>(g1, g2, isdone);
      break;
    case Geometry::wkb_multilinestring:
      res= distance_multilinestring_geometry<Coordsys>(g1, g2, isdone);
      break;
    case Geometry::wkb_polygon:
      res= distance_polygon_geometry<Coordsys>(g1, g2, isdone);
      break;
    case Geometry::wkb_multipolygon:
      res= distance_polygon_geometry<Coordsys>(g1, g2, isdone);
      break;
    default:
      DBUG_ASSERT(false);
      break;
    }
  }
  CATCH_ALL("st_distance", null_value= true)

  return res;
}


// check whether all segments of a linestring are colinear.
template <typename Point_range>
static bool is_colinear(const Point_range &ls)
{
  if (ls.size() < 3)
    return true;

  double x1, x2, x3, y1, y2, y3, X1, X2, Y1, Y2;

  for (size_t i= 0; i < ls.size() - 2; i++)
  {
    x1= ls[i].template get<0>();
    x2= ls[i + 1].template get<0>();
    x3= ls[i + 2].template get<0>();

    y1= ls[i].template get<1>();
    y2= ls[i + 1].template get<1>();
    y3= ls[i + 2].template get<1>();

    X1= x2 - x1;
    X2= x3 - x2;
    Y1= y2 - y1;
    Y2= y3 - y2;

    if (X1 * Y2 - X2 * Y1 != 0)
      return false;
  }

  return true;
}

#ifndef DBUG_OFF
longlong Item_func_gis_debug::val_int()
{
  longlong val= args[0]->val_int();
  if (!args[0]->null_value)
    current_thd->set_gis_debug(static_cast<int>(val));
  return current_thd->get_gis_debug();
}
#endif


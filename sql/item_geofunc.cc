/* Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "item_geofunc.h"

#include "gstream.h"      // Gis_read_stream
#include "sql_class.h"    // THD
#include "gis_bg_traits.h"

#include "parse_tree_helpers.h"
#include <rapidjson/document.h>
#include "item_geofunc_internal.h"


// check whether all segments of a linestring are colinear.
template <typename Point_range>
bool is_colinear(const Point_range &ls)
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
      if (args[1]->unsigned_flag)
        ullstr(dimension_argument, option_string);
      else
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
      if (args[2]->unsigned_flag)
        ullstr(srid_argument, srid_string);
      else
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

  str->length(0);
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

  // By taking over, str owns swkt->ptr and the memory will be released when
  // str points to another buffer in next call of this function
  // (done in post_fix_result), or when str's owner Item_xxx node is destroyed.
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
        // The linestring consists of 4 or more points, but only the
        // first two contain real data, so we need to trim it down.
        line_hull.resize(2);
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
    covert integers to strings. Binary data is not allowed.

    PARAM_ITEM and INT_ITEM checks are to allow prepared statements and usage of
    user-defined variables respectively.
  */
  if (Item_func_geohash::is_item_null(args[1]))
    return false;

  if (args[1]->collation.collation == &my_charset_bin &&
      args[1]->type() != PARAM_ITEM && args[1]->type() != INT_ITEM)
  {
    my_error(ER_INCORRECT_TYPE, MYF(0), "SRID", func_name());
    return true;
  }

  switch (args[1]->field_type())
  {
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
  Convert this into a Gis_geometry_collection object.
  @param geodata Stores the result object's WKB data.
  @return The Gis_geometry_collection object created from this object.
 */
Gis_geometry_collection *
BG_geometry_collection::as_geometry_collection(String *geodata) const
{
  if (m_geos.size() == 0)
    return empty_collection(geodata, m_srid);

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
      {
        geo2->has_geom_header_space(true);
        m_geos.push_back(geo2);
      }
    }

    /*
      GCs with no-overlapping components can only be returned by
      combine_sub_results, which combines geometries from BG set operations,
      so no nested GCs or other user defined GCs are really set to true here.
    */
    set_comp_no_overlapped(geo->is_components_no_overlapped() || ngeom == 1);
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
  if (pres == NULL || pres->reserve(GEOM_HEADER_SIZE + geosize, 256))
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
        if (dist < 0 || boost::math::isnan(dist))
        {
          isdone= true;
          distance= dist;
          goto error;
        }
        if (!initialized)
        {
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
      distance= min_distance;
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

  if (!my_isfinite(distance) || distance < 0)
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


#ifndef DBUG_OFF
longlong Item_func_gis_debug::val_int()
{
  longlong val= args[0]->val_int();
  if (!args[0]->null_value)
    current_thd->set_gis_debug(static_cast<int>(val));
  return current_thd->get_gis_debug();
}
#endif



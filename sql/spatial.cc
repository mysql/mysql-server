#include "mysql_priv.h"


#define MAX_DIGITS_IN_DOUBLE 16

/***************************** GClassInfo *******************************/

#define IMPLEMENT_GEOM(class_name, type_id, name)	\
{							\
  (GF_InitFromText) &class_name::init_from_wkt,		\
  (GF_GetDataAsText) &class_name::get_data_as_wkt,	\
  (GF_GetDataSize) &class_name::get_data_size,		\
  (GF_GetMBR) &class_name::get_mbr,			\
  (GF_GetD) &class_name::get_x,				\
  (GF_GetD) &class_name::get_y,				\
  (GF_GetD) &class_name::length,			\
  (GF_GetD) &class_name::area,				\
  (GF_GetI) &class_name::is_closed,			\
  (GF_GetUI) &class_name::num_interior_ring,		\
  (GF_GetUI) &class_name::num_points,			\
  (GF_GetUI) &class_name::num_geometries,		\
  (GF_GetUI) &class_name::dimension,			\
  (GF_GetWS) &class_name::start_point,			\
  (GF_GetWS) &class_name::end_point,			\
  (GF_GetWS) &class_name::exterior_ring,		\
  (GF_GetWS) &class_name::centroid,			\
  (GF_GetUIWS) &class_name::point_n,			\
  (GF_GetUIWS) &class_name::interior_ring_n,		\
  (GF_GetUIWS) &class_name::geometry_n,			\
  class_name::type_id,					\
  name,							\
  NULL							\
},


static Geometry::GClassInfo ci_collection[] =
{
  IMPLEMENT_GEOM(GPoint, wkbPoint, "POINT")
  IMPLEMENT_GEOM(GLineString, wkbLineString, "LINESTRING")
  IMPLEMENT_GEOM(GPolygon, wkbPolygon, "POLYGON")
  IMPLEMENT_GEOM(GMultiPoint, wkbMultiPoint, "MULTIPOINT")
  IMPLEMENT_GEOM(GMultiLineString, wkbMultiLineString, "MULTILINESTRING")
  IMPLEMENT_GEOM(GMultiPolygon, wkbMultiPolygon, "MULTIPOLYGON")
  IMPLEMENT_GEOM(GGeometryCollection, wkbGeometryCollection, "GEOMETRYCOLLECTION")
};

static Geometry::GClassInfo *ci_collection_end = ci_collection + sizeof(ci_collection)/sizeof(ci_collection[0]);

/***************************** Geometry *******************************/

Geometry::GClassInfo *Geometry::find_class(int type_id)
{
  for (GClassInfo *cur_rt = ci_collection; cur_rt < ci_collection_end; ++cur_rt)
  {
    if (cur_rt->m_type_id == type_id)
    {
      return cur_rt;
    }
  }
  return NULL;
}
 
Geometry::GClassInfo *Geometry::find_class(const char *name, size_t len)
{
  for (GClassInfo *cur_rt = ci_collection; 
              cur_rt < ci_collection_end; ++cur_rt)
  {
    if ((cur_rt->m_name[len] == 0) && 
	(my_strnncoll(&my_charset_latin1, (const uchar*)cur_rt->m_name, len,
					  (const uchar*)name, len) == 0))
    {
      return cur_rt;
    }
  }
  return NULL;
}

int Geometry::create_from_wkb(const char *data, uint32 data_len)
{
  uint32 geom_type;

  if (data_len < 1 + 4)
    return 1;
  data++;
//FIXME: check byte ordering
  geom_type= uint4korr(data);
  data+= 4;
  m_vmt= find_class(geom_type);
  if (!m_vmt) 
    return -1;
  m_data= data;
  m_data_end= data + data_len;
  return 0;
}

int Geometry::create_from_wkt(GTextReadStream *trs, String *wkt, int init_stream)
{
  int name_len;
  const char *name = trs->get_next_word(&name_len);
  if (!name)
  {
    trs->set_error_msg("Geometry name expected");
    return -1;
  }
  if (!(m_vmt = find_class(name, name_len)))
    return -1;
  if (wkt->reserve(1 + 4, 512))
    return 1;
  wkt->q_append((char)wkbNDR);
  wkt->q_append((uint32)get_class_info()->m_type_id);
  if (trs->get_next_symbol() != '(')
  {
    trs->set_error_msg("'(' expected");
    return -1;
  }
  if (init_from_wkt(trs, wkt)) return 1;
  if (trs->get_next_symbol() != ')')
  {        
    trs->set_error_msg("')' expected");                             
    return -1;
  }                  
  if (init_stream)  
  {
    init_from_wkb(wkt->ptr(), wkt->length());
    shift_wkb_header();
  }
  return 0;
}

int Geometry::envelope(String *result) const
{
  MBR mbr;

  get_mbr(&mbr);

  if (result->reserve(1+4*3+sizeof(double)*10))
    return 1;

  result->q_append((char)wkbNDR);
  result->q_append((uint32)wkbPolygon);
  result->q_append((uint32)1);
  result->q_append((uint32)5);
  result->q_append(mbr.xmin);
  result->q_append(mbr.ymin);
  result->q_append(mbr.xmax);
  result->q_append(mbr.ymin);
  result->q_append(mbr.xmax);
  result->q_append(mbr.ymax);
  result->q_append(mbr.xmin);
  result->q_append(mbr.ymax);
  result->q_append(mbr.xmin);
  result->q_append(mbr.ymin);

  return 0;
}

/***************************** Point *******************************/

size_t GPoint::get_data_size() const
{
  return POINT_DATA_SIZE;
}

int GPoint::init_from_wkt(GTextReadStream *trs, String *wkb)
{
  double x, y;
  if (wkb->reserve(sizeof(double)*2))
    return 1;
  if (trs->get_next_number(&x))
    return 1;
  if (trs->get_next_number(&y))
    return 1;
  wkb->q_append(x);
  wkb->q_append(y);

  return 0;
}

int GPoint::get_data_as_wkt(String *txt) const
{
  double x, y;
  if (get_xy(&x, &y))
    return 1;
  if (txt->reserve(MAX_DIGITS_IN_DOUBLE * 2 + 1))
    return 1;
  txt->qs_append(x);
  txt->qs_append(' ');
  txt->qs_append(y);
  return 0;
}

int GPoint::get_mbr(MBR *mbr) const
{
  double x, y;
  if (get_xy(&x, &y))
    return 1;
  mbr->add_xy(x, y);
  return 0;
}

/***************************** LineString *******************************/

size_t GLineString::get_data_size() const 
{
  uint32 n_points = uint4korr(m_data);

  return 4 + n_points*POINT_DATA_SIZE;
}

int GLineString::init_from_wkt(GTextReadStream *trs, String *wkb)
{
  uint32 n_points = 0;
  int np_pos = wkb->length();
  GPoint p;

  if (wkb->reserve(4, 512))
    return 1;
  
  wkb->q_append((uint32)n_points);

  for (;;)
  {
    if (p.init_from_wkt(trs, wkb))
      return 1;
    ++n_points;
    if (trs->get_next_toc_type() == GTextReadStream::comma)
      trs->get_next_symbol();
    else break;
  }

  if (n_points<2)
  {
    trs->set_error_msg("Too few points in LINESTRING");
    return 1;
  }

  wkb->WriteAtPosition(np_pos, n_points);

  return 0;
}

int GLineString::get_data_as_wkt(String *txt) const
{
  uint32 n_points;
  const char *data = m_data;

  if (no_data(data, 4))
    return 1;

  n_points = uint4korr(data);
  data += 4;

  if (no_data(data, sizeof(double) * 2 * n_points))
    return 1;

  if (txt->reserve(((MAX_DIGITS_IN_DOUBLE + 1)*2 + 1) * n_points))
    return 1;
  for (; n_points>0; --n_points)
  {
    double x, y;
    float8get(x, data);
    data += sizeof(double);
    float8get(y, data);
    data += sizeof(double);
    txt->qs_append(x);
    txt->qs_append(' ');
    txt->qs_append(y);
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  return 0;
}

int GLineString::get_mbr(MBR *mbr) const
{
  uint32 n_points;
  const char *data = m_data;

  if (no_data(data, 4))
    return 1;

  n_points = uint4korr(data);
  data += 4;

  if (no_data(data, sizeof(double) * 2 * n_points))
    return 1;
  for (; n_points>0; --n_points)
  {
    mbr->add_xy(data, data + 8);
    data += 8+8;
  }

  return 0;
}

int GLineString::length(double *len) const
{
  uint32 n_points;
  double prev_x, prev_y;
  const char *data = m_data;

  *len=0;
  if (no_data(data, 4))
    return 1;
  n_points = uint4korr(data);
  data += 4;

  if (no_data(data, sizeof(double) * 2 * n_points))
    return 1;

  --n_points;
  float8get(prev_x, data);
  data += 8;
  float8get(prev_y, data);
  data += 8;

  for (; n_points>0; --n_points)
  {
    double x, y;
    float8get(x, data);
    data += 8;
    float8get(y, data);
    data += 8;
    *len+=sqrt(pow(prev_x-x,2)+pow(prev_y-y,2));
    prev_x=x;
    prev_y=y;
  }
  return 0;
}

int GLineString::is_closed(int *closed) const

{
  uint32 n_points;
  double x1, y1, x2, y2;

  const char *data = m_data;

  if (no_data(data, 4))
    return 1;
  n_points = uint4korr(data);
  data += 4;
  if (no_data(data, (8+8) * n_points))
    return 1;
  float8get(x1, data);
  data += 8;
  float8get(y1, data);
  data += 8 + (n_points-2)*POINT_DATA_SIZE;
  float8get(x2, data);
  data += 8;
  float8get(y2, data);

  *closed=(x1==x2)&&(y1==y2);

  return 0;
}

int GLineString::num_points(uint32 *n_points) const
{
  *n_points = uint4korr(m_data);
  return 0;
}

int GLineString::start_point(String *result) const
{
  const char *data= m_data + 4;
  if (no_data(data, 8 + 8))
    return 1;

  if (result->reserve(1 + 4 + sizeof(double) * 2))
    return 1;

  result->q_append((char) wkbNDR);
  result->q_append((uint32) wkbPoint);
  double d;
  float8get(d, data);
  result->q_append(d);
  float8get(d, data + 8);
  result->q_append(d);

  return 0;
}

int GLineString::end_point(String *result) const
{
  const char *data= m_data;
  uint32 n_points;

  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);

  data+= 4 + (n_points - 1) * POINT_DATA_SIZE;

  if (no_data(data, 8 + 8))
    return 1;

  if (result->reserve(1 + 4 + sizeof(double) * 2))
    return 1;
  result->q_append((char) wkbNDR);
  result->q_append((uint32) wkbPoint);
  double d;
  float8get(d, data);
  result->q_append(d);
  float8get(d, data + 8);
  result->q_append(d);

  return 0;
}


int GLineString::point_n(uint32 num, String *result) const
{
  const char *data= m_data;
  uint32 n_points;

  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);

  if ((uint32) (num - 1) >= n_points) // means (num > n_points || num < 1)
    return 1;

  data+= 4 + (num - 1) * POINT_DATA_SIZE;

  if (no_data(data, 8 + 8))
    return 1;
  if (result->reserve(1 + 4 + sizeof(double) * 2))
    return 1;

  result->q_append((char) wkbNDR);
  result->q_append((uint32) wkbPoint);
  double d;
  float8get(d, data);
  result->q_append(d);
  float8get(d, data + 8);
  result->q_append(d);

  return 0;
}

/***************************** Polygon *******************************/

size_t GPolygon::get_data_size() const 
{
  uint32 n_linear_rings = 0;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;

  n_linear_rings = uint4korr(data);
  data += 4;
  for (; n_linear_rings>0; --n_linear_rings)
  {
    if (no_data(data, 4))
      return 1;
    data += 4 + uint4korr(data)*POINT_DATA_SIZE;
  }
  return data - m_data;
}

int GPolygon::init_from_wkt(GTextReadStream *trs, String *wkb)
{
  uint32 n_linear_rings = 0;
  int lr_pos = wkb->length();

  if (wkb->reserve(4, 512))
    return 1;

  wkb->q_append((uint32)n_linear_rings);

  for (;;)  
  {
    GLineString ls;
    size_t ls_pos=wkb->length();
    if (trs->get_next_symbol() != '(')
    {
      trs->set_error_msg("'(' expected");
      return 1;
    }
    if (ls.init_from_wkt(trs, wkb))
      return 1;
    if (trs->get_next_symbol() != ')')
    {
      trs->set_error_msg("')' expected");
      return 1;
    }
    ls.init_from_wkb(wkb->ptr()+ls_pos, wkb->length()-ls_pos);
    int closed;
    ls.is_closed(&closed);
    if (!closed)
    {
      trs->set_error_msg("POLYGON's linear ring isn't closed");
      return 1;
    }
    ++n_linear_rings;
    if (trs->get_next_toc_type() == GTextReadStream::comma)
      trs->get_next_symbol();
    else 
      break;
  }
  wkb->WriteAtPosition(lr_pos, n_linear_rings);
  return 0;
}

int GPolygon::get_data_as_wkt(String *txt) const
{
  uint32 n_linear_rings;
  const char *data= m_data;

  if (no_data(data, 4))
    return 1;

  n_linear_rings= uint4korr(data);
  data+= 4;

  for (; n_linear_rings > 0; --n_linear_rings)
  {
    if (no_data(data, 4))
      return 1;
    uint32 n_points= uint4korr(data);
    data+= 4;
    if (no_data(data, (8 + 8) * n_points))
      return 1;

    if (txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
      return 1;
    txt->qs_append('(');
    for (; n_points>0; --n_points)
    {
      double d;
      float8get(d, data);
      txt->qs_append(d);
      txt->qs_append(' ');
      float8get(d, data + 8);
      txt->qs_append(d);
      txt->qs_append(',');

      data+= 8 + 8;
    }
    (*txt) [txt->length() - 1]= ')';
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  return 0;
}

int GPolygon::get_mbr(MBR *mbr) const
{
  uint32 n_linear_rings;

  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_linear_rings = uint4korr(data);
  data += 4;
  for (; n_linear_rings>0; --n_linear_rings)
  {
    if (no_data(data, 4))
      return 1;
    uint32 n_points = uint4korr(data);
    data += 4;
    if (no_data(data, (8+8) * n_points))
      return 1;
    for (; n_points>0; --n_points)
    {
      mbr->add_xy(data, data + 8);
      data += 8+8;
    }
  }
  return 0;
}

int GPolygon::area(double *ar) const
{
  uint32 n_linear_rings;
  double result = -1.0;

  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_linear_rings = uint4korr(data);
  data += 4;
  for (; n_linear_rings>0; --n_linear_rings)
  {
    double prev_x, prev_y;
    double lr_area=0;
    if (no_data(data, 4))
      return 1;
    uint32 n_points = uint4korr(data);
    if (no_data(data, (8+8) * n_points))
      return 1;
    float8get(prev_x, data+4);
    float8get(prev_y, data+(4+8));
    data += (4+8+8);

    --n_points;
    for (; n_points>0; --n_points)
    {
      double x, y;
      float8get(x, data);
      float8get(y, data + 8);
      lr_area+=(prev_x+x)*(prev_y-y);
      prev_x=x;
      prev_y=y;
      data += (8+8);
    }
    lr_area=fabs(lr_area)/2;
    if (result==-1) result=lr_area;
    else result-=lr_area;
  }
  *ar=fabs(result);
  return 0;
}


int GPolygon::exterior_ring(String *result) const
{
  uint32 n_points;
  const char *data = m_data + 4; // skip n_linerings

  if (no_data(data, 4))
    return 1;
  n_points = uint4korr(data);
  data += 4;
  if (no_data(data, n_points * POINT_DATA_SIZE))
    return 1;

  if (result->reserve(1+4+4+ n_points * POINT_DATA_SIZE))
    return 1;

  result->q_append((char)wkbNDR);
  result->q_append((uint32)wkbLineString);
  result->q_append(n_points);
  result->q_append(data, n_points * POINT_DATA_SIZE); 

  return 0;
}

int GPolygon::num_interior_ring(uint32 *n_int_rings) const
{
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  *n_int_rings = uint4korr(data);
  --(*n_int_rings);

  return 0;
}

int GPolygon::interior_ring_n(uint32 num, String *result) const
{
  const char *data = m_data;
  uint32 n_linear_rings;
  uint32 n_points;

  if (no_data(data, 4))
    return 1;

  n_linear_rings = uint4korr(data);
  data += 4;
  if ((num >= n_linear_rings) || (num < 1))
    return -1;

  for (; num > 0; --num)
  {
    if (no_data(data, 4))
      return 1;
    data += 4 + uint4korr(data) * POINT_DATA_SIZE;
  }
  if (no_data(data, 4))
    return 1;
  n_points = uint4korr(data);
  int points_size = n_points * POINT_DATA_SIZE;
  data += 4;
  if (no_data(data, points_size))
    return 1;

  if (result->reserve(1+4+4+ points_size))
    return 1;

  result->q_append((char)wkbNDR);
  result->q_append((uint32)wkbLineString);
  result->q_append(n_points);
  result->q_append(data, points_size); 

  return 0;
}

int GPolygon::centroid_xy(double *x, double *y) const
{
  uint32 n_linear_rings;
  uint32 i;
  double res_area, res_cx, res_cy;
  const char *data = m_data;
  LINT_INIT(res_area);
  LINT_INIT(res_cx);
  LINT_INIT(res_cy);

  if (no_data(data, 4))
    return 1;
  n_linear_rings = uint4korr(data);
  data += 4;

  for (i = 0; i < n_linear_rings; ++i)
  {
    if (no_data(data, 4))
      return 1;
    uint32 n_points = uint4korr(data);
    double prev_x, prev_y;
    double cur_area = 0;
    double cur_cx = 0;
    double cur_cy = 0;

    data += 4;
    if (no_data(data, (8+8) * n_points))
      return 1;
    float8get(prev_x, data);
    float8get(prev_y, data+8);
    data += (8+8);

    uint32 n = n_points - 1;
    for (; n > 0; --n)
    {
      double x, y;
      float8get(x, data);
      float8get(y, data + 8);

      cur_area += (prev_x + x) * (prev_y - y);
      cur_cx += x;
      cur_cy += y;
      prev_x = x;
      prev_y = y;
      data += (8+8);
    }
    cur_area = fabs(cur_area) / 2;
    cur_cx = cur_cx / (n_points - 1);
    cur_cy = cur_cy / (n_points - 1);

    if (i)
    {
      double d_area = res_area - cur_area;
      if (d_area <= 0)
        return 1;
      res_cx = (res_area * res_cx - cur_area * cur_cx) / d_area;
      res_cy = (res_area * res_cy - cur_area * cur_cy) / d_area;
    }
    else
    {
      res_area = cur_area;
      res_cx = cur_cx;
      res_cy = cur_cy;
    }
  }

  *x = res_cx;
  *y = res_cy;

  return 0;
}

int GPolygon::centroid(String *result) const
{
  double x, y;

  this->centroid_xy(&x, &y);
  if (result->reserve(1 + 4 + sizeof(double) * 2))
    return 1;

  result->q_append((char)wkbNDR);
  result->q_append((uint32)wkbPoint);
  result->q_append(x);
  result->q_append(y);

  return 0;
}


/***************************** MultiPoint *******************************/

size_t GMultiPoint::get_data_size() const 
{
  return 4 + uint4korr(m_data)*(POINT_DATA_SIZE + WKB_HEADER_SIZE);
}

int GMultiPoint::init_from_wkt(GTextReadStream *trs, String *wkb)
{
  uint32 n_points = 0;
  int np_pos = wkb->length();
  GPoint p;

  if (wkb->reserve(4, 512))
    return 1;
  wkb->q_append((uint32)n_points);

  for (;;)
  {
    if (wkb->reserve(1+4, 512))
      return 1;
    wkb->q_append((char)wkbNDR);
    wkb->q_append((uint32)wkbPoint);
    if (p.init_from_wkt(trs, wkb))
      return 1;
    ++n_points;
    if (trs->get_next_toc_type() == GTextReadStream::comma)
      trs->get_next_symbol();
    else 
      break;
  }
  wkb->WriteAtPosition(np_pos, n_points);

  return 0;
}

int GMultiPoint::get_data_as_wkt(String *txt) const
{
  uint32 n_points;
  const char *data= m_data;
  if (no_data(data, 4))
    return 1;

  n_points= uint4korr(data);
  data+= 4;
  if (no_data(data, n_points * (8 + 8 + WKB_HEADER_SIZE)))
    return 1;

  if (txt->reserve(((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
    return 1;

  for (; n_points>0; --n_points)
  {
    double d;
    float8get(d, data + WKB_HEADER_SIZE);
    txt->qs_append(d);
    txt->qs_append(' ');
    float8get(d, data + WKB_HEADER_SIZE + 8);
    txt->qs_append(d);
    txt->qs_append(',');
    data+= WKB_HEADER_SIZE + 8 + 8;
  }
  txt->length(txt->length()-1);
  return 0;
}

int GMultiPoint::get_mbr(MBR *mbr) const
{
  uint32 n_points;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_points = uint4korr(data);
  data += 4;
  if (no_data(data, n_points * (8+8+WKB_HEADER_SIZE)))
    return 1;
  for (; n_points>0; --n_points)
  {
    mbr->add_xy(data + WKB_HEADER_SIZE, data + 8 + WKB_HEADER_SIZE);
    data += (8+8+WKB_HEADER_SIZE);
  }
  return 0;
}

int GMultiPoint::num_geometries(uint32 *num) const
{
  *num = uint4korr(m_data);
  return 0;
}

int GMultiPoint::geometry_n(uint32 num, String *result) const
{
  const char *data= m_data;
  uint32 n_points;
  if (no_data(data, 4))
    return 1;
  n_points= uint4korr(data);
  data+= 4;
  if ((num > n_points) || (num < 1))
    return -1;
  data+= (num - 1) * (WKB_HEADER_SIZE + POINT_DATA_SIZE);
  if (result->reserve(WKB_HEADER_SIZE + POINT_DATA_SIZE))
    return 1;
  result->q_append(data, WKB_HEADER_SIZE + POINT_DATA_SIZE);

  return 0;
}

/***************************** MultiLineString *******************************/

size_t GMultiLineString::get_data_size() const 
{
  uint32 n_line_strings = 0;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_line_strings = uint4korr(data);
  data += 4;

  for (; n_line_strings>0; --n_line_strings)
  {
    if (no_data(data, WKB_HEADER_SIZE + 4))
      return 1;
    data += WKB_HEADER_SIZE + 4 + uint4korr(data + WKB_HEADER_SIZE) * POINT_DATA_SIZE;
  }
  return data - m_data;
}

int GMultiLineString::init_from_wkt(GTextReadStream *trs, String *wkb)
{
  uint32 n_line_strings = 0;
  int ls_pos = wkb->length();

  if (wkb->reserve(4, 512))
    return 1;

  wkb->q_append((uint32)n_line_strings);
  
  for (;;)
  {
    GLineString ls;

    if (wkb->reserve(1+4, 512))
      return 1;
    wkb->q_append((char)wkbNDR);
    wkb->q_append((uint32)wkbLineString);

    if (trs->get_next_symbol() != '(')
    {
      trs->set_error_msg("'(' expected");
      return 1;
    }
    if (ls.init_from_wkt(trs, wkb))
      return 1;

    if (trs->get_next_symbol() != ')')
    {
      trs->set_error_msg("')' expected");
      return 1;
    }
    ++n_line_strings;
    if (trs->get_next_toc_type() == GTextReadStream::comma) 
      trs->get_next_symbol();
    else 
      break;
  }
  wkb->WriteAtPosition(ls_pos, n_line_strings);

  return 0;
}

int GMultiLineString::get_data_as_wkt(String *txt) const
{
  uint32 n_line_strings;
  const char *data= m_data;
  if (no_data(data, 4))
    return 1;
  n_line_strings= uint4korr(data);
  data+= 4;
  for (; n_line_strings > 0; --n_line_strings)
  {
    if (no_data(data, (WKB_HEADER_SIZE + 4)))
      return 1;
    uint32 n_points= uint4korr(data + WKB_HEADER_SIZE);
    data+= WKB_HEADER_SIZE + 4;
    if (no_data(data, n_points * (8 + 8)))
      return 1;

    if (txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points))
      return 1;
    txt->qs_append('(');
    for (; n_points>0; --n_points)
    {
      double d;
      float8get(d, data);
      txt->qs_append(d);
      txt->qs_append(' ');
      float8get(d, data + 8);
      txt->qs_append(d);
      txt->qs_append(',');
      data+= 8 + 8;
    }
    (*txt) [txt->length() - 1] = ')';
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  return 0;
}

int GMultiLineString::get_mbr(MBR *mbr) const
{
  uint32 n_line_strings;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_line_strings = uint4korr(data);
  data += 4;

  for (; n_line_strings>0; --n_line_strings)
  {
    if (no_data(data, WKB_HEADER_SIZE + 4))
      return 1;
    uint32 n_points = uint4korr(data + WKB_HEADER_SIZE);
    data += 4+WKB_HEADER_SIZE;
    if (no_data(data, (8+8)*n_points))
      return 1;

    for (; n_points>0; --n_points)
    {
      mbr->add_xy(data, data + 8);
      data += 8+8;
    }
  }
  return 0;
}

int GMultiLineString::num_geometries(uint32 *num) const
{
  *num = uint4korr(m_data);
  return 0;
}

int GMultiLineString::geometry_n(uint32 num, String *result) const
{
  uint32 n_line_strings;
  const char *data= m_data;
  if (no_data(data, 4))
    return 1;
  n_line_strings= uint4korr(data);
  data+= 4;

  if ((num > n_line_strings) || (num < 1))
    return -1;
 
  for (; num > 0; --num)
  {
    if (no_data(data, WKB_HEADER_SIZE + 4))
      return 1;
    uint32 n_points= uint4korr(data + WKB_HEADER_SIZE);
    if (num == 1)
    {
      if (result->reserve(WKB_HEADER_SIZE + 4 + POINT_DATA_SIZE * n_points))
	return 1;
      result->q_append(data, WKB_HEADER_SIZE + 4 + POINT_DATA_SIZE *n_points);
      break;
    }
    else
    {
      data+= WKB_HEADER_SIZE + 4 + POINT_DATA_SIZE * n_points;
    }
  }
  return 0;
}

int GMultiLineString::length(double *len) const
{
  uint32 n_line_strings;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_line_strings = uint4korr(data);
  data += 4;
  *len=0;
  for (; n_line_strings>0; --n_line_strings)
  {
    double ls_len;
    GLineString ls;
    data += WKB_HEADER_SIZE;
    ls.init_from_wkb(data, m_data_end - data);
    if (ls.length(&ls_len))
      return 1;
    *len+=ls_len;
    data += ls.get_data_size();
  }
  return 0;
}

int GMultiLineString::is_closed(int *closed) const
{
  uint32 n_line_strings;
  const char *data = m_data;
  if (no_data(data, 1))
    return 1;
  n_line_strings = uint4korr(data);
  data += 4 + WKB_HEADER_SIZE;
  for (; n_line_strings>0; --n_line_strings)
  {
    GLineString ls;
    ls.init_from_wkb(data, m_data_end - data);
    if (ls.is_closed(closed))
      return 1;
    if (!*closed)
      return 0;
    data += ls.get_data_size() + WKB_HEADER_SIZE;
  }
  return 0;
}

/***************************** MultiPolygon *******************************/

size_t GMultiPolygon::get_data_size() const 
{
  uint32 n_polygons;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_polygons = uint4korr(data);
  data += 4;

  for (; n_polygons>0; --n_polygons)
  {
    if (no_data(data, 4 + WKB_HEADER_SIZE))
      return 1;
    uint32 n_linear_rings = uint4korr(data + WKB_HEADER_SIZE);
    data += 4 + WKB_HEADER_SIZE;

    for (; n_linear_rings > 0; --n_linear_rings)
    {
      data += 4 + uint4korr(data) * POINT_DATA_SIZE;
    }
  }
  return data - m_data;
}

int GMultiPolygon::init_from_wkt(GTextReadStream *trs, String *wkb)
{
  uint32 n_polygons = 0;
  int np_pos = wkb->length();
  GPolygon p;

  if (wkb->reserve(4, 512))
    return 1;

  wkb->q_append((uint32)n_polygons);

  for (;;)  
  {
    if (wkb->reserve(1+4, 512))
      return 1;
    wkb->q_append((char)wkbNDR);
    wkb->q_append((uint32)wkbPolygon);

    if (trs->get_next_symbol() != '(')
    {
      trs->set_error_msg("'(' expected");
      return 1;
    }
    if (p.init_from_wkt(trs, wkb))
      return 1;
    if (trs->get_next_symbol() != ')')
    {
      trs->set_error_msg("')' expected");
      return 1;
    }
    ++n_polygons;
    if (trs->get_next_toc_type() == GTextReadStream::comma)
      trs->get_next_symbol();
    else
      break;
  }
  wkb->WriteAtPosition(np_pos, n_polygons);
  return 0;
}

int GMultiPolygon::get_data_as_wkt(String *txt) const
{
  uint32 n_polygons;
  const char *data= m_data;
  if (no_data(data, 4))
    return 1;
  n_polygons= uint4korr(data);
  data+= 4;

  for (; n_polygons>0; --n_polygons)
  {
    if (no_data(data, 4 + WKB_HEADER_SIZE))
      return 1;
    data+= WKB_HEADER_SIZE;
    uint32 n_linear_rings= uint4korr(data);
    data+= 4;

    if (txt->reserve(1, 512))
      return 1;
    txt->q_append('(');
    for (; n_linear_rings>0; --n_linear_rings)
    {
      if (no_data(data, 4))
        return 1;
      uint32 n_points= uint4korr(data);
      data+= 4;
      if (no_data(data, (8 + 8) * n_points)) return 1;

      if (txt->reserve(2 + ((MAX_DIGITS_IN_DOUBLE + 1) * 2 + 1) * n_points, 
		       512)) return 1;
      txt->qs_append('(');
      for (; n_points>0; --n_points)
      {
        double d;
        float8get(d, data);
        txt->qs_append(d);
        txt->qs_append(' ');
        float8get(d, data + 8);
        txt->qs_append(d);
        txt->qs_append(',');
        data+= 8 + 8;
      }
      (*txt) [txt->length() - 1] = ')';
      txt->qs_append(',');
    }
    (*txt) [txt->length() - 1] = ')';
    txt->qs_append(',');
  }
  txt->length(txt->length() - 1);
  return 0;
}

int GMultiPolygon::get_mbr(MBR *mbr) const
{
  uint32 n_polygons;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_polygons = uint4korr(data);
  data += 4;

  for (; n_polygons>0; --n_polygons)
  {
    if (no_data(data, 4+WKB_HEADER_SIZE))
      return 1;
    uint32 n_linear_rings = uint4korr(data + WKB_HEADER_SIZE);
    data += WKB_HEADER_SIZE + 4;

    for (; n_linear_rings>0; --n_linear_rings)
    {
      if (no_data(data, 4))
        return 1;
      uint32 n_points = uint4korr(data);
      data += 4;
      if (no_data(data, (8+8)*n_points))
        return 1;

      for (; n_points>0; --n_points)
      {
        mbr->add_xy(data, data + 8);
        data += 8+8;
      }
    }
  }
  return 0;
}

int GMultiPolygon::num_geometries(uint32 *num) const
{
  *num = uint4korr(m_data);
  return 0;
}

int GMultiPolygon::geometry_n(uint32 num, String *result) const
{
  uint32 n_polygons;
  const char *data= m_data, *polygon_n;
  LINT_INIT(polygon_n);

  if (no_data(data, 4))
    return 1;
  n_polygons= uint4korr(data);
  data+= 4;

  if ((num > n_polygons) || (num < 1))
    return -1;

  for (; num > 0; --num)
  {
    if (no_data(data, WKB_HEADER_SIZE + 4))
      return 1;
    uint32 n_linear_rings= uint4korr(data + WKB_HEADER_SIZE);

    if (num == 1)
      polygon_n= data;
    data+= WKB_HEADER_SIZE + 4;
    for (; n_linear_rings > 0; --n_linear_rings)
    {
      if (no_data(data, 4))
	return 1;
      uint32 n_points= uint4korr(data);
      data+= 4 + POINT_DATA_SIZE * n_points;
    }
    if (num == 1)
    {
      if (result->reserve(data - polygon_n))
	return -1;
       result->q_append(polygon_n, data - polygon_n);
      break;
    }
  }
  return 0;
}

int GMultiPolygon::area(double *ar) const
{
  uint32 n_polygons;
  const char *data = m_data;
  double result = 0;
  if (no_data(data, 4))
    return 1;
  n_polygons = uint4korr(data);
  data += 4;

  for (; n_polygons>0; --n_polygons)
  {
    double p_area;

    GPolygon p;
    data += WKB_HEADER_SIZE;
    p.init_from_wkb(data, m_data_end - data);
    if (p.area(&p_area))
      return 1;
    result += p_area;
    data += p.get_data_size();
  }
  *ar = result;
  return 0;
}

int GMultiPolygon::centroid(String *result) const
{
  uint32 n_polygons;
  uint i;
  GPolygon p;
  double res_area, res_cx, res_cy;
  double cur_area, cur_cx, cur_cy;

  LINT_INIT(res_area);
  LINT_INIT(res_cx);
  LINT_INIT(res_cy);

  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_polygons = uint4korr(data);
  data += 4;

  for (i = 0; i < n_polygons; ++i)
  {
    data += WKB_HEADER_SIZE;
    p.init_from_wkb(data, m_data_end - data);
    if (p.area(&cur_area))
      return 1;

    if (p.centroid_xy(&cur_cx, &cur_cy))
      return 1;

    if (i)
    {
      double sum_area = res_area + cur_area;
      res_cx = (res_area * res_cx + cur_area * cur_cx) / sum_area;
      res_cy = (res_area * res_cy + cur_area * cur_cy) / sum_area;
    }
    else
    {
      res_area = cur_area;
      res_cx = cur_cx;
      res_cy = cur_cy;
    }

    data += p.get_data_size();
  }

  if (result->reserve(1 + 4 + sizeof(double) * 2))
    return 1;
  result->q_append((char)wkbNDR);
  result->q_append((uint32)wkbPoint);
  result->q_append(res_cx);
  result->q_append(res_cy);

  return 0;
}

/***************************** GeometryCollection *******************************/

size_t GGeometryCollection::get_data_size() const 
{
  uint32 n_objects;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_objects = uint4korr(data);
  data += 4;

  for (; n_objects>0; --n_objects)
  {
    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    uint32 wkb_type = uint4korr(data + sizeof(char));
    data += WKB_HEADER_SIZE;

    Geometry geom;

    if (geom.init(wkb_type))
      return 0;

    geom.init_from_wkb(data, m_data_end - data);
    size_t object_size=geom.get_data_size();
    data += object_size;
  }
  return data - m_data;
}

int GGeometryCollection::init_from_wkt(GTextReadStream *trs, String *wkb)
{
  uint32 n_objects = 0;
  int no_pos = wkb->length();
  Geometry g;

  if (wkb->reserve(4, 512))
    return 1;
  wkb->q_append((uint32)n_objects);

  for (;;)
  {
    if (g.create_from_wkt(trs, wkb))
      return 1;

    if (g.get_class_info()->m_type_id==wkbGeometryCollection)
    {
      trs->set_error_msg("Unexpected GEOMETRYCOLLECTION");
      return 1;
    }
    ++n_objects;
    if (trs->get_next_toc_type() == GTextReadStream::comma)
      trs->get_next_symbol();
    else break;
  }
  wkb->WriteAtPosition(no_pos, n_objects);

  return 0;
}

int GGeometryCollection::get_data_as_wkt(String *txt) const
{
  uint32 n_objects;
  const char *data = m_data;
  Geometry geom;
  if (no_data(data, 4))
    return 1;
  n_objects = uint4korr(data);
  data += 4;

  for (; n_objects>0; --n_objects)
  {
    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    uint32 wkb_type = uint4korr(data + sizeof(char));
    data += WKB_HEADER_SIZE;

    if (geom.init(wkb_type))
      return 1;
    geom.init_from_wkb(data, m_data_end - data);
    if (geom.as_wkt(txt))
      return 1;
    data += geom.get_data_size();
    txt->reserve(1, 512);
    txt->q_append(',');
  }
  txt->length(txt->length() - 1);
  return 0;
}

int GGeometryCollection::get_mbr(MBR *mbr) const
{
  uint32 n_objects;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_objects = uint4korr(data);
  data += 4;
  for (; n_objects>0; --n_objects)
  {
    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    uint32 wkb_type = uint4korr(data + sizeof(char));
    data += WKB_HEADER_SIZE;
    Geometry geom;

    if (geom.init(wkb_type))
      return 1;
    geom.init_from_wkb(data, m_data_end - data);
    geom.get_mbr(mbr);
    data += geom.get_data_size();
  }
  return 0;
}

int GGeometryCollection::num_geometries(uint32 *num) const
{
  *num = uint4korr(m_data);
  return 0;
}

int GGeometryCollection::geometry_n(uint32 num, String *result) const
{
  const char *data = m_data;
  uint32 n_objects;
  if (no_data(data, 4))
    return 1;
  n_objects = uint4korr(data);
  data += 4;

  if ((num > n_objects) || (num < 1))
  {
    return -1;
  }
  for (; num > 0; --num)
  {
    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    uint32 wkb_type = uint4korr(data + sizeof(char));
    data += WKB_HEADER_SIZE;

    Geometry geom;
    if (geom.init(wkb_type))
      return 1;
    geom.init_from_wkb(data, m_data_end - data);
    if (num == 1)
    {
      if (result->reserve(1+4+geom.get_data_size()))
        return 1;
      result->q_append((char)wkbNDR);
      result->q_append((uint32)wkb_type);
      result->q_append(data, geom.get_data_size());
      break;
    }
    else
    {
      data += geom.get_data_size();
    }
  }
  return 0;
}

int GGeometryCollection::dimension(uint32 *dim) const
{
  uint32 n_objects;
  *dim = 0;
  const char *data = m_data;
  if (no_data(data, 4))
    return 1;
  n_objects = uint4korr(data);
  data += 4;

  for (; n_objects > 0; --n_objects)
  {
    if (no_data(data, WKB_HEADER_SIZE))
      return 1;
    uint32 wkb_type = uint4korr(data + sizeof(char));
    data += WKB_HEADER_SIZE;

    uint32 d;

    Geometry geom;
    if (geom.init(wkb_type))
      return 1;
    geom.init_from_wkb(data, m_data_end - data);
    if (geom.dimension(&d))
      return 1;

    if (d > *dim)
      *dim = d;
    data += geom.get_data_size();
  }
  return 0;
}

/***************************** /objects *******************************/

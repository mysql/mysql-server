/*****************************************************************************

Copyright (c) 2013, 2015, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file gis/gis0geo.cc
InnoDB R-tree related functions.

Created 2013/03/27 Allen Lai and Jimmy Yang
*******************************************************/

#include "page0types.h"
#include "gis0geo.h"
#include "page0cur.h"
#include "ut0rnd.h"
#include "mach0data.h"

#include <spatial.h>

/* These definitions are for comparing 2 mbrs. */

/* Check if a intersects b.
Return false if a intersects b, otherwise true. */
#define INTERSECT_CMP(amin, amax, bmin, bmax) \
(((amin) > (bmax)) || ((bmin) > (amax)))

/* Check if b contains a.
Return false if b contains a, otherwise true. */
#define CONTAIN_CMP(amin, amax, bmin, bmax) \
(((bmin) > (amin)) || ((bmax) < (amax)))

/* Check if b is within a.
Return false if b is within a, otherwise true. */
#define WITHIN_CMP(amin, amax, bmin, bmax) \
(((amin) > (bmin)) || ((amax) < (bmax)))

/* Check if a disjoints b.
Return false if a disjoints b, otherwise true. */
#define DISJOINT_CMP(amin, amax, bmin, bmax) \
(((amin) <= (bmax)) && ((bmin) <= (amax)))

/* Check if a equals b.
Return false if equal, otherwise true. */
#define EQUAL_CMP(amin, amax, bmin, bmax) \
(((amin) != (bmin)) || ((amax) != (bmax)))

/****************************************************************
Functions for generating mbr
****************************************************************/
/*************************************************************//**
Add one point stored in wkb to a given mbr.
@return 0 if the point in wkb is valid, otherwise -1. */
static
int
rtree_add_point_to_mbr(
/*===================*/
	uchar**	wkb,		/*!< in: pointer to wkb,
				where point is stored */
	uchar*	end,		/*!< in: end of wkb. */
	uint	n_dims,		/*!< in: dimensions. */
	uchar	byte_order,	/*!< in: byte order. */
	double*	mbr)		/*!< in/out: mbr, which
				must be of length n_dims * 2. */
{
	double	ord;
	double*	mbr_end = mbr + n_dims * 2;

	while (mbr < mbr_end) {
		if ((*wkb) + sizeof(double) > end) {
			return(-1);
		}

		ord = mach_double_read(*wkb);
		(*wkb) += sizeof(double);

		if (ord < *mbr) {
			*mbr = ord;
		}
		mbr++;

		if (ord > *mbr) {
			*mbr = ord;
		}
		mbr++;
	}

	return(0);
}

/*************************************************************//**
Get mbr of point stored in wkb.
@return 0 if ok, otherwise -1. */
static
int
rtree_get_point_mbr(
/*================*/
	uchar**	wkb,		/*!< in: pointer to wkb,
				where point is stored. */
	uchar*	end,		/*!< in: end of wkb. */
	uint	n_dims,		/*!< in: dimensions. */
	uchar	byte_order,	/*!< in: byte order. */
	double*	mbr)		/*!< in/out: mbr,
				must be of length n_dims * 2. */
{
	return rtree_add_point_to_mbr(wkb, end, n_dims, byte_order, mbr);
}


/*************************************************************//**
Get mbr of linestring stored in wkb.
@return 0 if the linestring is valid, otherwise -1. */
static
int
rtree_get_linestring_mbr(
/*=====================*/
	uchar**	wkb,		/*!< in: pointer to wkb,
				where point is stored. */
	uchar*	end,		/*!< in: end of wkb. */
	uint	n_dims,		/*!< in: dimensions. */
	uchar	byte_order,	/*!< in: byte order. */
	double*	mbr)		/*!< in/out: mbr,
				must be of length n_dims * 2. */
{
	uint	n_points;

	n_points = uint4korr(*wkb);
	(*wkb) += 4;

	for (; n_points > 0; --n_points) {
		/* Add next point to mbr */
		if (rtree_add_point_to_mbr(wkb, end, n_dims,
					   byte_order, mbr)) {
			return(-1);
		}
	}

	return(0);
}

/*************************************************************//**
Get mbr of polygon stored in wkb.
@return 0 if the polygon is valid, otherwise -1. */
static
int
rtree_get_polygon_mbr(
/*==================*/
	uchar**	wkb,		/*!< in: pointer to wkb,
				where point is stored. */
	uchar*	end,		/*!< in: end of wkb. */
	uint	n_dims,		/*!< in: dimensions. */
	uchar	byte_order,	/*!< in: byte order. */
	double*	mbr)		/*!< in/out: mbr,
				must be of length n_dims * 2. */
{
	uint	n_linear_rings;
	uint	n_points;

	n_linear_rings = uint4korr((*wkb));
	(*wkb) += 4;

	for (; n_linear_rings > 0; --n_linear_rings) {
		n_points = uint4korr((*wkb));
		(*wkb) += 4;

		for (; n_points > 0; --n_points) {
			/* Add next point to mbr */
			if (rtree_add_point_to_mbr(wkb, end, n_dims,
						   byte_order, mbr)) {
				return(-1);
			}
		}
	}

	return(0);
}

/*************************************************************//**
Get mbr of geometry stored in wkb.
@return 0 if the geometry is valid, otherwise -1. */
static
int
rtree_get_geometry_mbr(
/*===================*/
	uchar**	wkb,		/*!< in: pointer to wkb,
				where point is stored. */
	uchar*	end,		/*!< in: end of wkb. */
	uint	n_dims,		/*!< in: dimensions. */
	double*	mbr,		/*!< in/out: mbr. */
	int	top)		/*!< in: if it is the top,
				which means it's not called
				by itself. */
{
	int	res;
	uchar	byte_order = 2;
	uint	wkb_type = 0;
	uint	n_items;

	byte_order = *(*wkb);
	++(*wkb);

	wkb_type = uint4korr((*wkb));
	(*wkb) += 4;

	switch ((enum wkbType) wkb_type) {
	case wkbPoint:
		res = rtree_get_point_mbr(wkb, end, n_dims, byte_order, mbr);
		break;
	case wkbLineString:
		res = rtree_get_linestring_mbr(wkb, end, n_dims,
					       byte_order, mbr);
		break;
	case wkbPolygon:
		res = rtree_get_polygon_mbr(wkb, end, n_dims, byte_order, mbr);
		break;
	case wkbMultiPoint:
		n_items = uint4korr((*wkb));
		(*wkb) += 4;
		for (; n_items > 0; --n_items) {
			byte_order = *(*wkb);
			++(*wkb);
			(*wkb) += 4;
			if (rtree_get_point_mbr(wkb, end, n_dims,
						byte_order, mbr)) {
				return(-1);
			}
		}
		res = 0;
		break;
	case wkbMultiLineString:
		n_items = uint4korr((*wkb));
		(*wkb) += 4;
		for (; n_items > 0; --n_items) {
			byte_order = *(*wkb);
			++(*wkb);
			(*wkb) += 4;
			if (rtree_get_linestring_mbr(wkb, end, n_dims,
						     byte_order, mbr)) {
				return(-1);
			}
		}
		res = 0;
		break;
	case wkbMultiPolygon:
		n_items = uint4korr((*wkb));
		(*wkb) += 4;
		for (; n_items > 0; --n_items) {
			byte_order = *(*wkb);
			++(*wkb);
			(*wkb) += 4;
			if (rtree_get_polygon_mbr(wkb, end, n_dims,
						  byte_order, mbr)) {
				return(-1);
			}
		}
		res = 0;
		break;
	case wkbGeometryCollection:
		if (!top) {
			return(-1);
		}

		n_items = uint4korr((*wkb));
		(*wkb) += 4;
		for (; n_items > 0; --n_items) {
			if (rtree_get_geometry_mbr(wkb, end, n_dims,
						   mbr, 0)) {
				return(-1);
			}
		}
		res = 0;
		break;
	default:
		res = -1;
	}

	return(res);
}

/*************************************************************//**
Calculate Minimal Bounding Rectangle (MBR) of the spatial object
stored in "well-known binary representation" (wkb) format.
@return 0 if ok. */
int
rtree_mbr_from_wkb(
/*===============*/
	uchar*	wkb,		/*!< in: wkb */
	uint	size,		/*!< in: size of wkb. */
	uint	n_dims,		/*!< in: dimensions. */
	double*	mbr)		/*!< in/out: mbr, which must
				be of length n_dim2 * 2. */
{
	for (uint i = 0; i < n_dims; ++i) {
		mbr[i * 2] = DBL_MAX;
		mbr[i * 2 + 1] = -DBL_MAX;
	}

	return rtree_get_geometry_mbr(&wkb, wkb + size, n_dims, mbr, 1);
}


/****************************************************************
Functions for Rtree split
****************************************************************/
/*************************************************************//**
Join 2 mbrs of dimensions n_dim. */
static
void
mbr_join(
/*=====*/
	double*		a,	/*!< in/out: the first mbr,
				where the joined result will be. */
	const double*	b,	/*!< in: the second mbr. */
	int		n_dim)	/*!< in: dimensions. */
{
	double*		end = a + n_dim * 2;

	do {
		if (a[0] > b[0]) {
			a[0] = b[0];
		}

		if (a[1] < b[1]) {
			a[1] = b[1];
		}

		a += 2;
		b += 2;

	} while (a != end);
}

/*************************************************************//**
Counts the square of mbr which is the join of a and b. Both a and b
are of dimensions n_dim. */
static
double
mbr_join_square(
/*============*/
	const double*	a,	/*!< in: the first mbr. */
	const double*	b,	/*!< in: the second mbr. */
	int		n_dim)	/*!< in: dimensions. */
{
	const double*	end = a + n_dim * 2;
	double		square = 1.0;

	do {
		square *= std::max(a[1], b[1]) - std::min(a[0], b[0]);

		a += 2;
		b += 2;
	} while (a != end);

	/* Check for infinity or NaN, so we don't get NaN in calculations */
	if (my_isinf(square) || my_isnan(square)) {
		return DBL_MAX;
	}

	return square;
}

/*************************************************************//**
Counts the square of mbr of dimension n_dim. */
static
double
count_square(
/*=========*/
	const double*	a,	/*!< in: the mbr. */
	int		n_dim)	/*!< in: dimensions. */
{
	const double*	end = a + n_dim * 2;
	double		square = 1.0;

	do {
		square *= a[1] - a[0];
		a += 2;
	} while (a != end);

	return square;
}

/*************************************************************//**
Copy mbr of dimension n_dim from src to dst. */
inline
static
void
copy_coords(
/*========*/
	double*		dst,	/*!< in/out: destination. */
	const double*	src,	/*!< in: source. */
	int		n_dim)	/*!< in: dimensions. */
{
	memcpy(dst, src, DATA_MBR_LEN);
}

/*************************************************************//**
Select two nodes to collect group upon */
static
void
pick_seeds(
/*=======*/
	rtr_split_node_t*	node,		/*!< in: split nodes. */
	int			n_entries,	/*!< in: entries number. */
	rtr_split_node_t**	seed_a,		/*!< out: seed 1. */
	rtr_split_node_t**	seed_b,		/*!< out: seed 2. */
	int			n_dim)		/*!< in: dimensions. */
{
	rtr_split_node_t*	cur1;
	rtr_split_node_t*	lim1 = node + (n_entries - 1);
	rtr_split_node_t*	cur2;
	rtr_split_node_t*	lim2 = node + n_entries;

	double			max_d = -DBL_MAX;
	double			d;

	*seed_a = node;
	*seed_b = node + 1;

	for (cur1 = node; cur1 < lim1; ++cur1) {
		for (cur2 = cur1 + 1; cur2 < lim2; ++cur2) {
			d = mbr_join_square(cur1->coords, cur2->coords, n_dim) -
				cur1->square - cur2->square;
			if (d > max_d) {
				max_d = d;
				*seed_a = cur1;
				*seed_b = cur2;
			}
		}
	}
}

/*********************************************************//**
Generates a random iboolean value.
@return the random value */
static
ibool
ut_rnd_gen_ibool(void)
/*=================*/
{
	ulint    x;

	x = ut_rnd_gen_ulint();

	if (((x >> 20) + (x >> 15)) & 1) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************//**
Select next node and group where to add. */
static
void
pick_next(
/*======*/
	rtr_split_node_t*	node,		/*!< in: split nodes. */
	int			n_entries,	/*!< in: entries number. */
	double*			g1,		/*!< in: mbr of group 1. */
	double*			g2,		/*!< in: mbr of group 2. */
	rtr_split_node_t**	choice,		/*!< out: the next node.*/
	int*			n_group,	/*!< out: group number.*/
	int			n_dim)		/*!< in: dimensions. */
{
	rtr_split_node_t*	cur = node;
	rtr_split_node_t*	end = node + n_entries;
	double			max_diff = -DBL_MAX;

	for (; cur < end; ++cur) {
		double	diff;
		double	abs_diff;

		if (cur->n_node != 0) {
			continue;
		}

		diff = mbr_join_square(g1, cur->coords, n_dim) -
		       mbr_join_square(g2, cur->coords, n_dim);

		abs_diff = fabs(diff);
		if (abs_diff > max_diff) {
			max_diff = abs_diff;

			/* Introduce some randomness if the record
			is identical */
			if (diff == 0) {
				diff = static_cast<double>(
					ut_rnd_gen_ibool());
			}

			*n_group = 1 + (diff > 0);
			*choice = cur;
		}
	}
}

/*************************************************************//**
Mark not-in-group entries as n_group. */
static
void
mark_all_entries(
/*=============*/
	rtr_split_node_t*	node,		/*!< in/out: split nodes. */
	int			n_entries,	/*!< in: entries number. */
	int			n_group)	/*!< in: group number. */
{
	rtr_split_node_t*	cur = node;
	rtr_split_node_t*	end = node + n_entries;
	for (; cur < end; ++cur) {
		if (cur->n_node != 0) {
			continue;
		}
		cur->n_node = n_group;
	}
}

/*************************************************************//**
Split rtree node.
Return which group the first rec is in. */
int
split_rtree_node(
/*=============*/
	rtr_split_node_t*	node,		/*!< in: split nodes. */
	int			n_entries,	/*!< in: entries number. */
	int			all_size,	/*!< in: total key's size. */
	int			key_size,	/*!< in: key's size. */
	int			min_size,	/*!< in: minimal group size. */
	int			size1,		/*!< in: size of group. */
	int			size2,		/*!< in: initial group sizes */
	double**		d_buffer,	/*!< in/out: buffer. */
	int			n_dim,		/*!< in: dimensions. */
	uchar*			first_rec)	/*!< in: the first rec. */
{
	rtr_split_node_t*	cur;
	rtr_split_node_t*	a = NULL;
	rtr_split_node_t*	b = NULL;
	double*			g1 = reserve_coords(d_buffer, n_dim);
	double*			g2 = reserve_coords(d_buffer, n_dim);
	rtr_split_node_t*	next = NULL;
	int			next_node = 0;
	int			i;
	int			first_rec_group = 1;
	rtr_split_node_t*	end = node + n_entries;

	if (all_size < min_size * 2) {
		return 1;
	}

	cur = node;
	for (; cur < end; ++cur) {
		cur->square = count_square(cur->coords, n_dim);
		cur->n_node = 0;
	}

	pick_seeds(node, n_entries, &a, &b, n_dim);
	a->n_node = 1;
	b->n_node = 2;

	copy_coords(g1, a->coords, n_dim);
	size1 += key_size;
	copy_coords(g2, b->coords, n_dim);
	size2 += key_size;

	for (i = n_entries - 2; i > 0; --i) {
		/* Can't write into group 2 */
		if (all_size - (size2 + key_size) < min_size) {
			mark_all_entries(node, n_entries, 1);
			break;
		}

		/* Can't write into group 1 */
		if (all_size - (size1 + key_size) < min_size) {
			mark_all_entries(node, n_entries, 2);
			break;
		}

		pick_next(node, n_entries, g1, g2, &next, &next_node, n_dim);
		if (next_node == 1) {
			size1 += key_size;
			mbr_join(g1, next->coords, n_dim);
		} else {
			size2 += key_size;
			mbr_join(g2, next->coords, n_dim);
		}

		next->n_node = next_node;

		/* Find out where the first rec (of the page) will be at,
		and inform the caller */
		if (first_rec && first_rec == next->key) {
			first_rec_group = next_node;
		}
	}

	return(first_rec_group);
}

/*************************************************************//**
Compares two keys a and b depending on nextflag
nextflag can contain these flags:
   MBR_INTERSECT(a,b)  a overlaps b
   MBR_CONTAIN(a,b)    a contains b
   MBR_DISJOINT(a,b)   a disjoint b
   MBR_WITHIN(a,b)     a within   b
   MBR_EQUAL(a,b)      All coordinates of MBRs are equal
Return 0 on success, otherwise 1. */
int
rtree_key_cmp(
/*==========*/
	page_cur_mode_t	mode,	/*!< in: compare method. */
	const uchar*	b,	/*!< in: first key. */
	int		b_len,	/*!< in: first key len. */
	const uchar*	a,	/*!< in: second key. */
	int		a_len)	/*!< in: second key len. */
{
	double		amin, amax, bmin, bmax;
	int		key_len;
	int		keyseg_len;

	keyseg_len = 2 * sizeof(double);
	for (key_len = a_len; key_len > 0; key_len -= keyseg_len) {
		amin = mach_double_read(a);
		bmin = mach_double_read(b);
		amax = mach_double_read(a + sizeof(double));
		bmax = mach_double_read(b + sizeof(double));

		switch (mode) {
		case PAGE_CUR_INTERSECT:
			if (INTERSECT_CMP(amin, amax, bmin, bmax)) {
				return(1);
			}
			break;
		case PAGE_CUR_CONTAIN:
			if (CONTAIN_CMP(amin, amax, bmin, bmax)) {
				return(1);
			}
			break;
		case PAGE_CUR_WITHIN:
			if (WITHIN_CMP(amin, amax, bmin, bmax)) {
				return(1);
			}
			break;
		case PAGE_CUR_MBR_EQUAL:
			if (EQUAL_CMP(amin, amax, bmin, bmax)) {
				return(1);
			}
			break;
		case PAGE_CUR_DISJOINT:
			int result;

			result = DISJOINT_CMP(amin, amax, bmin, bmax);
			if (result == 0) {
				return(0);
			}

			if (key_len - keyseg_len <= 0) {
				return(1);
			}

			break;
		default:
			/* if unknown comparison operator */
			ut_ad(0);
		}

		a += keyseg_len;
		b += keyseg_len;
	}

	return(0);
}

/*************************************************************//**
Calculates MBR_AREA(a+b) - MBR_AREA(a)
Note: when 'a' and 'b' objects are far from each other,
the area increase can be really big, so this function
can return 'inf' as a result.
Return the area increaed. */
double
rtree_area_increase(
	const uchar*	a,		/*!< in: original mbr. */
	const uchar*	b,		/*!< in: new mbr. */
	int		mbr_len,	/*!< in: mbr length of a and b. */
	double*		ab_area)	/*!< out: increased area. */
{
	double		a_area = 1.0;
	double		loc_ab_area = 1.0;
	double		amin, amax, bmin, bmax;
	int		key_len;
	int		keyseg_len;
	double		data_round = 1.0;

	keyseg_len = 2 * sizeof(double);

	for (key_len = mbr_len; key_len > 0; key_len -= keyseg_len) {
		double	area;

		amin = mach_double_read(a);
		bmin = mach_double_read(b);
		amax = mach_double_read(a + sizeof(double));
		bmax = mach_double_read(b + sizeof(double));

		area = amax - amin;
		if (area == 0) {
			a_area *= LINE_MBR_WEIGHTS;
		} else {
			a_area *= area;
		}

		area = (double)std::max(amax, bmax) -
		       (double)std::min(amin, bmin);
		if (area == 0) {
			loc_ab_area *= LINE_MBR_WEIGHTS;
		} else {
			loc_ab_area *= area;
		}

		/* Value of amax or bmin can be so large that small difference
		are ignored. For example: 3.2884281489988079e+284 - 100 =
		3.2884281489988079e+284. This results some area difference
		are not detected */
		if (loc_ab_area == a_area) {
			if (bmin < amin || bmax > amax) {
				data_round *= ((double)std::max(amax, bmax)
					       - amax
					       + (amin - (double)std::min(
								amin, bmin)));
			} else {
				data_round *= area;
			}
		}

		a += keyseg_len;
		b += keyseg_len;
	}

	*ab_area = loc_ab_area;

	if (loc_ab_area == a_area && data_round != 1.0) {
		return(data_round);
	}

	return(loc_ab_area - a_area);
}

/** Calculates overlapping area
@param[in]	a	mbr a
@param[in]	b	mbr b
@param[in]	mbr_len	mbr length
@return overlapping area */
double
rtree_area_overlapping(
	const uchar*	a,
	const uchar*	b,
	int		mbr_len)
{
	double	area = 1.0;
	double	amin;
	double	amax;
	double	bmin;
	double	bmax;
	int	key_len;
	int	keyseg_len;

	keyseg_len = 2 * sizeof(double);

	for (key_len = mbr_len; key_len > 0; key_len -= keyseg_len) {
		amin = mach_double_read(a);
		bmin = mach_double_read(b);
		amax = mach_double_read(a + sizeof(double));
		bmax = mach_double_read(b + sizeof(double));

		amin = std::max(amin, bmin);
		amax = std::min(amax, bmax);

		if (amin > amax) {
			return(0);
		} else {
			area *= (amax - amin);
		}

		a += keyseg_len;
		b += keyseg_len;
	}

	return(area);
}

/** Get the wkb of default POINT value, which represents POINT(0 0)
if it's of dimension 2, etc.
@param[in]	n_dims		dimensions
@param[out]	wkb		wkb buffer for default POINT
@param[in]	len		length of wkb buffer
@return non-0 indicate the length of wkb of the default POINT,
0 if the buffer is too small */
uint
get_wkb_of_default_point(
	uint	n_dims,
	uchar*	wkb,
	uint	len)
{
	if (len < GEOM_HEADER_SIZE + sizeof(double) * n_dims) {
		return(0);
	}

	/** POINT wkb comprises SRID, wkb header(byte order and type)
	and coordinates of the POINT */
	len = GEOM_HEADER_SIZE + sizeof(double) * n_dims;
	/** We always use 0 as default coordinate */
	memset(wkb, 0, len);
	/** We don't need to write SRID, write 0x01 for Byte Order */
	mach_write_to_n_little_endian(wkb + SRID_SIZE, 1, 0x01);
	/** Write wkbType::wkbPoint for the POINT type */
	mach_write_to_n_little_endian(wkb + SRID_SIZE + 1, 4, wkbPoint);

	return(len);
}

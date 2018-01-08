/*****************************************************************************

Copyright (c) 2013, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file gis/gis0geo.cc
InnoDB R-tree related functions.

Created 2013/03/27 Allen Lai and Jimmy Yang
*******************************************************/

#include <cmath>
#include "page0cur.h"

namespace dd {
class Spatial_reference_system;
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
	int			n_dim,		/*!< in: dimensions. */
	const dd::Spatial_reference_system*	srs) /*!< in: SRS of R-tree */
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
			d = mbr_join_area(srs, cur1->coords, cur2->coords,
				n_dim) - cur1->square - cur2->square;
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
	int			n_dim,		/*!< in: dimensions. */
	const dd::Spatial_reference_system*	srs) /*!< in: SRS of R-tree */
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

		diff = mbr_join_area(srs, g1, cur->coords, n_dim) -
		       mbr_join_area(srs, g2, cur->coords, n_dim);

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
	uchar*			first_rec,	/*!< in: the first rec. */
	const dd::Spatial_reference_system*	srs) /*!< in: SRS of R-tree */
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
		cur->square = compute_area(srs, cur->coords, n_dim);
		cur->n_node = 0;
	}

	pick_seeds(node, n_entries, &a, &b, n_dim, srs);
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

		pick_next(node, n_entries, g1, g2, &next, &next_node, n_dim,
			  srs);
		if (next_node == 1) {
			size1 += key_size;
			mbr_join(srs, g1, next->coords, n_dim);
		} else {
			size2 += key_size;
			mbr_join(srs, g2, next->coords, n_dim);
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

/** Compares two keys a and b depending on nextflag
nextflag can contain these flags:
   MBR_INTERSECT(a,b)  a overlaps b
   MBR_CONTAIN(a,b)    a contains b
   MBR_DISJOINT(a,b)   a disjoint b
   MBR_WITHIN(a,b)     a within   b
   MBR_EQUAL(a,b)      All coordinates of MBRs are equal
@param[in]	mode	compare method
@param[in]	a	first key
@param[in]	a_len	first key len
@param[in]	b	second key
@param[in]	b_len	second_key_len
@param[in]	srs	Spatial reference system of R-tree
@retval 0 on success, otherwise 1. */
int
rtree_key_cmp(
	page_cur_mode_t	mode,
	const uchar*	a,
	int		a_len,
	const uchar*	b,
	int		b_len,
	const dd::Spatial_reference_system*	srs)
{
	rtr_mbr_t	x, y;

	/* Dimension length. */
	uint	dim_len = SPDIMS * sizeof(double);

	x.xmin = mach_double_read(a);
	y.xmin = mach_double_read(b);
	x.xmax = mach_double_read(a + sizeof(double));
	y.xmax = mach_double_read(b + sizeof(double));

	x.ymin = mach_double_read(a + dim_len);
	y.ymin = mach_double_read(b + dim_len);
	x.ymax = mach_double_read(a + dim_len + sizeof(double));
	y.ymax = mach_double_read(b + dim_len + sizeof(double));

	switch (mode) {
	case PAGE_CUR_INTERSECT:
		if (mbr_intersect_cmp(srs, &x, &y)) {
			return(0);
		}
		break;
	case PAGE_CUR_CONTAIN:
		if (mbr_contain_cmp(srs, &x, &y)) {
			return(0);
		}
		break;
	case PAGE_CUR_WITHIN:
		if (mbr_within_cmp(srs, &x, &y)) {
			return(0);
		}
		break;
	case PAGE_CUR_MBR_EQUAL:
		if (mbr_equal_cmp(srs, &x, &y)) {
			return(0);
		}
		break;
	case PAGE_CUR_DISJOINT:
		if (!mbr_disjoint_cmp(srs, &x, &y)
		    || (b_len - (2 * dim_len) > 0)) {
			return(0);
		}
		break;
	default:
		/* if unknown comparison operator */
		ut_ad(0);
	}

	return(1);
}

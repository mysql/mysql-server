/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************************//**
@file include/fut0lst.h
File-based list utilities

Created 11/28/1995 Heikki Tuuri
***********************************************************************/

#ifndef fut0lst_h
#define fut0lst_h

#include "univ.i"

#include "fil0fil.h"
#include "mtr0mtr.h"


/* The C 'types' of base node and list node: these should be used to
write self-documenting code. Of course, the sizeof macro cannot be
applied to these types! */

typedef	byte	flst_base_node_t;
typedef	byte	flst_node_t;

/* The physical size of a list base node in bytes */
#define	FLST_BASE_NODE_SIZE	(4 + 2 * FIL_ADDR_SIZE)

/* The physical size of a list node in bytes */
#define	FLST_NODE_SIZE		(2 * FIL_ADDR_SIZE)

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Initializes a list base node. */
UNIV_INLINE
void
flst_init(
/*======*/
	flst_base_node_t*	base,	/*!< in: pointer to base node */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Adds a node as the last node in a list. */
UNIV_INTERN
void
flst_add_last(
/*==========*/
	flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	flst_node_t*		node,	/*!< in: node to add */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Adds a node as the first node in a list. */
UNIV_INTERN
void
flst_add_first(
/*===========*/
	flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	flst_node_t*		node,	/*!< in: node to add */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Inserts a node after another in a list. */
UNIV_INTERN
void
flst_insert_after(
/*==============*/
	flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	flst_node_t*		node1,	/*!< in: node to insert after */
	flst_node_t*		node2,	/*!< in: node to add */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Inserts a node before another in a list. */
UNIV_INTERN
void
flst_insert_before(
/*===============*/
	flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	flst_node_t*		node2,	/*!< in: node to insert */
	flst_node_t*		node3,	/*!< in: node to insert before */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Removes a node. */
UNIV_INTERN
void
flst_remove(
/*========*/
	flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	flst_node_t*		node2,	/*!< in: node to remove */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Cuts off the tail of the list, including the node given. The number of
nodes which will be removed must be provided by the caller, as this function
does not measure the length of the tail. */
UNIV_INTERN
void
flst_cut_end(
/*=========*/
	flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	flst_node_t*		node2,	/*!< in: first node to remove */
	ulint			n_nodes,/*!< in: number of nodes to remove,
					must be >= 1 */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Cuts off the tail of the list, not including the given node. The number of
nodes which will be removed must be provided by the caller, as this function
does not measure the length of the tail. */
UNIV_INTERN
void
flst_truncate_end(
/*==============*/
	flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	flst_node_t*		node2,	/*!< in: first node not to remove */
	ulint			n_nodes,/*!< in: number of nodes to remove */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Gets list length.
@return	length */
UNIV_INLINE
ulint
flst_get_len(
/*=========*/
	const flst_base_node_t*	base,	/*!< in: pointer to base node */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Gets list first node address.
@return	file address */
UNIV_INLINE
fil_addr_t
flst_get_first(
/*===========*/
	const flst_base_node_t*	base,	/*!< in: pointer to base node */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Gets list last node address.
@return	file address */
UNIV_INLINE
fil_addr_t
flst_get_last(
/*==========*/
	const flst_base_node_t*	base,	/*!< in: pointer to base node */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Gets list next node address.
@return	file address */
UNIV_INLINE
fil_addr_t
flst_get_next_addr(
/*===============*/
	const flst_node_t*	node,	/*!< in: pointer to node */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Gets list prev node address.
@return	file address */
UNIV_INLINE
fil_addr_t
flst_get_prev_addr(
/*===============*/
	const flst_node_t*	node,	/*!< in: pointer to node */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Writes a file address. */
UNIV_INLINE
void
flst_write_addr(
/*============*/
	fil_faddr_t*	faddr,	/*!< in: pointer to file faddress */
	fil_addr_t	addr,	/*!< in: file address */
	mtr_t*		mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Reads a file address.
@return	file address */
UNIV_INLINE
fil_addr_t
flst_read_addr(
/*===========*/
	const fil_faddr_t*	faddr,	/*!< in: pointer to file faddress */
	mtr_t*			mtr);	/*!< in: mini-transaction handle */
/********************************************************************//**
Validates a file-based list.
@return	TRUE if ok */
UNIV_INTERN
ibool
flst_validate(
/*==========*/
	const flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	mtr_t*			mtr1);	/*!< in: mtr */
/********************************************************************//**
Prints info of a file-based list. */
UNIV_INTERN
void
flst_print(
/*=======*/
	const flst_base_node_t*	base,	/*!< in: pointer to base node of list */
	mtr_t*			mtr);	/*!< in: mtr */


#ifndef UNIV_NONINL
#include "fut0lst.ic"
#endif

#endif /* !UNIV_HOTBACKUP */

#endif

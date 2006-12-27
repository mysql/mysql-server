/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
 * @file ndb_nodegroup_map.h
 *
 * Declarations of data types for node group map
 */

#ifndef NDB_NODEGROUP_MAP_H
#define NDB_NODEGROUP_MAP_H

#define MAX_MAPS_PER_NODE_GROUP 4
#define MAX_NODE_GROUP_MAPS 128
typedef struct node_group_map
{
  uint no_maps;
  uint curr_index;
  uint16 map_array[MAX_MAPS_PER_NODE_GROUP];
} NODE_GROUP_MAP;

#endif

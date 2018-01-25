/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XCOM_DETECTOR_H
#define XCOM_DETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#define DETECTOR_LIVE_TIMEOUT 5.0

typedef double	detector_state[NSERVERS];
struct site_def;

void note_detected(struct site_def const *site, node_no node);
int	may_be_dead(detector_state const ds, node_no i, double seconds);
void init_detector(detector_state ds);
void invalidate_detector_sites(struct site_def *site);
void update_xcom_id(node_no node, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif


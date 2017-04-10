/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_DEFAULTS_H
#define PFS_DEFAULTS_H

/**
  @file storage/perfschema/pfs_defaults.h
  Default setup (declarations).
*/

#include "mysql/psi/psi_thread.h"

/**
  Configure the performance schema setup tables with default content.
  The tables populated are:
  - SETUP_ACTORS
  - SETUP_OBJECTS
*/
void install_default_setup(PSI_thread_bootstrap *thread_boot);

#endif

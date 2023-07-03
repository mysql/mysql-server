/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file mysql/psi/psi_abi_v1.h
  ABI check for mysql/psi/psi.h, when using PSI_VERSION_2.
  This file is only used to automate detection of changes between versions.
  Do not include this file, include mysql/psi/psi.h instead.
*/
#define USE_PSI_2
#define HAVE_PSI_INTERFACE
#define MY_GLOBAL_INCLUDED
#include "mysql/psi/psi.h"


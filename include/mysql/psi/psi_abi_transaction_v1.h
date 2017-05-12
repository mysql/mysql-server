/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

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
  @file include/mysql/psi/psi_abi_transaction_v1.h
  ABI check for mysql/psi/psi_transaction.h, when using
  PSI_TRANSACTION_VERSION_1.
  This file is only used to automate detection of changes between versions.
  Do not include this file, include mysql/psi/psi_transaction.h instead.
*/
#define USE_PSI_TRANSACTION_1
#define HAVE_PSI_INTERFACE
#define MY_GLOBAL_INCLUDED
#define MY_PSI_CONFIG_INCLUDED
#include "mysql/psi/psi_transaction.h"

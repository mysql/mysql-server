/* Copyright (c) 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef AUDIT_LOG_PRUNE_ADAPT_BITS_H
#define AUDIT_LOG_PRUNE_ADAPT_BITS_H

#include <cstddef>  // size_t

/**
  Adjust pruning cycle to be slower than rotation on time, both:
  - internal, defined via rotate_on_time
  - external, if some app uses rotated files with a given period

  @param external_rotate_cycle External cycle in seconds to adjust to.
*/
typedef void (*audit_log_prune_adapt_v1_t)(size_t external_rotate_cycle);

#endif /* AUDIT_LOG_PRUNE_ADAPT_BITS_H */

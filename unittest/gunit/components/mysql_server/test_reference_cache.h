/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
#include <mysql/components/service.h>
#ifndef TEST_REFERENCE_CACHE_MYSQL_TEST_FOO
#define TEST_REFERENCE_CACHE_MYSQL_TEST_FOO
/** @file components/test/test_reference_cache_foo.h */
/** a test service to test the reference caching on */

BEGIN_SERVICE_DEFINITION(mysql_test_foo)
DECLARE_BOOL_METHOD(emit, (int arg));
END_SERVICE_DEFINITION(mysql_test_foo)

BEGIN_SERVICE_DEFINITION(test_ref_cache_producer)
DECLARE_BOOL_METHOD(mysql_test_ref_cache_produce_event, (int arg));
DECLARE_BOOL_METHOD(mysql_test_ref_cache_flush, ());
DECLARE_BOOL_METHOD(mysql_test_ref_cache_release_cache, ());
DECLARE_BOOL_METHOD(mysql_test_ref_cache_benchmark_run, (int, int, int, int));
DECLARE_BOOL_METHOD(mysql_test_ref_cache_benchmark_kill, ());
END_SERVICE_DEFINITION(test_ref_cache_producer)

BEGIN_SERVICE_DEFINITION(test_ref_cache_consumer)
DECLARE_BOOL_METHOD(mysql_test_ref_cache_consumer_counter_reset, ());
DECLARE_BOOL_METHOD(mysql_test_ref_cache_consumer_counter_get, ());
END_SERVICE_DEFINITION(test_ref_cache_consumer)

#endif /* TEST_REFERENCE_CACHE_MYSQL_TEST_FOO */

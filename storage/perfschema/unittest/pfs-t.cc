/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include <my_pthread.h>
#include <pfs_server.h>
#include <pfs_instr_class.h>
#include <pfs_instr.h>
#include <pfs_global.h>
#include <tap.h>

#include <string.h>
#include <memory.h>

#include "stub_print_error.h"
#include "stub_server_misc.h"

/* test helpers, to simulate the setup */

void setup_thread(PSI_thread *t, bool enabled)
{
  PFS_thread *t2= (PFS_thread*) t;
  t2->m_enabled= enabled;
}

/* test helpers, to inspect data */

PFS_file* lookup_file_by_name(const char* name)
{
  uint i;
  PFS_file *pfs;
  uint len= strlen(name);
  size_t dirlen;
  const char *filename;
  uint filename_length;;

  for (i= 0; i < file_max; i++)
  {
    pfs= & file_array[i];
    if (pfs->m_lock.is_populated())
    {
      /*
        When a file "foo" is instrumented, the name is normalized
        to "/path/to/current/directory/foo", so we remove the
        directory name here to find it back.
      */
      dirlen= dirname_length(pfs->m_filename);
      filename= pfs->m_filename + dirlen;
      filename_length= pfs->m_filename_length - dirlen;
      if ((len == filename_length) &&
          (strncmp(name, filename, filename_length) == 0))
        return pfs;
    }
  }

  return NULL;
}

/* tests */

void test_bootstrap()
{
  void *psi;
  void *psi_2;
  PSI_bootstrap *boot;
  PFS_global_param param;

  diag("test_bootstrap");

  param.m_enabled= true;
  param.m_mutex_class_sizing= 0;
  param.m_rwlock_class_sizing= 0;
  param.m_cond_class_sizing= 0;
  param.m_thread_class_sizing= 0;
  param.m_table_share_sizing= 0;
  param.m_file_class_sizing= 0;
  param.m_mutex_sizing= 0;
  param.m_rwlock_sizing= 0;
  param.m_cond_sizing= 0;
  param.m_thread_sizing= 0;
  param.m_table_sizing= 0;
  param.m_file_sizing= 0;
  param.m_file_handle_sizing= 0;
  param.m_events_waits_history_sizing= 0;
  param.m_events_waits_history_long_sizing= 0;

  boot= initialize_performance_schema(& param);
  ok(boot != NULL, "boot");
  ok(boot->get_interface != NULL, "boot->get_interface");

  psi= boot->get_interface(0);
  ok(psi == NULL, "no version 0");

  psi= boot->get_interface(PSI_VERSION_1);
  ok(psi != NULL, "version 1");

  psi_2= boot->get_interface(PSI_VERSION_2);
  ok(psi_2 == NULL, "version 2");

  shutdown_performance_schema();
}

/*
  Not a test, helper for testing pfs.cc
*/
PSI * load_perfschema()
{
  void *psi;
  PSI_bootstrap *boot;
  PFS_global_param param;

  param.m_enabled= true;
  param.m_mutex_class_sizing= 10;
  param.m_rwlock_class_sizing= 10;
  param.m_cond_class_sizing= 10;
  param.m_thread_class_sizing= 10;
  param.m_table_share_sizing= 10;
  param.m_file_class_sizing= 10;
  param.m_mutex_sizing= 10;
  param.m_rwlock_sizing= 10;
  param.m_cond_sizing= 10;
  param.m_thread_sizing= 10;
  param.m_table_sizing= 10;
  param.m_file_sizing= 10;
  param.m_file_handle_sizing= 50;
  param.m_events_waits_history_sizing= 10;
  param.m_events_waits_history_long_sizing= 10;

  /* test_bootstrap() covered this, assuming it just works */
  boot= initialize_performance_schema(& param);
  psi= boot->get_interface(PSI_VERSION_1);

  return (PSI*) psi;
}

void test_bad_registration()
{
  PSI *psi;

  diag("test_bad_registration");

  psi= load_perfschema();

  /*
    Test that length('wait/synch/mutex/' (17) + category + '/' (1)) < 32
    --> category can be up to 13 chars for a mutex.
  */

  PSI_mutex_key dummy_mutex_key= 9999;
  PSI_mutex_info bad_mutex_1[]=
  {
    { & dummy_mutex_key, "X", 0}
  };

  psi->register_mutex("/", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key= 9999;
  psi->register_mutex("a/", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key= 9999;
  psi->register_mutex("/b", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key= 9999;
  psi->register_mutex("a/b", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key= 9999;
  psi->register_mutex("12345678901234", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key= 9999;
  psi->register_mutex("1234567890123", bad_mutex_1, 1);
  ok(dummy_mutex_key == 1, "assigned key");

  /*
    Test that length('wait/synch/mutex/' (17) + category + '/' (1) + name) <= 128
    --> category + name can be up to 110 chars for a mutex.
  */

  dummy_mutex_key= 9999;
  PSI_mutex_info bad_mutex_2[]=
  {
    { & dummy_mutex_key,
      /* 110 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "1234567890",
      0}
  };

  psi->register_mutex("X", bad_mutex_2, 1);
  ok(dummy_mutex_key == 0, "zero key");

  dummy_mutex_key= 9999;
  PSI_mutex_info bad_mutex_3[]=
  {
    { & dummy_mutex_key,
      /* 109 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "123456789",
      0}
  };

  psi->register_mutex("XX", bad_mutex_3, 1);
  ok(dummy_mutex_key == 0, "zero key");

  psi->register_mutex("X", bad_mutex_3, 1);
  ok(dummy_mutex_key == 2, "assigned key");

  /*
    Test that length('wait/synch/rwlock/' (18) + category + '/' (1)) < 32
    --> category can be up to 12 chars for a rwlock.
  */

  PSI_rwlock_key dummy_rwlock_key= 9999;
  PSI_rwlock_info bad_rwlock_1[]=
  {
    { & dummy_rwlock_key, "X", 0}
  };

  psi->register_rwlock("/", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key= 9999;
  psi->register_rwlock("a/", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key= 9999;
  psi->register_rwlock("/b", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key= 9999;
  psi->register_rwlock("a/b", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key= 9999;
  psi->register_rwlock("1234567890123", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key= 9999;
  psi->register_rwlock("123456789012", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 1, "assigned key");

  /*
    Test that length('wait/synch/rwlock/' (18) + category + '/' (1) + name) <= 128
    --> category + name can be up to 109 chars for a rwlock.
  */

  dummy_rwlock_key= 9999;
  PSI_rwlock_info bad_rwlock_2[]=
  {
    { & dummy_rwlock_key,
      /* 109 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "123456789",
      0}
  };

  psi->register_rwlock("X", bad_rwlock_2, 1);
  ok(dummy_rwlock_key == 0, "zero key");

  dummy_rwlock_key= 9999;
  PSI_rwlock_info bad_rwlock_3[]=
  {
    { & dummy_rwlock_key,
      /* 108 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "12345678",
      0}
  };

  psi->register_rwlock("XX", bad_rwlock_3, 1);
  ok(dummy_rwlock_key == 0, "zero key");

  psi->register_rwlock("X", bad_rwlock_3, 1);
  ok(dummy_rwlock_key == 2, "assigned key");

  /*
    Test that length('wait/synch/cond/' (16) + category + '/' (1)) < 32
    --> category can be up to 14 chars for a cond.
  */

  PSI_cond_key dummy_cond_key= 9999;
  PSI_cond_info bad_cond_1[]=
  {
    { & dummy_cond_key, "X", 0}
  };

  psi->register_cond("/", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key= 9999;
  psi->register_cond("a/", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key= 9999;
  psi->register_cond("/b", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key= 9999;
  psi->register_cond("a/b", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key= 9999;
  psi->register_cond("123456789012345", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key= 9999;
  psi->register_cond("12345678901234", bad_cond_1, 1);
  ok(dummy_cond_key == 1, "assigned key");

  /*
    Test that length('wait/synch/cond/' (16) + category + '/' (1) + name) <= 128
    --> category + name can be up to 111 chars for a cond.
  */

  dummy_cond_key= 9999;
  PSI_cond_info bad_cond_2[]=
  {
    { & dummy_cond_key,
      /* 111 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "12345678901",
      0}
  };

  psi->register_cond("X", bad_cond_2, 1);
  ok(dummy_cond_key == 0, "zero key");

  dummy_cond_key= 9999;
  PSI_cond_info bad_cond_3[]=
  {
    { & dummy_cond_key,
      /* 110 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "1234567890",
      0}
  };

  psi->register_cond("XX", bad_cond_3, 1);
  ok(dummy_cond_key == 0, "zero key");

  psi->register_cond("X", bad_cond_3, 1);
  ok(dummy_cond_key == 2, "assigned key");

  /*
    Test that length('thread/' (7) + category + '/' (1)) < 32
    --> category can be up to 23 chars for a thread.
  */

  PSI_thread_key dummy_thread_key= 9999;
  PSI_thread_info bad_thread_1[]=
  {
    { & dummy_thread_key, "X", 0}
  };

  psi->register_thread("/", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key= 9999;
  psi->register_thread("a/", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key= 9999;
  psi->register_thread("/b", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key= 9999;
  psi->register_thread("a/b", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key= 9999;
  psi->register_thread("123456789012345678901234", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key= 9999;
  psi->register_thread("12345678901234567890123", bad_thread_1, 1);
  ok(dummy_thread_key == 1, "assigned key");

  /*
    Test that length('thread/' (7) + category + '/' (1) + name) <= 128
    --> category + name can be up to 120 chars for a thread.
  */

  dummy_thread_key= 9999;
  PSI_thread_info bad_thread_2[]=
  {
    { & dummy_thread_key,
      /* 120 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890",
      0}
  };

  psi->register_thread("X", bad_thread_2, 1);
  ok(dummy_thread_key == 0, "zero key");

  dummy_thread_key= 9999;
  PSI_thread_info bad_thread_3[]=
  {
    { & dummy_thread_key,
      /* 119 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "1234567890123456789",
      0}
  };

  psi->register_thread("XX", bad_thread_3, 1);
  ok(dummy_thread_key == 0, "zero key");

  psi->register_thread("X", bad_thread_3, 1);
  ok(dummy_thread_key == 2, "assigned key");

  /*
    Test that length('wait/io/file/' (13) + category + '/' (1)) < 32
    --> category can be up to 17 chars for a file.
  */

  PSI_file_key dummy_file_key= 9999;
  PSI_file_info bad_file_1[]=
  {
    { & dummy_file_key, "X", 0}
  };

  psi->register_file("/", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key= 9999;
  psi->register_file("a/", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key= 9999;
  psi->register_file("/b", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key= 9999;
  psi->register_file("a/b", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key= 9999;
  psi->register_file("123456789012345678", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key= 9999;
  psi->register_file("12345678901234567", bad_file_1, 1);
  ok(dummy_file_key == 1, "assigned key");

  /*
    Test that length('wait/io/file/' (13) + category + '/' (1) + name) <= 128
    --> category + name can be up to 114 chars for a file.
  */

  dummy_file_key= 9999;
  PSI_file_info bad_file_2[]=
  {
    { & dummy_file_key,
      /* 114 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "12345678901234",
      0}
  };

  psi->register_file("X", bad_file_2, 1);
  ok(dummy_file_key == 0, "zero key");

  dummy_file_key= 9999;
  PSI_file_info bad_file_3[]=
  {
    { & dummy_file_key,
      /* 113 chars name */
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "1234567890123",
      0}
  };

  psi->register_file("XX", bad_file_3, 1);
  ok(dummy_file_key == 0, "zero key");

  psi->register_file("X", bad_file_3, 1);
  ok(dummy_file_key == 2, "assigned key");

  shutdown_performance_schema();
}

void test_init_disabled()
{
  PSI *psi;

  diag("test_init_disabled");

  psi= load_perfschema();

  PSI_mutex_key mutex_key_A;
  PSI_mutex_info all_mutex[]=
  {
    { & mutex_key_A, "M-A", 0}
  };

  PSI_rwlock_key rwlock_key_A;
  PSI_rwlock_info all_rwlock[]=
  {
    { & rwlock_key_A, "RW-A", 0}
  };

  PSI_cond_key cond_key_A;
  PSI_cond_info all_cond[]=
  {
    { & cond_key_A, "C-A", 0}
  };

  PSI_file_key file_key_A;
  PSI_file_info all_file[]=
  {
    { & file_key_A, "F-A", 0}
  };

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[]=
  {
    { & thread_key_1, "T-1", 0}
  };

  psi->register_mutex("test", all_mutex, 1);
  psi->register_rwlock("test", all_rwlock, 1);
  psi->register_cond("test", all_cond, 1);
  psi->register_file("test", all_file, 1);
  psi->register_thread("test", all_thread, 1);

  PFS_mutex_class *mutex_class_A;
  PFS_rwlock_class *rwlock_class_A;
  PFS_cond_class *cond_class_A;
  PFS_file_class *file_class_A;
  PSI_mutex *mutex_A1;
  PSI_rwlock *rwlock_A1;
  PSI_cond *cond_A1;
  PFS_file *file_A1;
  PSI_thread *thread_1;

  /* Preparation */

  thread_1= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread_1 != NULL, "T-1");
  psi->set_thread_id(thread_1, 1);

  mutex_class_A= find_mutex_class(mutex_key_A);
  ok(mutex_class_A != NULL, "mutex class A");

  rwlock_class_A= find_rwlock_class(rwlock_key_A);
  ok(rwlock_class_A != NULL, "rwlock class A");

  cond_class_A= find_cond_class(cond_key_A);
  ok(cond_class_A != NULL, "cond class A");

  file_class_A= find_file_class(file_key_A);
  ok(file_class_A != NULL, "file class A");

  /* Pretend thread T-1 is running, and disabled */
  /* ------------------------------------------- */

  psi->set_thread(thread_1);
  setup_thread(thread_1, false);

  /* disabled M-A + disabled T-1: no instrumentation */

  mutex_class_A->m_enabled= false;
  mutex_A1= psi->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* enabled M-A + disabled T-1: no instrumentation */

  mutex_class_A->m_enabled= true;
  mutex_A1= psi->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* broken key + disabled T-1: no instrumentation */

  mutex_class_A->m_enabled= true;
  mutex_A1= psi->init_mutex(0, NULL);
  ok(mutex_A1 == NULL, "not instrumented");
  mutex_A1= psi->init_mutex(99, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* disabled RW-A + disabled T-1: no instrumentation */

  rwlock_class_A->m_enabled= false;
  rwlock_A1= psi->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* enabled RW-A + disabled T-1: no instrumentation */

  rwlock_class_A->m_enabled= true;
  rwlock_A1= psi->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* broken key + disabled T-1: no instrumentation */

  rwlock_class_A->m_enabled= true;
  rwlock_A1= psi->init_rwlock(0, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");
  rwlock_A1= psi->init_rwlock(99, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* disabled C-A + disabled T-1: no instrumentation */

  cond_class_A->m_enabled= false;
  cond_A1= psi->init_cond(cond_key_A, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* enabled C-A + disabled T-1: no instrumentation */

  cond_class_A->m_enabled= true;
  cond_A1= psi->init_cond(cond_key_A, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* broken key + disabled T-1: no instrumentation */

  cond_class_A->m_enabled= true;
  cond_A1= psi->init_cond(0, NULL);
  ok(cond_A1 == NULL, "not instrumented");
  cond_A1= psi->init_cond(99, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* disabled F-A + disabled T-1: no instrumentation */

  file_class_A->m_enabled= false;
  psi->create_file(file_key_A, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* enabled F-A + disabled T-1: no instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(file_key_A, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* broken key + disabled T-1: no instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(0, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");
  psi->create_file(99, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* Pretend thread T-1 is enabled */
  /* ----------------------------- */

  setup_thread(thread_1, true);

  /* disabled M-A + enabled T-1: no instrumentation */

  mutex_class_A->m_enabled= false;
  mutex_A1= psi->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* enabled M-A + enabled T-1: instrumentation */

  mutex_class_A->m_enabled= true;
  mutex_A1= psi->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "instrumented");
  psi->destroy_mutex(mutex_A1);

  /* broken key + enabled T-1: no instrumentation */

  mutex_class_A->m_enabled= true;
  mutex_A1= psi->init_mutex(0, NULL);
  ok(mutex_A1 == NULL, "not instrumented");
  mutex_A1= psi->init_mutex(99, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* disabled RW-A + enabled T-1: no instrumentation */

  rwlock_class_A->m_enabled= false;
  rwlock_A1= psi->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* enabled RW-A + enabled T-1: instrumentation */

  rwlock_class_A->m_enabled= true;
  rwlock_A1= psi->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "instrumented");
  psi->destroy_rwlock(rwlock_A1);

  /* broken key + enabled T-1: no instrumentation */

  rwlock_class_A->m_enabled= true;
  rwlock_A1= psi->init_rwlock(0, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");
  rwlock_A1= psi->init_rwlock(99, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* disabled C-A + enabled T-1: no instrumentation */

  cond_class_A->m_enabled= false;
  cond_A1= psi->init_cond(cond_key_A, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* enabled C-A + enabled T-1: instrumentation */

  cond_class_A->m_enabled= true;
  cond_A1= psi->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "instrumented");
  psi->destroy_cond(cond_A1);

  /* broken key + enabled T-1: no instrumentation */

  cond_class_A->m_enabled= true;
  cond_A1= psi->init_cond(0, NULL);
  ok(cond_A1 == NULL, "not instrumented");
  cond_A1= psi->init_cond(99, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* disabled F-A + enabled T-1: no instrumentation */

  file_class_A->m_enabled= false;
  psi->create_file(file_key_A, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* enabled F-A + open failed + enabled T-1: no instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(file_key_A, "foo", (File) -1);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* enabled F-A + out-of-descriptors + enabled T-1: no instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(file_key_A, "foo", (File) 65000);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");
  ok(file_handle_lost == 1, "lost a file handle");
  file_handle_lost= 0;

  /* enabled F-A + enabled T-1: instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(file_key_A, "foo-instrumented", (File) 12);
  file_A1= lookup_file_by_name("foo-instrumented");
  ok(file_A1 != NULL, "instrumented");

  /* broken key + enabled T-1: no instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(0, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");
  psi->create_file(99, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* Pretend the running thread is not instrumented */
  /* ---------------------------------------------- */

  psi->delete_current_thread();

  /* disabled M-A + unknown thread: no instrumentation */

  mutex_class_A->m_enabled= false;
  mutex_A1= psi->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* enabled M-A + unknown thread: no instrumentation */

  mutex_class_A->m_enabled= true;
  mutex_A1= psi->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* broken key + unknown thread: no instrumentation */

  mutex_class_A->m_enabled= true;
  mutex_A1= psi->init_mutex(0, NULL);
  ok(mutex_A1 == NULL, "not instrumented");
  mutex_A1= psi->init_mutex(99, NULL);
  ok(mutex_A1 == NULL, "not instrumented");

  /* disabled RW-A + unknown thread: no instrumentation */

  rwlock_class_A->m_enabled= false;
  rwlock_A1= psi->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* enabled RW-A + unknown thread: no instrumentation */

  rwlock_class_A->m_enabled= true;
  rwlock_A1= psi->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* broken key + unknown thread: no instrumentation */

  rwlock_class_A->m_enabled= true;
  rwlock_A1= psi->init_rwlock(0, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");
  rwlock_A1= psi->init_rwlock(99, NULL);
  ok(rwlock_A1 == NULL, "not instrumented");

  /* disabled C-A + unknown thread: no instrumentation */

  cond_class_A->m_enabled= false;
  cond_A1= psi->init_cond(cond_key_A, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* enabled C-A + unknown thread: no instrumentation */

  cond_class_A->m_enabled= true;
  cond_A1= psi->init_cond(cond_key_A, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* broken key + unknown thread: no instrumentation */

  cond_class_A->m_enabled= true;
  cond_A1= psi->init_cond(0, NULL);
  ok(cond_A1 == NULL, "not instrumented");
  cond_A1= psi->init_cond(99, NULL);
  ok(cond_A1 == NULL, "not instrumented");

  /* disabled F-A + unknown thread: no instrumentation */

  file_class_A->m_enabled= false;
  psi->create_file(file_key_A, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* enabled F-A + unknown thread: no instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(file_key_A, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* broken key + unknown thread: no instrumentation */

  file_class_A->m_enabled= true;
  psi->create_file(0, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");
  psi->create_file(99, "foo", (File) 12);
  file_A1= lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  shutdown_performance_schema();
}

void test_locker_disabled()
{
  PSI *psi;

  diag("test_locker_disabled");

  psi= load_perfschema();

  PSI_mutex_key mutex_key_A;
  PSI_mutex_info all_mutex[]=
  {
    { & mutex_key_A, "M-A", 0}
  };

  PSI_rwlock_key rwlock_key_A;
  PSI_rwlock_info all_rwlock[]=
  {
    { & rwlock_key_A, "RW-A", 0}
  };

  PSI_cond_key cond_key_A;
  PSI_cond_info all_cond[]=
  {
    { & cond_key_A, "C-A", 0}
  };

  PSI_file_key file_key_A;
  PSI_file_info all_file[]=
  {
    { & file_key_A, "F-A", 0}
  };

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[]=
  {
    { & thread_key_1, "T-1", 0}
  };

  psi->register_mutex("test", all_mutex, 1);
  psi->register_rwlock("test", all_rwlock, 1);
  psi->register_cond("test", all_cond, 1);
  psi->register_file("test", all_file, 1);
  psi->register_thread("test", all_thread, 1);

  PFS_mutex_class *mutex_class_A;
  PFS_rwlock_class *rwlock_class_A;
  PFS_cond_class *cond_class_A;
  PFS_file_class *file_class_A;
  PSI_mutex *mutex_A1;
  PSI_rwlock *rwlock_A1;
  PSI_cond *cond_A1;
  PSI_file *file_A1;
  PSI_thread *thread_1;

  /* Preparation */

  thread_1= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread_1 != NULL, "T-1");
  psi->set_thread_id(thread_1, 1);

  mutex_class_A= find_mutex_class(mutex_key_A);
  ok(mutex_class_A != NULL, "mutex info A");

  rwlock_class_A= find_rwlock_class(rwlock_key_A);
  ok(rwlock_class_A != NULL, "rwlock info A");

  cond_class_A= find_cond_class(cond_key_A);
  ok(cond_class_A != NULL, "cond info A");

  file_class_A= find_file_class(file_key_A);
  ok(file_class_A != NULL, "file info A");

  /* Pretend thread T-1 is running, and enabled */
  /* ------------------------------------------ */

  psi->set_thread(thread_1);
  setup_thread(thread_1, true);

  /* Enable all instruments, instantiate objects */

  mutex_class_A->m_enabled= true;
  mutex_A1= psi->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "instrumented");

  rwlock_class_A->m_enabled= true;
  rwlock_A1= psi->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "instrumented");

  cond_class_A->m_enabled= true;
  cond_A1= psi->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "instrumented");

  file_class_A->m_enabled= true;
  psi->create_file(file_key_A, "foo", (File) 12);
  file_A1= (PSI_file*) lookup_file_by_name("foo");
  ok(file_A1 != NULL, "instrumented");

  PSI_mutex_locker *mutex_locker;
  PSI_mutex_locker_state mutex_state;
  PSI_rwlock_locker *rwlock_locker;
  PSI_rwlock_locker_state rwlock_state;
  PSI_cond_locker *cond_locker;
  PSI_cond_locker_state cond_state;
  PSI_file_locker *file_locker;
  PSI_file_locker_state file_state;

  /* Pretend thread T-1 is disabled */
  /* ------------------------------ */

  setup_thread(thread_1, false);
  flag_events_waits_current= true;
  mutex_class_A->m_enabled= true;
  rwlock_class_A->m_enabled= true;
  cond_class_A->m_enabled= true;
  file_class_A->m_enabled= true;

  mutex_locker= psi->get_thread_mutex_locker(&mutex_state, mutex_A1, PSI_MUTEX_LOCK);
  ok(mutex_locker == NULL, "no locker");
  rwlock_locker= psi->get_thread_rwlock_locker(&rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK);
  ok(rwlock_locker == NULL, "no locker");
  cond_locker= psi->get_thread_cond_locker(&cond_state, cond_A1, mutex_A1, PSI_COND_WAIT);
  ok(cond_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_name_locker(&file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_stream_locker(&file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");

  /* Pretend the consumer is disabled */
  /* -------------------------------- */

  setup_thread(thread_1, true);
  flag_events_waits_current= false;
  mutex_class_A->m_enabled= true;
  rwlock_class_A->m_enabled= true;
  cond_class_A->m_enabled= true;
  file_class_A->m_enabled= true;

  mutex_locker= psi->get_thread_mutex_locker(&mutex_state, mutex_A1, PSI_MUTEX_LOCK);
  ok(mutex_locker == NULL, "no locker");
  rwlock_locker= psi->get_thread_rwlock_locker(&rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK);
  ok(rwlock_locker == NULL, "no locker");
  cond_locker= psi->get_thread_cond_locker(&cond_state, cond_A1, mutex_A1, PSI_COND_WAIT);
  ok(cond_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_name_locker(&file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_stream_locker(&file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");

  /* Pretend the instrument is disabled */
  /* ---------------------------------- */

  setup_thread(thread_1, true);
  flag_events_waits_current= true;
  mutex_class_A->m_enabled= false;
  rwlock_class_A->m_enabled= false;
  cond_class_A->m_enabled= false;
  file_class_A->m_enabled= false;

  mutex_locker= psi->get_thread_mutex_locker(&mutex_state, mutex_A1, PSI_MUTEX_LOCK);
  ok(mutex_locker == NULL, "no locker");
  rwlock_locker= psi->get_thread_rwlock_locker(&rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK);
  ok(rwlock_locker == NULL, "no locker");
  cond_locker= psi->get_thread_cond_locker(&cond_state, cond_A1, mutex_A1, PSI_COND_WAIT);
  ok(cond_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_name_locker(&file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_stream_locker(&file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");

  /* Pretend everything is enabled */
  /* ----------------------------- */

  setup_thread(thread_1, true);
  flag_events_waits_current= true;
  mutex_class_A->m_enabled= true;
  rwlock_class_A->m_enabled= true;
  cond_class_A->m_enabled= true;
  file_class_A->m_enabled= true;

  mutex_locker= psi->get_thread_mutex_locker(&mutex_state, mutex_A1, PSI_MUTEX_LOCK);
  ok(mutex_locker != NULL, "locker");
  psi->start_mutex_wait(mutex_locker, __FILE__, __LINE__);
  psi->end_mutex_wait(mutex_locker, 0);
  rwlock_locker= psi->get_thread_rwlock_locker(&rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK);
  ok(rwlock_locker != NULL, "locker");
  psi->start_rwlock_rdwait(rwlock_locker, __FILE__, __LINE__);
  psi->end_rwlock_rdwait(rwlock_locker, 0);
  cond_locker= psi->get_thread_cond_locker(&cond_state, cond_A1, mutex_A1, PSI_COND_WAIT);
  ok(cond_locker != NULL, "locker");
  psi->start_cond_wait(cond_locker, __FILE__, __LINE__);
  psi->end_cond_wait(cond_locker, 0);
  file_locker= psi->get_thread_file_name_locker(&file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker != NULL, "locker");
  psi->start_file_open_wait(file_locker, __FILE__, __LINE__);
  psi->end_file_open_wait(file_locker);
  file_locker= psi->get_thread_file_stream_locker(&file_state, file_A1, PSI_FILE_READ);
  ok(file_locker != NULL, "locker");
  psi->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  psi->end_file_wait(file_locker, 10);
  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_READ);
  ok(file_locker != NULL, "locker");
  psi->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  psi->end_file_wait(file_locker, 10);

  /* Pretend the running thread is not instrumented */
  /* ---------------------------------------------- */

  psi->delete_current_thread();
  flag_events_waits_current= true;
  mutex_class_A->m_enabled= true;
  rwlock_class_A->m_enabled= true;
  cond_class_A->m_enabled= true;
  file_class_A->m_enabled= true;

  mutex_locker= psi->get_thread_mutex_locker(&mutex_state, mutex_A1, PSI_MUTEX_LOCK);
  ok(mutex_locker == NULL, "no locker");
  rwlock_locker= psi->get_thread_rwlock_locker(&rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK);
  ok(rwlock_locker == NULL, "no locker");
  cond_locker= psi->get_thread_cond_locker(&cond_state, cond_A1, mutex_A1, PSI_COND_WAIT);
  ok(cond_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_name_locker(&file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_stream_locker(&file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");

  shutdown_performance_schema();
}

void test_file_instrumentation_leak()
{
  PSI *psi;

  diag("test_file_instrumentation_leak");

  psi= load_perfschema();

  PSI_file_key file_key_A;
  PSI_file_key file_key_B;
  PSI_file_info all_file[]=
  {
    { & file_key_A, "F-A", 0},
    { & file_key_B, "F-B", 0}
  };

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[]=
  {
    { & thread_key_1, "T-1", 0}
  };

  psi->register_file("test", all_file, 2);
  psi->register_thread("test", all_thread, 1);

  PFS_file_class *file_class_A;
  PFS_file_class *file_class_B;
  PSI_file_locker_state file_state;
  PSI_thread *thread_1;

  /* Preparation */

  thread_1= psi->new_thread(thread_key_1, NULL, 0);
  ok(thread_1 != NULL, "T-1");
  psi->set_thread_id(thread_1, 1);

  file_class_A= find_file_class(file_key_A);
  ok(file_class_A != NULL, "file info A");

  file_class_B= find_file_class(file_key_B);
  ok(file_class_B != NULL, "file info B");

  psi->set_thread(thread_1);

  /* Pretend everything is enabled */
  /* ----------------------------- */

  setup_thread(thread_1, true);
  flag_events_waits_current= true;
  file_class_A->m_enabled= true;
  file_class_B->m_enabled= true;

  PSI_file_locker *file_locker;

  /* Simulate OPEN + READ of 100 bytes + CLOSE on descriptor 12 */

  file_locker= psi->get_thread_file_name_locker(&file_state, file_key_A, PSI_FILE_OPEN, "AAA", NULL);
  ok(file_locker != NULL, "locker");
  psi->start_file_open_wait(file_locker, __FILE__, __LINE__);
  psi->end_file_open_wait_and_bind_to_descriptor(file_locker, 12);

  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_READ);
  ok(file_locker != NULL, "locker");
  psi->start_file_wait(file_locker, 100, __FILE__, __LINE__);
  psi->end_file_wait(file_locker, 100);

  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_CLOSE);
  ok(file_locker != NULL, "locker");
  psi->start_file_wait(file_locker, 0, __FILE__, __LINE__);
  psi->end_file_wait(file_locker, 0);

  /* Simulate uninstrumented-OPEN + WRITE on descriptor 24 */

  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 24, PSI_FILE_WRITE);
  ok(file_locker == NULL, "no locker, since the open was not instrumented");

  /*
    Simulate uninstrumented-OPEN + WRITE on descriptor 12 :
    the instrumentation should not leak (don't charge the file io on unknown B to "AAA")
  */

  file_locker= psi->get_thread_file_descriptor_locker(&file_state, (File) 12, PSI_FILE_WRITE);
  ok(file_locker == NULL, "no locker, no leak");

  shutdown_performance_schema();
}

void test_enabled()
{
#ifdef LATER
  PSI *psi;

  diag("test_enabled");

  psi= load_perfschema();

  PSI_mutex_key mutex_key_A;
  PSI_mutex_key mutex_key_B;
  PSI_mutex_info all_mutex[]=
  {
    { & mutex_key_A, "M-A", 0},
    { & mutex_key_B, "M-B", 0}
  };

  PSI_rwlock_key rwlock_key_A;
  PSI_rwlock_key rwlock_key_B;
  PSI_rwlock_info all_rwlock[]=
  {
    { & rwlock_key_A, "RW-A", 0},
    { & rwlock_key_B, "RW-B", 0}
  };

  PSI_cond_key cond_key_A;
  PSI_cond_key cond_key_B;
  PSI_cond_info all_cond[]=
  {
    { & cond_key_A, "C-A", 0},
    { & cond_key_B, "C-B", 0}
  };

  shutdown_performance_schema();
#endif
}

void do_all_tests()
{
  /* Using initialize_performance_schema(), no partial init needed. */

  test_bootstrap();
  test_bad_registration();
  test_init_disabled();
  test_locker_disabled();
  test_file_instrumentation_leak();
}

int main(int, char **)
{
  plan(153);
  MY_INIT("pfs-t");
  do_all_tests();
  return 0;
}


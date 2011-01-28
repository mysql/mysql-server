/* Copyright (c) 2010, 2011 Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_VISITOR_H
#define PFS_VISITOR_H

#include "pfs_stat.h"

/**
  @file storage/perfschema/pfs_visitor.h
  Visitors (declarations).
*/

/**
  @addtogroup Performance_schema_buffers
  @{
*/

struct PFS_thread;
struct PFS_instr_class;
struct PFS_mutex_class;
struct PFS_rwlock_class;
struct PFS_cond_class;
struct PFS_file_class;
struct PFS_table_share;
struct PFS_mutex;
struct PFS_rwlock;
struct PFS_cond;
struct PFS_file;
struct PFS_table;
struct PFS_stage_class;
struct PFS_statement_class;

/**
  Interface class to visit groups of connections.
  @sa PFS_connection_iterator
*/
class PFS_connection_visitor
{
public:
  PFS_connection_visitor() {}
  virtual ~PFS_connection_visitor() {}
  /** Visit all connections. */
  virtual void visit_global() {}
  /** Visit all a thread. */
  virtual void visit_thread(PFS_thread *pfs) {}
};

/**
  Iterator over groups of connections.
  @sa PFS_connection_visitor
*/
class PFS_connection_iterator
{
public:
  /**
    Visit all connections.
    @param with_threads when true, visit also all threads.
    @param visitor the visitor to call
  */
  static void visit_global(bool with_threads,
                           PFS_connection_visitor *visitor);
  /**
    Visit a thread or connection.
    @param thread the thread to visit.
    @param visitor the visitor to call
  */
  static inline void visit_thread(PFS_thread *thread,
                                  PFS_connection_visitor *visitor)
  { visitor->visit_thread(thread); }
};

/**
  Interface class to visit groups of instrumentation point instances.
  @sa PFS_instance_iterator
*/
class PFS_instance_visitor
{
public:
  PFS_instance_visitor() {}
  virtual ~PFS_instance_visitor() {}
  /** Visit a mutex class. */
  virtual void visit_mutex_class(PFS_mutex_class *pfs) {}
  /** Visit a rwlock class. */
  virtual void visit_rwlock_class(PFS_rwlock_class *pfs) {}
  /** Visit a cond class. */
  virtual void visit_cond_class(PFS_cond_class *pfs) {}
  /** Visit a file class. */
  virtual void visit_file_class(PFS_file_class *pfs) {}
  /** Visit a mutex instance. */
  virtual void visit_mutex(PFS_mutex *pfs) {}
  /** Visit a rwlock instance. */
  virtual void visit_rwlock(PFS_rwlock *pfs) {}
  /** Visit a cond instance. */
  virtual void visit_cond(PFS_cond *pfs) {}
  /** Visit a file instance. */
  virtual void visit_file(PFS_file *pfs) {}
};

/**
  Iterator over groups of instrumentation point instances.
  @sa PFS_instance_visitor
*/
class PFS_instance_iterator
{
public:
  /**
    Visit a mutex class and related instances.
    @param klass the klass to visit.
    @param visitor the visitor to call
  */
  static void visit_mutex_instances(PFS_mutex_class *klass,
                                    PFS_instance_visitor *visitor);
  /**
    Visit a rwlock class and related instances.
    @param klass the klass to visit.
    @param visitor the visitor to call
  */
  static void visit_rwlock_instances(PFS_rwlock_class *klass,
                                     PFS_instance_visitor *visitor);
  /**
    Visit a cond class and related instances.
    @param klass the klass to visit.
    @param visitor the visitor to call
  */
  static void visit_cond_instances(PFS_cond_class *klass,
                                   PFS_instance_visitor *visitor);
  /**
    Visit a file class and related instances.
    @param klass the klass to visit.
    @param visitor the visitor to call
  */
  static void visit_file_instances(PFS_file_class *klass,
                                   PFS_instance_visitor *visitor);
};

/**
  Interface class to visit groups of SQL objects.
  @sa PFS_object_iterator
*/
class PFS_object_visitor
{
public:
  PFS_object_visitor() {}
  virtual ~PFS_object_visitor() {}
  /** Visit global data. */
  virtual void visit_global() {}
  /** Visit a table share. */
  virtual void visit_table_share(PFS_table_share *pfs) {}
  /** Visit a table share index. */
  virtual void visit_table_share_index(PFS_table_share *pfs, uint index) {}
  /** Visit a table. */
  virtual void visit_table(PFS_table *pfs) {}
  /** Visit a table index. */
  virtual void visit_table_index(PFS_table *pfs, uint index) {}
};

/**
  Iterator over groups of SQL objects.
  @sa PFS_object_visitor
*/
class PFS_object_iterator
{
public:
  /** Visit all tables and related handles. */
  static void visit_all_tables(PFS_object_visitor *visitor);
  /** Visit a table and related table handles. */
  static void visit_tables(PFS_table_share *share,
                           PFS_object_visitor *visitor);
  /** Visit a table index and related table handles indexes. */
  static void visit_table_indexes(PFS_table_share *share,
                                  uint index,
                                  PFS_object_visitor *visitor);
};

/**
  A concrete connection visitor that aggregates
  wait statistics.
*/
class PFS_connection_wait_visitor : public PFS_connection_visitor
{
public:
  /** Constructor. */
  PFS_connection_wait_visitor(PFS_instr_class *klass);
  virtual ~PFS_connection_wait_visitor();
  virtual void visit_global();
  virtual void visit_thread(PFS_thread *pfs);

  /** EVENT_NAME instrument index. */
  uint m_index;
  /** Wait statistic collected. */
  PFS_single_stat m_stat;
};

/**
  A concrete instance visitor that aggregates
  wait statistics.
*/
class PFS_instance_wait_visitor : public PFS_instance_visitor
{
public:
  PFS_instance_wait_visitor();
  virtual ~PFS_instance_wait_visitor();
  virtual void visit_mutex_class(PFS_mutex_class *pfs);
  virtual void visit_rwlock_class(PFS_rwlock_class *pfs);
  virtual void visit_cond_class(PFS_cond_class *pfs);
  virtual void visit_file_class(PFS_file_class *pfs);
  virtual void visit_mutex(PFS_mutex *pfs);
  virtual void visit_rwlock(PFS_rwlock *pfs);
  virtual void visit_cond(PFS_cond *pfs);
  virtual void visit_file(PFS_file *pfs);

  /** Wait statistic collected. */
  PFS_single_stat m_stat;
};

/**
  A concrete object visitor that aggregates
  table io wait statistics.
*/
class PFS_table_io_wait_visitor : public PFS_object_visitor
{
public:
  PFS_table_io_wait_visitor();
  virtual ~PFS_table_io_wait_visitor();
  virtual void visit_global();
  virtual void visit_table_share(PFS_table_share *pfs);
  virtual void visit_table(PFS_table *pfs);

  /** Table io wait statistic collected. */
  PFS_single_stat m_stat;
};

/**
  A concrete object visitor that aggregates
  table io statistics.
*/
class PFS_table_io_stat_visitor : public PFS_object_visitor
{
public:
  PFS_table_io_stat_visitor();
  virtual ~PFS_table_io_stat_visitor();
  virtual void visit_table_share(PFS_table_share *pfs);
  virtual void visit_table(PFS_table *pfs);

  /** Table io statistic collected. */
  PFS_table_io_stat m_stat;
};

/**
  A concrete object visitor that aggregates
  index io statistics.
*/
class PFS_index_io_stat_visitor : public PFS_object_visitor
{
public:
  PFS_index_io_stat_visitor();
  virtual ~PFS_index_io_stat_visitor();
  virtual void visit_table_share_index(PFS_table_share *pfs, uint index);
  virtual void visit_table_index(PFS_table *pfs, uint index);

  /** Index io statistic collected. */
  PFS_table_io_stat m_stat;
};

/**
  A concrete object visitor that aggregates
  table lock wait statistics.
*/
class PFS_table_lock_wait_visitor : public PFS_object_visitor
{
public:
  PFS_table_lock_wait_visitor();
  virtual ~PFS_table_lock_wait_visitor();
  virtual void visit_global();
  virtual void visit_table_share(PFS_table_share *pfs);
  virtual void visit_table(PFS_table *pfs);

  /** Table lock wait statistic collected. */
  PFS_single_stat m_stat;
};

/**
  A concrete object visitor that aggregates
  table lock statistics.
*/
class PFS_table_lock_stat_visitor : public PFS_object_visitor
{
public:
  PFS_table_lock_stat_visitor();
  virtual ~PFS_table_lock_stat_visitor();
  virtual void visit_table_share(PFS_table_share *pfs);
  virtual void visit_table(PFS_table *pfs);

  /** Table lock statistic collected. */
  PFS_table_lock_stat m_stat;
};

/** @} */
#endif


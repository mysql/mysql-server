/*****************************************************************************

Copyright (c) 2021, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************/ /**
 @file include/log0consumer.h

 Redo log functions and types related to the log consumption.

 *******************************************************/

#ifndef log0consumer_h
#define log0consumer_h

#include "log0types.h" /* lsn_t, log_t& */

class Log_consumer {
 public:
  virtual ~Log_consumer() {}

  /** @return Name of this consumer. */
  virtual const std::string &get_name() const = 0;

  /** @return Maximum LSN up to which this consumer has consumed redo. */
  virtual lsn_t get_consumed_lsn() const = 0;

  /** Request the log consumer to consume faster.
  @remarks This is called whenever the redo log consumer
  is the most lagging one and it is critical to consume
  the oldest redo log file. */
  virtual void consumption_requested() = 0;
};

class Log_user_consumer : public Log_consumer {
 public:
  explicit Log_user_consumer(const std::string &name);

  const std::string &get_name() const override;

  /** Set the lsn reported by get_consumed_lsn() to the given value.
  It is required that the given value is greater or equal to the value
  currently reported by the get_consumed_lsn().
  @param[in]  consumed_lsn    the given lsn to report */
  void set_consumed_lsn(lsn_t consumed_lsn);

  lsn_t get_consumed_lsn() const override;

  void consumption_requested() override;

 private:
  /** Name of this consumer (saved value from ctor). */
  const std::string m_name;

  /** Value reported by get_consumed_lsn().
  Set by set_consumed_lsn(lsn). */
  lsn_t m_consumed_lsn{};
};

class Log_checkpoint_consumer : public Log_consumer {
 public:
  explicit Log_checkpoint_consumer(log_t &log);

  const std::string &get_name() const override;

  lsn_t get_consumed_lsn() const override;

  void consumption_requested() override;

 private:
  log_t &m_log;
};

/** Register the given redo log consumer.
@param[in,out]  log           redo log
@param[in]      log_consumer  redo log consumer to register */
void log_consumer_register(log_t &log, Log_consumer *log_consumer);

/** Unregister the given redo log consumer.
@param[in,out]  log           redo log
@param[in]      log_consumer  redo log consumer to unregister */
void log_consumer_unregister(log_t &log, Log_consumer *log_consumer);

/** Find the registered redo log consumer which has the smallest value
reported by get_consumed_lsn() - ie. the most lagging consumer. When
multiple consumers have the same value, any of them might be returned.
@param[in]  log               the redo log
@param[out] oldest_needed_lsn the oldest lsn needed by the most lagging consumer
@return the most lagging consumer */
Log_consumer *log_consumer_get_oldest(const log_t &log,
                                      lsn_t &oldest_needed_lsn);

#endif /* !log0consumer_h */

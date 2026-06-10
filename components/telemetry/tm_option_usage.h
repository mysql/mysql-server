/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TELEMETRY_OPTION_USAGE_H_INCLUDED
#define TELEMETRY_OPTION_USAGE_H_INCLUDED

#include <cstddef>

namespace telemetry {

class OptionUsage {
 public:
  OptionUsage(size_t sample_every_n)
      : m_count(0), m_sample_every_n(sample_every_n) {}
  virtual ~OptionUsage() = default;

  void sample();
  virtual void report() = 0;

 private:
  size_t m_count;
  size_t m_sample_every_n;
};

class TraceOptionUsage : public OptionUsage {
 public:
  // TODO: revise sampling rate for traces
  TraceOptionUsage() : OptionUsage(1000) {}
  ~TraceOptionUsage() override = default;

  void report() override;
};

class MetricOptionUsage : public OptionUsage {
 public:
  // TODO: revise sampling rate for metrics
  MetricOptionUsage() : OptionUsage(1000) {}
  ~MetricOptionUsage() override = default;

  void report() override;
};

class LogOptionUsage : public OptionUsage {
 public:
  // TODO: revise sampling rate for logs
  LogOptionUsage() : OptionUsage(1000) {}
  ~LogOptionUsage() override = default;

  void report() override;
};

}  // namespace telemetry

#endif /* TELEMETRY_OPTION_USAGE_H_INCLUDED */

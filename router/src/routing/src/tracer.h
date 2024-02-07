/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_TRACER_INCLUDED
#define ROUTING_TRACER_INCLUDED

#include <chrono>
#include <cstdio>  // fputs, stderr
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

/**
 * traces the timestamps of events in a stderr log.
 *
 * If enabled, the tracer outputs:
 *
 * - duration since Tracer was created
 * - duration since last Event
 * - direction (from|to client|server)
 * - the event
 *
 * to stderr.
 */
class Tracer {
 public:
  using clock_type = std::chrono::steady_clock;

  Tracer() = default;

  explicit Tracer(bool enabled) : enabled_{enabled} {
    if (!enabled_) return;

    last_ = start_ = clock_type::now();
  }

  class Event {
   public:
    class Stage {
     public:
      Stage(std::string name) : name_(name) {}

      std::string name() const { return name_; }

     private:
      std::string name_;
    };

    enum class Direction {
      kClientToRouter,
      kRouterToClient,
      kServerToRouter,
      kRouterToServer,
      kClientClose,
      kServerClose,
    };

    enum class Wait {
      kRead,
      kSend,
    };

    std::optional<Direction> direction() const { return direction_; }
    std::optional<Stage> stage() const { return stage_; }

    Event &stage(std::string_view s) {
      stage_ = std::string(s);
      return *this;
    }

    Event &direction(Direction dir) {
      direction_ = dir;
      return *this;
    }

   private:
    std::optional<Direction> direction_;
    std::optional<Stage> stage_;
  };

  static std::string direction(Event::Direction direction) {
    using Direction = Event::Direction;

    switch (direction) {
      case Direction::kClientToRouter:
        return "c->r   ";
      case Direction::kRouterToClient:
        return "c<-r   ";
      case Direction::kRouterToServer:
        return "   r->s";
      case Direction::kServerToRouter:
        return "   r<-s";
      case Direction::kServerClose:
        return "   r..s";
      case Direction::kClientClose:
        return "c..r   ";
    }

    return "       ";
  }

  static std::string stage(Event::Stage st) { return st.name(); }

  void trace(Event e) {
    if (!enabled_) return;

    auto now = clock_type::now();

    auto delta_now =
        std::chrono::duration_cast<std::chrono::microseconds>(now - start_);
    auto delta_last =
        std::chrono::duration_cast<std::chrono::microseconds>(now - last_);

    std::ostringstream oss;
    oss << "/* " << std::setw(10) << delta_now.count() << " us ("
        << std::showpos << std::setw(10) << delta_last.count() << std::noshowpos
        << " us) */  ";
    if (e.direction()) {
      oss << direction(*(e.direction()));
    } else {
      oss << "       ";
    }

    oss << " ";

    if (e.stage()) {
      oss << stage(*(e.stage()));
    } else {
      oss << "none";
    }

    oss << "\n";

    fputs(oss.str().c_str(), stderr);

    last_ = now;
  }

  explicit operator bool() const { return enabled_; }

 private:
  bool enabled_{false};

  clock_type::time_point start_{};
  clock_type::time_point last_{};
};

#endif

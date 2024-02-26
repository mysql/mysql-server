/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstring>  // memcpy
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "hexify.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/flags.h"
#include "scope_guard.h"

#ifdef __linux__
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef SCM_TIMESTAMPING_OPT_STATS
// only include if really needed.
#include <linux/netlink.h>
#endif
#endif

using mysql_harness::hexify;

std::ostream &operator<<(std::ostream &os, std::chrono::nanoseconds e) {
  os << e.count();

  return os;
}

std::ostream &operator<<(std::ostream &os, std::chrono::microseconds e) {
  os << e.count();

  return os;
}

template <class T>
std::chrono::nanoseconds timespec_to_duration(T tv) {
  auto dur =
      std::chrono::seconds(tv.tv_sec) + std::chrono::nanoseconds(tv.tv_nsec);

  return std::chrono::duration_cast<std::chrono::nanoseconds>(dur);
}

template <class T>
std::chrono::microseconds timeval_to_duration(T tv) {
  auto dur =
      std::chrono::seconds(tv.tv_sec) + std::chrono::microseconds(tv.tv_usec);

  return std::chrono::duration_cast<std::chrono::microseconds>(dur);
}

class SocketLevel {
 public:
  constexpr SocketLevel(int lvl, int type) : lvl_(lvl), type_(type) {}

  constexpr int level() const { return lvl_; }
  constexpr int type() const { return type_; }

 private:
  int lvl_;
  int type_;
};

template <int Lvl, int Type>
class ControlMsgBase {
 public:
  static constexpr int level() { return Lvl; }
  static constexpr int type() { return Type; }

  static constexpr SocketLevel socket_level() { return {level(), type()}; }
};

inline bool operator==(SocketLevel a, SocketLevel b) {
  return a.type() == b.type() && a.level() == b.level();
}

template <int Lvl, int Type, class V>
class SocketTimestampBase : public ControlMsgBase<Lvl, Type> {
 public:
  using value_type = V;

  SocketTimestampBase(const void *data, size_t data_len) {
    if (data_len < sizeof(ts_)) {
      std::cerr << __LINE__ << ": " << __func__ << ": expected " << data_len
                << " >= " << sizeof(ts_) << "\n";
      abort();
    }

    memcpy(&ts_, data, sizeof(ts_));
  }

  std::chrono::microseconds timestamp() const {
    return timeval_to_duration(ts_);
  }

  std::string to_string() const {
    std::ostringstream oss;

    oss << "socket::timestamp {" << timestamp() << "}";

    return oss.str();
  }

 private:
  value_type ts_;
};

#ifdef SO_TIMESTAMP_NEW
using SocketTimestampNew =
    SocketTimestampBase<SOL_SOCKET, SO_TIMESTAMP_NEW, __kernel_sock_timeval>;

using SocketTimestampOld =
    SocketTimestampBase<SOL_SOCKET, SO_TIMESTAMP_OLD, __kernel_old_timeval>;
#endif

#ifdef SO_TIMESTAMP
// linux + freebsd
using SocketTimestamp = SocketTimestampBase<SOL_SOCKET, SO_TIMESTAMP,
#ifdef SO_TIMESTAMP_NEW
#if SO_TIMESTAMP == SO_TIMESTAMP_NEW
                                            SocketTimestampNew::value_type
#else
                                            SocketTimestampOld::value_type
#endif
#else
                                            timeval
#endif
                                            >;
#endif

template <int Lvl, int Type, class V>
class SocketTimestampNanosecondBase : public ControlMsgBase<Lvl, Type> {
 public:
  using value_type = V;

  SocketTimestampNanosecondBase(const void *data, size_t data_len) {
    if (data_len < sizeof(ts_)) {
      std::cerr << __LINE__ << ": " << __func__ << ": expected " << data_len
                << " >= " << sizeof(ts_) << "\n";
      abort();
    }

    memcpy(&ts_, data, sizeof(ts_));
  }

  std::chrono::nanoseconds timestamp() const {
    return timespec_to_duration(ts_);
  }

  std::string to_string() const {
    std::ostringstream oss;

    oss << "socket::timestamp {" << timestamp() << "}";

    return oss.str();
  }

 private:
  value_type ts_;
};

#ifdef SO_TIMESTAMPNS_NEW
using SocketTimestampNanosecondNew =
    SocketTimestampNanosecondBase<SOL_SOCKET, SO_TIMESTAMPNS_NEW,
                                  __kernel_timespec>;

using SocketTimestampNanosecondOld =
    SocketTimestampNanosecondBase<SOL_SOCKET, SO_TIMESTAMPNS_OLD, timespec>;
#endif

#ifdef SO_TIMESTAMPNS
using SocketTimestampNanosecond =
    SocketTimestampNanosecondBase<SOL_SOCKET, SO_TIMESTAMPNS,
#ifdef SO_TIMESTAMPNS_NEW
#if SO_TIMESTAMPNS == SO_TIMESTAMPNS_NEW
                                  SocketTimestampNanosecondNew::value_type
#else
                                  SocketTimestampNanosecondOld::value_type
#endif
#else
                                  timespec
#endif
                                  >;
#endif

template <int Lvl, int Type, class V>
class SocketTimestampingBase : public ControlMsgBase<Lvl, Type> {
 public:
  using value_type = V;
  using timespec_type =
      std::decay_t<decltype(std::declval<value_type>().ts[0])>;

  SocketTimestampingBase(const void *data, size_t data_len) {
    if (data_len < sizeof(ts_)) {
      std::cerr << __LINE__ << ": " << __func__ << ": expected " << data_len
                << " >= " << sizeof(ts_) << "\n";
      abort();
    }

    memcpy(&ts_, data, sizeof(ts_));
  }

  std::chrono::nanoseconds software_timestamp() const {
    return timespec_to_duration(ts_.ts[0]);
  }

  std::chrono::nanoseconds hardware_timestamp_old() const {
    return timespec_to_duration(ts_.ts[1]);
  }

  std::chrono::nanoseconds hardware_timestamp() const {
    return timespec_to_duration(ts_.ts[2]);
  }

  std::string to_string() const {
    std::ostringstream oss;

    oss << "socket::timestamping {";

    oss << "sw: " << software_timestamp() << ", ";
    oss << "hw: " << hardware_timestamp();

    oss << "}";

    return oss.str();
  }

 private:
  value_type ts_;
};

template <typename, typename = void>
constexpr bool is_type_complete_v = false;

template <typename T>
constexpr bool is_type_complete_v<T, std::void_t<decltype(sizeof(T))>> = true;

namespace fallback {
// scm_timestamping is defined in <linux/errqueue.h>
// in 3.17 and later.
//
// EL6 has headers from 2.6... better define them ourselves.
struct scm_timestamping {
  std::array<timespec, 3> ts;
};
}  // namespace fallback

#ifdef SO_TIMESTAMPING_NEW
using SocketTimestampingNew =
    SocketTimestampingBase<SOL_SOCKET, SO_TIMESTAMPING_NEW, scm_timestamping64>;

using SocketTimestampingOld =
    SocketTimestampingBase<SOL_SOCKET, SO_TIMESTAMPING_OLD, scm_timestamping>;
#endif

#ifdef SO_TIMESTAMPING
using SocketTimestamping =
    SocketTimestampingBase<SOL_SOCKET, SO_TIMESTAMPING,
#ifdef SO_TIMESTAMPING_NEW
#if SO_TIMESTAMPING == SO_TIMESTAMPING_NEW
                           SocketTimestampingNew::value_type
#else
                           SocketTimestampingOld::value_type
#endif
#else
                           std::conditional_t<
                               is_type_complete_v<struct scm_timestamping>,
                               scm_timestamping, fallback::scm_timestamping>
#endif
                           >;
#endif

// if they are defined, check they have the expected value.
#ifdef SO_EE_ORIGIN_NONE
static_assert(SO_EE_ORIGIN_NONE == 0);
#endif
#ifdef SO_EE_ORIGIN_LOCAL
static_assert(SO_EE_ORIGIN_LOCAL == 1);
#endif
#ifdef SO_EE_ORIGIN_ICMP
static_assert(SO_EE_ORIGIN_ICMP == 2);
#endif
#ifdef SO_EE_ORIGIN_ICMP6
static_assert(SO_EE_ORIGIN_ICMP6 == 3);
#endif
#ifdef SO_EE_ORIGIN_TXSTATUS
static_assert(SO_EE_ORIGIN_TXSTATUS == 4);
#endif
#ifdef SO_EE_ORIGIN_ZEROCOPY
static_assert(SO_EE_ORIGIN_ZEROCOPY == 5);
#endif
#ifdef SO_EE_ORIGIN_TXTIME
static_assert(SO_EE_ORIGIN_TXTIME == 6);
#endif

#if defined(SOL_IP) && defined(IP_RECVERR)
// windows-sdk 18362 defines IP_RECVERR, but not SOL_IP
class IpRecvErr : public ControlMsgBase<SOL_IP, IP_RECVERR> {
 public:
  class SockExtendedError {
   public:
#ifdef __linux__
    enum class Origin{
        none = 0,      // SO_EE_ORIGIN_NONE
        local = 1,     // SO_EE_ORIGIN_LOCAL
        icmp = 2,      // SO_EE_ORIGIN_ICMP
        icmp6 = 3,     // SO_EE_ORIGIN_ICMP6
        txstatus = 4,  // SO_EE_ORIGIN_TXSTATUS (timestamping)
        zerocopy = 5,  // SO_EE_ORIGIN_ZEROCOPY (TCP transmit feedback)
        txtime = 6,    // SO_EE_ORIGIN_TXTIME   (SO_TXTIME, SCM_TXTIME)
    };

    enum class Source : uint32_t{
        send = 0,   // SCM_TSTAMP_SND
        sched = 1,  // SCM_TSTAMP_SCHED
        ack = 2,    // SCM_TSTAMP_ACK
    };
#endif

    SockExtendedError(const void *data, size_t data_len) {
      if (data_len < sizeof(err_)) {
        std::cerr << __LINE__ << ": " << __func__ << ": expected " << data_len
                  << " >= " << sizeof(err_) << "\n";
        abort();
      }

      memcpy(&err_, data, sizeof(err_));
    }

    std::string to_string() const {
      std::ostringstream oss;

#ifdef __linux__
      if (err_.ee_errno == ENOMSG &&
          Origin{err_.ee_origin} == Origin::txstatus) {
        oss << "tstmp: { "
            << "type: ";
        switch (Source{err_.ee_info}) {
          case Source::send:
            oss << "tx-send";
            break;
          case Source::sched:
            oss << "tx-sched";
            break;
          case Source::ack:
            oss << "tx-ack";
            break;
          default:
            oss << "<" << (int)err_.ee_info << ">";
            break;
        }
        oss << ", id: " << err_.ee_data << " }";
      } else {
        oss << "errno: "
            << std::error_code{static_cast<int>(err_.ee_errno),
                               std::system_category()}
            << "\n";
        oss << "origin: " << (int)err_.ee_origin << "\n";

        oss << "type: " << (int)err_.ee_type << "\n";
        oss << "code: " << (int)err_.ee_code << "\n";

        oss << "info: " << err_.ee_info << "\n";
        oss << "data: " << err_.ee_data << "\n";
      }
#endif
#ifdef _WIN32
      oss << "origin: " << (int)err_.protocol << "\n";

      oss << "type: " << (int)err_.type << "\n";
      oss << "code: " << (int)err_.code << "\n";

      oss << "info: " << err_.info << "\n";
#endif

      return oss.str();
    }

   private:
#ifdef __linux__
    sock_extended_err err_;
    // ee_errno
    // ee_origin
    // ee_type
    // ee_code
    // u8 ee_pad
    // u8  ee_info
    // u32 ee_data
#elif defined(_WIN32)
    in_recverr err_;
    // IPPROTO protocol
    // ULONG info
    // UINT8 type
    // UINT8 code
#endif
  };

  IpRecvErr(const void *data, size_t data_len) : err_{data, data_len} {}

  std::string to_string() const { return "ip::recverr: " + err_.to_string(); }

 private:
  SockExtendedError err_;
};
#endif

template <int T, class V>
class Nla {
 public:
  using value_type = V;

  Nla(const uint8_t *data, size_t data_len) { memcpy(&v_, data, data_len); }

  static constexpr uint16_t type() { return T; }

  value_type value() const { return v_; }

 private:
  value_type v_;
};

using TcpBusy = Nla<1, uint64_t>;            // usec busy sending data
using TcpRwndLimited = Nla<2, uint64_t>;     // usec limited by receive window
using TcpSendBufLimited = Nla<3, uint64_t>;  // usec limited by send buffer
using TcpDataSegsOut =
    Nla<4, uint64_t>;  // data pkts send including retransmission
using TcpTotalRetrans = Nla<5, uint64_t>;          // data pkts retransmitted
using TcpPacingRate = Nla<6, uint64_t>;            // pacing rate b/sec
using TcpDeliveryRate = Nla<7, uint64_t>;          // delivery rate b/sec
using TcpSendCongestionWindow = Nla<8, uint32_t>;  // send congestion window

// reordering metric
//
// default: 3
// max: (300)
//
// see: sysctl tcp_reordering
//
// https://www.kernel.org/doc/Documentation/networking/ip-sysctl.txt
using TcpReordering = Nla<9, uint32_t>;

// minmum RTT
using TcpMinRtt = Nla<10, uint32_t>;
using TcpRecurRetrans =
    Nla<11, uint8_t>;  // recurring retansmits of the current pkt
using TcpDeliveryRateAppLimited =
    Nla<12, uint8_t>;  // delivery rate application limited
using TcpSendQueueSize = Nla<13, uint32_t>;  // data bytes pending in send queue

enum class CongestionControlState {
  Open = 0,
  Disorder = 1,
  CWR = 2,
  Recovery = 3,
  Loss = 4,
};
using TcpCongestionControlState = Nla<14, uint8_t>;  // ca_state of socket
using TcpSendSlowStartSizeThreshold = Nla<15, uint32_t>;
using TcpDelivered =
    Nla<16, uint32_t>;  // Data packets delivered incl out-of-order
using TcpDeliveredCe =
    Nla<17, uint32_t>;  // Data packets delivered incl out-of-order with CE
                        // (ECN::Congestion Encountered)
using TcpBytesSent =
    Nla<18, uint64_t>;  // Data bytes sent (incl. retransmission)
using TcpBytesRetrans = Nla<19, uint64_t>;  // Data bytes retransmitted
using TcpDSackDups = Nla<20, uint32_t>;     // DSACK blocks received
using TcpReordSeen = Nla<21, uint32_t>;     // Reorderings Seen
using TcpSrtt = Nla<22, uint32_t>;          // smoothed RTT
using TcpTimeoutRehash =
    Nla<23, uint16_t>;                      // Timeout-triggered rehash attempts
using TcpBytesNotSent = Nla<24, uint32_t>;  // Bytes in write-queue not yet sent
using TcpEdt = Nla<25, uint64_t>;  // Earliest departure time (CLOCK_MONOTONIC)
using TcpTtl = Nla<26, uint8_t>;   // TTL or hope of a packet received

template <class T>
class Printer;

template <class, class = void>
struct has_printer : std::false_type {};

template <class T>
struct has_printer<T, std::void_t<decltype(Printer<T>::name)>>
    : std::true_type {};

template <class, class = void>
struct has_printer_unit : std::false_type {};

template <class T>
struct has_printer_unit<T, std::void_t<decltype(Printer<T>::unit)>>
    : std::true_type {};

template <class T, std::enable_if_t<has_printer<T>::value> * = nullptr>
std::ostream &operator<<(std::ostream &os, T v) {
  os << Printer<T>::name << ": ";

  if (std::is_same_v<decltype(v.value()), uint8_t>) {
    os << (int)v.value();
  } else {
    os << v.value();
  }

  if constexpr (has_printer_unit<T>::value) {
    if (!Printer<T>::unit.empty()) {
      os << " " << Printer<T>::unit;
    }
  }

  return os;
}

template <>
class Printer<TcpBusy> {
 public:
  static constexpr std::string_view name{"busy-sending-data"};
  static constexpr std::string_view unit{"usec"};
};

template <>
class Printer<TcpRwndLimited> {
 public:
  static constexpr std::string_view name{"limited-by-receive-window"};
  static constexpr std::string_view unit{"usec"};
};

template <>
class Printer<TcpSendBufLimited> {
 public:
  static constexpr std::string_view name{"limited-by-send-buffer"};
  static constexpr std::string_view unit{"usec"};
};

template <>
class Printer<TcpDataSegsOut> {
 public:
  static constexpr std::string_view name{"data-pkts-sent"};
  static constexpr std::string_view unit{"pkts"};
};

template <>
class Printer<TcpTotalRetrans> {
 public:
  static constexpr std::string_view name{"data-pkts-retransmitted"};
  static constexpr std::string_view unit{"pkts"};
};

template <>
class Printer<TcpPacingRate> {
 public:
  static constexpr std::string_view name{"pacing-rate"};
  static constexpr std::string_view unit{"b/sec"};
};

template <>
class Printer<TcpDeliveryRate> {
 public:
  static constexpr std::string_view name{"delivery-rate"};
  static constexpr std::string_view unit{"b/sec"};
};

template <>
class Printer<TcpSendCongestionWindow> {
 public:
  static constexpr std::string_view name{"send-cwnd"};
};

template <>
class Printer<TcpReordering> {
 public:
  static constexpr std::string_view name{"reordering-metric"};
};

template <>
class Printer<TcpMinRtt> {
 public:
  static constexpr std::string_view name{"min-rtt"};
};

template <>
class Printer<TcpRecurRetrans> {
 public:
  static constexpr std::string_view name{"recurring-retransmissions"};
};

template <>
class Printer<TcpDeliveryRateAppLimited> {
 public:
  static constexpr std::string_view name{"delivery-rate-app-limited"};
};

template <>
class Printer<TcpCongestionControlState> {
 public:
  static constexpr std::string_view name{"congestion-control-state"};
};

template <>
class Printer<TcpSendQueueSize> {
 public:
  static constexpr std::string_view name{"send-queue-size"};
  static constexpr std::string_view unit{"b"};
};

template <>
class Printer<TcpSendSlowStartSizeThreshold> {
 public:
  static constexpr std::string_view name{"slow-start-size-threshold"};
};

template <>
class Printer<TcpDelivered> {
 public:
  static constexpr std::string_view name{"delivered"};
  static constexpr std::string_view unit{"pkts"};
};

template <>
class Printer<TcpDeliveredCe> {
 public:
  static constexpr std::string_view name{"delivered-ce"};
  static constexpr std::string_view unit{"pkts"};
};

template <>
class Printer<TcpBytesSent> {
 public:
  static constexpr std::string_view name{"bytes-sent"};
  static constexpr std::string_view unit{"b"};
};

template <>
class Printer<TcpBytesRetrans> {
 public:
  static constexpr std::string_view name{"bytes-retrans"};
  static constexpr std::string_view unit{"b"};
};

template <>
class Printer<TcpDSackDups> {
 public:
  static constexpr std::string_view name{"dsack-dups"};
};

template <>
class Printer<TcpReordSeen> {
 public:
  static constexpr std::string_view name{"reording-events-seen"};
};

template <>
class Printer<TcpSrtt> {
 public:
  static constexpr std::string_view name{"srtt"};
  static constexpr std::string_view unit{"usec"};
};

template <>
class Printer<TcpTimeoutRehash> {
 public:
  static constexpr std::string_view name{"timeout-rehash"};
};

template <>
class Printer<TcpBytesNotSent> {
 public:
  static constexpr std::string_view name{"bytes-not-sent"};
  static constexpr std::string_view unit{"b"};
};

template <>
class Printer<TcpEdt> {
 public:
  static constexpr std::string_view name{"earliest-departure-time"};
  static constexpr std::string_view unit{"ticks"};
};

template <>
class Printer<TcpTtl> {
 public:
  static constexpr std::string_view name{"ttl"};
};

#ifdef SCM_TIMESTAMPING_OPT_STATS
class Stats : public ControlMsgBase<SOL_SOCKET, SCM_TIMESTAMPING_OPT_STATS> {
 public:
  Stats(const void *data, size_t data_len)
      : stats_(static_cast<const uint8_t *>(data),
               static_cast<const uint8_t *>(data) + data_len) {}

  class iterator {
   public:
    using reference_type =
        std::pair<uint16_t, std::pair<const uint8_t *, size_t>>;

    iterator(const uint8_t *cur, const uint8_t *end) : cur_{cur}, end_{end} {}

    reference_type operator*() {
      const auto attr = reinterpret_cast<const nlattr *>(cur_);
      const auto attr_len = attr->nla_len;
      const auto attr_type = attr->nla_type;

      const auto payload_len = attr_len - NLA_HDRLEN;
      const auto payload = cur_ + NLA_HDRLEN;

      return {attr_type, {payload, payload_len}};
    }

    iterator &operator++() {
      const auto attr = reinterpret_cast<const nlattr *>(cur_);

      size_t attr_len = attr->nla_len;
      auto payload_len = attr_len - NLA_HDRLEN;

      cur_ += NLA_HDRLEN + NLA_ALIGN(payload_len);

      if (cur_ > end_) {
        // safe-guard
        cur_ = end_;
      }

      return *this;
    }

    bool operator!=(const iterator &other) const { return cur_ != other.cur_; }

   private:
    const uint8_t *cur_;
    const uint8_t *end_;
  };

  iterator begin() const {
    return {stats_.data(), stats_.data() + stats_.size()};
  }
  iterator end() const {
    return {stats_.data() + stats_.size(), stats_.data() + stats_.size()};
  }

  std::string to_string() const {
    std::ostringstream oss;
    oss << "socket::stats: {\n";

    for (auto kv : *this) {
      const auto attr_type = kv.first;

      const auto *payload = kv.second.first;
      const auto payload_len = kv.second.second;

      oss << "  ";

      switch (attr_type) {
        case TcpBusy::type():
          oss << TcpBusy(payload, payload_len);
          break;
        case TcpRwndLimited::type():
          oss << TcpRwndLimited(payload, payload_len);
          break;
        case TcpSendBufLimited::type():
          oss << TcpSendBufLimited(payload, payload_len);
          break;
        case TcpDataSegsOut::type():
          oss << TcpDataSegsOut(payload, payload_len);
          break;
        case TcpTotalRetrans::type():
          oss << TcpTotalRetrans(payload, payload_len);
          break;
        case TcpPacingRate::type():
          oss << TcpPacingRate(payload, payload_len);
          break;
        case TcpDeliveryRate::type():
          oss << TcpDeliveryRate(payload, payload_len);
          break;
        case TcpSendCongestionWindow::type():
          oss << TcpSendCongestionWindow(payload, payload_len);
          break;
        case TcpReordering::type():
          oss << TcpReordering(payload, payload_len);
          break;
        case TcpMinRtt::type():
          oss << TcpMinRtt(payload, payload_len);
          break;
        case TcpRecurRetrans::type():
          oss << TcpRecurRetrans(payload, payload_len);
          break;
        case TcpDeliveryRateAppLimited::type():
          oss << TcpDeliveryRateAppLimited(payload, payload_len);
          break;
        case TcpCongestionControlState::type():
          oss << TcpCongestionControlState(payload, payload_len);
          break;
        case TcpSendQueueSize::type():
          oss << TcpSendQueueSize(payload, payload_len);
          break;
        case TcpSendSlowStartSizeThreshold::type():
          oss << TcpSendSlowStartSizeThreshold(payload, payload_len);
          break;
        case TcpDelivered::type():
          oss << TcpDelivered(payload, payload_len);
          break;
        case TcpDeliveredCe::type():
          oss << TcpDeliveredCe(payload, payload_len);
          break;
        case TcpBytesSent::type():
          oss << TcpBytesSent(payload, payload_len);
          break;
        case TcpBytesRetrans::type():
          oss << TcpBytesRetrans(payload, payload_len);
          break;
        case TcpDSackDups::type():
          oss << TcpDSackDups(payload, payload_len);
          break;
        case TcpReordSeen::type():
          oss << TcpReordSeen(payload, payload_len);
          break;
        case TcpSrtt::type():
          oss << TcpSrtt(payload, payload_len);
          break;
        case TcpTimeoutRehash::type():
          oss << TcpTimeoutRehash(payload, payload_len);
          break;
        case TcpBytesNotSent::type():
          oss << TcpBytesNotSent(payload, payload_len);
          break;
        case TcpEdt::type():
          oss << TcpEdt(payload, payload_len);
          break;
        case TcpTtl::type():
          oss << TcpTtl(payload, payload_len);
          break;
        default:
          oss << "attr<" << attr_type << ">: (len: " << payload_len << ")";
      }
      oss << "\n";
    }

    oss << "}";

    return oss.str();
  }

 private:
  std::vector<uint8_t> stats_;
};
#endif  // SCM_TIMESTAMPING_OPT_STATS

class CmsgLevel : public SocketLevel {
 public:
  using SocketLevel::SocketLevel;

  std::string parse(const void *data [[maybe_unused]],
                    size_t data_len [[maybe_unused]]) const {
    if (false) {
#if defined(SOL_IP) && defined(IP_RECVERR)
    } else if (*this == IpRecvErr::socket_level()) {
      return IpRecvErr(data, data_len).to_string();
#endif
#ifdef SO_TIMESTAMP_NEW
    } else if (*this == SocketTimestampNew::socket_level()) {
      return SocketTimestampNew(data, data_len).to_string();
    } else if (*this == SocketTimestampOld::socket_level()) {
      return SocketTimestampOld(data, data_len).to_string();
#elif defined(SO_TIMESTAMP)
    } else if (*this == SocketTimestamp::socket_level()) {
      return SocketTimestamp(data, data_len).to_string();
#endif
#ifdef SO_TIMESTAMPING_NEW
    } else if (*this == SocketTimestampingNew::socket_level()) {
      return SocketTimestampingNew(data, data_len).to_string();
    } else if (*this == SocketTimestampingOld::socket_level()) {
      return SocketTimestampingOld(data, data_len).to_string();
#elif defined(SO_TIMESTAMPING)
    } else if (*this == SocketTimestamping::socket_level()) {
      return SocketTimestamping(data, data_len).to_string();
#endif
#ifdef SO_TIMESTAMPNS_NEW
    } else if (*this == SocketTimestampNanosecondNew::socket_level()) {
      return SocketTimestampNanosecondNew(data, data_len).to_string();
    } else if (*this == SocketTimestampNanosecondOld::socket_level()) {
      return SocketTimestampNanosecondOld(data, data_len).to_string();
#elif defined(SO_TIMESTAMPNS)
    } else if (*this == SocketTimestampNanosecond::socket_level()) {
      return SocketTimestampNanosecond(data, data_len).to_string();
#endif
#ifdef SCM_TIMESTAMPING_OPT_STATS
    } else if (*this == Stats::socket_level()) {
      return Stats(data, data_len).to_string();
#endif
    } else {
      return to_string();
    }
  }

  std::string to_string() const {
    std::string o;
    switch (level()) {
      case SOL_SOCKET:
        o = "socket::";
        switch (type()) {
#ifdef SO_TIMESTAMPING_NEW
          case SO_TIMESTAMPING_OLD:
            return o + "timestamping (old)";
          case SO_TIMESTAMPING_NEW:
            return o + "timestamping (new)";
#elif defined(SO_TIMESTAMPING)
          case SO_TIMESTAMPING:
            return o + "timestamping";
#endif
#ifdef SO_TIMESTAMP_NEW
          case SO_TIMESTAMP_OLD:
            return o + "timestamp (old)";
          case SO_TIMESTAMP_NEW:
            return o + "timestamp (new)";
#elif defined(SO_TIMESTAMP)
          case SO_TIMESTAMP:
            return o + "timestamp";
#endif
#ifdef SO_TIMESTAMPNS_NEW
          case SO_TIMESTAMPNS_OLD:
            return o + "timestamp [ns] (old)";
          case SO_TIMESTAMPNS_NEW:
            return o + "timestamp [ns] (new)";
#elif defined(SO_TIMESTAMPNS)
          case SO_TIMESTAMPNS:
            return o + "timestamp [ns]";
#endif
#ifdef SCM_TIMESTAMPING_OPT_STATS
          case SCM_TIMESTAMPING_OPT_STATS:
            return o + "timestamping::stats";
#endif
          case SO_DEBUG:
            return o + "debug";
          default:
            return o + "<" + std::to_string(type()) + ">";
        }
#ifdef SOL_IP
      case SOL_IP:
#else
      case IPPROTO_IP:
#endif
        o = "ip::";
        switch (type()) {
#ifdef IP_TTL
          case IP_TTL:
            return o + "ttl";
#endif
#ifdef IP_PKTINFO
          case IP_PKTINFO:
            return o + "pktinfo";
#endif
#ifdef IP_RECVERR
          case IP_RECVERR:
            return o + "recverr";
#endif
          default:
            return o + "<" + std::to_string(type()) + ">";
        }
    }
    return std::to_string(level()) + "::" + std::to_string(type());
  }
};

std::ostream &operator<<(std::ostream &os, CmsgLevel l) {
  os << l.to_string();
  return os;
}

namespace local {
// net::socket_base::msg_hdr is protected, let's build a variant which handles
// .control.

class CMsg {
 public:
  CMsg(int lvl, int type, net::const_buffer data)
      : lvl_{lvl}, type_{type}, data_{data} {}

  int level() const { return lvl_; }
  int type() const { return type_; }

  net::const_buffer data() const { return data_; }

 private:
  int lvl_;
  int type_;
  net::const_buffer data_;
};

class msg_hdr : public net::impl::socket::msghdr_base {
  std::array<net::impl::socket::iovec_base, 16> iov_{};

 public:
  /**
   * iterator over the control-message part of a msghdr.
   */
  class cmsg_iterator {
   public:
    using msg_hdr_type = msg_hdr;
#ifdef _WIN32
    using control_msg_hdr_type = WSACMSGHDR;
#else
    using control_msg_hdr_type = cmsghdr;
#endif

    cmsg_iterator(const msg_hdr_type *mhdr)
        : mhdr_{mhdr}, cur_{mhdr_ != nullptr ? first_hdr(mhdr_) : nullptr} {}

    CMsg operator*() { return {level(), type(), data()}; }

    cmsg_iterator &operator++() {
      cur_ = next_hdr();
      return *this;
    }

    bool operator!=(cmsg_iterator &other) { return cur_ != other.cur_; }

   private:
    [[nodiscard]] static constexpr size_t hdr_len() {
#ifdef _WIN32
      return WSA_CMSG_LEN(0);
#else
      return CMSG_LEN(0);
#endif
    }

    [[nodiscard]] control_msg_hdr_type *next_hdr() const {
#ifdef _WIN32
      return WSA_CMSG_NXTHDR(mhdr_, cur_);
#else
      return CMSG_NXTHDR(const_cast<msg_hdr_type *>(mhdr_), cur_);
#endif
    }

    [[nodiscard]] static control_msg_hdr_type *first_hdr(
        const msg_hdr_type *mhdr) {
#ifdef _WIN32
      return WSA_CMSG_FIRSTHDR(mhdr);
#else
      return CMSG_FIRSTHDR(mhdr);
#endif
    }

    [[nodiscard]] int level() const { return cur_->cmsg_level; }
    [[nodiscard]] int type() const { return cur_->cmsg_type; }

    [[nodiscard]] net::const_buffer data() const {
      return {
#ifdef _WIN32
          WSA_CMSG_DATA(cur_),
#else
          CMSG_DATA(cur_),
#endif
          cur_->cmsg_len - hdr_len()};
    }

    const msg_hdr_type *mhdr_;

    control_msg_hdr_type *cur_;
  };
  template <class BufferSequence>
  explicit msg_hdr(const BufferSequence &buffers)
      : net::impl::socket::msghdr_base{} {
    const size_t iovs_capacity = iov_.size();
    const auto bufend = buffer_sequence_end(buffers);
    size_t i = 0;

    for (auto bufcur = buffer_sequence_begin(buffers);
         (i < iovs_capacity) && (bufcur != bufend); ++i, ++bufcur) {
#ifdef _WIN32
      iov_[i].buf =
          const_cast<CHAR *>(reinterpret_cast<const CHAR *>(bufcur->data()));
      iov_[i].len = static_cast<DWORD>(bufcur->size());
#else
      iov_[i] = {const_cast<void *>(bufcur->data()), bufcur->size()};
#endif
    }

#ifdef _WIN32
    this->lpBuffers = iov_.data();
    this->dwBufferCount = static_cast<DWORD>(i);
#else
    this->msg_iov = iov_.data();
    this->msg_iovlen = i;
#endif
  }

  /**
   * set sender of the message.
   *
   * used by the UDP and Linux TCP Fast Open.
   */
  template <class endpoint_type>
  void set_sender(endpoint_type &ep) {
#ifdef _WIN32
    this->name = static_cast<SOCKADDR *>(ep.data());
    this->namelen = ep.capacity();
#else
    this->msg_name = ep.data();
    this->msg_namelen = ep.capacity();
#endif
  }

  /**
   * set the size of the sender after data was received.
   */
  template <class endpoint_type>
  void resize_sender(endpoint_type &ep) {
#ifdef _WIN32
    ep.resize(this->namelen);
#else
    ep.resize(this->msg_namelen);
#endif
  }

  /**
   * set recipient of the message.
   */
  template <class endpoint_type>
  void set_recipient(const endpoint_type &ep) {
#ifdef _WIN32
    this->name =
        const_cast<SOCKADDR *>(static_cast<const SOCKADDR *>(ep.data()));
    this->namelen = ep.size();
#else
    this->msg_name = const_cast<void *>(ep.data());
    this->msg_namelen = ep.size();
#endif
  }

  void set_control(net::mutable_buffer control) {
#ifdef _WIN32
    this->Control.len = control.size();
    this->Control.buf = static_cast<char *>(control.data());
#else
    this->msg_control = control.data();
    this->msg_controllen = control.size();
#endif
  }

  [[nodiscard]] cmsg_iterator begin() const { return {this}; }
  [[nodiscard]] cmsg_iterator end() const { return {nullptr}; }

  int flags() const {
#ifdef _WIN32
    return dwFlags;
#else
    return msg_flags;
#endif
  }
};
}  // namespace local

stdx::expected<size_t, std::error_code> recv_with_cmsg(
    net::ip::tcp::socket &sock, net::mutable_buffer data, int flags) {
  std::array<char, 8192> control{};

  local::msg_hdr mhdr(data);
  mhdr.set_control(net::buffer(control));

  auto recv_res = net::impl::socket::recvmsg(sock.native_handle(), mhdr, flags);
  if (!recv_res) return stdx::make_unexpected(recv_res.error());
#ifdef MSG_ERRQUEUE
  if (mhdr.flags() & MSG_ERRQUEUE) {
    // payload which triggered the error.
    //
    // - without _TSONLY: payload is the original payload
    // - with _TSONLY & !_STATS: payload is empty
    // - with _TSONLY & _STATS: payload is  stats
    auto payload =
        std::string_view(static_cast<const char *>(data.data()), *recv_res);

    std::cerr << __LINE__ << ": payload: (" << payload.size() << ")\n"
              << hexify(payload);
  } else {
    if (mhdr.flags() & MSG_TRUNC) {
      std::cerr << __LINE__
                << ": payload: some discarded "
                   "(payload-buffer too small)"
                << "\n";
    }
  }
#endif

  if (mhdr.flags() & MSG_CTRUNC) {
    std::cerr << __LINE__
              << ": cmsg: some discarded "
                 "(control-buffer too small)"
              << "\n";
  }

  auto it = mhdr.begin();
  auto end = mhdr.end();

  if (it != end) {
    bool is_send{false};
#ifdef MSG_ERRQUEUE
    is_send = (mhdr.flags() & MSG_ERRQUEUE) != 0;
#endif

    std::cerr << (is_send ? ">>" : "<< ") << " <cmsg>\n";
    for (; it != end; ++it) {
      auto cmsg = *it;

      std::cerr << CmsgLevel(cmsg.level(), cmsg.type())
                       .parse(cmsg.data().data(), cmsg.data().size())
                << "\n";
    }
    std::cerr << (is_send ? ">>" : "<< ") << " </cmsg>\n";
  }

  if (data.size() != 0 && *recv_res == 0) {
    return stdx::make_unexpected(make_error_code(net::stream_errc::eof));
  }

  return recv_res;
}

class error_handler {
 public:
  error_handler(net::ip::tcp::socket &sock) : sock_{sock} {}

  stdx::expected<size_t, std::error_code> recv_from_errorqueue() {
#ifdef MSG_ERRQUEUE
    const auto recv_res =
        recv_with_cmsg(sock_, net::mutable_buffer(), MSG_ERRQUEUE);
    if (!recv_res) return recv_res;

    if (*recv_res != 0) {
      std::cerr << __LINE__ << ": ERR: "
                << "OK: " << recv_res.value() << "\n";
    }

    return recv_res;
#else
    return stdx::make_unexpected(
        make_error_code(std::errc::operation_not_supported));
#endif
  }

  void operator()(std::error_code ec) {
    if (ec) return;

    auto recv_res = recv_from_errorqueue();
    if (recv_res) {
      sock_.async_wait(net::socket_base::wait_error, error_handler(sock_));
      return;
    }

    // If there is nothing in the error queue, there must be an event
    // on the normal socket.
    //
    // If the socket isn't closed yet, drain it and wait for the
    // close.

    std::array<uint8_t, 1024> discard_buf{};

    // set the socket non-blocking for the receive() in case there is nothing on
    // the socket.
    //
    // Linux has MSG_DONTWAIT, but windows doesn't.

    int flags{
#ifdef MSG_DONTWAIT
        MSG_DONTWAIT
#endif
    };

    auto was_non_blocking = (flags != 0) || sock_.native_non_blocking();
    if (!was_non_blocking) sock_.native_non_blocking(true);

    const auto discard_res = sock_.receive(net::buffer(discard_buf), flags);
    if (!discard_res) {
      if (discard_res.error() == net::stream_errc::eof) {
        std::cerr << __LINE__ << ": HUP -> close\n";
        sock_.close();
        return;
      } else {
        std::cerr << __LINE__ << ": ERR: "
                  << "DISCARD: " << discard_res.error() << "\n";
      }
    } else {
      // looks like there was no read handler in
      // place before the HUP arrived.
      std::cerr << __LINE__ << ": ERR: "
                << "DISCARD: " << *discard_res << "\n"
                << hexify(net::buffer(discard_buf.data(), *discard_res));
    }

    if (!was_non_blocking) sock_.native_non_blocking(false);

    sock_.async_wait(net::socket_base::wait_error, error_handler(sock_));
  }

 private:
  net::ip::tcp::socket &sock_;
};

class read_handler {
 public:
  read_handler(net::ip::tcp::socket &sock) : sock_{sock} {}

  void operator()(std::error_code ec);

 private:
  net::ip::tcp::socket &sock_;
};

void read_handler::operator()(std::error_code ec) {
  if (ec) {
    std::cerr << __LINE__ << ": IN: " << ec << " \n";
    return;
  }

  std::array<char, 1024> data;

  auto read_res = recv_with_cmsg(sock_, net::buffer(data), 0);
  if (!read_res) {
    if (read_res.error() == net::stream_errc::eof) {
      std::cerr << __LINE__ << ": IN -> close"
                << "\n";

      sock_.close();
      return;
    }
    std::cerr << __LINE__ << ": IN: " << read_res.error() << "\n";
    return;
  }

  std::cerr << "<< " << std::string_view(data.data(), *read_res) << "\n";

  // read more
  sock_.async_wait(net::socket_base::wait_read, read_handler(sock_));
}

// https://www.kernel.org/doc/Documentation/networking/timestamping.txt
// is a good usecase of handling EPOLLERR.
//
//  SOF_TIMESTAMPING_RX_SOFTWARE  | 2.6.30 | rx time from data into kernel
//  SOF_TIMESTAMPING_RX_HARDWARE  | 2.6.30 | rx time from network adapter
//  SOF_TIMESTAMPING_TX_SCHED     | 3.17   | tx time when queue
//  SOF_TIMESTAMPING_TX_SOFTWARE  | 2.6.30 | tx time before network adapter
//  SOF_TIMESTAMPING_TX_HARDWARE  | 2.6.30 | tx time from network adapter
//  SOF_TIMESTAMPING_TX_ACK       | 3.17   | tx time after all ACKed
//  SOF_TIMESTAMPING_SOFTWARE     | 2.6.30 | report software timestamps
//  SOF_TIMESTAMPING_RAW_HARDWARE | 2.6.30 | report hardware timestamps
//  SOF_TIMESTAMPING_OPT_ID       | 3.17   | ee_data is a an ID
//  SOF_TIMESTAMPING_OPT_CMSG     | 3.19   |
//  SOF_TIMESTAMPING_OPT_TSONLY   | 4.0    | timestamps only in cmsg
//  SOF_TIMESTAMPING_OPT_STATS    | 4.10   | stats
//  SOF_TIMESTAMPING_OPT_PKTINFO  | 4.13   |
//  SOF_TIMESTAMPING_OPT_TX_SWHW  | 4.13   | get both HW and SW timestamps

enum class SocketTimestampingFlags {
  TX_HARDWARE = 1 << 0,
  TX_SOFTWARE = 1 << 1,
  RX_HARDWARE = 1 << 2,
  RX_SOFTWARE = 1 << 3,
  SOFTWARE = 1 << 4,
  SYS_HARDWARE = 1 << 5,
  RAW_HARDWARE = 1 << 6,
  OPT_ID = 1 << 7,
  TX_SCHED = 1 << 8,
  TX_ACK = 1 << 9,
  OPT_CMSG = 1 << 10,
  OPT_TSONLY = 1 << 11,
  OPT_STATS = 1 << 12,
  OPT_PKTINFO = 1 << 13,
  OPT_TX_SWHW = 1 << 14,
  BIND_PHC = 1 << 15,
};

namespace stdx {
template <>
struct is_flags<SocketTimestampingFlags> : std::true_type {};
}  // namespace stdx

#ifdef __linux__
static constexpr const stdx::flags<SocketTimestampingFlags>
    set_timestamping_vals_2_6_30 = SocketTimestampingFlags::RX_SOFTWARE |  //
                                   SocketTimestampingFlags::RX_HARDWARE |  //
                                   SocketTimestampingFlags::TX_SOFTWARE |  //
                                   SocketTimestampingFlags::TX_HARDWARE |  //
                                   SocketTimestampingFlags::SOFTWARE |     //
                                   SocketTimestampingFlags::RAW_HARDWARE;  //

static constexpr const stdx::flags<SocketTimestampingFlags>
    set_timestamping_vals_3_17 =
        set_timestamping_vals_2_6_30 |       // use old settings
        SocketTimestampingFlags::TX_SCHED |  //
        SocketTimestampingFlags::TX_ACK |    //
        SocketTimestampingFlags::OPT_ID;     //

static constexpr const stdx::flags<SocketTimestampingFlags>
    set_timestamping_vals_4_0 =
        set_timestamping_vals_3_17 |          // use old settings
        SocketTimestampingFlags::OPT_TSONLY;  // timestamps only in cmsg

static constexpr const stdx::flags<SocketTimestampingFlags>
    set_timestamping_vals_4_10 =
        set_timestamping_vals_4_0 |          // use old settings
        SocketTimestampingFlags::OPT_STATS;  // stats

static constexpr const stdx::flags<SocketTimestampingFlags>
    set_timestamping_vals_4_13 =
        set_timestamping_vals_4_10 |  // use old settings
        SocketTimestampingFlags::OPT_TX_SWHW;
#endif

#ifdef __linux__
using socket_timestamping =
    net::socket_option::integer<SocketTimestamping::level(),
                                SocketTimestamping::type()>;

using socket_timestamp = net::socket_option::integer<SocketTimestamp::level(),
                                                     SocketTimestamp::type()>;

using socket_timestamp_ns =
    net::socket_option::integer<SocketTimestampNanosecond::level(),
                                SocketTimestampNanosecond::type()>;
#endif

stdx::expected<void, std::error_code> run() {
  net::io_context io_ctx;

  net::ip::tcp::socket sock(io_ctx);

  // www.oracle.com
  auto connect_res = sock.connect(net::ip::tcp::endpoint{
      net::ip::make_address("137.254.120.50").value(), 80});
  if (!connect_res) return stdx::make_unexpected(connect_res.error());

  sock.async_wait(net::socket_base::wait_error, error_handler(sock));

#ifdef __linux__
  auto set_sock_opt =
      sock.set_option(socket_timestamping(
                          set_timestamping_vals_4_13.underlying_value()))
          .or_else([&](std::error_code /* ec */) {
            return sock.set_option(socket_timestamping(
                set_timestamping_vals_4_10.underlying_value()));
          })
          .or_else([&](std::error_code /* ec */) {
            return sock.set_option(socket_timestamping(
                set_timestamping_vals_4_0.underlying_value()));
          })
          .or_else([&](std::error_code /* ec */) {
            return sock.set_option(socket_timestamping(
                set_timestamping_vals_3_17.underlying_value()));
          })
          .or_else([&](std::error_code /* ec */) {
            return sock.set_option(socket_timestamping(
                set_timestamping_vals_2_6_30.underlying_value()));
          })
          .or_else([&](std::error_code /* ec */) {
            return sock.set_option(socket_timestamp_ns(1));
          })
          .or_else([&](std::error_code /* ec */) {
            return sock.set_option(socket_timestamp(1));
          });
  if (!set_sock_opt) {
    std::cerr << "!! couldn't set any timestamping option: "
              << set_sock_opt.error() << ", continuing\n";
  }
#endif

  using namespace std::chrono_literals;

  std::this_thread::sleep_for(20ms);

  // make sure the 'wait_read' fires too.
  std::string_view payload{"GET / HTTP/1.0\r\n\r\n"};

  std::cerr << ">> sending (size=" << payload.size() << ")\n"
            << payload << "\n";
  sock.write_some(net::buffer(payload));

  std::cerr << ">> shuting down send-side\n";
  sock.shutdown(net::socket_base::shutdown_send);

  //  sock.async_wait(net::socket_base::wait_read,
  //  read_handler(sock));

  io_ctx.run();

  return {};
}

/**
 * Linux Timestamping
 */

int main() {
  const auto run_res = run();
  if (!run_res) {
    std::cerr << "ERROR: " << run_res.error().message() << " "
              << run_res.error() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

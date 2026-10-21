//
// Copyright (c) 2025 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_COBALT_IO_ENDPOINT_HPP
#define BOOST_COBALT_IO_ENDPOINT_HPP

#include <boost/cobalt/io/detail/config.hpp>
#include <boost/cobalt/config.hpp>
#include <boost/cobalt/detail/exception.hpp>

#include <boost/asio/detail/socket_types.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/static_string.hpp>
#include <boost/system/result.hpp>

#include <array>
#include <algorithm>

#if defined(BOOST_COBALT_NO_PMR)
#include <boost/container/pmr/vector.hpp>
#else
#include <vector>
#endif

#include <span>

namespace boost::cobalt::detail
{

BOOST_COBALT_IO_DECL BOOST_NORETURN void
throw_bad_endpoint_access(
    boost::source_location const& loc);

}

namespace boost::cobalt::io
{

struct endpoint;
struct stream_socket;


#if __GNUC__ && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#endif

struct protocol_type
{
  using family_t   = int;
  using type_t     = int;
  using protocol_t = int;

  constexpr family_t     family() const noexcept {return family_;};
  constexpr type_t         type() const noexcept {return type_;};
  constexpr protocol_t protocol() const noexcept {return protocol_;};

  constexpr explicit
  protocol_type(family_t family     = static_cast<family_t>(0),
                type_t type         = static_cast<type_t>(0),
                protocol_t protocol = static_cast<protocol_t>(0)) noexcept
      : family_(family), type_(type), protocol_(protocol)
  {}

  template<typename OtherProtocol>
    requires requires (const OtherProtocol & op)
      {
        {static_cast<family_t>(op.family())};
        {static_cast<type_t>(op.type())};
        {static_cast<protocol_t>(op.protocol())};
      }
  constexpr protocol_type(const OtherProtocol & op) noexcept
      : family_(static_cast<family_t>(op.family()))
      , type_(static_cast<type_t>(op.type()))
      , protocol_(static_cast<protocol_t>(op.protocol()))
  {}

  friend
  constexpr auto operator<=>(const protocol_type & , const protocol_type &) noexcept = default;

  using endpoint = io::endpoint;
  // for the asio acceptor
  using socket = stream_socket;
 private:
  family_t family_     = static_cast<family_t>(0);
  type_t type_         = static_cast<type_t>(0);
  protocol_t protocol_ = static_cast<protocol_t>(0);
};

template<protocol_type::family_t Family     = static_cast<protocol_type::family_t>(0),
         protocol_type::type_t Type         = static_cast<protocol_type::type_t>(0),
         protocol_type::protocol_t Protocol = static_cast<protocol_type::protocol_t>(0)>
struct static_protocol
{
  using family_t   = protocol_type::family_t  ;
  using type_t     = protocol_type::type_t    ;
  using protocol_t = protocol_type::protocol_t;

  constexpr family_t   family()   const noexcept {return family_t(Family);     };
  constexpr type_t     type()     const noexcept {return type_t(Type);         };
  constexpr protocol_t protocol() const noexcept {return protocol_t(Protocol); };

  using endpoint = io::endpoint;
};


constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_UNSPEC), static_cast<protocol_type::type_t>(0), BOOST_ASIO_OS_DEF(IPPROTO_IP)>   ip    {};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET),   static_cast<protocol_type::type_t>(0), BOOST_ASIO_OS_DEF(IPPROTO_IP)>   ip_v4 {};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET6),  static_cast<protocol_type::type_t>(0), BOOST_ASIO_OS_DEF(IPPROTO_IP)>   ip_v6 {};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_UNSPEC), BOOST_ASIO_OS_DEF(SOCK_STREAM), BOOST_ASIO_OS_DEF(IPPROTO_TCP)>  tcp   {};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET),   BOOST_ASIO_OS_DEF(SOCK_STREAM), BOOST_ASIO_OS_DEF(IPPROTO_TCP)>  tcp_v4{};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET6),  BOOST_ASIO_OS_DEF(SOCK_STREAM), BOOST_ASIO_OS_DEF(IPPROTO_TCP)>  tcp_v6{};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_UNSPEC), BOOST_ASIO_OS_DEF(SOCK_DGRAM),  BOOST_ASIO_OS_DEF(IPPROTO_UDP)>  udp   {};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET),   BOOST_ASIO_OS_DEF(SOCK_DGRAM),  BOOST_ASIO_OS_DEF(IPPROTO_UDP)>  udp_v4{};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET6),  BOOST_ASIO_OS_DEF(SOCK_DGRAM),  BOOST_ASIO_OS_DEF(IPPROTO_ICMP)> udp_v6{};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_UNSPEC), BOOST_ASIO_OS_DEF(SOCK_DGRAM),  BOOST_ASIO_OS_DEF(IPPROTO_ICMP)> icmp  {};
constexpr static_protocol<AF_UNIX,                      BOOST_ASIO_OS_DEF(SOCK_STREAM)>    local_stream   {};
constexpr static_protocol<AF_UNIX,                      BOOST_ASIO_OS_DEF(SOCK_DGRAM)>     local_datagram {};
constexpr static_protocol<AF_UNIX,                      BOOST_ASIO_OS_DEF(SOCK_SEQPACKET)> local_seqpacket{};
constexpr static_protocol<AF_UNIX>                                                         local_protocol {};

#if defined(IPPROTO_SCTP)
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_UNSPEC), BOOST_ASIO_OS_DEF(SOCK_SEQPACKET), IPPROTO_SCTP>  sctp   {};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET),   BOOST_ASIO_OS_DEF(SOCK_SEQPACKET), IPPROTO_SCTP>  sctp_v4{};
constexpr static_protocol<BOOST_ASIO_OS_DEF(AF_INET6),  BOOST_ASIO_OS_DEF(SOCK_SEQPACKET), IPPROTO_SCTP>  sctp_v6{};
#endif

template<protocol_type::family_t Family>
struct make_endpoint_tag {};

template<protocol_type::family_t Family>
struct get_endpoint_tag {};

struct endpoint
{
  using storage_type = asio::detail::sockaddr_storage_type;
  using addr_type = asio::detail::socket_addr_type;
  void resize(std::size_t size)
  {
    BOOST_ASSERT(size < sizeof(storage_));
    size_ = size;
  }

        void * data()       {return &storage_; }
  const void * data() const {return &storage_; }
  std::size_t size() const {return size_;}
  std::size_t capacity() const {return sizeof(storage_);}

  void set_type    (protocol_type::type_t type)         { type_ = type;}
  void set_protocol(protocol_type::protocol_t protocol) { protocol_ = protocol;}

  protocol_type protocol() const
  {
    return protocol_type{static_cast<protocol_type::family_t>(base_.sa_family), type_, protocol_};
  }

  endpoint() = default;
  endpoint(const endpoint & ep) : storage_(ep.storage_), size_(ep.size_), protocol_(ep.protocol_), type_(ep.type_)
  {
  }

  template<protocol_type::family_t   Family,
           protocol_type::type_t     Type,
           protocol_type::protocol_t Protocol,
           typename ... Args>
      requires requires (make_endpoint_tag<Family> proto,
                         addr_type* addr, Args && ... args)
      {
        {tag_invoke(proto, addr, std::forward<Args>(args)...)} -> std::convertible_to<std::size_t>;
      }
  endpoint(static_protocol<Family, Type, Protocol> proto, Args && ... args)
      : base_{},
        protocol_(Protocol), type_(Type)
  {
    size_ = tag_invoke(make_endpoint_tag<Family>{}, &base_, std::forward<Args>(args)...);
  }

  template<typename OtherEndpoint>
  requires requires (OtherEndpoint oe)
      {
        {oe.protocol()} -> std::convertible_to<protocol_type>;
        {oe.data()} -> std::convertible_to<void*>;
        {oe.size()} -> std::convertible_to<std::size_t>;
      }
  endpoint(OtherEndpoint && oe) :
        base_{},
        protocol_(static_cast<protocol_type::protocol_t>(oe.protocol().protocol())),
        type_(static_cast<protocol_type::type_t>(oe.protocol().type()))
  {
    resize(oe.size());
    memcpy(data(), oe.data(), size());
  }

  template<static_protocol Protocol>
  requires requires (get_endpoint_tag<Protocol.family()> tag,
                     protocol_type actual,
                     const addr_type * addr) {{tag_invoke(tag, actual, addr)};}
  friend auto get_if(const endpoint * ep)
        -> decltype(tag_invoke(get_endpoint_tag<Protocol.family()>{}, protocol_type{},
                               static_cast<const addr_type *>(nullptr)))
  {
    const auto actual = ep->protocol();
    if (Protocol.type()     != 0 && actual.type()     != 0 && actual.type()     != Protocol.type())
      return nullptr;
    if (Protocol.protocol() != 0 && actual.protocol() != 0 && actual.protocol() != Protocol.protocol())
      return nullptr;
    return tag_invoke(get_endpoint_tag<Protocol.family()>{}, ep->protocol(), &ep->base_);
  }

 private:
  union {
    asio::detail::socket_addr_type base_{};
    storage_type storage_;
  };
  std::size_t size_{sizeof(base_)};
  protocol_type::protocol_t protocol_ = static_cast<protocol_type::protocol_t>(0);
  protocol_type::type_t         type_ = static_cast<protocol_type::type_t>(0);
};

#if __GNUC__ && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#if defined(BOOST_COBALT_NO_PMR)
using endpoint_sequence = std::vector<endpoint>;
#else
using endpoint_sequence = pmr::vector<endpoint>;
#endif


class bad_endpoint_access : public std::exception
{
 public:

  bad_endpoint_access() noexcept = default;

  char const * what() const noexcept
  {
    return "bad_endpoint_access";
  }
};

template<static_protocol Protocol>
    requires requires (get_endpoint_tag<Protocol.family()> tag,
                       protocol_type actual,
                       endpoint::addr_type * addr) {{tag_invoke(tag, actual, addr)};}
auto get(const endpoint & ep, const boost::source_location & loc = BOOST_CURRENT_LOCATION)
{
  auto e = get_if<Protocol>(&ep);
  if (!e)
    cobalt::detail::throw_bad_endpoint_access(loc);
  return *e;
}

struct local_endpoint
{
  std::string_view path() const { return unix_.sun_path;}
 private:
  union {
    asio::detail::sockaddr_storage_type addr_;
    asio::detail::sockaddr_un_type unix_;
  };
};

BOOST_COBALT_IO_DECL
std::size_t tag_invoke(make_endpoint_tag<AF_UNIX>,
                       asio::detail::socket_addr_type* base,
                       std::string_view sv);

BOOST_COBALT_IO_DECL
const local_endpoint* tag_invoke(get_endpoint_tag<AF_UNIX>,
                                 protocol_type actual,
                                 const endpoint::addr_type * addr);

struct ip_address_v4
{
    std::uint16_t port() const {return boost::endian::big_to_native(in_.sin_port);}
    std::uint32_t addr() const {return in_.sin_addr.s_addr;}
    BOOST_COBALT_IO_DECL boost::static_string<15> addr_str() const;
 private:
  union {
    asio::detail::sockaddr_storage_type addr_{};
    asio::detail::sockaddr_in4_type in_;
  };
};

BOOST_COBALT_IO_DECL
std::size_t tag_invoke(make_endpoint_tag<AF_INET>,
                       asio::detail::socket_addr_type* base,
                       std::uint32_t address,
                       std::uint16_t port);

BOOST_COBALT_IO_DECL
std::size_t tag_invoke(make_endpoint_tag<AF_INET>,
                       asio::detail::socket_addr_type* base,
                       std::string_view address,
                       std::uint16_t port);

BOOST_COBALT_IO_DECL
const ip_address_v4* tag_invoke(get_endpoint_tag<AF_INET>,
                                 protocol_type actual,
                                 const endpoint::addr_type * addr);


struct ip_address_v6
{
  std::uint16_t port() const {return boost::endian::big_to_native(in_.sin6_port);}
  std::array<std::uint8_t, 16u> addr() const
  {
    std::array<std::uint8_t, 16u> res;
    const auto & in = in_.sin6_addr.s6_addr;
    std::copy(std::begin(in), std::end(in), res.begin());
    return res;
  }
  BOOST_COBALT_IO_DECL boost::static_string<45> addr_str() const;
 private:
  union {
    asio::detail::sockaddr_storage_type addr_{};
    asio::detail::sockaddr_in6_type in_;
  };
};

BOOST_COBALT_IO_DECL
std::size_t tag_invoke(make_endpoint_tag<AF_INET6>,
                       asio::detail::socket_addr_type* base,
                       std::span<std::uint8_t, 16> address,
                       std::uint16_t port);

BOOST_COBALT_IO_DECL
std::size_t tag_invoke(make_endpoint_tag<AF_INET6>,
                       asio::detail::socket_addr_type* base,
                       std::string_view address,
                       std::uint16_t port);

BOOST_COBALT_IO_DECL
const ip_address_v6* tag_invoke(get_endpoint_tag<AF_INET6>,
                                protocol_type actual,
                                const endpoint::addr_type * addr);


struct BOOST_SYMBOL_VISIBLE ip_address
{

  bool is_ipv6() const { return addr_.ss_family == BOOST_ASIO_OS_DEF(AF_INET6); }
  bool is_ipv4() const { return addr_.ss_family == BOOST_ASIO_OS_DEF(AF_INET); }

  std::uint16_t port() const {return boost::endian::big_to_native(addr_.ss_family == AF_INET ? in_.sin_port : in6_.sin6_port);}

  BOOST_COBALT_IO_DECL std::array<std::uint8_t, 16u> addr() const;
  BOOST_COBALT_IO_DECL boost::static_string<45> addr_str() const;
 private:
  union {
    asio::detail::sockaddr_storage_type addr_{};
    asio::detail::sockaddr_in4_type in_;
    asio::detail::sockaddr_in6_type in6_;
  };
};

BOOST_COBALT_IO_DECL
std::size_t tag_invoke(make_endpoint_tag<AF_UNSPEC>,
                       asio::detail::socket_addr_type* base,
                       std::string_view address,
                       std::uint16_t port);

BOOST_COBALT_IO_DECL
const ip_address* tag_invoke(get_endpoint_tag<AF_UNSPEC>,
                                protocol_type actual,
                                const endpoint::addr_type * addr);


}

#endif //BOOST_COBALT_IO_ENDPOINT_HPP

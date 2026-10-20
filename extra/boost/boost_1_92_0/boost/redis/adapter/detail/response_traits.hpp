/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ADAPTER_DETAIL_RESPONSE_TRAITS_HPP
#define BOOST_REDIS_ADAPTER_DETAIL_RESPONSE_TRAITS_HPP

#include <boost/redis/adapter/detail/result_traits.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/response.hpp>

#include <boost/mp11.hpp>
#include <boost/system.hpp>

#include <limits>
#include <string_view>
#include <tuple>
#include <variant>

namespace boost::redis::adapter::detail {

template <class Response>
class static_adapter {
private:
   static constexpr auto size = std::tuple_size<Response>::value;
   using adapter_tuple = mp11::mp_transform<adapter_t, Response>;
   using variant_type = mp11::mp_rename<adapter_tuple, std::variant>;
   using adapters_array_type = std::array<variant_type, size>;

   adapters_array_type adapters_;
   std::size_t i_ = 0;

public:
   explicit static_adapter(Response& r) { assigner<size - 1>::assign(adapters_, r); }

   void on_init()
   {
      using std::visit;
      visit(
         [&](auto& arg) {
            arg.on_init();
         },
         adapters_.at(i_));
   }

   void on_done()
   {
      using std::visit;
      visit(
         [&](auto& arg) {
            arg.on_done();
         },
         adapters_.at(i_));
      i_ += 1;
   }

   template <class String>
   void on_node(resp3::basic_node<String> const& nd, system::error_code& ec)
   {
      using std::visit;

      // I am usure whether this should be an error or an assertion.
      BOOST_ASSERT(i_ < adapters_.size());
      visit(
         [&](auto& arg) {
            arg.on_node(nd, ec);
         },
         adapters_.at(i_));
   }
};

template <class>
struct response_traits;

template <>
struct response_traits<ignore_t> {
   using response_type = ignore_t;
   using adapter_type = ignore;

   static auto adapt(response_type&) noexcept { return ignore{}; }
};

template <>
struct response_traits<result<ignore_t>> {
   using response_type = result<ignore_t>;
   using adapter_type = ignore;

   static auto adapt(response_type&) noexcept { return ignore{}; }
};

template <class String, class Allocator>
struct response_traits<result<resp3::basic_tree<String, Allocator>>> {
   using response_type = result<resp3::basic_tree<String, Allocator>>;
   using adapter_type = general_aggregate<response_type>;

   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class String>
struct response_traits<resp3::basic_tree<String>> {
   using response_type = resp3::basic_tree<String>;
   using adapter_type = general_aggregate<response_type>;

   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <>
struct response_traits<resp3::flat_tree> {
   using response_type = resp3::flat_tree;
   using adapter_type = general_aggregate<response_type>;

   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <>
struct response_traits<generic_flat_response> {
   using response_type = generic_flat_response;
   using adapter_type = general_aggregate<response_type>;

   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class... Ts>
struct response_traits<response<Ts...>> {
   using response_type = response<Ts...>;
   using adapter_type = static_adapter<response_type>;

   static auto adapt(response_type& r) noexcept { return adapter_type{r}; }
};

}  // namespace boost::redis::adapter::detail

#endif  // BOOST_REDIS_ADAPTER_DETAIL_RESPONSE_TRAITS_HPP

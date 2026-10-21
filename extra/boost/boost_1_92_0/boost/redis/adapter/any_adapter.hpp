/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_ANY_ADAPTER_HPP
#define BOOST_REDIS_ANY_ADAPTER_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/resp3/node.hpp>

#include <boost/system/error_code.hpp>

#include <functional>
#include <type_traits>

namespace boost::redis {

/** @brief A type-erased reference to a response.
 *
 *  A type-erased response adapter. It can be executed using @ref connection::async_exec.
 *  Using this type instead of raw response references enables separate compilation.
 *
 *  Given a response object `resp` that can be passed to `async_exec`, the following two
 *  statements have the same effect:
 *
 *  @code
 *      co_await conn.async_exec(req, resp);
 *      co_await conn.async_exec(req, any_response(resp));
 *  @endcode
 */
class any_adapter {
public:
   /** @brief Parse events that an adapter must support.
    */
   enum class parse_event
   {
      /// Called before the parser starts processing data
      init,
      /// Called for each and every node of RESP3 data
      node,
      /// Called when done processing a complete RESP3 message
      done
   };

   /// The type erased implementation type.
   using impl_t = std::function<void(parse_event, resp3::node_view const&, system::error_code&)>;

   template <class T>
   static auto create_impl(T& resp) -> impl_t;

   /// Contructs from a type erased adaper
   any_adapter(impl_t fn = [](parse_event, resp3::node_view const&, system::error_code&) { })
   : impl_{std::move(fn)}
   { }

   /**
     * @brief Constructor.
     * 
     * Creates a type-erased response adapter from `resp` by calling
     * `boost_redis_adapt`. `T` must be a valid Redis response type.
     * Any type passed to @ref connection::async_exec qualifies.
     *
     * This object stores a reference to `resp`, which must be kept alive
     * while `*this` is being used.
     */
   template <class T, class = std::enable_if_t<!std::is_same_v<T, any_adapter>>>
   explicit any_adapter(T& resp)
   : impl_(create_impl(resp))
   { }

   /// Calls the implementation with the arguments `impl_(parse_event::init, ...);`
   void on_init()
   {
      system::error_code ec;
      impl_(parse_event::init, {}, ec);
   };

   /// Calls the implementation with the arguments `impl_(parse_event::done, ...);`
   void on_done()
   {
      system::error_code ec;
      impl_(parse_event::done, {}, ec);
   };

   /// Calls the implementation with the arguments `impl_(parse_event::node, ...);`
   void on_node(resp3::node_view const& nd, system::error_code& ec)
   {
      impl_(parse_event::node, nd, ec);
   };

private:
   impl_t impl_;
};

namespace detail {

template <class Adapter>
any_adapter::impl_t make_any_adapter_impl(Adapter&& value)
{
   return [adapter = std::move(value)](
             any_adapter::parse_event ev,
             resp3::node_view const& nd,
             system::error_code& ec) mutable {
      switch (ev) {
         case any_adapter::parse_event::init: adapter.on_init(); break;
         case any_adapter::parse_event::node: adapter.on_node(nd, ec); break;
         case any_adapter::parse_event::done: adapter.on_done(); break;
      }
   };
}

}  // namespace detail

}  // namespace boost::redis

template <class T>
auto boost::redis::any_adapter::create_impl(T& resp) -> impl_t
{
   using adapter::boost_redis_adapt;
   return detail::make_any_adapter_impl(boost_redis_adapt(resp));
}

#endif

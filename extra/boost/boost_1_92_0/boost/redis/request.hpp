/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_REQUEST_HPP
#define BOOST_REDIS_REQUEST_HPP

#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>

#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

// NOTE: For some commands like hset it would be a good idea to assert
// the value type is a pair.

namespace boost::redis {

namespace detail {
auto has_response(std::string_view cmd) -> bool;
struct request_access;

enum class pubsub_change_type
{
   subscribe,
   unsubscribe,
   psubscribe,
   punsubscribe,
};

struct pubsub_change {
   pubsub_change_type type;
   std::size_t channel_offset;
   std::size_t channel_size;
};

}  // namespace detail

/** @brief Represents a Redis request.
 *  
 *  A request is composed of one or more Redis commands and is
 *  referred to in the redis documentation as a pipeline. See
 *  <a href="https://redis.io/docs/latest/develop/using-commands/pipelining/"></a> for more info.
 *
 *  For example:
 *
 *  @code
 *  request r;
 *  r.push("SET", "k1", "some_value");
 *  r.push("SET", "k2", "other_value");
 *  r.push("GET", "k3");
 *  @endcode
 *
 *  Uses a `std::string` for internal storage.
 */
class request {
public:
   /// Request configuration options.
   struct config {
      /** @brief (Deprecated) If `true`, calls to @ref basic_connection::async_exec will
       * complete with error if the connection is lost while the
       * request hasn't been sent yet.
       *
       * @par Deprecated
       * This setting is deprecated and should be always left out as the default
       * (waiting for a connection to be established again).
       * If you need to limit how much time a @ref basic_connection::async_exec
       * operation is allowed to take, use `asio::cancel_after`, instead.
       */
      bool cancel_on_connection_lost = false;

      /** @brief (Deprecated) If `true`, @ref basic_connection::async_exec will complete with
       * @ref boost::redis::error::not_connected if the call happens
       * before the connection with Redis was established.
       *
       * @par Deprecated
       * This setting is deprecated and should be always left out as the default
       * (waiting for a connection to be established).
       * If you need to limit how much time a @ref basic_connection::async_exec
       * operation is allowed to take, use `asio::cancel_after`, instead.
       */
      bool cancel_if_not_connected = false;

      /** @brief If `false`, @ref basic_connection::async_exec will not
       * automatically cancel this request if the connection is lost.
       * Affects only requests that have been written to the server
       * but have not been responded when
       * the connection is lost.
       */
      bool cancel_if_unresponded = true;

      /** @brief (Deprecated) If this request has a `HELLO` command and this flag
       * is `true`, it will be moved to the
       * front of the queue of awaiting requests. This makes it
       * possible to send `HELLO` commands and authenticate before other
       * commands are sent.
       *
       * @par Deprecated
       * This field has been superseded by @ref config::setup.
       * This setup request will always be run first on connection establishment.
       * Please use it to run any required setup commands.
       * This field will be removed in subsequent releases.
       */
      bool hello_with_priority = true;
   };

   /** @brief Constructor
    *  
    *  @param cfg Configuration options.
    */
   explicit request(config cfg = config{false, false, true, true})
   : cfg_{cfg}
   { }

   /// Returns the number of responses expected for this request.
   [[nodiscard]] auto get_expected_responses() const noexcept -> std::size_t
   {
      return expected_responses_;
   };

   /// Returns the number of commands contained in this request.
   [[nodiscard]] auto get_commands() const noexcept -> std::size_t { return commands_; };

   [[nodiscard]] auto payload() const noexcept -> std::string_view { return payload_; }

   [[nodiscard]]
   BOOST_DEPRECATED(
      "The hello_with_priority attribute and related functions are deprecated. "
      "Use config::setup to run setup commands, instead.") auto has_hello_priority() const noexcept
      -> auto const&
   {
      return has_hello_priority_;
   }

   /// Clears the request preserving allocated memory.
   void clear()
   {
      payload_.clear();
      pubsub_changes_.clear();
      commands_ = 0;
      expected_responses_ = 0;
      has_hello_priority_ = false;
   }

   /// Calls `std::string::reserve` on the internal storage.
   void reserve(std::size_t new_cap = 0) { payload_.reserve(new_cap); }

   /// Returns a reference to the config object.
   [[nodiscard]] auto get_config() const noexcept -> config const& { return cfg_; }

   /// Returns a reference to the config object.
   [[nodiscard]] auto get_config() noexcept -> config& { return cfg_; }

   /** @brief Appends a new command to the end of the request.
    *
    *  For example:
    *
    *  @code
    *  request req;
    *  req.push("SET", "key", "some string", "EX", 2);
    *  @endcode
    *
    *  This will add a `SET` command with value `"some string"` and an
    *  expiration of 2 seconds.
    *
    *  Command arguments should either be convertible to `std::string_view`,
    *  integral types, or support the `boost_redis_to_bulk` function.
    *  This function is a customization point that must be made available
    *  using ADL and must have the following signature:
    *
    *  @code
    *  void boost_redis_to_bulk(std::string& to, T const& t);
    *  @endcode
    *
    *
    *  See cpp20_serialization.cpp
    *
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param args Command arguments. `args` is allowed to be empty.
    *  @tparam Ts Types of the command arguments.
    *
    */
   template <class... Ts>
   void push(std::string_view cmd, Ts const&... args)
   {
      auto constexpr pack_size = sizeof...(Ts);
      resp3::add_header(payload_, resp3::type::array, 1 + pack_size);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, std::tie(std::forward<Ts const&>(args)...));

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  This overload is useful for commands that have a key and have a
    *  dynamic range of arguments. For example:
    *
    *  @code
    *  std::map<std::string, std::string> map
    *     { {"key1", "value1"}
    *     , {"key2", "value2"}
    *     , {"key3", "value3"}
    *     };
    *
    *  request req;
    *  req.push_range("HSET", "key", map.cbegin(), map.cend());
    *  @endcode
    *
    *  This will generate the following command:
    *
    *  @code
    *  HSET key key1 value1 key2 value2 key3 value3
    *  @endcode
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed. 
    *        
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param key The command key. It will be added as the first argument to the command.
    *  @param begin Iterator to the begin of the range.
    *  @param end Iterator to the end of the range.
    *  @tparam ForwardIterator A forward iterator with an element type that supports one of the points above.
    *
    *  See cpp20_serialization.cpp
    */
   template <class ForwardIterator>
   void push_range(
      std::string_view cmd,
      std::string_view key,
      ForwardIterator begin,
      ForwardIterator end,
      typename std::iterator_traits<ForwardIterator>::value_type* = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, resp3::type::array, 2 + size * distance);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, key);

      for (; begin != end; ++begin)
         resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  This overload is useful for commands that have a dynamic number
    *  of arguments and don't have a key. For example:
    *
    *  @code
    *  std::set<std::string> keys
    *     { "key1" , "key2" , "key3" };
    *
    *  request req;
    *  req.push("MGET", keys.begin(), keys.end());
    *  @endcode
    *
    *  This will generate the following command:
    *
    *  @code
    *  MGET key1 key2 key3
    *  @endcode
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param begin Iterator to the begin of the range.
    *  @param end Iterator to the end of the range.
    *  @tparam ForwardIterator A forward iterator with an element type that supports one of the points above.
    *
    *  See cpp20_serialization.cpp
    */
   template <class ForwardIterator>
   void push_range(
      std::string_view cmd,
      ForwardIterator begin,
      ForwardIterator end,
      typename std::iterator_traits<ForwardIterator>::value_type* = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, resp3::type::array, 1 + size * distance);
      resp3::add_bulk(payload_, cmd);

      for (; begin != end; ++begin)
         resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  Equivalent to the overload taking a range of begin and end
    *  iterators.
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param key The command key. It will be added as the first argument to the command.
    *  @param range Range containing the command arguments.
    *  @tparam Range A type that can be passed to `std::begin()` and `std::end()` to obtain
    *          iterators.
    */
   template <class Range>
   void push_range(
      std::string_view cmd,
      std::string_view key,
      Range const& range,
      decltype(std::begin(range))* = nullptr)
   {
      using std::begin;
      using std::end;
      push_range(cmd, key, begin(range), end(range));
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  Equivalent to the overload taking a range of begin and end
    *  iterators.
    *
    *  *If the passed range is empty, no command is added* and this
    *  function becomes a no-op.
    *
    *  The value type of the passed range should satisfy one of the following:
    *
    *    @li The type is convertible to `std::string_view`. One argument is added
    *        per element in the range.
    *    @li The type is an integral type. One argument is added
    *        per element in the range.
    *    @li The type supports the `boost_redis_to_bulk` function. One argument is added
    *        per element in the range. This function is a customization point that must be made available
    *        using ADL and must have the signature `void boost_redis_to_bulk(std::string& to, T const& t);`.
    *    @li The type is a `std::pair` instantiation, with both arguments supporting one of
    *        the points above. Two arguments are added per element in the range.
    *        Nested pairs are not allowed.
    *    @li The type is a `std::tuple` instantiation, with every argument supporting
    *        one of the points above. N arguments are added per element in the range,
    *        with N being the tuple size. Nested tuples are not allowed.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param range Range containing the command arguments.
    *  @tparam Range A type that can be passed to `std::begin()` and `std::end()` to obtain
    *          iterators.
    */
   template <class Range>
   void push_range(
      std::string_view cmd,
      Range const& range,
      decltype(std::cbegin(range))* = nullptr)
   {
      using std::cbegin;
      using std::cend;
      push_range(cmd, cbegin(range), cend(range));
   }

   /** @brief Appends the commands in another request to the end of the request.
    *
    *  Appends all the commands contained in `other` to the end of
    *  this request. Configuration flags in `*this`,
    *  like @ref config::cancel_if_unresponded, are *not* modified,
    *  even if `other` has a different config than `*this`.
    *  
    *  @param other The request containing the commands to append.
    */
   void append(const request& other);

   /**
    * @brief Appends a SUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `{"ch1", "ch2"}`, the resulting command
    * is `SUBSCRIBE ch1 ch2`.
    *
    * Subscriptions created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   void subscribe(std::initializer_list<std::string_view> channels)
   {
      subscribe(channels.begin(), channels.end());
   }

   /**
    * @brief Appends a SUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `["ch1", "ch2"]`, the resulting command
    * is `SUBSCRIBE ch1 ch2`.
    *
    * Subscriptions created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class Range>
   void subscribe(Range&& channels, decltype(std::cbegin(channels))* = nullptr)
   {
      subscribe(std::cbegin(channels), std::cend(channels));
   }

   /**
    * @brief Appends a SUBSCRIBE command to the end of the request.
    *
    * [`channels_begin`, `channels_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["ch1", "ch2"]`, the resulting command
    * is `SUBSCRIBE ch1 ch2`.
    *
    * Subscriptions created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class ForwardIt>
   void subscribe(ForwardIt channels_begin, ForwardIt channels_end)
   {
      push_pubsub("SUBSCRIBE", detail::pubsub_change_type::subscribe, channels_begin, channels_end);
   }

   /**
    * @brief Appends an UNSUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `{"ch1", "ch2"}`, the resulting command
    * is `UNSUBSCRIBE ch1 ch2`.
    *
    * Subscriptions removed using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   void unsubscribe(std::initializer_list<std::string_view> channels)
   {
      unsubscribe(channels.begin(), channels.end());
   }

   /**
    * @brief Appends an UNSUBSCRIBE command to the end of the request.
    *
    * If `channels` contains `["ch1", "ch2"]`, the resulting command
    * is `UNSUBSCRIBE ch1 ch2`.
    *
    * Subscriptions removed using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class Range>
   void unsubscribe(Range&& channels, decltype(std::cbegin(channels))* = nullptr)
   {
      unsubscribe(std::cbegin(channels), std::cend(channels));
   }

   /**
    * @brief Appends an UNSUBSCRIBE command to the end of the request.
    *
    * [`channels_begin`, `channels_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["ch1", "ch2"]`, the resulting command
    * is `UNSUBSCRIBE ch1 ch2`.
    *
    * Subscriptions removed using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class ForwardIt>
   void unsubscribe(ForwardIt channels_begin, ForwardIt channels_end)
   {
      push_pubsub(
         "UNSUBSCRIBE",
         detail::pubsub_change_type::unsubscribe,
         channels_begin,
         channels_end);
   }

   /**
    * @brief Appends a PSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `{"news.*", "events.*"}`, the resulting command
    * is `PSUBSCRIBE news.* events.*`.
    *
    * Subscriptions created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   void psubscribe(std::initializer_list<std::string_view> patterns)
   {
      psubscribe(patterns.begin(), patterns.end());
   }

   /**
    * @brief Appends a PSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `["news.*", "events.*"]`, the resulting command
    * is `PSUBSCRIBE news.* events.*`.
    *
    * Subscriptions created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class Range>
   void psubscribe(Range&& patterns, decltype(std::cbegin(patterns))* = nullptr)
   {
      psubscribe(std::cbegin(patterns), std::cend(patterns));
   }

   /**
    * @brief Appends a PSUBSCRIBE command to the end of the request.
    *
    * [`patterns_begin`, `patterns_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["news.*", "events.*"]`, the resulting command
    * is `PSUBSCRIBE news.* events.*`.
    *
    * Subscriptions created using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class ForwardIt>
   void psubscribe(ForwardIt patterns_begin, ForwardIt patterns_end)
   {
      push_pubsub(
         "PSUBSCRIBE",
         detail::pubsub_change_type::psubscribe,
         patterns_begin,
         patterns_end);
   }

   /**
    * @brief Appends a PUNSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `{"news.*", "events.*"}`, the resulting command
    * is `PUNSUBSCRIBE news.* events.*`.
    *
    * Subscriptions removed using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   void punsubscribe(std::initializer_list<std::string_view> patterns)
   {
      punsubscribe(patterns.begin(), patterns.end());
   }

   /**
    * @brief Appends a PUNSUBSCRIBE command to the end of the request.
    *
    * If `patterns` contains `["news.*", "events.*"]`, the resulting command
    * is `PUNSUBSCRIBE news.* events.*`.
    *
    * Subscriptions removed using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class Range>
   void punsubscribe(Range&& patterns, decltype(std::cbegin(patterns))* = nullptr)
   {
      punsubscribe(std::cbegin(patterns), std::cend(patterns));
   }

   /**
    * @brief Appends a PUNSUBSCRIBE command to the end of the request.
    *
    * [`patterns_begin`, `patterns_end`) should point to a valid
    * range of elements convertible to `std::string_view`.
    * If the range contains `["news.*", "events.*"]`, the resulting command
    * is `PUNSUBSCRIBE news.* events.*`.
    *
    * Subscriptions removed using this function are tracked
    * to enable PubSub state restoration. After successfully executing
    * the request, the connection will store any newly subscribed channels and patterns.
    * Every time a reconnection happens,
    * a suitable `SUBSCRIBE`/`PSUBSCRIBE` command is issued automatically,
    * to restore the subscriptions that were active before the reconnection.
    * 
    * PubSub store restoration only happens when using @ref subscribe,
    * @ref unsubscribe, @ref psubscribe or @ref punsubscribe.
    * Subscription commands added by @ref push or @ref push_range are not tracked.
    */
   template <class ForwardIt>
   void punsubscribe(ForwardIt patterns_begin, ForwardIt patterns_end)
   {
      push_pubsub(
         "PUNSUBSCRIBE",
         detail::pubsub_change_type::punsubscribe,
         patterns_begin,
         patterns_end);
   }

   /** @brief Appends a HELLO 3 command to the end of the request.
    *
    * Equivalent to adding the Redis command `HELLO 3`.
    */
   void hello();

   /** @brief Appends a HELLO 3 command with AUTH to the end of the request.
    *
    * Equivalent to the adding the following Redis command:
    * @code
    * HELLO 3 AUTH <username> <password>
    * @endcode
    *
    * @param username The ACL username.
    * @param password The password for the user.
    */
   void hello(std::string_view username, std::string_view password);

   /** @brief Appends a HELLO 3 command with SETNAME to the end of the request.
    *
    * Equivalent to adding the following Redis command:
    * @code
    * HELLO 3 SETNAME <client_name>
    * @endcode
    *
    * @param client_name The client name (visible in CLIENT LIST).
    */
   void hello_setname(std::string_view client_name);

   /** @brief Appends a HELLO 3 command with AUTH and SETNAME to the end of the request.
    *
    * Equivalent to adding the following Redis command:
    * @code
    * HELLO 3 AUTH <username> <password> SETNAME <client_name>
    * @endcode
    *
    * @param username The ACL username.
    * @param password The password for the user.
    * @param client_name The client name (visible in CLIENT LIST).
    */
   void hello_setname(
      std::string_view username,
      std::string_view password,
      std::string_view client_name);

private:
   void check_cmd(std::string_view cmd)
   {
      ++commands_;

      if (!detail::has_response(cmd))
         ++expected_responses_;

      if (cmd == "HELLO")
         has_hello_priority_ = cfg_.hello_with_priority;
   }

   config cfg_;
   std::string payload_;
   std::size_t commands_ = 0;
   std::size_t expected_responses_ = 0;
   bool has_hello_priority_ = false;
   std::vector<detail::pubsub_change> pubsub_changes_{};

   void add_pubsub_arg(detail::pubsub_change_type type, std::string_view value);

   template <class ForwardIt>
   void push_pubsub(
      std::string_view cmd,
      detail::pubsub_change_type type,
      ForwardIt channels_begin,
      ForwardIt channels_end)
   {
      static_assert(
         std::is_convertible_v<
            typename std::iterator_traits<ForwardIt>::value_type,
            std::string_view>,
         "subscribe, psubscribe, unsubscribe and punsubscribe should be passed ranges of elements "
         "convertible to std::string_view");
      if (channels_begin == channels_end)
         return;

      auto const distance = std::distance(channels_begin, channels_end);
      resp3::add_header(payload_, resp3::type::array, 1 + distance);
      resp3::add_bulk(payload_, cmd);

      for (; channels_begin != channels_end; ++channels_begin)
         add_pubsub_arg(type, *channels_begin);

      ++commands_;  // these commands don't have a response
   }

   friend struct detail::request_access;
};

namespace detail {

struct request_access {
   inline static void set_priority(request& r, bool value) { r.has_hello_priority_ = value; }
   inline static bool has_priority(const request& r) { return r.has_hello_priority_; }
   inline static const std::vector<detail::pubsub_change>& pubsub_changes(const request& r)
   {
      return r.pubsub_changes_;
   }
};

// Creates a HELLO 3 request
request make_hello_request();

}  // namespace detail

}  // namespace boost::redis

#endif  // BOOST_REDIS_REQUEST_HPP

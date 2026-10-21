/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_OPERATION_HPP
#define BOOST_REDIS_OPERATION_HPP

namespace boost::redis {

/** @brief Connection operations that can be cancelled.
 *  
 *  The operations listed below can be passed to the
 *  @ref boost::redis::connection::cancel member function.
 */
enum class operation
{
   /**
    * @brief (Deprecated) Resolve operation.
    * 
    * Cancelling a single resolve operation is probably not what you
    * want, since there is no way to detect when a connection is performing name resolution.
    * Use @ref operation::run to cancel the current @ref basic_connection::async_run operation,
    * which includes name resolution.
    */
   resolve,

   /**
    * @brief (Deprecated) Connect operation.
    * 
    * Cancelling a single connect operation is probably not what you
    * want, since there is no way to detect when a connection is performing a connect operation.
    * Use @ref operation::run  to cancel the current @ref basic_connection::async_run operation,
    * which includes connection establishment.
    */
   connect,

   /**
    * @brief (Deprecated) SSL handshake operation.
    * 
    * Cancelling a single connect operation is probably not what you
    * want, since there is no way to detect when a connection is performing an SSL handshake.
    * Use @ref operation::run  to cancel the current @ref basic_connection::async_run operation,
    * which includes the SSL handshake.
    */
   ssl_handshake,

   /// Refers to `connection::async_exec` operations.
   exec,

   /// Refers to `connection::async_run` operations.
   run,

   /**
    * @brief (Deprecated) Refers to `async_receive` and `async_receive2` operations.
    * 
    * To cancel `async_receive2`, use either @ref basic_connection::cancel with no arguments
    * or per-operation cancellation.
    */
   receive,

   /**
    * @brief (Deprecated) Cancels reconnection.
    * 
    * Cancelling reconnection doesn't really cancel anything.
    * It will only prevent further connections attempts from being
    * made once the current connection encounters an error.
    *
    * Use @ref operation::run  to cancel the current @ref basic_connection::async_run operation,
    * which includes reconnection. If you want to disable reconnection completely,
    * set @ref config::reconnect_wait_interval to zero before calling `async_run`.
    */
   reconnection,

   /**
    * @brief (Deprecated) Health check operation.
    * 
    * Cancelling the health checker only is probably not what you want.
    * Use @ref operation::run  to cancel the current @ref basic_connection::async_run operation,
    * which includes the health checker. If you want to disable health checks completely,
    * set @ref config::health_check_interval to zero before calling `async_run`.
    */
   health_check,

   /// Refers to all operations.
   all,
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_OPERATION_HPP

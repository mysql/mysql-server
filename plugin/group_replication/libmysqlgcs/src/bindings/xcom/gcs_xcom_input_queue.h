/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_XCOM_INPUT_QUEUE_INCLUDED
#define GCS_XCOM_INPUT_QUEUE_INCLUDED

#include <future>
#include <memory>
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_mpsc_queue.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_input_request.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_memory.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

struct xcom_input_request_ptr_deleter {
  constexpr xcom_input_request_ptr_deleter() noexcept = default;
  void operator()(xcom_input_request_ptr ptr) const {
    if (ptr != nullptr) {
      /* By calling the reply function, the reply object is deleted. Either
         because there is no one waiting (reply function deletes the object), or
         someone is waiting (or will wait) for the reply's future, in which case
         the object is deleted when the future's result is deleted. */
      ::xcom_input_request_reply(ptr, nullptr);
      ::xcom_input_request_free(ptr);
    }
  }
};

static void do_not_reply(void *reply, pax_msg *payload);
static void reply_by_resolving_future(void *reply, pax_msg *payload);

/**
 * MPSC queue with FIFO semantics used to send commands to XCom.
 */
/* The Queue template is only overridden by tests. */
template <typename Queue = Gcs_mpsc_queue<xcom_input_request,
                                          xcom_input_request_ptr_deleter>>
class Gcs_xcom_input_queue_impl {
 public:
  class Reply;
  using future_reply = std::future<std::unique_ptr<Reply>>;
  /**
   * Wraps XCom's reply to a queued request.
   *
   * A request sent to XCom contains three important pieces of data:
   *
   * 1. The payload, i.e. what we want XCom to do
   * 2. A function instructing XCom how to reply to the request
   * 3. A Reply object
   * (For more details please see xcom_input_request.)
   *
   * When GCS pushes a request to XCom, GCS receives back a future pointer to
   * the Reply object associated with the request. (future_reply)
   * Basically, this future's result is equivalent to the reply GCS would get if
   * it used a TCP socket to send the request to XCom.
   * XCom will resolve this future when it receives and processes the request.
   * When GCS retrieves the future's result, GCS obtains a std::unique_ptr to
   * the Reply. Unless you do any shenanigans with this std::unique_ptr, when it
   * goes out of scope the Reply object is destroyed.
   * The Reply object owns the payload of XCom's reply (pax_msg), so the Reply
   * object destroys the pax_msg when it is destroyed.
   */
  class Reply {
   public:
    Reply() noexcept : m_payload(nullptr), m_promise() {}
    ~Reply() { replace_pax_msg(&m_payload, nullptr); }
    /**
     * Associates the payload of XCom's reply to this object.
     * Resolves the future.
     *
     * @param payload XCom's reply
     */
    void resolve(pax_msg *payload) {
      m_payload = payload;
      m_promise.set_value(std::unique_ptr<Reply>(this));
    }
    /**
     * Get the future that will hold a pointer to this object after it has been
     * resolved.
     *
     * @returns a future pointer to this object when it already contains XCom's
     * reply
     */
    future_reply get_future() { return m_promise.get_future(); }
    /**
     * Get XCom's reply.
     *
     * @returns XCom's reply
     */
    pax_msg const *get_payload() { return m_payload; }

   private:
    /** XCom's reply. */
    pax_msg *m_payload;
    /** Simultaneously holds the future's shared state and takes care of the
     * lifetime of *this. */
    std::promise<std::unique_ptr<Reply>> m_promise;
  };
  // NOLINTNEXTLINE(modernize-use-equals-default)
  Gcs_xcom_input_queue_impl() noexcept {}
  // NOLINTNEXTLINE(modernize-use-equals-default)
  ~Gcs_xcom_input_queue_impl() {}
  Gcs_xcom_input_queue_impl(Gcs_xcom_input_queue_impl const &) = delete;
  Gcs_xcom_input_queue_impl(Gcs_xcom_input_queue_impl &&) = delete;
  Gcs_xcom_input_queue_impl &operator=(Gcs_xcom_input_queue_impl const &) =
      delete;
  Gcs_xcom_input_queue_impl &operator=(Gcs_xcom_input_queue_impl &&) = delete;
  /**
   * Sends the command @c msg to XCom.
   *
   * This method has fire-and-forget semantics, i.e. we do not wait or have
   * access to any potential reply from XCom.
   * Takes ownership of @c msg.
   *
   * @param msg the app_data_ptr to send to XCom
   * @retval false if there is no memory available
   * @retval true otherwise (operation was successful)
   */
  bool push(app_data_ptr msg) {
    Reply *reply = push_internal(msg, do_not_reply);
    /* reply is destroyed by XCom. */
    bool const successful = (reply != nullptr);
    return successful;
  }
  /**
   * Sends the command @c msg to XCom.
   *
   * This method has request-response semantics, i.e. we get back a future on
   * which we must wait for XCom's reply. Please note that you must retrieve the
   * future's value, otherwise it will leak. If you do not care for a reply, use
   * @c push instead.
   * Takes ownership of @c msg.
   *
   * @param msg the app_data_ptr to send to XCom
   * @returns a valid future (future.valid()) if successful, an invalid future
   *          (!future.valid()) otherwise.
   */
  future_reply push_and_get_reply(app_data_ptr msg) {
    future_reply future;
    Reply *reply = push_internal(msg, reply_by_resolving_future);
    /* reply is destroyed by XCom. */
    bool const successful = (reply != nullptr);
    if (successful) {
      future = reply->get_future();
    }
    return future;
  }
  /**
   * Attempts to retrieve all the queued app_data_ptr in one swoop.
   *
   * Transfers ownership of the returned pointer(s).
   * Note that this method is non-blocking.
   *
   * @retval app_data_ptr linked list of the queued commands if the queue is
   *                      not empty
   * @retval nullptr if the queue is empty
   */
  xcom_input_request_ptr pop() {
    xcom_input_request *payload = m_queue.pop();
    if (payload == nullptr) return nullptr;
    /* Process first. */
    xcom_input_request_ptr first_in = payload;  // Take ownership.
    xcom_input_request_ptr last_in = first_in;
    /* Process remaining. */
    payload = m_queue.pop();
    while (payload != nullptr) {
      ::xcom_input_request_set_next(last_in, payload);  // Take ownership.
      last_in = payload;
      payload = m_queue.pop();
    }
    return first_in;
  }
  /**
   * Empties the queue.
   */
  void reset() {
    xcom_input_request_ptr cursor = pop();
    while (cursor != nullptr) {
      xcom_input_request_ptr next_request =
          ::xcom_input_request_extract_next(cursor);
      xcom_input_request_ptr_deleter()(cursor);
      cursor = next_request;
    }
  }

 private:
  /**
   * Internal helper to implement @c push and @ push_and_get_reply.
   * Creates and pushes a request to XCom with the payload @c msg and using the
   * reply strategy @c reply_function.
   *
   * @param msg the request's payload
   * @param reply_function the reply strategy for this request
   * @retval Reply* if successful
   * @retval nullptr if unsuccessful
   */
  Reply *push_internal(app_data_ptr msg,
                       xcom_input_reply_function_ptr reply_function) {
    xcom_input_request_ptr xcom_request = nullptr;
    bool successful = false;
    auto *xcom_reply = new (std::nothrow) Reply();
    if (xcom_reply == nullptr) {
      /* purecov: begin inspected */
      // Because the app_data_ptr is allocated on the heap and not on the stack.
      xdr_free((xdrproc_t)xdr_app_data_ptr, (char *)&msg);
      goto end;
      /* purecov: end */
    }
    // Takes ownership of msg if successful.
    xcom_request = ::xcom_input_request_new(msg, reply_function, xcom_reply);
    if (xcom_request == nullptr) {
      /* purecov: begin inspected */
      // Because the app_data_ptr is allocated on the heap and not on the stack.
      xdr_free((xdrproc_t)xdr_app_data_ptr, (char *)&msg);
      delete xcom_reply;
      xcom_reply = nullptr;
      goto end;
      /* purecov: end */
    }
    /* If the push is successful, the queue takes ownership of xcom_request. */
    successful = m_queue.push(xcom_request);
    if (!successful) {
      delete xcom_reply;
      xcom_reply = nullptr;
      ::xcom_input_request_free(xcom_request);  // also destroys msg
    }
  end:
    return xcom_reply;
  }
  Queue m_queue;  // Wrapped implementation.
};
using Gcs_xcom_input_queue = Gcs_xcom_input_queue_impl<>;

static inline void do_not_reply(void *reply, pax_msg *payload) {
  auto *xcom_reply = static_cast<Gcs_xcom_input_queue::Reply *>(reply);
  delete xcom_reply;
  replace_pax_msg(&payload, nullptr);
}
static inline void reply_by_resolving_future(void *reply, pax_msg *payload) {
  auto *xcom_reply = static_cast<Gcs_xcom_input_queue::Reply *>(reply);
  xcom_reply->resolve(payload);  // Takes ownership of payload.
  /* xcom_reply will be deleted when the associated future has been waited for,
     its value retrieved, and its value is deleted. */
}

#endif /* GCS_XCOM_INPUT_QUEUE_INCLUDED */

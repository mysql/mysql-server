/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_LOWER_LAYER_COMPLETION_H_
#define ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_LOWER_LAYER_COMPLETION_H_

#include <utility>

namespace net {
namespace tls {

class NOP_token {
 public:
  void operator()(const std::error_code &, size_t) {}
};

template <typename FirstToken, typename SecondToken = NOP_token>
class LowerLayerReadCompletionToken {
 public:
  using First_token_type = std::decay_t<FirstToken>;
  using Second_token_type = std::decay_t<SecondToken>;

  LowerLayerReadCompletionToken(const LowerLayerReadCompletionToken &other)
      : first_token_{other.first_token_}, second_token_{other.second_token_} {}

  LowerLayerReadCompletionToken(LowerLayerReadCompletionToken &&other)
      : first_token_{std::move(other.first_token_)},
        second_token_{std::move(other.second_token_)} {}

  LowerLayerReadCompletionToken(FirstToken &token, SecondToken &second_token)
      : first_token_{std::forward<First_handler_type>(token)},
        second_token_{std::forward<Second_handler_type>(second_token)} {}

  LowerLayerReadCompletionToken(FirstToken &&token, SecondToken &&second_token)
      : first_token_{token}, second_token_{second_token} {}

  void operator()(std::error_code ec, size_t size) const {
    first_token_.handle_read(ec, size);
    second_token_(ec, size);
  }

 private:
  using First_handler_type =
      std::conditional_t<std::is_same<FirstToken, First_token_type>::value,
                         First_token_type &, First_token_type>;

  using Second_handler_type =
      std::conditional_t<std::is_same<SecondToken, Second_token_type>::value,
                         Second_token_type &, Second_token_type>;
  mutable FirstToken first_token_;
  mutable SecondToken second_token_;
};

template <typename FirstToken, typename SecondToken = NOP_token>
class LowerLayerWriteCompletionToken {
 public:
  using First_token_type = std::decay_t<FirstToken>;
  using Second_token_type = std::decay_t<SecondToken>;

  LowerLayerWriteCompletionToken(const LowerLayerWriteCompletionToken &other)
      : first_token_{other.first_token_}, second_token_{other.second_token_} {}

  LowerLayerWriteCompletionToken(LowerLayerWriteCompletionToken &&other)
      : first_token_{std::move(other.first_token_)},
        second_token_{std::move(other.second_token_)} {}

  LowerLayerWriteCompletionToken(FirstToken &token, SecondToken &second_token)
      : first_token_{std::forward<First_handler_type>(token)},
        second_token_{std::forward<Second_handler_type>(second_token)} {}

  LowerLayerWriteCompletionToken(FirstToken &&token, SecondToken &&second_token)
      : first_token_{token}, second_token_{second_token} {}

  void operator()(std::error_code ec, size_t size) const {
    first_token_.handle_write(ec, size);
    second_token_(ec, size);
  }

 private:
  using First_handler_type =
      std::conditional_t<std::is_same<FirstToken, First_token_type>::value,
                         First_token_type &, First_token_type>;

  using Second_handler_type =
      std::conditional_t<std::is_same<SecondToken, Second_token_type>::value,
                         Second_token_type &, Second_token_type>;

  mutable FirstToken first_token_;
  mutable SecondToken second_token_;
};

}  // namespace tls
}  // namespace net

#endif  // ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_LOWER_LAYER_COMPLETION_H_

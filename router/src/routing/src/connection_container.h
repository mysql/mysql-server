/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTING_CONNECTION_CONTAINER_INCLUDED
#define ROUTING_CONNECTION_CONTAINER_INCLUDED

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "connection.h"
#include "destination.h"
#include "mysql_routing_common.h"
#include "mysqlrouter/datatypes.h"
#include "tcp_address.h"

class MySQLRoutingConnection;

/**
 * @brief Basic Concurrent Map
 *
 * The concurrent_map is a hash-map, with fixed number of buckets.
 * The numer of buckets can be specified in constructor parameter
 * (num_buckets), by default is set to 23.
 */
template <typename Key, typename Value, typename Hash = std::hash<Key>>
class concurrent_map {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using hash_type = Hash;

  concurrent_map(unsigned num_buckets = kDefaultNumberOfBucket,
                 const Hash &hasher = Hash())
      : buckets_(num_buckets), hasher_(hasher) {}

  concurrent_map(const concurrent_map &other) = delete;
  concurrent_map &operator=(const concurrent_map &other) = delete;

  template <typename Predicate>
  void for_one(const Key &key, Predicate &p) {
    get_bucket(key).for_one(key, p);
  }

  template <typename Predicate>
  void for_each(Predicate &p) {
    for (auto &each_bucket : buckets_) {
      each_bucket.for_each(p);
    }
  }

  void put(const Key &key, Value &&value) {
    get_bucket(key).put(key, std::move(value));
  }

  void erase(const Key &key) { get_bucket(key).erase(key); }

  std::size_t size() const {
    std::size_t result{0};
    for (auto &each_bucket : buckets_) {
      result += each_bucket.size();
    }
    return result;
  }

 private:
  static const unsigned kDefaultNumberOfBucket = 127;

  class Bucket {
   public:
    void put(const Key &key, Value &&value) {
      std::lock_guard<std::mutex> lock(data_mutex_);
      data_.emplace(key, std::move(value));
    }

    void erase(const Key &key) {
      std::lock_guard<std::mutex> lock(data_mutex_);
      data_.erase(key);
    }

    template <typename Predicate>
    void for_one(const Key &key, Predicate &p) {
      std::lock_guard<std::mutex> lock(data_mutex_);
      const ConstBucketIterator found = data_.find(key);
      if (found != data_.end()) p(found->second);
    }

    template <typename Predicate>
    void for_each(Predicate &p) {
      std::lock_guard<std::mutex> lock(data_mutex_);
      std::for_each(data_.begin(), data_.end(), p);
    }

    std::size_t size() const {
      std::lock_guard<std::mutex> lock(data_mutex_);
      return data_.size();
    }

   private:
    using BucketData = std::map<Key, Value>;
    using BucketIterator = typename BucketData::iterator;
    using ConstBucketIterator = typename BucketData::const_iterator;

    BucketData data_;
    mutable std::mutex data_mutex_;
  };

  std::vector<Bucket> buckets_;
  Hash hasher_;

  Bucket &get_bucket(const Key &key) {
    const std::size_t bucket_index = hasher_(key) % buckets_.size();
    return buckets_[bucket_index];
  }

  const Bucket &get_bucket(const Key &key) const {
    const std::size_t bucket_index = hasher_(key) % buckets_.size();
    return buckets_[bucket_index];
  }
};

/**
 * @brief container for connections to MySQL Server.
 *
 * When thread of execution for connection to MySQL Server is completed, it
 * should call remove_connection to remove itself from connection container.
 */
class ConnectionContainer {
  concurrent_map<MySQLRoutingConnection *,
                 std::unique_ptr<MySQLRoutingConnection>>
      connections_;

 public:
  /**
   * @brief Adds new connection to container.
   *
   * @param connection The connection to MySQL server
   */
  void add_connection(std::unique_ptr<MySQLRoutingConnection> connection);

  /**
   * @brief Disconnects all connections to servers that are not allowed any
   * longer.
   *
   * @param nodes Allowed servers. Connections to servers that are not in nodes
   *        are closed.
   */
  void disconnect(const AllowedNodes &nodes);

  /**
   * @brief Disconnects all connection in the ConnectionContainer.
   */
  void disconnect_all();

  /**
   * @brief removes connection from container
   *
   * This function should be called by thread of execution when connection
   * thread completes. Do NOT call this function before connection's thread of
   * execution completes.
   *
   * @param connection The connection to remove from container
   */
  void remove_connection(MySQLRoutingConnection *connection);
};

#endif /* ROUTING_CONNECTION_CONTAINER_INCLUDED */

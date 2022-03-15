/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef REFERENCE_CACHING_H
#define REFERENCE_CACHING_H

#include <mysql/components/service.h>
#include <mysql/components/services/registry.h>

/**
   Handle for "producer channels".
   A producer channel is the singleton used to emit events that are subject
   to reference caching.
   This is needed to hold a flag that will trigger forced invalidation of all
   caches in a particular channel on their next access.
   Multi threaded access is expected to be safe and lock less to this handle
   once it's instantiated.
*/
DEFINE_SERVICE_HANDLE(reference_caching_channel);

/**
  A handle of a "reference cache". This is created by all events producer
  threads when they start and is maintained throughout their lifetimes.
  Operations on that are not thread safe and are assumed to be done by a
  single thread.
*/
DEFINE_SERVICE_HANDLE(reference_caching_cache);

/**
  @ingroup group_components_services_inventory
*/

/**
  A reference caching channel service.

  The goal of this service is to ensure that event producers spend
  a MINIMAL amount of time in the emitting code when there are no consumers of
  the produced events.

  Terminology
  -----------

  An event consumer is an implementation of a component service.
  An event producer is a piece of code that wants to inform all currently
  registered event consumers about an event.
  A channel is a device that serves as a singleton of all reference caches
  for all threads that are to produce events.
  A reference cache is something each thread producing events must maintain
  for its lifetime and use it to find the references to event consumers it
  needs to call.

  Typical lifetime of the event consumers
  ---------------------------------------

  At init time the event consumer will register implementations of any of
  the services it's interested in receiving notifications for.

  Then optionally it might force a channel invalidation to make sure all
  existing event producers will start sending notifications to it immediately.
  Or it can just sit and wait for the natural producer's reference cache
  refresh cycles to kick in.

  Now it is receiving notifications from all event producers as they come.

  When it wishes to no longer receive notifications it needs to mark itself
  as invisible for reference cache fill-ins. In this way all reference cache
  flushes will not pick this implementation up even if it's still registered
  in the registry.

  Eventually all active references will be removed by the natural producers
  flush cycles.
  The consumer may choose to expedite this by triggering a channel
  invalidation.

  As with all service implementations, when all references to the services
  are released it can unload.

  Typical lifetime of the event producers
  ---------------------------------------

  An event producer will usually start at init time by creating channel(s).

  Then, for each thread wishing to produce events, a reference cache must
  be created and maintained until the thread will no longer be producing
  events.

  Now the thread can produce events using the reference cache.
  This is done by calling the get method and then iterating over the
  resulting set of references and calling each one in turn as one would
  normally do for registry service references.
  It is assumed that the references are owned by the cache and thus they
  should not be released.

  With some cyclical (e.g. at the end of each statement or something)
  the event producing thread needs to flush the cache. This is to ensure
  that references to event consumers are not held for very long and that
  new event consumers are picked up. However flushing the cache is a relatively
  expensive operation and thus a balance between the number of events produced
  and the cache being flushed must be achieved.

  General remarks
  ---------------

  Channels are something each event producer must have to produce events.
  Channels are to be created by a single thread before the first event is ever
  produced. And, once created they are to be kept until after the last event is
  produced.

  Channels serve as singletons for caches and you only need one channel instance
  per event producer component.
  There usually will be multiple caches (one per event producing thread) per
  channel.

  Creating and destroying a channel is a relatively "expensive" operation that
  might involve some synchronization and should not be done frequently.

  Channels exist to allow a non-related thread to trigger invalidation of all
  currently active caches on that channel. This is necessary when for example
  event consumers are registered and are about to be removed.

  Invalidating a channel is a thread-safe operation that can be invoked without
  synchronization at any time.

  Each channel is associated with a specific set of service names.
  @note It is a set of service names, not implementation names !

  The names are stored and used in event caches to handle all implementations
  of that particular service.
*/
BEGIN_SERVICE_DEFINITION(reference_caching_channel)
/**
  Creates a channel and returns a handle for it.

  The handle created by this method must be destroyed by the invalidate
  method otherwise there might be leaks.

  The channel should be created before any events are to be produced on it.

  @param service_names a list of service names that this channel will operate
  on. Terminated by a NULL pointer
  @param[out] out_channel placeholder for the newly created handle.
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(create, (const char *service_names[],
                             reference_caching_channel *out_channel));

/**
  Destroys a channel handle

  Should make sure no other thread is using the handle and no caches are
  allocated on it.

  @param channel the handle to destroy
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(destroy, (reference_caching_channel channel));

/**
  Invalidate a channel

  This is thread safe to call without synchronization
  and relatively fast.

  Forces an asynchronous flush on all caches that are allocated on
  that channel when they're next accessed.

  @param channel the handle to destroy
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(invalidate, (reference_caching_channel channel));

/**
  Validate a channel

  This is thread safe to call without synchronization
  and relatively fast.

  This function is used to validate the channel. Which helps in
  getting the cached service references on that channel when they're
  next accessed.

  @param channel the handle to destroy
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(validate, (reference_caching_channel channel));

/**
  Fetches a reference caching channel by name.

  Usually consumers wishing to force reference cache flush would
  fetch the channel handle so they can then call invalidate on it.

  This is a relatively expensive operation as it might involve some
  synchronization.

  @param service_name a service name that this channel will operate on.
  @param[out] out_channel placeholder or NULL if not found.
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(fetch, (const char *service_name,
                            reference_caching_channel *out_channel));

END_SERVICE_DEFINITION(reference_caching_channel)

/**
  A service to maintain an "ignore list" for reference caches.

  When a service implementation is on that ignore list it will never be
  added to any reference caches when they're filled in even if the service
  implementation is in the registry and implements the service
  the reference caching channel is operating on.

  This is just a list of "implementations", i.e. the part of the service
  implementation name after the dot.
  The channel already has the name of the service so a full implementation
  name can be constructed if needed.
*/
BEGIN_SERVICE_DEFINITION(reference_caching_channel_ignore_list)
/**
  Adds an implementation name to the ignore list.

  @param channel the channel to add the ignored implementation to.
  @param implementation_name the second part of the service implementation
    name (the part after the dot).
  @retval false successfully added
  @retval true error adding (e.g. the ignore is present etc.
*/
DECLARE_BOOL_METHOD(add, (reference_caching_channel channel,
                          const char *implementation_name));
/**
  Remove an implementation name to the ignore list.

  @param channel the channel to remove the ignored implementation from.
  @param implementation_name the second part of the service implementation
    name (the part after the dot).
  @retval false successfully removed
  @retval true error removing or not present
*/
DECLARE_BOOL_METHOD(remove, (reference_caching_channel channel,
                             const char *implementation_name));
/**
  Empty the ignore list.

  @param channel the channel to remove the ignored implementation from.
  @retval false successfully removed all ignores
  @retval true error removing the ignores. State unknown.
*/
DECLARE_BOOL_METHOD(clear, (reference_caching_channel channel));
END_SERVICE_DEFINITION(reference_caching_channel_ignore_list)

/**
  Reference cache service.

  See the reference_caching_channel service for details on how to
  operate this.

  Manages thread caches for event producer threads.
*/
BEGIN_SERVICE_DEFINITION(reference_caching_cache)
/**
  Create a reference cache.

  Needs to be called before the first get() or flush() is called.
  Each call to create must be paired with a call to destroy() or
  there will be leaks.

  @param channel the reference cache channel to operate on.
  @param registry a handle to the registry so that no time is spent taking it
  @param[out] handle of the newly allocated cache.
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(create, (reference_caching_channel channel,
                             SERVICE_TYPE(registry) * registry,
                             reference_caching_cache *out_cache));
/**
  Destroy a reference cache.

  Needs to be called to dispose of each cache allocated by create().
  Needs to be called after all possible calls to get() and flush().
  Once called the cache handle is invalid and should not be used anymore.

  @param channel the reference cache channel to operate on.
  @param[out] handle of the newly allocated cache.
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(destroy, (reference_caching_cache cache));

/**
  Gets a set of service references for an event producer to call.

  This is the main event producer function that will be called when the
  event producer needs to produce an event.

  The cache must be a valid one. And the channel too.

  If the cache is empty or invalidated (either via the channel or via a call
  to flush) it will try to fill it by consulting the registry and acquiring
  references to all implementations of the service the channel is created for.
  Once that is done the cache will be marked as full. This a cache "miss":
  a relatively expensive operation and care must be taken so it doesn't
  happen very often.

  If the cache is full (not flushed) this call will return all references
  stored into the cache (might be zero too).
  This is a very fast operation since the cache is single-thread-use and thus
  no synchronization will be done (except for checking the channel's state
  of course). This is a cache "hit" and should be the normal operation of the
  cache.

  @param cache the cache to use
  @param service_index to get references to. Must be one of the services the
  channel serves
  @param[out] an array of my_h_service terminated with an empty service (0).
  @retval true failure
  @retval false success
*/
DECLARE_BOOL_METHOD(get, (reference_caching_cache cache, unsigned service_index,
                          const my_h_service **refs));
/**
  Flush a reference cache

  This causes the reference cache supplied to be flushed. When in this state
  the next call to get() will be a guaranteed cache miss and will fill in the
  cache.

  @param cache the cache to flush
  @retval true failure
  @retval false success
*/
DECLARE_BOOL_METHOD(flush, (reference_caching_cache cache));
END_SERVICE_DEFINITION(reference_caching_cache)

#endif /* REFERENCE_CACHING_H */

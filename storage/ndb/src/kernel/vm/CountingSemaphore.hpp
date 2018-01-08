/*
   Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef COUNTING_SEMAPHORE_HPP
#define COUNTING_SEMAPHORE_HPP

#define JAM_FILE_ID 245


/**
  * CountingSemaphore
  *
  * Helper for limiting concurrency on some resources.
  * The Semaphore is created with some maximum concurrency level
  * Up to this many resources may be concurrently used.
  * When more than this number of resources are used concurrently,
  * further requests must queue until a resource is released.
  * 
  * This structure does not manage queueing and restarting of 
  * resource allocation requests, just monitors the number of
  * resources in use, and the number of resource requests 
  * queued up.
  *
  * To be useful, some external request queueing and dequeuing
  * mechanism is required.
  */
 class CountingSemaphore
 {
 public:
   CountingSemaphore():
     inUse(0),
     queuedRequests(0),
     totalResources(1)
     {};

   ~CountingSemaphore() {};

   /** 
    * init
    * Initialise the totalResources
    */
   void init(Uint32 _totalResources)
   {
     assert(inUse == 0);
     totalResources = _totalResources;
   }

   /**
    * requestMustQueue
    * 
    * Part of semaphore P()/acquire()/down() implementation
    * 
    * Called to request a resource.
    * Returns whether the request must be queued, or
    * can be satisfied immediately.
    *
    * true  - no resource available, queue request.
    * false - resource available, proceed.
    *
    * e.g. if (<sema>.requestMustQueue()) {
    *        queue_request;
    *        return;
    *      }
    *
    *      proceed;
    */
   bool requestMustQueue()
   {
     assert(inUse <= totalResources);
     if (inUse == totalResources)
     {
       queuedRequests++;
       return true;
     }
     else
     {
       assert(queuedRequests == 0);
       inUse++;
       return false;
     }
   }
   
   /**
    * releaseMustStartQueued
    *
    * Part of semaphore V()/release()/up()
    *
    * Called to release a resource.
    * Returns whether some queued resource request
    * must be restarted.
    *
    * true  - a queued request exists and must be started.
    * false - no queued request exists, proceed.
    *
    * e.g.
    *   if (<sema>.releaseMustStartQueued()) {
    *     dequeue_request;
    *     begin_request_processing;
    *   }
    *   
    *   proceed;
    */
   bool releaseMustStartQueued()
   {
     assert(inUse > 0);
     if (queuedRequests > 0)
     {
       assert(inUse == totalResources);
       queuedRequests--;
       return true;
     }

     inUse--;
     return false;
   }

   /**
    * getTotalRequests
    * 
    * Returns the sum of the inuse resources and queued requests.
    * e.g. the offered concurrency on the resource.
    */
   Uint32 getTotalRequests() const
   {
     return inUse + queuedRequests;
   }

   /**
    * getResourcesAvailable()
    * 
    * Returns the number of resources available currently
    */
   Uint32 getResourcesAvailable() const
   {
     assert(inUse <= totalResources);
     return (totalResources - inUse);
   }


   /* inUse - number resources currently in use */
   Uint32 inUse;
   
   /* queuedRequests - number requests waiting 'outside' */
   Uint32 queuedRequests;
   
   /* totalResources - the maximum resources in use at one time */
   Uint32 totalResources;
 }; /* CountingSemaphore */


#undef JAM_FILE_ID

#endif

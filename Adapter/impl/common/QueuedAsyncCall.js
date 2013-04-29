/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

"use strict";

var udebug = unified_debug.getLogger("QueuedAsyncCall.js");

/* Sometimes, some set of async calls have to be serialized, in some context.
   The dictionary calls on an NdbSession,
   the execute calls on an NdbTransaction,
   the getAutoIncrementValue calls on a table -- 
   all of these have to run one at a time. 
   
   A QueuedAsyncCall is an object that encapsulates a call, its arguments,
   and its callback. 
   
   This is the parent class of all sorts of QueuedAsyncCall objects.
*/


/* Public constructor.
   The queue is simply an array.
   The callback will be wrapped.
*/
function QueuedAsyncCall(queue, callback) {
  this.queue = queue;

  /* Function Generator */
  function wrapCallback(queue, callback) {
    return function wrappedCallback(err, obj) {
      var thisCall, next;
      thisCall = queue.shift();  // Our own QueuedAsyncCall
      udebug.log("wrappedCallback", thisCall.description, typeof callback);
      if(queue.length) {
        udebug.log("Run from queue");
        queue[0].run();
      }
      /* Run the user's callback function */
      if(typeof callback === 'function') {  callback(err, obj);  }
    };
  }
  
  this.callback = wrapCallback(queue, callback);
}


/* This is the public method that you call to run an enqueued call.
   You must have given the QueuedAsyncCall a run() method.
   enqueue() returns 0 if the call runs immediately, 
   or its position in the queue if deferred.
*/
QueuedAsyncCall.prototype.enqueue = function() {
  assert(typeof this.run === 'function');
  this.queue.push(this);
  var pos = this.queue.length - 1;
  if(pos === 0) {
    udebug.log("enqueue", this.description, "- run immediate");
    this.run();
  }
  else {
    udebug.log("enqueue", this.description, "- deferred, position", pos);
  }
  return pos;
};


exports.QueuedAsyncCall = QueuedAsyncCall;

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
   The callbacks will be wrapped.
   The mainCallback will be run after advancing the queue.
   The preCallback will be run while still holding exclusive accesss to the
   exec queue. 
   The preCallback may return a postCallback, containing members 
   "fn", "arg0", and "arg1".  
   If so, fn(arg0, arg1) will be run after advancing the queue.
*/
function QueuedAsyncCall(queue, mainCallback, preCallback) {
  this.queue = queue;

  /* Function Generator */
  function wrapCallbacks(queue, mainCallback, preCallback) {
    return function wrappedCallback(err, obj) {
      var thisCall, next, postCallback;
      postCallback = null;
      thisCall = queue.shift();  // Our own QueuedAsyncCall
      udebug.log(thisCall.description, "has returned");
      /* Run the user's pre-callback function */
      if(typeof preCallback === 'function') {
        postCallback = preCallback(err, obj);
      }
      /* Launch the next queued async call */
      if(queue.length) {
        udebug.log("Next queued:", queue[0].description);
        queue[0].run();
      }
      /* The preCallback may have returned a postCallback */
      if(postCallback && postCallback.fn) {
        postCallback.fn(postCallback.arg0, postCallback.arg1);
      }
      /* Run the user's main callback function */
      if(typeof mainCallback === 'function') {  mainCallback(err, obj);  }
    };
  }
  
  this.callback = wrapCallbacks(queue, mainCallback, preCallback);
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
    udebug.log("enqueue", this.description, "- position", pos);
  }
  return pos;
};


exports.QueuedAsyncCall = QueuedAsyncCall;

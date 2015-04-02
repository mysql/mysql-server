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

var udebug = unified_debug.getLogger("QueuedAsyncCall.js"),
    assert = require("assert"),
    serial = 1;

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
   The preCallback will be run while still holding exclusive accesss to the
   exec queue. 
   The mainCallback will be run after advancing the queue.
   The preCallback may return a postCallback, containing members 
   "fn", "arg0", and "arg1".  
   If so, fn(arg0, arg1) will be run after advancing the queue.
*/
function QueuedAsyncCall(queue, mainCallback) {
  this.queue = queue;
  this.serial = serial++;

  /* Function Generator */
  function wrapCallbacks(queue, mainCallback) {
    return function wrappedCallback(err, obj) {
      //
      var thisCall, nextCall, postCallback;
      thisCall = queue.shift();  // Our own QueuedAsyncCall
      udebug.log_detail(thisCall.description, "has returned");
 
      /* Note the next queued async call.
         This must be done before the preCallback, because recursion is tricky.
      */
      if(queue.length) {
        udebug.log_detail("Next queued:", queue[0].description);
        nextCall = queue[0];
      }      
      /* Run the user's pre-callback function */
      if(typeof thisCall.preCallback === 'function') {
        postCallback = thisCall.preCallback(err, obj);
      }      

      /* Advance the queue */
      if(nextCall) {
        nextCall.run();
      }

      /* The preCallback may have returned a postCallback */
      if(postCallback && postCallback.fn) {
        udebug.log(thisCall.description," /// Running postCallback");
        postCallback.fn(postCallback.arg0, postCallback.arg1);
      }
      /* Run the user's main callback function */
      else if(typeof mainCallback === 'function') {  
        mainCallback(err, obj);  
      }
      else {
        udebug.log(thisCall.description," /// No callback");
      }
    };
  }
  
  this.callback = wrapCallbacks(queue, mainCallback);
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
    udebug.log("enqueue #", this.serial, this.description, "- run immediate");
    this.run();
  }
  else {
    udebug.log("enqueue #", this.serial, this.description, "- position", pos);
  }
  return pos;
};


exports.QueuedAsyncCall = QueuedAsyncCall;

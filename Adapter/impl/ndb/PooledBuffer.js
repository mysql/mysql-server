

"use strict";

var reuseLists = [];
reuseLists[0] = [];  // just so v8 knows the element type

function getPooledBuffer(size) {
  var b;
  if(reuseLists[size] && reuseLists[size].length) {
    b = reuseLists[size].pop();
  }
  else {
    b = new Buffer(size);
  }
  return b;
}


/* By putting a buffer on a reuseList, 
   we prevent it from ever being garbage collected.
*/
function releasePooledBuffer(b) {  
  if(b && b.length > 0 && b.length < 4096) {
    if(typeof reuseLists[b.length] === 'undefined') {
      reuseLists[b.length] = [];
    }
    reuseLists[b.length].push(b);
  }
}


exports.get = getPooledBuffer;
exports.release = releasePooledBuffer;

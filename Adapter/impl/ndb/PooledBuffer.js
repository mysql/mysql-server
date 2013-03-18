

var reuseLists = [];

function getPooledBuffer(size) {
  var b;
  if(reuseLists[size] && reuseLists[size].length) {
    b = reuseLists[size].pop();
  }
  else {
    b = new Buffer(size);
  }
  return b;
};


function releasePooledBuffer(b) {
  
  if(typeof reuseLists[b.length] === 'undefined') {
    reuseLists[b.length] = [];
  }
  reuseLists[b.length].push(b);
};


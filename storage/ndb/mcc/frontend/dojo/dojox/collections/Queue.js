//>>built
define("dojox/collections/Queue",["dojo/_base/kernel","dojo/_base/array","./_base"],function(_1,_2,_3){
_3.Queue=function(_4){
var q=[];
if(_4){
q=q.concat(_4);
}
this.count=q.length;
this.clear=function(){
q=[];
this.count=q.length;
};
this.clone=function(){
return new _3.Queue(q);
};
this.contains=function(o){
for(var i=0;i<q.length;i++){
if(q[i]==o){
return true;
}
}
return false;
};
this.copyTo=function(_5,i){
_5.splice(i,0,q);
};
this.dequeue=function(){
var r=q.shift();
this.count=q.length;
return r;
};
this.enqueue=function(o){
this.count=q.push(o);
};
this.forEach=function(fn,_6){
_1.forEach(q,fn,_6);
};
this.getIterator=function(){
return new _3.Iterator(q);
};
this.peek=function(){
return q[0];
};
this.toArray=function(){
return [].concat(q);
};
};
return _3.Queue;
});

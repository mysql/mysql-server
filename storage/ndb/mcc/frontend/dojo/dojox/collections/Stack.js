//>>built
define("dojox/collections/Stack",["dojo/_base/kernel","dojo/_base/array","./_base"],function(_1,_2,_3){
_3.Stack=function(_4){
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
return new _3.Stack(q);
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
this.forEach=function(fn,_6){
_1.forEach(q,fn,_6);
};
this.getIterator=function(){
return new _3.Iterator(q);
};
this.peek=function(){
return q[(q.length-1)];
};
this.pop=function(){
var r=q.pop();
this.count=q.length;
return r;
};
this.push=function(o){
this.count=q.push(o);
};
this.toArray=function(){
return [].concat(q);
};
};
return _3.Stack;
});

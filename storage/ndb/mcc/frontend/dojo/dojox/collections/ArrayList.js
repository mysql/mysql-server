//>>built
define("dojox/collections/ArrayList",["dojo/_base/kernel","dojo/_base/array","./_base"],function(_1,_2,_3){
_3.ArrayList=function(_4){
var _5=[];
if(_4){
_5=_5.concat(_4);
}
this.count=_5.length;
this.add=function(_6){
_5.push(_6);
this.count=_5.length;
};
this.addRange=function(a){
if(a.getIterator){
var e=a.getIterator();
while(!e.atEnd()){
this.add(e.get());
}
this.count=_5.length;
}else{
for(var i=0;i<a.length;i++){
_5.push(a[i]);
}
this.count=_5.length;
}
};
this.clear=function(){
_5.splice(0,_5.length);
this.count=0;
};
this.clone=function(){
return new _3.ArrayList(_5);
};
this.contains=function(_7){
for(var i=0;i<_5.length;i++){
if(_5[i]==_7){
return true;
}
}
return false;
};
this.forEach=function(fn,_8){
_1.forEach(_5,fn,_8);
};
this.getIterator=function(){
return new _3.Iterator(_5);
};
this.indexOf=function(_9){
for(var i=0;i<_5.length;i++){
if(_5[i]==_9){
return i;
}
}
return -1;
};
this.insert=function(i,_a){
_5.splice(i,0,_a);
this.count=_5.length;
};
this.item=function(i){
return _5[i];
};
this.remove=function(_b){
var i=this.indexOf(_b);
if(i>=0){
_5.splice(i,1);
}
this.count=_5.length;
};
this.removeAt=function(i){
_5.splice(i,1);
this.count=_5.length;
};
this.reverse=function(){
_5.reverse();
};
this.sort=function(fn){
if(fn){
_5.sort(fn);
}else{
_5.sort();
}
};
this.setByIndex=function(i,_c){
_5[i]=_c;
this.count=_5.length;
};
this.toArray=function(){
return [].concat(_5);
};
this.toString=function(_d){
return _5.join((_d||","));
};
};
return _3.ArrayList;
});

//>>built
define("dojox/collections/SortedList",["dojo/_base/kernel","dojo/_base/array","./_base"],function(_1,_2,_3){
_3.SortedList=function(_4){
var _5=this;
var _6={};
var q=[];
var _7=function(a,b){
if(a.key>b.key){
return 1;
}
if(a.key<b.key){
return -1;
}
return 0;
};
var _8=function(){
q=[];
var e=_5.getIterator();
while(!e.atEnd()){
q.push(e.get());
}
q.sort(_7);
};
var _9={};
this.count=q.length;
this.add=function(k,v){
if(!_6[k]){
_6[k]=new _3.DictionaryEntry(k,v);
this.count=q.push(_6[k]);
q.sort(_7);
}
};
this.clear=function(){
_6={};
q=[];
this.count=q.length;
};
this.clone=function(){
return new _3.SortedList(this);
};
this.contains=this.containsKey=function(k){
if(_9[k]){
return false;
}
return (_6[k]!=null);
};
this.containsValue=function(o){
var e=this.getIterator();
while(!e.atEnd()){
var _a=e.get();
if(_a.value==o){
return true;
}
}
return false;
};
this.copyTo=function(_b,i){
var e=this.getIterator();
var _c=i;
while(!e.atEnd()){
_b.splice(_c,0,e.get());
_c++;
}
};
this.entry=function(k){
return _6[k];
};
this.forEach=function(fn,_d){
_1.forEach(q,fn,_d);
};
this.getByIndex=function(i){
return q[i].valueOf();
};
this.getIterator=function(){
return new _3.DictionaryIterator(_6);
};
this.getKey=function(i){
return q[i].key;
};
this.getKeyList=function(){
var _e=[];
var e=this.getIterator();
while(!e.atEnd()){
_e.push(e.get().key);
}
return _e;
};
this.getValueList=function(){
var _f=[];
var e=this.getIterator();
while(!e.atEnd()){
_f.push(e.get().value);
}
return _f;
};
this.indexOfKey=function(k){
for(var i=0;i<q.length;i++){
if(q[i].key==k){
return i;
}
}
return -1;
};
this.indexOfValue=function(o){
for(var i=0;i<q.length;i++){
if(q[i].value==o){
return i;
}
}
return -1;
};
this.item=function(k){
if(k in _6&&!_9[k]){
return _6[k].valueOf();
}
return undefined;
};
this.remove=function(k){
delete _6[k];
_8();
this.count=q.length;
};
this.removeAt=function(i){
delete _6[q[i].key];
_8();
this.count=q.length;
};
this.replace=function(k,v){
if(!_6[k]){
this.add(k,v);
return false;
}else{
_6[k]=new _3.DictionaryEntry(k,v);
_8();
return true;
}
};
this.setByIndex=function(i,o){
_6[q[i].key].value=o;
_8();
this.count=q.length;
};
if(_4){
var e=_4.getIterator();
while(!e.atEnd()){
var _10=e.get();
q[q.length]=_6[_10.key]=new _3.DictionaryEntry(_10.key,_10.value);
}
q.sort(_7);
}
};
return _3.SortedList;
});

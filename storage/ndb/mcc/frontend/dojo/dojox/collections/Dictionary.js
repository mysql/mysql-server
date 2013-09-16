//>>built
define("dojox/collections/Dictionary",["dojo/_base/kernel","dojo/_base/array","./_base"],function(_1,_2,_3){
_3.Dictionary=function(_4){
var _5={};
this.count=0;
var _6={};
this.add=function(k,v){
var b=(k in _5);
_5[k]=new _3.DictionaryEntry(k,v);
if(!b){
this.count++;
}
};
this.clear=function(){
_5={};
this.count=0;
};
this.clone=function(){
return new _3.Dictionary(this);
};
this.contains=this.containsKey=function(k){
if(_6[k]){
return false;
}
return (_5[k]!=null);
};
this.containsValue=function(v){
var e=this.getIterator();
while(e.get()){
if(e.element.value==v){
return true;
}
}
return false;
};
this.entry=function(k){
return _5[k];
};
this.forEach=function(fn,_7){
var a=[];
for(var p in _5){
if(!_6[p]){
a.push(_5[p]);
}
}
_1.forEach(a,fn,_7);
};
this.getKeyList=function(){
return (this.getIterator()).map(function(_8){
return _8.key;
});
};
this.getValueList=function(){
return (this.getIterator()).map(function(_9){
return _9.value;
});
};
this.item=function(k){
if(k in _5){
return _5[k].valueOf();
}
return undefined;
};
this.getIterator=function(){
return new _3.DictionaryIterator(_5);
};
this.remove=function(k){
if(k in _5&&!_6[k]){
delete _5[k];
this.count--;
return true;
}
return false;
};
if(_4){
var e=_4.getIterator();
while(e.get()){
this.add(e.element.key,e.element.value);
}
}
};
return _3.Dictionary;
});

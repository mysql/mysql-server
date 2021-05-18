//>>built
define("dojox/collections/_base",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3){
var _4=_2.getObject("dojox.collections",true);
_4.DictionaryEntry=function(k,v){
this.key=k;
this.value=v;
this.valueOf=function(){
return this.value;
};
this.toString=function(){
return String(this.value);
};
};
_4.Iterator=function(a){
var _5=0;
this.element=a[_5]||null;
this.atEnd=function(){
return (_5>=a.length);
};
this.get=function(){
if(this.atEnd()){
return null;
}
this.element=a[_5++];
return this.element;
};
this.map=function(fn,_6){
return _3.map(a,fn,_6);
};
this.reset=function(){
_5=0;
this.element=a[_5];
};
};
_4.DictionaryIterator=function(_7){
var a=[];
var _8={};
for(var p in _7){
if(!_8[p]){
a.push(_7[p]);
}
}
var _9=0;
this.element=a[_9]||null;
this.atEnd=function(){
return (_9>=a.length);
};
this.get=function(){
if(this.atEnd()){
return null;
}
this.element=a[_9++];
return this.element;
};
this.map=function(fn,_a){
return _3.map(a,fn,_a);
};
this.reset=function(){
_9=0;
this.element=a[_9];
};
};
return _4;
});

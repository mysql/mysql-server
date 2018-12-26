/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/behavior",["./_base/kernel","./_base/lang","./_base/array","./_base/connect","./query","./ready"],function(_1,_2,_3,_4,_5,_6){
_1.behavior=new function(){
function _7(_8,_9){
if(!_8[_9]){
_8[_9]=[];
}
return _8[_9];
};
var _a=0;
function _b(_c,_d,_e){
var _f={};
for(var x in _c){
if(typeof _f[x]=="undefined"){
if(!_e){
_d(_c[x],x);
}else{
_e.call(_d,_c[x],x);
}
}
}
};
this._behaviors={};
this.add=function(_10){
_b(_10,this,function(_11,_12){
var _13=_7(this._behaviors,_12);
if(typeof _13["id"]!="number"){
_13.id=_a++;
}
var _14=[];
_13.push(_14);
if((_2.isString(_11))||(_2.isFunction(_11))){
_11={found:_11};
}
_b(_11,function(_15,_16){
_7(_14,_16).push(_15);
});
});
};
var _17=function(_18,_19,_1a){
if(_2.isString(_19)){
if(_1a=="found"){
_4.publish(_19,[_18]);
}else{
_4.connect(_18,_1a,function(){
_4.publish(_19,arguments);
});
}
}else{
if(_2.isFunction(_19)){
if(_1a=="found"){
_19(_18);
}else{
_4.connect(_18,_1a,_19);
}
}
}
};
this.apply=function(){
_b(this._behaviors,function(_1b,id){
_5(id).forEach(function(_1c){
var _1d=0;
var bid="_dj_behavior_"+_1b.id;
if(typeof _1c[bid]=="number"){
_1d=_1c[bid];
if(_1d==(_1b.length)){
return;
}
}
for(var x=_1d,_1e;_1e=_1b[x];x++){
_b(_1e,function(_1f,_20){
if(_2.isArray(_1f)){
_3.forEach(_1f,function(_21){
_17(_1c,_21,_20);
});
}
});
}
_1c[bid]=_1b.length;
});
});
};
};
_6(_1.behavior,"apply");
return _1.behavior;
});

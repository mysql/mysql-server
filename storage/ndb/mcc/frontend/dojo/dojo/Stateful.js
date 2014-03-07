/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/Stateful",["./_base/kernel","./_base/declare","./_base/lang","./_base/array"],function(_1,_2,_3,_4){
return _1.declare("dojo.Stateful",null,{postscript:function(_5){
if(_5){
_3.mixin(this,_5);
}
},get:function(_6){
return this[_6];
},set:function(_7,_8){
if(typeof _7==="object"){
for(var x in _7){
this.set(x,_7[x]);
}
return this;
}
var _9=this[_7];
this[_7]=_8;
if(this._watchCallbacks){
this._watchCallbacks(_7,_9,_8);
}
return this;
},watch:function(_a,_b){
var _c=this._watchCallbacks;
if(!_c){
var _d=this;
_c=this._watchCallbacks=function(_e,_f,_10,_11){
var _12=function(_13){
if(_13){
_13=_13.slice();
for(var i=0,l=_13.length;i<l;i++){
try{
_13[i].call(_d,_e,_f,_10);
}
catch(e){
console.error(e);
}
}
}
};
_12(_c["_"+_e]);
if(!_11){
_12(_c["*"]);
}
};
}
if(!_b&&typeof _a==="function"){
_b=_a;
_a="*";
}else{
_a="_"+_a;
}
var _14=_c[_a];
if(typeof _14!=="object"){
_14=_c[_a]=[];
}
_14.push(_b);
return {unwatch:function(){
_14.splice(_4.indexOf(_14,_b),1);
}};
}});
});

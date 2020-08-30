/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/promise/all",["../_base/array","../_base/lang","../Deferred","../when"],function(_1,_2,_3,_4){
"use strict";
var _5=_1.some;
return function all(_6){
var _7,_1;
if(_2.isArray(_6)){
_1=_6;
}else{
if(_6&&typeof _6==="object"){
_7=_6;
}
}
var _8;
var _9=[];
if(_7){
_1=[];
for(var _a in _7){
if(Object.hasOwnProperty.call(_7,_a)){
_9.push(_a);
_1.push(_7[_a]);
}
}
_8={};
}else{
if(_1){
_8=[];
}
}
if(!_1||!_1.length){
return new _3().resolve(_8);
}
var _b=new _3();
_b.promise.always(function(){
_8=_9=null;
});
var _c=_1.length;
_5(_1,function(_d,_e){
if(!_7){
_9.push(_e);
}
_4(_d,function(_f){
if(!_b.isFulfilled()){
_8[_9[_e]]=_f;
if(--_c===0){
_b.resolve(_8);
}
}
},_b.reject);
return _b.isFulfilled();
});
return _b.promise;
};
});

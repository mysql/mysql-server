/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/data/util/simpleFetch",["dojo/_base/lang","dojo/_base/window","./sorter"],function(_1,_2,_3){
var _4=_1.getObject("dojo.data.util.simpleFetch",true);
_4.fetch=function(_5){
_5=_5||{};
if(!_5.store){
_5.store=this;
}
var _6=this;
var _7=function(_8,_9){
if(_9.onError){
var _a=_9.scope||_2.global;
_9.onError.call(_a,_8,_9);
}
};
var _b=function(_c,_d){
var _e=_d.abort||null;
var _f=false;
var _10=_d.start?_d.start:0;
var _11=(_d.count&&(_d.count!==Infinity))?(_10+_d.count):_c.length;
_d.abort=function(){
_f=true;
if(_e){
_e.call(_d);
}
};
var _12=_d.scope||_2.global;
if(!_d.store){
_d.store=_6;
}
if(_d.onBegin){
_d.onBegin.call(_12,_c.length,_d);
}
if(_d.sort){
_c.sort(_3.createSortFunction(_d.sort,_6));
}
if(_d.onItem){
for(var i=_10;(i<_c.length)&&(i<_11);++i){
var _13=_c[i];
if(!_f){
_d.onItem.call(_12,_13,_d);
}
}
}
if(_d.onComplete&&!_f){
var _14=null;
if(!_d.onItem){
_14=_c.slice(_10,_11);
}
_d.onComplete.call(_12,_14,_d);
}
};
this._fetchItems(_5,_b,_7);
return _5;
};
return _4;
});

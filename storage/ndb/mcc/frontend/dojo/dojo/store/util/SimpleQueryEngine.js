/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/store/util/SimpleQueryEngine",["../../_base/array"],function(_1){
return function(_2,_3){
switch(typeof _2){
default:
throw new Error("Can not query with a "+typeof _2);
case "object":
case "undefined":
var _4=_2;
_2=function(_5){
for(var _6 in _4){
var _7=_4[_6];
if(_7&&_7.test){
if(!_7.test(_5[_6])){
return false;
}
}else{
if(_7!=_5[_6]){
return false;
}
}
}
return true;
};
break;
case "string":
if(!this[_2]){
throw new Error("No filter function "+_2+" was found in store");
}
_2=this[_2];
case "function":
}
function _8(_9){
var _a=_1.filter(_9,_2);
if(_3&&_3.sort){
_a.sort(function(a,b){
for(var _b,i=0;_b=_3.sort[i];i++){
var _c=a[_b.attribute];
var _d=b[_b.attribute];
if(_c!=_d){
return !!_b.descending==_c>_d?-1:1;
}
}
return 0;
});
}
if(_3&&(_3.start||_3.count)){
var _e=_a.length;
_a=_a.slice(_3.start||0,(_3.start||0)+(_3.count||Infinity));
_a.total=_e;
}
return _a;
};
_8.matches=_2;
return _8;
};
});

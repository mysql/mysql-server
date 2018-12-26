/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/io-query",["./_base/lang"],function(_1){
var _2={};
function _3(_4){
var _5=encodeURIComponent,_6=[];
for(var _7 in _4){
var _8=_4[_7];
if(_8!=_2[_7]){
var _9=_5(_7)+"=";
if(_1.isArray(_8)){
for(var i=0,l=_8.length;i<l;++i){
_6.push(_9+_5(_8[i]));
}
}else{
_6.push(_9+_5(_8));
}
}
}
return _6.join("&");
};
function _a(_b){
var _c=decodeURIComponent,qp=_b.split("&"),_d={},_e,_f;
for(var i=0,l=qp.length,_10;i<l;++i){
_10=qp[i];
if(_10.length){
var s=_10.indexOf("=");
if(s<0){
_e=_c(_10);
_f="";
}else{
_e=_c(_10.slice(0,s));
_f=_c(_10.slice(s+1));
}
if(typeof _d[_e]=="string"){
_d[_e]=[_d[_e]];
}
if(_1.isArray(_d[_e])){
_d[_e].push(_f);
}else{
_d[_e]=_f;
}
}
}
return _d;
};
return {objectToQuery:_3,queryToObject:_a};
});

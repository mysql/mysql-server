/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom-prop",["exports","./_base/kernel","./_base/sniff","./_base/lang","./dom","./dom-style","./dom-construct","./_base/connect"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9={},_a=0,_b=_2._scopeName+"attrid";
var _c={col:1,colgroup:1,table:1,tbody:1,tfoot:1,thead:1,tr:1,title:1};
_1.names={"class":"className","for":"htmlFor",tabindex:"tabIndex",readonly:"readOnly",colspan:"colSpan",frameborder:"frameBorder",rowspan:"rowSpan",valuetype:"valueType"};
_1.get=function getProp(_d,_e){
_d=_5.byId(_d);
var lc=_e.toLowerCase(),_f=_1.names[lc]||_e;
return _d[_f];
};
_1.set=function setProp(_10,_11,_12){
_10=_5.byId(_10);
var l=arguments.length;
if(l==2&&typeof _11!="string"){
for(var x in _11){
_1.set(_10,x,_11[x]);
}
return _10;
}
var lc=_11.toLowerCase(),_13=_1.names[lc]||_11;
if(_13=="style"&&typeof _12!="string"){
_6.style(_10,_12);
return _10;
}
if(_13=="innerHTML"){
if(_3("ie")&&_10.tagName.toLowerCase() in _c){
_7.empty(_10);
_10.appendChild(_7.toDom(_12,_10.ownerDocument));
}else{
_10[_13]=_12;
}
return _10;
}
if(_4.isFunction(_12)){
var _14=_10[_b];
if(!_14){
_14=_a++;
_10[_b]=_14;
}
if(!_9[_14]){
_9[_14]={};
}
var h=_9[_14][_13];
if(h){
_8.disconnect(h);
}else{
try{
delete _10[_13];
}
catch(e){
}
}
if(_12){
_9[_14][_13]=_8.connect(_10,_13,_12);
}else{
_10[_13]=null;
}
return _10;
}
_10[_13]=_12;
return _10;
};
});

/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom-attr",["exports","./_base/sniff","./_base/lang","./dom","./dom-style","./dom-prop"],function(_1,_2,_3,_4,_5,_6){
var _7={innerHTML:1,className:1,htmlFor:_2("ie"),value:1},_8={classname:"class",htmlfor:"for",tabindex:"tabIndex",readonly:"readOnly"};
function _9(_a,_b){
var _c=_a.getAttributeNode&&_a.getAttributeNode(_b);
return _c&&_c.specified;
};
_1.has=function hasAttr(_d,_e){
var lc=_e.toLowerCase();
return _7[_6.names[lc]||_e]||_9(_4.byId(_d),_8[lc]||_e);
};
_1.get=function getAttr(_f,_10){
_f=_4.byId(_f);
var lc=_10.toLowerCase(),_11=_6.names[lc]||_10,_12=_7[_11];
value=_f[_11];
if(_12&&typeof value!="undefined"){
return value;
}
if(_11!="href"&&(typeof value=="boolean"||_3.isFunction(value))){
return value;
}
var _13=_8[lc]||_10;
return _9(_f,_13)?_f.getAttribute(_13):null;
};
_1.set=function setAttr(_14,_15,_16){
_14=_4.byId(_14);
if(arguments.length==2){
for(var x in _15){
_1.set(_14,x,_15[x]);
}
return _14;
}
var lc=_15.toLowerCase(),_17=_6.names[lc]||_15,_18=_7[_17];
if(_17=="style"&&typeof _16!="string"){
_5.set(_14,_16);
return _14;
}
if(_18||typeof _16=="boolean"||_3.isFunction(_16)){
return _6.set(_14,_15,_16);
}
_14.setAttribute(_8[lc]||_15,_16);
return _14;
};
_1.remove=function removeAttr(_19,_1a){
_4.byId(_19).removeAttribute(_8[_1a.toLowerCase()]||_1a);
};
_1.getNodeProp=function getNodeProp(_1b,_1c){
_1b=_4.byId(_1b);
var lc=_1c.toLowerCase(),_1d=_6.names[lc]||_1c;
if((_1d in _1b)&&_1d!="href"){
return _1b[_1d];
}
var _1e=_8[lc]||_1c;
return _9(_1b,_1e)?_1b.getAttribute(_1e):null;
};
});

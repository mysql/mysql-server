//>>built
define("dijit/a11y",["dojo/_base/array","dojo/_base/config","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-style","dojo/_base/sniff","./_base/manager","."],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=(_9._isElementShown=function(_b){
var s=_6.get(_b);
return (s.visibility!="hidden")&&(s.visibility!="collapsed")&&(s.display!="none")&&(_5.get(_b,"type")!="hidden");
});
_9.hasDefaultTabStop=function(_c){
switch(_c.nodeName.toLowerCase()){
case "a":
return _5.has(_c,"href");
case "area":
case "button":
case "input":
case "object":
case "select":
case "textarea":
return true;
case "iframe":
var _d;
try{
var _e=_c.contentDocument;
if("designMode" in _e&&_e.designMode=="on"){
return true;
}
_d=_e.body;
}
catch(e1){
try{
_d=_c.contentWindow.document.body;
}
catch(e2){
return false;
}
}
return _d&&(_d.contentEditable=="true"||(_d.firstChild&&_d.firstChild.contentEditable=="true"));
default:
return _c.contentEditable=="true";
}
};
var _f=(_9.isTabNavigable=function(_10){
if(_5.get(_10,"disabled")){
return false;
}else{
if(_5.has(_10,"tabIndex")){
return _5.get(_10,"tabIndex")>=0;
}else{
return _9.hasDefaultTabStop(_10);
}
}
});
_9._getTabNavigable=function(_11){
var _12,_13,_14,_15,_16,_17,_18={};
function _19(_1a){
return _1a&&_1a.tagName.toLowerCase()=="input"&&_1a.type&&_1a.type.toLowerCase()=="radio"&&_1a.name&&_1a.name.toLowerCase();
};
var _1b=function(_1c){
for(var _1d=_1c.firstChild;_1d;_1d=_1d.nextSibling){
if(_1d.nodeType!=1||(_7("ie")&&_1d.scopeName!=="HTML")||!_a(_1d)){
continue;
}
if(_f(_1d)){
var _1e=_5.get(_1d,"tabIndex");
if(!_5.has(_1d,"tabIndex")||_1e==0){
if(!_12){
_12=_1d;
}
_13=_1d;
}else{
if(_1e>0){
if(!_14||_1e<_15){
_15=_1e;
_14=_1d;
}
if(!_16||_1e>=_17){
_17=_1e;
_16=_1d;
}
}
}
var rn=_19(_1d);
if(_5.get(_1d,"checked")&&rn){
_18[rn]=_1d;
}
}
if(_1d.nodeName.toUpperCase()!="SELECT"){
_1b(_1d);
}
}
};
if(_a(_11)){
_1b(_11);
}
function rs(_1f){
return _18[_19(_1f)]||_1f;
};
return {first:rs(_12),last:rs(_13),lowest:rs(_14),highest:rs(_16)};
};
_9.getFirstInTabbingOrder=function(_20){
var _21=_9._getTabNavigable(_4.byId(_20));
return _21.lowest?_21.lowest:_21.first;
};
_9.getLastInTabbingOrder=function(_22){
var _23=_9._getTabNavigable(_4.byId(_22));
return _23.last?_23.last:_23.highest;
};
return {hasDefaultTabStop:_9.hasDefaultTabStop,isTabNavigable:_9.isTabNavigable,_getTabNavigable:_9._getTabNavigable,getFirstInTabbingOrder:_9.getFirstInTabbingOrder,getLastInTabbingOrder:_9.getLastInTabbingOrder};
});

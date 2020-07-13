/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dom-prop",["exports","./_base/kernel","./sniff","./_base/lang","./dom","./dom-style","./dom-construct","./_base/connect"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9={},_a=1,_b=_2._scopeName+"attrid";
_3.add("dom-textContent",function(_c,_d,_e){
return "textContent" in _e;
});
_1.names={"class":"className","for":"htmlFor",tabindex:"tabIndex",readonly:"readOnly",colspan:"colSpan",frameborder:"frameBorder",rowspan:"rowSpan",textcontent:"textContent",valuetype:"valueType"};
function _f(_10){
var _11="",ch=_10.childNodes;
for(var i=0,n;n=ch[i];i++){
if(n.nodeType!=8){
if(n.nodeType==1){
_11+=_f(n);
}else{
_11+=n.nodeValue;
}
}
}
return _11;
};
_1.get=function getProp(_12,_13){
_12=_5.byId(_12);
var lc=_13.toLowerCase(),_14=_1.names[lc]||_13;
if(_14=="textContent"&&!_3("dom-textContent")){
return _f(_12);
}
return _12[_14];
};
_1.set=function setProp(_15,_16,_17){
_15=_5.byId(_15);
var l=arguments.length;
if(l==2&&typeof _16!="string"){
for(var x in _16){
_1.set(_15,x,_16[x]);
}
return _15;
}
var lc=_16.toLowerCase(),_18=_1.names[lc]||_16;
if(_18=="style"&&typeof _17!="string"){
_6.set(_15,_17);
return _15;
}
if(_18=="innerHTML"){
if(_3("ie")&&_15.tagName.toLowerCase() in {col:1,colgroup:1,table:1,tbody:1,tfoot:1,thead:1,tr:1,title:1}){
_7.empty(_15);
_15.appendChild(_7.toDom(_17,_15.ownerDocument));
}else{
_15[_18]=_17;
}
return _15;
}
if(_18=="textContent"&&!_3("dom-textContent")){
_7.empty(_15);
_15.appendChild(_15.ownerDocument.createTextNode(_17));
return _15;
}
if(_4.isFunction(_17)){
var _19=_15[_b];
if(!_19){
_19=_a++;
_15[_b]=_19;
}
if(!_9[_19]){
_9[_19]={};
}
var h=_9[_19][_18];
if(h){
_8.disconnect(h);
}else{
try{
delete _15[_18];
}
catch(e){
}
}
if(_17){
_9[_19][_18]=_8.connect(_15,_18,_17);
}else{
_15[_18]=null;
}
return _15;
}
_15[_18]=_17;
return _15;
};
});

/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/has",["./global","require","module"],function(_1,_2,_3){
var _4=_2.has||function(){
};
if(!1){
var _5=typeof window!="undefined"&&typeof location!="undefined"&&typeof document!="undefined"&&window.location==location&&window.document==document,_6=_5&&document,_7=_6&&_6.createElement("DiV"),_8=(_3.config&&_3.config())||{};
_4=function(_9){
return typeof _8[_9]=="function"?(_8[_9]=_8[_9](_1,_6,_7)):_8[_9];
};
_4.cache=_8;
_4.add=function(_a,_b,_c,_d){
(typeof _8[_a]=="undefined"||_d)&&(_8[_a]=_b);
return _c&&_4(_a);
};
1||_4.add("host-browser",_5);
0&&_4.add("host-node",(typeof process=="object"&&process.versions&&process.versions.node&&process.versions.v8));
0&&_4.add("host-rhino",(typeof load=="function"&&(typeof Packages=="function"||typeof Packages=="object")));
1||_4.add("dom",_5);
1||_4.add("dojo-dom-ready-api",1);
1||_4.add("dojo-sniff",1);
}
if(1){
_4.add("dom-addeventlistener",!!document.addEventListener);
_4.add("touch","ontouchstart" in document||("onpointerdown" in document&&navigator.maxTouchPoints>0)||window.navigator.msMaxTouchPoints);
_4.add("touch-events","ontouchstart" in document);
_4.add("pointer-events","pointerEnabled" in window.navigator?window.navigator.pointerEnabled:"PointerEvent" in window);
_4.add("MSPointer",window.navigator.msPointerEnabled);
_4.add("touch-action",_4("touch")&&_4("pointer-events"));
_4.add("device-width",screen.availWidth||innerWidth);
var _e=document.createElement("form");
_4.add("dom-attributes-explicit",_e.attributes.length==0);
_4.add("dom-attributes-specified-flag",_e.attributes.length>0&&_e.attributes.length<40);
}
_4.clearElement=function(_f){
_f.innerHTML="";
return _f;
};
_4.normalize=function(id,_10){
var _11=id.match(/[\?:]|[^:\?]*/g),i=0,get=function(_12){
var _13=_11[i++];
if(_13==":"){
return 0;
}else{
if(_11[i++]=="?"){
if(!_12&&_4(_13)){
return get();
}else{
get(true);
return get(_12);
}
}
return _13||0;
}
};
id=get();
return id&&_10(id);
};
_4.load=function(id,_14,_15){
if(id){
_14([id],_15);
}else{
_15();
}
};
return _4;
});

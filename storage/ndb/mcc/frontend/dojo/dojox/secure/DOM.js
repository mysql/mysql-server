//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/lang/observable"],function(_1,_2,_3){
_2.provide("dojox.secure.DOM");
_2.require("dojox.lang.observable");
_3.secure.DOM=function(_4){
function _5(_6){
if(!_6){
return _6;
}
var _7=_6;
do{
if(_7==_4){
return _8(_6);
}
}while((_7=_7.parentNode));
return null;
};
function _8(_9){
if(_9){
if(_9.nodeType){
var _a=_b(_9);
if(_9.nodeType==1&&typeof _a.style=="function"){
_a.style=_c(_9.style);
_a.ownerDocument=_d;
_a.childNodes={__get__:function(i){
return _8(_9.childNodes[i]);
},length:0};
}
return _a;
}
if(_9&&typeof _9=="object"){
if(_9.__observable){
return _9.__observable;
}
_a=_9 instanceof Array?[]:{};
_9.__observable=_a;
for(var i in _9){
if(i!="__observable"){
_a[i]=_8(_9[i]);
}
}
_a.data__=_9;
return _a;
}
if(typeof _9=="function"){
var _e=function(_f){
if(typeof _f=="function"){
return function(){
for(var i=0;i<arguments.length;i++){
arguments[i]=_8(arguments[i]);
}
return _e(_f.apply(_8(this),arguments));
};
}
return _3.secure.unwrap(_f);
};
return function(){
if(_9.safetyCheck){
_9.safetyCheck.apply(_e(this),arguments);
}
for(var i=0;i<arguments.length;i++){
arguments[i]=_e(arguments[i]);
}
return _8(_9.apply(_e(this),arguments));
};
}
}
return _9;
};
unwrap=_3.secure.unwrap;
function _10(css){
css+="";
if(css.match(/behavior:|content:|javascript:|binding|expression|\@import/)){
throw new Error("Illegal CSS");
}
var id=_4.id||(_4.id="safe"+(""+Math.random()).substring(2));
return css.replace(/(\}|^)\s*([^\{]*\{)/g,function(t,a,b){
return a+" #"+id+" "+b;
});
};
function _11(url){
if(url.match(/:/)&&!url.match(/^(http|ftp|mailto)/)){
throw new Error("Unsafe URL "+url);
}
};
function _12(el){
if(el&&el.nodeType==1){
if(el.tagName.match(/script/i)){
var src=el.src;
if(src&&src!=""){
el.parentNode.removeChild(el);
_2.xhrGet({url:src,secure:true}).addCallback(function(_13){
_d.evaluate(_13);
});
}else{
var _14=el.innerHTML;
el.parentNode.removeChild(el);
_8.evaluate(_14);
}
}
if(el.tagName.match(/link/i)){
throw new Error("illegal tag");
}
if(el.tagName.match(/style/i)){
var _15=function(_16){
if(el.styleSheet){
el.styleSheet.cssText=_16;
}else{
var _17=doc.createTextNode(_16);
if(el.childNodes[0]){
el.replaceChild(_17,el.childNodes[0]);
}else{
el.appendChild(_17);
}
}
};
src=el.src;
if(src&&src!=""){
alert("src"+src);
el.src=null;
_2.xhrGet({url:src,secure:true}).addCallback(function(_18){
_15(_10(_18));
});
}
_15(_10(el.innerHTML));
}
if(el.style){
_10(el.style.cssText);
}
if(el.href){
_11(el.href);
}
if(el.src){
_11(el.src);
}
var _19,i=0;
while((_19=el.attributes[i++])){
if(_19.name.substring(0,2)=="on"&&_19.value!="null"&&_19.value!=""){
throw new Error("event handlers not allowed in the HTML, they must be set with element.addEventListener");
}
}
var _1a=el.childNodes;
for(var i=0,l=_1a.length;i<l;i++){
_12(_1a[i]);
}
}
};
function _1b(_1c){
var div=document.createElement("div");
if(_1c.match(/<object/i)){
throw new Error("The object tag is not allowed");
}
div.innerHTML=_1c;
_12(div);
return div;
};
var doc=_4.ownerDocument;
var _d={getElementById:function(id){
return _5(doc.getElementById(id));
},createElement:function(_1d){
return _8(doc.createElement(_1d));
},createTextNode:function(_1e){
return _8(doc.createTextNode(_1e));
},write:function(str){
var div=_1b(str);
while(div.childNodes.length){
_4.appendChild(div.childNodes[0]);
}
}};
_d.open=_d.close=function(){
};
var _1f={innerHTML:function(_20,_21){
_20.innerHTML=_1b(_21).innerHTML;
}};
_1f.outerHTML=function(_22,_23){
throw new Error("Can not set this property");
};
function _24(_25,_26){
return function(_27,_28){
_12(_28[_26]);
return _27[_25](_28[0]);
};
};
var _29={appendChild:_24("appendChild",0),insertBefore:_24("insertBefore",0),replaceChild:_24("replaceChild",1),cloneNode:function(_2a,_2b){
return _2a.cloneNode(_2b[0]);
},addEventListener:function(_2c,_2d){
_2.connect(_2c,"on"+_2d[0],this,function(_2e){
_2e=_b(_2e||window.event);
_2d[1].call(this,_2e);
});
}};
_29.childNodes=_29.style=_29.ownerDocument=function(){
};
function _2f(_30){
return _3.lang.makeObservable(function(_31,_32){
var _33;
return _31[_32];
},_30,function(_34,_35,_36,_37){
for(var i=0;i<_37.length;i++){
_37[i]=unwrap(_37[i]);
}
if(_29[_36]){
return _8(_29[_36].call(_34,_35,_37));
}
return _8(_35[_36].apply(_35,_37));
},_29);
};
var _b=_2f(function(_38,_39,_3a){
if(_1f[_39]){
_1f[_39](_38,_3a);
}
_38[_39]=_3a;
});
var _3b={behavior:1,MozBinding:1};
var _c=_2f(function(_3c,_3d,_3e){
if(!_3b[_3d]){
_3c[_3d]=_10(_3e);
}
});
_8.safeHTML=_1b;
_8.safeCSS=_10;
return _8;
};
_3.secure.unwrap=function unwrap(_3f){
return (_3f&&_3f.data__)||_3f;
};
});

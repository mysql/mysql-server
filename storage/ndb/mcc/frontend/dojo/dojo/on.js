/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/on",["./has!dom-addeventlistener?:./aspect","./_base/kernel","./has"],function(_1,_2,_3){
"use strict";
if(1){
var _4=window.ScriptEngineMajorVersion;
_3.add("jscript",_4&&(_4()+ScriptEngineMinorVersion()/10));
_3.add("event-orientationchange",_3("touch")&&!_3("android"));
_3.add("event-stopimmediatepropagation",window.Event&&!!window.Event.prototype&&!!window.Event.prototype.stopImmediatePropagation);
_3.add("event-focusin",function(_5,_6,_7){
return "onfocusin" in _7;
});
if(_3("touch")){
_3.add("touch-can-modify-event-delegate",function(){
var _8=function(){
};
_8.prototype=document.createEvent("MouseEvents");
try{
var _9=new _8;
_9.target=null;
return _9.target===null;
}
catch(e){
return false;
}
});
}
}
var on=function(_a,_b,_c,_d){
if(typeof _a.on=="function"&&typeof _b!="function"&&!_a.nodeType){
return _a.on(_b,_c);
}
return on.parse(_a,_b,_c,_e,_d,this);
};
on.pausable=function(_f,_10,_11,_12){
var _13;
var _14=on(_f,_10,function(){
if(!_13){
return _11.apply(this,arguments);
}
},_12);
_14.pause=function(){
_13=true;
};
_14.resume=function(){
_13=false;
};
return _14;
};
on.once=function(_15,_16,_17,_18){
var _19=on(_15,_16,function(){
_19.remove();
return _17.apply(this,arguments);
});
return _19;
};
on.parse=function(_1a,_1b,_1c,_1d,_1e,_1f){
if(_1b.call){
return _1b.call(_1f,_1a,_1c);
}
if(_1b.indexOf(",")>-1){
var _20=_1b.split(/\s*,\s*/);
var _21=[];
var i=0;
var _22;
while(_22=_20[i++]){
_21.push(_1d(_1a,_22,_1c,_1e,_1f));
}
_21.remove=function(){
for(var i=0;i<_21.length;i++){
_21[i].remove();
}
};
return _21;
}
return _1d(_1a,_1b,_1c,_1e,_1f);
};
var _23=/^touch/;
function _e(_24,_25,_26,_27,_28){
var _29=_25.match(/(.*):(.*)/);
if(_29){
_25=_29[2];
_29=_29[1];
return on.selector(_29,_25).call(_28,_24,_26);
}
if(_3("touch")){
if(_23.test(_25)){
_26=_2a(_26);
}
if(!_3("event-orientationchange")&&(_25=="orientationchange")){
_25="resize";
_24=window;
_26=_2a(_26);
}
}
if(_2b){
_26=_2b(_26);
}
if(_24.addEventListener){
var _2c=_25 in _2d,_2e=_2c?_2d[_25]:_25;
_24.addEventListener(_2e,_26,_2c);
return {remove:function(){
_24.removeEventListener(_2e,_26,_2c);
}};
}
_25="on"+_25;
if(_2f&&_24.attachEvent){
return _2f(_24,_25,_26);
}
throw new Error("Target must be an event emitter");
};
on.selector=function(_30,_31,_32){
return function(_33,_34){
var _35=typeof _30=="function"?{matches:_30}:this,_36=_31.bubble;
function _37(_38){
_35=_35&&_35.matches?_35:_2.query;
while(!_35.matches(_38,_30,_33)){
if(_38==_33||_32===false||!(_38=_38.parentNode)||_38.nodeType!=1){
return;
}
}
return _38;
};
if(_36){
return on(_33,_36(_37),_34);
}
return on(_33,_31,function(_39){
var _3a=_37(_39.target);
if(_3a){
return _34.call(_3a,_39);
}
});
};
};
function _3b(){
this.cancelable=false;
};
function _3c(){
this.bubbles=false;
};
var _3d=[].slice,_3e=on.emit=function(_3f,_40,_41){
var _42=_3d.call(arguments,2);
var _43="on"+_40;
if("parentNode" in _3f){
var _44=_42[0]={};
for(var i in _41){
_44[i]=_41[i];
}
_44.preventDefault=_3b;
_44.stopPropagation=_3c;
_44.target=_3f;
_44.type=_40;
_41=_44;
}
do{
_3f[_43]&&_3f[_43].apply(_3f,_42);
}while(_41&&_41.bubbles&&(_3f=_3f.parentNode));
return _41&&_41.cancelable&&_41;
};
var _2d=_3("event-focusin")?{}:{focusin:"focus",focusout:"blur"};
if(!_3("event-stopimmediatepropagation")){
var _45=function(){
this.immediatelyStopped=true;
this.modified=true;
};
var _2b=function(_46){
return function(_47){
if(!_47.immediatelyStopped){
_47.stopImmediatePropagation=_45;
return _46.apply(this,arguments);
}
};
};
}
if(_3("dom-addeventlistener")){
on.emit=function(_48,_49,_4a){
if(_48.dispatchEvent&&document.createEvent){
var _4b=_48.ownerDocument||document;
var _4c=_4b.createEvent("HTMLEvents");
_4c.initEvent(_49,!!_4a.bubbles,!!_4a.cancelable);
for(var i in _4a){
var _4d=_4a[i];
if(!(i in _4c)){
_4c[i]=_4a[i];
}
}
return _48.dispatchEvent(_4c)&&_4c;
}
return _3e.apply(on,arguments);
};
}else{
on._fixEvent=function(evt,_4e){
if(!evt){
var w=_4e&&(_4e.ownerDocument||_4e.document||_4e).parentWindow||window;
evt=w.event;
}
if(!evt){
return evt;
}
if(_4f&&evt.type==_4f.type){
evt=_4f;
}
if(!evt.target){
evt.target=evt.srcElement;
evt.currentTarget=(_4e||evt.srcElement);
if(evt.type=="mouseover"){
evt.relatedTarget=evt.fromElement;
}
if(evt.type=="mouseout"){
evt.relatedTarget=evt.toElement;
}
if(!evt.stopPropagation){
evt.stopPropagation=_50;
evt.preventDefault=_51;
}
switch(evt.type){
case "keypress":
var c=("charCode" in evt?evt.charCode:evt.keyCode);
if(c==10){
c=0;
evt.keyCode=13;
}else{
if(c==13||c==27){
c=0;
}else{
if(c==3){
c=99;
}
}
}
evt.charCode=c;
_52(evt);
break;
}
}
return evt;
};
var _4f,_53=function(_54){
this.handle=_54;
};
_53.prototype.remove=function(){
delete _dojoIEListeners_[this.handle];
};
var _55=function(_56){
return function(evt){
evt=on._fixEvent(evt,this);
var _57=_56.call(this,evt);
if(evt.modified){
if(!_4f){
setTimeout(function(){
_4f=null;
});
}
_4f=evt;
}
return _57;
};
};
var _2f=function(_58,_59,_5a){
_5a=_55(_5a);
if(((_58.ownerDocument?_58.ownerDocument.parentWindow:_58.parentWindow||_58.window||window)!=top||_3("jscript")<5.8)&&!_3("config-_allow_leaks")){
if(typeof _dojoIEListeners_=="undefined"){
_dojoIEListeners_=[];
}
var _5b=_58[_59];
if(!_5b||!_5b.listeners){
var _5c=_5b;
_5b=Function("event","var callee = arguments.callee; for(var i = 0; i<callee.listeners.length; i++){var listener = _dojoIEListeners_[callee.listeners[i]]; if(listener){listener.call(this,event);}}");
_5b.listeners=[];
_58[_59]=_5b;
_5b.global=this;
if(_5c){
_5b.listeners.push(_dojoIEListeners_.push(_5c)-1);
}
}
var _5d;
_5b.listeners.push(_5d=(_5b.global._dojoIEListeners_.push(_5a)-1));
return new _53(_5d);
}
return _1.after(_58,_59,_5a,true);
};
var _52=function(evt){
evt.keyChar=evt.charCode?String.fromCharCode(evt.charCode):"";
evt.charOrCode=evt.keyChar||evt.keyCode;
};
var _50=function(){
this.cancelBubble=true;
};
var _51=on._preventDefault=function(){
this.bubbledKeyCode=this.keyCode;
if(this.ctrlKey){
try{
this.keyCode=0;
}
catch(e){
}
}
this.defaultPrevented=true;
this.returnValue=false;
};
}
if(_3("touch")){
var _5e=function(){
};
var _5f=window.orientation;
var _2a=function(_60){
return function(_61){
var _62=_61.corrected;
if(!_62){
var _63=_61.type;
try{
delete _61.type;
}
catch(e){
}
if(_61.type){
if(_3("touch-can-modify-event-delegate")){
_5e.prototype=_61;
_62=new _5e;
}else{
_62={};
for(var _64 in _61){
_62[_64]=_61[_64];
}
}
_62.preventDefault=function(){
_61.preventDefault();
};
_62.stopPropagation=function(){
_61.stopPropagation();
};
}else{
_62=_61;
_62.type=_63;
}
_61.corrected=_62;
if(_63=="resize"){
if(_5f==window.orientation){
return null;
}
_5f=window.orientation;
_62.type="orientationchange";
return _60.call(this,_62);
}
if(!("rotation" in _62)){
_62.rotation=0;
_62.scale=1;
}
if(window.TouchEvent&&_61 instanceof TouchEvent){
var _65=_62.changedTouches[0];
for(var i in _65){
delete _62[i];
_62[i]=_65[i];
}
}
}
return _60.call(this,_62);
};
};
}
return on;
});

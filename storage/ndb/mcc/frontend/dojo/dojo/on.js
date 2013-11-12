/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
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
}
var on=function(_5,_6,_7,_8){
if(_5.on){
return _5.on(_6,_7);
}
return on.parse(_5,_6,_7,_9,_8,this);
};
on.pausable=function(_a,_b,_c,_d){
var _e;
var _f=on(_a,_b,function(){
if(!_e){
return _c.apply(this,arguments);
}
},_d);
_f.pause=function(){
_e=true;
};
_f.resume=function(){
_e=false;
};
return _f;
};
on.once=function(_10,_11,_12,_13){
var _14=on(_10,_11,function(){
_14.remove();
return _12.apply(this,arguments);
});
return _14;
};
on.parse=function(_15,_16,_17,_18,_19,_1a){
if(_16.call){
return _16.call(_1a,_15,_17);
}
if(_16.indexOf(",")>-1){
var _1b=_16.split(/\s*,\s*/);
var _1c=[];
var i=0;
var _1d;
while(_1d=_1b[i++]){
_1c.push(_18(_15,_1d,_17,_19,_1a));
}
_1c.remove=function(){
for(var i=0;i<_1c.length;i++){
_1c[i].remove();
}
};
return _1c;
}
return _18(_15,_16,_17,_19,_1a);
};
var _1e=/^touch/;
function _9(_1f,_20,_21,_22,_23){
var _24=_20.match(/(.*):(.*)/);
if(_24){
_20=_24[2];
_24=_24[1];
return on.selector(_24,_20).call(_23,_1f,_21);
}
if(_3("touch")){
if(_1e.test(_20)){
_21=_25(_21);
}
if(!_3("event-orientationchange")&&(_20=="orientationchange")){
_20="resize";
_1f=window;
_21=_25(_21);
}
}
if(_1f.addEventListener){
var _26=_20 in _27;
_1f.addEventListener(_26?_27[_20]:_20,_21,_26);
return {remove:function(){
_1f.removeEventListener(_20,_21,_26);
}};
}
_20="on"+_20;
if(_28&&_1f.attachEvent){
return _28(_1f,_20,_21);
}
throw new Error("Target must be an event emitter");
};
on.selector=function(_29,_2a,_2b){
return function(_2c,_2d){
var _2e=this;
var _2f=_2a.bubble;
if(_2f){
_2a=_2f;
}else{
if(_2b!==false){
_2b=true;
}
}
return on(_2c,_2a,function(_30){
var _31=_30.target;
_2e=_2e&&_2e.matches?_2e:_2.query;
while(!_2e.matches(_31,_29,_2c)){
if(_31==_2c||!_2b||!(_31=_31.parentNode)){
return;
}
}
return _2d.call(_31,_30);
});
};
};
function _32(){
this.cancelable=false;
};
function _33(){
this.bubbles=false;
};
var _34=[].slice,_35=on.emit=function(_36,_37,_38){
var _39=_34.call(arguments,2);
var _3a="on"+_37;
if("parentNode" in _36){
var _3b=_39[0]={};
for(var i in _38){
_3b[i]=_38[i];
}
_3b.preventDefault=_32;
_3b.stopPropagation=_33;
_3b.target=_36;
_3b.type=_37;
_38=_3b;
}
do{
_36[_3a]&&_36[_3a].apply(_36,_39);
}while(_38&&_38.bubbles&&(_36=_36.parentNode));
return _38&&_38.cancelable&&_38;
};
var _27={};
if(_3("dom-addeventlistener")){
_27={focusin:"focus",focusout:"blur"};
if(_3("opera")){
_27.keydown="keypress";
}
on.emit=function(_3c,_3d,_3e){
if(_3c.dispatchEvent&&document.createEvent){
var _3f=document.createEvent("HTMLEvents");
_3f.initEvent(_3d,!!_3e.bubbles,!!_3e.cancelable);
for(var i in _3e){
var _40=_3e[i];
if(!(i in _3f)){
_3f[i]=_3e[i];
}
}
return _3c.dispatchEvent(_3f)&&_3f;
}
return _35.apply(on,arguments);
};
}else{
on._fixEvent=function(evt,_41){
if(!evt){
var w=_41&&(_41.ownerDocument||_41.document||_41).parentWindow||window;
evt=w.event;
}
if(!evt){
return (evt);
}
if(!evt.target){
evt.target=evt.srcElement;
evt.currentTarget=(_41||evt.srcElement);
if(evt.type=="mouseover"){
evt.relatedTarget=evt.fromElement;
}
if(evt.type=="mouseout"){
evt.relatedTarget=evt.toElement;
}
if(!evt.stopPropagation){
evt.stopPropagation=_42;
evt.preventDefault=_43;
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
_44(evt);
break;
}
}
return evt;
};
var _45=function(_46){
this.handle=_46;
};
_45.prototype.remove=function(){
delete _dojoIEListeners_[this.handle];
};
var _47=function(_48){
return function(evt){
evt=on._fixEvent(evt,this);
return _48.call(this,evt);
};
};
var _28=function(_49,_4a,_4b){
_4b=_47(_4b);
if(((_49.ownerDocument?_49.ownerDocument.parentWindow:_49.parentWindow||_49.window||window)!=top||_3("jscript")<5.8)&&!_3("config-_allow_leaks")){
if(typeof _dojoIEListeners_=="undefined"){
_dojoIEListeners_=[];
}
var _4c=_49[_4a];
if(!_4c||!_4c.listeners){
var _4d=_4c;
_49[_4a]=_4c=Function("event","var callee = arguments.callee; for(var i = 0; i<callee.listeners.length; i++){var listener = _dojoIEListeners_[callee.listeners[i]]; if(listener){listener.call(this,event);}}");
_4c.listeners=[];
_4c.global=this;
if(_4d){
_4c.listeners.push(_dojoIEListeners_.push(_4d)-1);
}
}
var _4e;
_4c.listeners.push(_4e=(_4c.global._dojoIEListeners_.push(_4b)-1));
return new _45(_4e);
}
return _1.after(_49,_4a,_4b,true);
};
var _44=function(evt){
evt.keyChar=evt.charCode?String.fromCharCode(evt.charCode):"";
evt.charOrCode=evt.keyChar||evt.keyCode;
};
var _42=function(){
this.cancelBubble=true;
};
var _43=on._preventDefault=function(){
this.bubbledKeyCode=this.keyCode;
if(this.ctrlKey){
try{
this.keyCode=0;
}
catch(e){
}
}
this.returnValue=false;
};
}
if(_3("touch")){
var _4f=function(){
};
var _50=window.orientation;
var _25=function(_51){
return function(_52){
var _53=_52.corrected;
if(!_53){
var _54=_52.type;
try{
delete _52.type;
}
catch(e){
}
if(_52.type){
_4f.prototype=_52;
var _53=new _4f;
_53.preventDefault=function(){
_52.preventDefault();
};
_53.stopPropagation=function(){
_52.stopPropagation();
};
}else{
_53=_52;
_53.type=_54;
}
_52.corrected=_53;
if(_54=="resize"){
if(_50==window.orientation){
return null;
}
_50=window.orientation;
_53.type="orientationchange";
return _51.call(this,_53);
}
if(!("rotation" in _53)){
_53.rotation=0;
_53.scale=1;
}
var _55=_53.changedTouches[0];
for(var i in _55){
delete _53[i];
_53[i]=_55[i];
}
}
return _51.call(this,_53);
};
};
}
return on;
});

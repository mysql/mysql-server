/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/on",["./has!dom-addeventlistener?:./aspect","./_base/kernel","./sniff"],function(_1,_2,_3){
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
var _20;
if(_1b.call){
return _1b.call(_1f,_1a,_1c);
}
if(_1b instanceof Array){
_20=_1b;
}else{
if(_1b.indexOf(",")>-1){
_20=_1b.split(/\s*,\s*/);
}
}
if(_20){
var _21=[];
var i=0;
var _22;
while(_22=_20[i++]){
_21.push(on.parse(_1a,_22,_1c,_1d,_1e,_1f));
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
on.matches=function(_30,_31,_32,_33,_34){
_34=_34&&(typeof _34.matches=="function")?_34:_2.query;
_33=_33!==false;
if(_30.nodeType!=1){
_30=_30.parentNode;
}
while(!_34.matches(_30,_31,_32)){
if(_30==_32||_33===false||!(_30=_30.parentNode)||_30.nodeType!=1){
return false;
}
}
return _30;
};
on.selector=function(_35,_36,_37){
return function(_38,_39){
var _3a=typeof _35=="function"?{matches:_35}:this,_3b=_36.bubble;
function _3c(_3d){
return on.matches(_3d,_35,_38,_37,_3a);
};
if(_3b){
return on(_38,_3b(_3c),_39);
}
return on(_38,_36,function(_3e){
var _3f=_3c(_3e.target);
if(_3f){
_3e.selectorTarget=_3f;
return _39.call(_3f,_3e);
}
});
};
};
function _40(){
this.cancelable=false;
this.defaultPrevented=true;
};
function _41(){
this.bubbles=false;
};
var _42=[].slice,_43=on.emit=function(_44,_45,_46){
var _47=_42.call(arguments,2);
var _48="on"+_45;
if("parentNode" in _44){
var _49=_47[0]={};
for(var i in _46){
_49[i]=_46[i];
}
_49.preventDefault=_40;
_49.stopPropagation=_41;
_49.target=_44;
_49.type=_45;
_46=_49;
}
do{
_44[_48]&&_44[_48].apply(_44,_47);
}while(_46&&_46.bubbles&&(_44=_44.parentNode));
return _46&&_46.cancelable&&_46;
};
var _2d=_3("event-focusin")?{}:{focusin:"focus",focusout:"blur"};
if(!_3("event-stopimmediatepropagation")){
var _4a=function(){
this.immediatelyStopped=true;
this.modified=true;
};
var _2b=function(_4b){
return function(_4c){
if(!_4c.immediatelyStopped){
_4c.stopImmediatePropagation=_4a;
return _4b.apply(this,arguments);
}
};
};
}
if(_3("dom-addeventlistener")){
on.emit=function(_4d,_4e,_4f){
if(_4d.dispatchEvent&&document.createEvent){
var _50=_4d.ownerDocument||document;
var _51=_50.createEvent("HTMLEvents");
_51.initEvent(_4e,!!_4f.bubbles,!!_4f.cancelable);
for(var i in _4f){
if(!(i in _51)){
_51[i]=_4f[i];
}
}
return _4d.dispatchEvent(_51)&&_51;
}
return _43.apply(on,arguments);
};
}else{
on._fixEvent=function(evt,_52){
if(!evt){
var w=_52&&(_52.ownerDocument||_52.document||_52).parentWindow||window;
evt=w.event;
}
if(!evt){
return evt;
}
try{
if(_53&&evt.type==_53.type&&evt.srcElement==_53.target){
evt=_53;
}
}
catch(e){
}
if(!evt.target){
evt.target=evt.srcElement;
evt.currentTarget=(_52||evt.srcElement);
if(evt.type=="mouseover"){
evt.relatedTarget=evt.fromElement;
}
if(evt.type=="mouseout"){
evt.relatedTarget=evt.toElement;
}
if(!evt.stopPropagation){
evt.stopPropagation=_54;
evt.preventDefault=_55;
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
_56(evt);
break;
}
}
return evt;
};
var _53,_57=function(_58){
this.handle=_58;
};
_57.prototype.remove=function(){
delete _dojoIEListeners_[this.handle];
};
var _59=function(_5a){
return function(evt){
evt=on._fixEvent(evt,this);
var _5b=_5a.call(this,evt);
if(evt.modified){
if(!_53){
setTimeout(function(){
_53=null;
});
}
_53=evt;
}
return _5b;
};
};
var _2f=function(_5c,_5d,_5e){
_5e=_59(_5e);
if(((_5c.ownerDocument?_5c.ownerDocument.parentWindow:_5c.parentWindow||_5c.window||window)!=top||_3("jscript")<5.8)&&!_3("config-_allow_leaks")){
if(typeof _dojoIEListeners_=="undefined"){
_dojoIEListeners_=[];
}
var _5f=_5c[_5d];
if(!_5f||!_5f.listeners){
var _60=_5f;
_5f=Function("event","var callee = arguments.callee; for(var i = 0; i<callee.listeners.length; i++){var listener = _dojoIEListeners_[callee.listeners[i]]; if(listener){listener.call(this,event);}}");
_5f.listeners=[];
_5c[_5d]=_5f;
_5f.global=this;
if(_60){
_5f.listeners.push(_dojoIEListeners_.push(_60)-1);
}
}
var _61;
_5f.listeners.push(_61=(_5f.global._dojoIEListeners_.push(_5e)-1));
return new _57(_61);
}
return _1.after(_5c,_5d,_5e,true);
};
var _56=function(evt){
evt.keyChar=evt.charCode?String.fromCharCode(evt.charCode):"";
evt.charOrCode=evt.keyChar||evt.keyCode;
};
var _54=function(){
this.cancelBubble=true;
};
var _55=on._preventDefault=function(){
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
this.modified=true;
};
}
if(_3("touch")){
var _62=function(){
};
var _63=window.orientation;
var _2a=function(_64){
return function(_65){
var _66=_65.corrected;
if(!_66){
var _67=_65.type;
try{
delete _65.type;
}
catch(e){
}
if(_65.type){
if(_3("touch-can-modify-event-delegate")){
_62.prototype=_65;
_66=new _62;
}else{
_66={};
for(var _68 in _65){
_66[_68]=_65[_68];
}
}
_66.preventDefault=function(){
_65.preventDefault();
};
_66.stopPropagation=function(){
_65.stopPropagation();
};
}else{
_66=_65;
_66.type=_67;
}
_65.corrected=_66;
if(_67=="resize"){
if(_63==window.orientation){
return null;
}
_63=window.orientation;
_66.type="orientationchange";
return _64.call(this,_66);
}
if(!("rotation" in _66)){
_66.rotation=0;
_66.scale=1;
}
if(window.TouchEvent&&_65 instanceof TouchEvent){
var _69=_66.changedTouches[0];
for(var i in _69){
delete _66[i];
_66[i]=_69[i];
}
}
}
return _64.call(this,_66);
};
};
}
return on;
});

/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/aspect",[],function(){
"use strict";
var _1;
function _2(_3,_4,_5,_6){
var _7=_3[_4];
var _8=_4=="around";
var _9;
if(_8){
var _a=_5(function(){
return _7.advice(this,arguments);
});
_9={remove:function(){
if(_a){
_a=_3=_5=null;
}
},advice:function(_b,_c){
return _a?_a.apply(_b,_c):_7.advice(_b,_c);
}};
}else{
_9={remove:function(){
if(_9.advice){
var _d=_9.previous;
var _e=_9.next;
if(!_e&&!_d){
delete _3[_4];
}else{
if(_d){
_d.next=_e;
}else{
_3[_4]=_e;
}
if(_e){
_e.previous=_d;
}
}
_3=_5=_9.advice=null;
}
},id:_3.nextId++,advice:_5,receiveArguments:_6};
}
if(_7&&!_8){
if(_4=="after"){
while(_7.next&&(_7=_7.next)){
}
_7.next=_9;
_9.previous=_7;
}else{
if(_4=="before"){
_3[_4]=_9;
_9.next=_7;
_7.previous=_9;
}
}
}else{
_3[_4]=_9;
}
return _9;
};
function _f(_10){
return function(_11,_12,_13,_14){
var _15=_11[_12],_16;
if(!_15||_15.target!=_11){
_11[_12]=_16=function(){
var _17=_16.nextId;
var _18=arguments;
var _19=_16.before;
while(_19){
if(_19.advice){
_18=_19.advice.apply(this,_18)||_18;
}
_19=_19.next;
}
if(_16.around){
var _1a=_16.around.advice(this,_18);
}
var _1b=_16.after;
while(_1b&&_1b.id<_17){
if(_1b.advice){
if(_1b.receiveArguments){
var _1c=_1b.advice.apply(this,_18);
_1a=_1c===_1?_1a:_1c;
}else{
_1a=_1b.advice.call(this,_1a,_18);
}
}
_1b=_1b.next;
}
return _1a;
};
if(_15){
_16.around={advice:function(_1d,_1e){
return _15.apply(_1d,_1e);
}};
}
_16.target=_11;
_16.nextId=_16.nextId||0;
}
var _1f=_2((_16||_15),_10,_13,_14);
_13=null;
return _1f;
};
};
var _20=_f("after");
var _21=_f("before");
var _22=_f("around");
return {before:_21,around:_22,after:_20};
});

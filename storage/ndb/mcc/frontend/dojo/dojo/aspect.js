/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/aspect",[],function(){
"use strict";
function _1(_2,_3,_4,_5){
var _6=_2[_3];
var _7=_3=="around";
var _8;
if(_7){
var _9=_4(function(){
return _6.advice(this,arguments);
});
_8={remove:function(){
_8.cancelled=true;
},advice:function(_a,_b){
return _8.cancelled?_6.advice(_a,_b):_9.apply(_a,_b);
}};
}else{
_8={remove:function(){
var _c=_8.previous;
var _d=_8.next;
if(!_d&&!_c){
delete _2[_3];
}else{
if(_c){
_c.next=_d;
}else{
_2[_3]=_d;
}
if(_d){
_d.previous=_c;
}
}
},advice:_4,receiveArguments:_5};
}
if(_6&&!_7){
if(_3=="after"){
var _e=_6;
while(_e){
_6=_e;
_e=_e.next;
}
_6.next=_8;
_8.previous=_6;
}else{
if(_3=="before"){
_2[_3]=_8;
_8.next=_6;
_6.previous=_8;
}
}
}else{
_2[_3]=_8;
}
return _8;
};
function _f(_10){
return function(_11,_12,_13,_14){
var _15=_11[_12],_16;
if(!_15||_15.target!=_11){
_16=_11[_12]=function(){
var _17=arguments;
var _18=_16.before;
while(_18){
_17=_18.advice.apply(this,_17)||_17;
_18=_18.next;
}
if(_16.around){
var _19=_16.around.advice(this,_17);
}
var _1a=_16.after;
while(_1a){
_19=_1a.receiveArguments?_1a.advice.apply(this,_17)||_19:_1a.advice.call(this,_19);
_1a=_1a.next;
}
return _19;
};
if(_15){
_16.around={advice:function(_1b,_1c){
return _15.apply(_1b,_1c);
}};
}
_16.target=_11;
}
var _1d=_1((_16||_15),_10,_13,_14);
_13=null;
return _1d;
};
};
return {before:_f("before"),around:_f("around"),after:_f("after")};
});

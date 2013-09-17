/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/Deferred",["./kernel","./lang"],function(_1,_2){
var _3=function(){
};
var _4=Object.freeze||function(){
};
_1.Deferred=function(_5){
var _6,_7,_8,_9,_a;
var _b=(this.promise={});
function _c(_d){
if(_7){
throw new Error("This deferred has already been resolved");
}
_6=_d;
_7=true;
_e();
};
function _e(){
var _f;
while(!_f&&_a){
var _10=_a;
_a=_a.next;
if((_f=(_10.progress==_3))){
_7=false;
}
var _11=(_8?_10.error:_10.resolved);
if(_11){
try{
var _12=_11(_6);
if(_12&&typeof _12.then==="function"){
_12.then(_2.hitch(_10.deferred,"resolve"),_2.hitch(_10.deferred,"reject"),_2.hitch(_10.deferred,"progress"));
continue;
}
var _13=_f&&_12===undefined;
if(_f&&!_13){
_8=_12 instanceof Error;
}
_10.deferred[_13&&_8?"reject":"resolve"](_13?_6:_12);
}
catch(e){
_10.deferred.reject(e);
}
}else{
if(_8){
_10.deferred.reject(_6);
}else{
_10.deferred.resolve(_6);
}
}
}
};
this.resolve=this.callback=function(_14){
this.fired=0;
this.results=[_14,null];
_c(_14);
};
this.reject=this.errback=function(_15){
_8=true;
this.fired=1;
_c(_15);
this.results=[null,_15];
if(!_15||_15.log!==false){
(_1.config.deferredOnError||function(x){
console.error(x);
})(_15);
}
};
this.progress=function(_16){
var _17=_a;
while(_17){
var _18=_17.progress;
_18&&_18(_16);
_17=_17.next;
}
};
this.addCallbacks=function(_19,_1a){
this.then(_19,_1a,_3);
return this;
};
_b.then=this.then=function(_1b,_1c,_1d){
var _1e=_1d==_3?this:new _1.Deferred(_b.cancel);
var _1f={resolved:_1b,error:_1c,progress:_1d,deferred:_1e};
if(_a){
_9=_9.next=_1f;
}else{
_a=_9=_1f;
}
if(_7){
_e();
}
return _1e.promise;
};
var _20=this;
_b.cancel=this.cancel=function(){
if(!_7){
var _21=_5&&_5(_20);
if(!_7){
if(!(_21 instanceof Error)){
_21=new Error(_21);
}
_21.log=false;
_20.reject(_21);
}
}
};
_4(_b);
};
_2.extend(_1.Deferred,{addCallback:function(_22){
return this.addCallbacks(_2.hitch.apply(_1,arguments));
},addErrback:function(_23){
return this.addCallbacks(null,_2.hitch.apply(_1,arguments));
},addBoth:function(_24){
var _25=_2.hitch.apply(_1,arguments);
return this.addCallbacks(_25,_25);
},fired:-1});
_1.Deferred.when=_1.when=function(_26,_27,_28,_29){
if(_26&&typeof _26.then==="function"){
return _26.then(_27,_28,_29);
}
return _27?_27(_26):_26;
};
return _1.Deferred;
});

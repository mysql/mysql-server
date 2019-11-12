/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/Deferred",["./kernel","../Deferred","../promise/Promise","../errors/CancelError","../has","./lang","../when"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=function(){
};
var _9=Object.freeze||function(){
};
var _a=_1.Deferred=function(_b){
var _c,_d,_e,_f,_10;
var _11=(this.promise=new _3());
function _12(_13){
if(_d){
throw new Error("This deferred has already been resolved");
}
_c=_13;
_d=true;
_14();
};
function _14(){
var _15;
while(!_15&&_10){
var _16=_10;
_10=_10.next;
if((_15=(_16.progress==_8))){
_d=false;
}
var _17=(_e?_16.error:_16.resolved);
if(_5("config-useDeferredInstrumentation")){
if(_e&&_2.instrumentRejected){
_2.instrumentRejected(_c,!!_17);
}
}
if(_17){
try{
var _18=_17(_c);
if(_18&&typeof _18.then==="function"){
_18.then(_6.hitch(_16.deferred,"resolve"),_6.hitch(_16.deferred,"reject"),_6.hitch(_16.deferred,"progress"));
continue;
}
var _19=_15&&_18===undefined;
if(_15&&!_19){
_e=_18 instanceof Error;
}
_16.deferred[_19&&_e?"reject":"resolve"](_19?_c:_18);
}
catch(e){
_16.deferred.reject(e);
}
}else{
if(_e){
_16.deferred.reject(_c);
}else{
_16.deferred.resolve(_c);
}
}
}
};
this.resolve=this.callback=function(_1a){
this.fired=0;
this.results=[_1a,null];
_12(_1a);
};
this.reject=this.errback=function(_1b){
_e=true;
this.fired=1;
if(_5("config-useDeferredInstrumentation")){
if(_2.instrumentRejected){
_2.instrumentRejected(_1b,!!_10);
}
}
_12(_1b);
this.results=[null,_1b];
};
this.progress=function(_1c){
var _1d=_10;
while(_1d){
var _1e=_1d.progress;
_1e&&_1e(_1c);
_1d=_1d.next;
}
};
this.addCallbacks=function(_1f,_20){
this.then(_1f,_20,_8);
return this;
};
_11.then=this.then=function(_21,_22,_23){
var _24=_23==_8?this:new _a(_11.cancel);
var _25={resolved:_21,error:_22,progress:_23,deferred:_24};
if(_10){
_f=_f.next=_25;
}else{
_10=_f=_25;
}
if(_d){
_14();
}
return _24.promise;
};
var _26=this;
_11.cancel=this.cancel=function(){
if(!_d){
var _27=_b&&_b(_26);
if(!_d){
if(!(_27 instanceof Error)){
_27=new _4(_27);
}
_27.log=false;
_26.reject(_27);
}
}
};
_9(_11);
};
_6.extend(_a,{addCallback:function(_28){
return this.addCallbacks(_6.hitch.apply(_1,arguments));
},addErrback:function(_29){
return this.addCallbacks(null,_6.hitch.apply(_1,arguments));
},addBoth:function(_2a){
var _2b=_6.hitch.apply(_1,arguments);
return this.addCallbacks(_2b,_2b);
},fired:-1});
_a.when=_1.when=_7;
return _a;
});

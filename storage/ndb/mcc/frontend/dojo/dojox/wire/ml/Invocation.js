//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/wire/ml/Action"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.Invocation");
_2.require("dojox.wire.ml.Action");
_2.declare("dojox.wire.ml.Invocation",_3.wire.ml.Action,{object:"",method:"",topic:"",parameters:"",result:"",error:"",_run:function(){
if(this.topic){
var _4=this._getParameters(arguments);
try{
_2.publish(this.topic,_4);
this.onComplete();
}
catch(e){
this.onError(e);
}
}else{
if(this.method){
var _5=(this.object?_3.wire.ml._getValue(this.object):_2.global);
if(!_5){
return;
}
var _4=this._getParameters(arguments);
var _6=_5[this.method];
if(!_6){
_6=_5.callMethod;
if(!_6){
return;
}
_4=[this.method,_4];
}
try{
var _7=false;
if(_5.getFeatures){
var _8=_5.getFeatures();
if((this.method=="fetch"&&_8["dojo.data.api.Read"])||(this.method=="save"&&_8["dojo.data.api.Write"])){
var _9=_4[0];
if(!_9.onComplete){
_9.onComplete=function(){
};
}
this.connect(_9,"onComplete","onComplete");
if(!_9.onError){
_9.onError=function(){
};
}
this.connect(_9,"onError","onError");
_7=true;
}
}
var r=_6.apply(_5,_4);
if(!_7){
if(r&&(r instanceof _2.Deferred)){
var _a=this;
r.addCallbacks(function(_b){
_a.onComplete(_b);
},function(_c){
_a.onError(_c);
});
}else{
this.onComplete(r);
}
}
}
catch(e){
this.onError(e);
}
}
}
},onComplete:function(_d){
if(this.result){
_3.wire.ml._setValue(this.result,_d);
}
if(this.error){
_3.wire.ml._setValue(this.error,"");
}
},onError:function(_e){
if(this.error){
if(_e&&_e.message){
_e=_e.message;
}
_3.wire.ml._setValue(this.error,_e);
}
},_getParameters:function(_f){
if(!this.parameters){
return _f;
}
var _10=[];
var _11=this.parameters.split(",");
if(_11.length==1){
var _12=_3.wire.ml._getValue(_2.trim(_11[0]),_f);
if(_2.isArray(_12)){
_10=_12;
}else{
_10.push(_12);
}
}else{
for(var i in _11){
_10.push(_3.wire.ml._getValue(_2.trim(_11[i]),_f));
}
}
return _10;
}});
});

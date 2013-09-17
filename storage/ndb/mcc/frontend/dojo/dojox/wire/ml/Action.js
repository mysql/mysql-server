//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_Widget,dijit/_Container,dojox/wire/Wire,dojox/wire/ml/util"],function(_1,_2,_3){
_2.provide("dojox.wire.ml.Action");
_2.require("dijit._Widget");
_2.require("dijit._Container");
_2.require("dojox.wire.Wire");
_2.require("dojox.wire.ml.util");
_2.declare("dojox.wire.ml.Action",[_1._Widget,_1._Container],{trigger:"",triggerEvent:"",triggerTopic:"",postCreate:function(){
this._connect();
},_connect:function(){
if(this.triggerEvent){
if(this.trigger){
var _4=_3.wire.ml._getValue(this.trigger);
if(_4){
if(!_4[this.triggerEvent]){
_4[this.triggerEvent]=function(){
};
}
this._triggerHandle=_2.connect(_4,this.triggerEvent,this,"run");
}
}else{
var _5=this.triggerEvent.toLowerCase();
if(_5=="onload"){
var _6=this;
_2.addOnLoad(function(){
_6._run.apply(_6,arguments);
});
}
}
}else{
if(this.triggerTopic){
this._triggerHandle=_2.subscribe(this.triggerTopic,this,"run");
}
}
},_disconnect:function(){
if(this._triggerHandle){
if(this.triggerTopic){
_2.unsubscribe(this.triggerTopic,this._triggerHandle);
}else{
_2.disconnect(this._triggerHandle);
}
}
},run:function(){
var _7=this.getChildren();
for(var i in _7){
var _8=_7[i];
if(_8 instanceof _3.wire.ml.ActionFilter){
if(!_8.filter.apply(_8,arguments)){
return;
}
}
}
this._run.apply(this,arguments);
},_run:function(){
var _9=this.getChildren();
for(var i in _9){
var _a=_9[i];
if(_a instanceof _3.wire.ml.Action){
_a.run.apply(_a,arguments);
}
}
},uninitialize:function(){
this._disconnect();
return true;
}});
_2.declare("dojox.wire.ml.ActionFilter",_1._Widget,{required:"",requiredValue:"",type:"",message:"",error:"",filter:function(){
if(this.required===""){
return true;
}else{
var _b=_3.wire.ml._getValue(this.required,arguments);
if(this.requiredValue===""){
if(_b){
return true;
}
}else{
var _c=this.requiredValue;
if(this.type!==""){
var _d=this.type.toLowerCase();
if(_d==="boolean"){
if(_c.toLowerCase()==="false"){
_c=false;
}else{
_c=true;
}
}else{
if(_d==="number"){
_c=parseInt(_c,10);
}
}
}
if(_b===_c){
return true;
}
}
}
if(this.message){
if(this.error){
_3.wire.ml._setValue(this.error,this.message);
}else{
alert(this.message);
}
}
return false;
}});
});

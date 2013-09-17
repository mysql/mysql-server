//>>built
define("dojox/app/main",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/Deferred","dojo/_base/connect","dojo/ready","dojo/_base/window","dojo/dom-construct","./scene"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_1.experimental("dojox.app");
var _a=_3([_9],{constructor:function(_b){
this.scenes={};
if(_b.stores){
for(var _c in _b.stores){
if(_c.charAt(0)!=="_"){
var _d=_b.stores[_c].type?_b.stores[_c].type:"dojo.store.Memory";
var _e={};
if(_b.stores[_c].params){
_1.mixin(_e,_b.stores[_c].params);
}
var _f=_1.getObject(_d);
if(_e.data&&_2.isString(_e.data)){
_e.data=_1.getObject(_e.data);
}
_b.stores[_c].store=new _f(_e);
}
}
}
},start:function(_10){
var _11=this.loadChild();
_4.when(_11,_1.hitch(this,function(){
this.startup();
this.setStatus(this.lifecycle.STARTED);
}));
},templateString:"<div></div>",selectedChild:null,baseClass:"application mblView",defaultViewType:_9,buildRendering:function(){
if(this.srcNodeRef===_7.body()){
this.srcNodeRef=_8.create("DIV",{},_7.body());
}
this.inherited(arguments);
}});
function _12(_13,_14,_15,_16){
var _17=_13.modules.concat(_13.dependencies);
if(_13.template){
_17.push("dojo/text!"+"app/"+_13.template);
}
require(_17,function(){
var _18=[_a];
for(var i=0;i<_13.modules.length;i++){
_18.push(arguments[i]);
}
if(_13.template){
var ext={templateString:arguments[arguments.length-1]};
}
App=_3(_18,ext);
_6(function(){
app=App(_13,_14||_7.body());
app.setStatus(app.lifecycle.STARTING);
app.start();
});
});
};
return function(_19,_1a){
if(!_19){
throw Error("App Config Missing");
}
if(_19.validate){
require(["dojox/json/schema","dojox/json/ref","dojo/text!dojox/application/schema/application.json"],function(_1b,_1c){
_1b=dojox.json.ref.resolveJson(_1b);
if(_1b.validate(_19,_1c)){
_12(_19,_1a);
}
});
}else{
_12(_19,_1a);
}
};
});

//>>built
define("dojox/app/main",["require","dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/config","dojo/_base/window","dojo/Evented","dojo/Deferred","dojo/when","dojo/has","dojo/on","dojo/ready","dojo/dom-construct","dojo/dom-attr","./utils/model","./utils/nls","./module/lifecycle","./utils/hash","./utils/constraints","./utils/config"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f,_10,_11,_12,_13){
_a.add("app-log-api",(_5["app"]||{}).debugApp);
var _14=_4(_7,{constructor:function(_15,_16){
_4.safeMixin(this,_15);
this.params=_15;
this.id=_15.id;
this.defaultView=_15.defaultView;
this.controllers=[];
this.children={};
this.loadedModels={};
this.loadedStores={};
this.setDomNode(_c.create("div",{id:this.id+"_Root",style:"width:100%; height:100%; overflow-y:hidden; overflow-x:hidden;"}));
_16.appendChild(this.domNode);
},createDataStore:function(_17){
if(_17.stores){
for(var _18 in _17.stores){
if(_18.charAt(0)!=="_"){
var _19=_17.stores[_18].type?_17.stores[_18].type:"dojo/store/Memory";
var _1a={};
if(_17.stores[_18].params){
_3.mixin(_1a,_17.stores[_18].params);
}
try{
var _1b=_1(_19);
}
catch(e){
throw new Error(_19+" must be listed in the dependencies");
}
if(_1a.data&&_3.isString(_1a.data)){
_1a.data=_3.getObject(_1a.data);
}
if(_17.stores[_18].observable){
try{
var _1c=_1("dojo/store/Observable");
}
catch(e){
throw new Error("dojo/store/Observable must be listed in the dependencies");
}
_17.stores[_18].store=_1c(new _1b(_1a));
}else{
_17.stores[_18].store=new _1b(_1a);
}
this.loadedStores[_18]=_17.stores[_18].store;
}
}
}
},createControllers:function(_1d){
if(_1d){
var _1e=[];
for(var i=0;i<_1d.length;i++){
_1e.push(_1d[i]);
}
var def=new _8();
var _1f;
try{
_1f=_1.on?_1.on("error",function(_20){
if(def.isResolved()||def.isRejected()){
return;
}
def.reject("load controllers error.");
if(_1f){
_1f.remove();
}
}):null;
_1(_1e,function(){
def.resolve.call(def,arguments);
if(_1f){
_1f.remove();
}
});
}
catch(e){
def.reject(e);
if(_1f){
_1f.remove();
}
}
var _21=new _8();
_9(def,_3.hitch(this,function(){
for(var i=0;i<arguments[0].length;i++){
this.controllers.push((new arguments[0][i](this)).bind());
}
_21.resolve(this);
}),function(){
_21.reject("load controllers error.");
});
return _21;
}
},trigger:function(_22,_23){
_2.deprecated("dojox.app.Application.trigger","Use dojox.app.Application.emit instead","2.0");
this.emit(_22,_23);
},start:function(){
this.createDataStore(this.params);
var _24=new _8();
var _25;
try{
_25=_e(this.params.models,this,this);
}
catch(e){
_24.reject(e);
return _24.promise;
}
_9(_25,_3.hitch(this,function(_26){
this.loadedModels=_3.isArray(_26)?_26[0]:_26;
this.setupControllers();
_9(_f(this.params),_3.hitch(this,function(nls){
if(nls){
_3.mixin(this.nls={},nls);
}
this.startup();
}));
}),function(){
_24.reject("load model error.");
});
},setDomNode:function(_27){
var _28=this.domNode;
this.domNode=_27;
this.emit("app-domNode",{oldNode:_28,newNode:_27});
},setupControllers:function(){
var _29=window.location.hash;
this._startView=_11.getTarget(_29,this.defaultView);
this._startParams=_11.getParams(_29);
},startup:function(){
this.selectedChildren={};
var _2a=this.createControllers(this.params.controllers);
if(this.hasOwnProperty("constraint")){
_12.register(this.params.constraints);
}else{
this.constraint="center";
}
var _2b=function(){
this.emit("app-load",{viewId:this.defaultView,initLoad:true,params:this._startParams,callback:_3.hitch(this,function(){
this.emit("app-transition",{viewId:this.defaultView,forceTransitionNone:true,opts:{params:this._startParams}});
if(this.defaultView!==this._startView){
this.emit("app-transition",{viewId:this._startView,opts:{params:this._startParams}});
}
this.setStatus(this.lifecycle.STARTED);
})});
};
_9(_2a,_3.hitch(this,function(){
if(this.template){
this.emit("app-init",{app:this,name:this.name,type:this.type,parent:this,templateString:this.templateString,controller:this.controller,callback:_3.hitch(this,function(_2c){
this.setDomNode(_2c.domNode);
_2b.call(this);
})});
}else{
_2b.call(this);
}
}));
}});
function _2d(_2e,_2f){
var _30;
_2e=_13.configProcessHas(_2e);
if(!_2e.loaderConfig){
_2e.loaderConfig={};
}
if(!_2e.loaderConfig.paths){
_2e.loaderConfig.paths={};
}
if(!_2e.loaderConfig.paths["app"]){
_30=window.location.pathname;
if(_30.charAt(_30.length)!="/"){
_30=_30.split("/");
_30.pop();
_30=_30.join("/");
}
_2e.loaderConfig.paths["app"]=_30;
}
_1(_2e.loaderConfig);
if(!_2e.modules){
_2e.modules=[];
}
_2e.modules.push("./module/lifecycle");
var _31=_2e.modules.concat(_2e.dependencies?_2e.dependencies:[]);
if(_2e.template){
_30=_2e.template;
if(_30.indexOf("./")==0){
_30="app/"+_30;
}
_31.push("dojo/text!"+_30);
}
_1(_31,function(){
var _32=[_14];
for(var i=0;i<_2e.modules.length;i++){
_32.push(arguments[i]);
}
if(_2e.template){
var ext={templateString:arguments[arguments.length-1]};
}
App=_4(_32,ext);
_b(function(){
var app=new App(_2e,_2f||_6.body());
if(_a("app-log-api")){
app.log=function(){
var msg="";
try{
for(var i=0;i<arguments.length-1;i++){
msg=msg+arguments[i];
}
}
catch(e){
}
};
}else{
app.log=function(){
};
}
app.transitionToView=function(_33,_34,_35){
var _36={bubbles:true,cancelable:true,detail:_34,triggerEvent:_35||null};
on.emit(_33,"startTransition",_36);
};
app.setStatus(app.lifecycle.STARTING);
var _37=app.id;
if(window[_37]){
_4.safeMixin(app,window[_37]);
}
window[_37]=app;
app.start();
});
});
};
return function(_38,_39){
if(!_38){
throw new Error("App Config Missing");
}
if(_38.validate){
_1(["dojox/json/schema","dojox/json/ref","dojo/text!dojox/application/schema/application.json"],function(_3a,_3b){
_3a=dojox.json.ref.resolveJson(_3a);
if(_3a.validate(_38,_3b)){
_2d(_38,_39);
}
});
}else{
_2d(_38,_39);
}
};
});

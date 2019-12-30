//>>built
define("dojox/app/main",["dojo/_base/kernel","require","dojo/_base/lang","dojo/_base/declare","dojo/Deferred","dojo/when","dojo/has","dojo/_base/config","dojo/on","dojo/ready","dojo/_base/window","dojo/dom-construct","./model","./View","./controllers/Load","./controllers/Transition","./controllers/Layout"],function(_1,_2,_3,_4,_5,_6,_7,_8,on,_9,_a,_b,_c,_d,_e,_f,_10){
_1.experimental("dojox.app");
_7.add("app-log-api",(_8["app"]||{}).debugApp);
var _11=_4(null,{constructor:function(_12,_13){
_3.mixin(this,_12);
this.params=_12;
this.id=_12.id;
this.defaultView=_12.defaultView;
this.widgetId=_12.id;
this.controllers=[];
this.children={};
this.loadedModels={};
this.domNode=_b.create("div",{id:this.id+"_Root",style:"width:100%; height:100%; overflow-y:hidden; overflow-x:hidden;"});
_13.appendChild(this.domNode);
},createDataStore:function(_14){
if(_14.stores){
for(var _15 in _14.stores){
if(_15.charAt(0)!=="_"){
var _16=_14.stores[_15].type?_14.stores[_15].type:"dojo/store/Memory";
var _17={};
if(_14.stores[_15].params){
_3.mixin(_17,_14.stores[_15].params);
}
var _18=_2(_16);
if(_17.data&&_3.isString(_17.data)){
_17.data=_3.getObject(_17.data);
}
_14.stores[_15].store=new _18(_17);
}
}
}
},createControllers:function(_19){
if(_19){
var _1a=[];
for(var i=0;i<_19.length;i++){
_1a.push(_19[i]);
}
var def=new _5();
var _1b;
try{
_1b=_2.on("error",function(_1c){
if(def.isResolved()||def.isRejected()){
return;
}
def.reject("load controllers error.");
_1b.remove();
});
_2(_1a,function(){
def.resolve.call(def,arguments);
_1b.remove();
});
}
catch(ex){
def.reject("load controllers error.");
_1b.remove();
}
var _1d=new _5();
_6(def,_3.hitch(this,function(){
for(var i=0;i<arguments[0].length;i++){
this.controllers.push(new arguments[0][i](this));
}
_1d.resolve(this);
}),function(){
_1d.reject("load controllers error.");
});
return _1d;
}
},trigger:function(_1e,_1f){
on.emit(this.domNode,_1e,_1f);
},start:function(){
this.createDataStore(this.params);
var _20=new _5();
var _21;
try{
_21=_c(this.params.models,this);
}
catch(ex){
_20.reject("load model error.");
return _20.promise;
}
if(_21.then){
_6(_21,_3.hitch(this,function(_22){
this.loadedModels=_22;
this.setupAppView();
}),function(){
_20.reject("load model error.");
});
}else{
this.loadedModels=_21;
this.setupAppView();
}
},setupAppView:function(){
if(this.template){
this.view=new _d({app:this,name:this.name,parent:this,templateString:this.templateString,definition:this.definition});
_6(this.view.start(),_3.hitch(this,function(){
this.domNode=this.view.domNode;
this.setupControllers();
this.startup();
}));
}else{
this.setupControllers();
this.startup();
}
},getParamsFromHash:function(_23){
var _24={};
if(_23&&_23.length){
for(var _25=_23.split("&"),x=0;x<_25.length;x++){
var tp=_25[x].split("="),_26=tp[0],_27=encodeURIComponent(tp[1]||"");
if(_26&&_27){
_24[_26]=_27;
}
}
}
return _24;
},setupControllers:function(){
if(!this.noAutoLoadControllers){
this.controllers.push(new _e(this));
this.controllers.push(new _f(this));
this.controllers.push(new _10(this));
}
var _28=window.location.hash;
this._startView=(((_28&&_28.charAt(0)=="#")?_28.substr(1):_28)||this.defaultView).split("&")[0];
this._startParams=this.getParamsFromHash(_28)||this.defaultParams||{};
},startup:function(){
var _29=this.createControllers(this.params.controllers);
_6(_29,_3.hitch(this,function(_2a){
this.trigger("load",{"viewId":this.defaultView,"params":this._startParams,"callback":_3.hitch(this,function(){
var _2b=this.defaultView.split(",");
_2b=_2b.shift();
this.selectedChild=this.children[this.id+"_"+_2b];
this.trigger("transition",{"viewId":this._startView,"params":this._startParams});
this.setStatus(this.lifecycle.STARTED);
})});
}));
}});
function _2c(_2d,_2e,_2f,_30){
var _31;
if(!_2d.loaderConfig){
_2d.loaderConfig={};
}
if(!_2d.loaderConfig.paths){
_2d.loaderConfig.paths={};
}
if(!_2d.loaderConfig.paths["app"]){
_31=window.location.pathname;
if(_31.charAt(_31.length)!="/"){
_31=_31.split("/");
_31.pop();
_31=_31.join("/");
}
_2d.loaderConfig.paths["app"]=_31;
}
_2(_2d.loaderConfig);
if(!_2d.modules){
_2d.modules=[];
}
_2d.modules.push("dojox/app/module/lifecycle");
var _32=_2d.modules.concat(_2d.dependencies);
if(_2d.template){
_31=_2d.template;
if(_31.indexOf("./")==0){
_31="app/"+_31;
}
_32.push("dojo/text!"+_31);
}
_2(_32,function(){
var _33=[_11];
for(var i=0;i<_2d.modules.length;i++){
_33.push(arguments[i]);
}
if(_2d.template){
var ext={templateString:arguments[arguments.length-1]};
}
App=_4(_33,ext);
_9(function(){
var app=new App(_2d,_2e||_a.body());
if(_7("app-log-api")){
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
app.setStatus(app.lifecycle.STARTING);
var _34=app.id;
if(window[_34]){
_3.mixin(app,window[_34]);
}
window[_34]=app;
app.start();
});
});
};
return function(_35,_36){
if(!_35){
throw Error("App Config Missing");
}
if(_35.validate){
_2(["dojox/json/schema","dojox/json/ref","dojo/text!dojox/application/schema/application.json"],function(_37,_38){
_37=dojox.json.ref.resolveJson(_37);
if(_37.validate(_35,_38)){
_2c(_35,_36);
}
});
}else{
_2c(_35,_36);
}
};
});

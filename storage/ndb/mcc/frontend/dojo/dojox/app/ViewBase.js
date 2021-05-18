//>>built
define("dojox/app/ViewBase",["require","dojo/when","dojo/on","dojo/dom-attr","dojo/dom-style","dojo/_base/declare","dojo/_base/lang","dojo/Deferred","./utils/model","./utils/constraints"],function(_1,_2,on,_3,_4,_5,_6,_7,_8,_9){
return _5("dojox.app.ViewBase",null,{constructor:function(_a){
this.id="";
this.name="";
this.children={};
this.selectedChildren={};
this.loadedStores={};
this._started=false;
_6.mixin(this,_a);
if(this.parent.views){
_6.mixin(this,this.parent.views[this.name]);
}
},start:function(){
if(this._started){
return this;
}
this._startDef=new _7();
_2(this.load(),_6.hitch(this,function(){
this._createDataStore(this);
this._setupModel();
}));
return this._startDef;
},load:function(){
var _b=this._loadViewController();
_2(_b,_6.hitch(this,function(_c){
if(_c){
_5.safeMixin(this,_c);
}
}));
return _b;
},_createDataStore:function(){
if(this.parent.loadedStores){
_6.mixin(this.loadedStores,this.parent.loadedStores);
}
if(this.stores){
for(var _d in this.stores){
if(_d.charAt(0)!=="_"){
var _e=this.stores[_d].type?this.stores[_d].type:"dojo/store/Memory";
var _f={};
if(this.stores[_d].params){
_6.mixin(_f,this.stores[_d].params);
}
try{
var _10=_1(_e);
}
catch(e){
throw new Error(_e+" must be listed in the dependencies");
}
if(_f.data&&_6.isString(_f.data)){
_f.data=_6.getObject(_f.data);
}
if(this.stores[_d].observable){
try{
var _11=_1("dojo/store/Observable");
}
catch(e){
throw new Error("dojo/store/Observable must be listed in the dependencies");
}
this.stores[_d].store=_11(new _10(_f));
}else{
this.stores[_d].store=new _10(_f);
}
this.loadedStores[_d]=this.stores[_d].store;
}
}
}
},_setupModel:function(){
if(!this.loadedModels){
var _12;
try{
_12=_8(this.models,this.parent,this.app);
}
catch(e){
throw new Error("Error creating models: "+e.message);
}
_2(_12,_6.hitch(this,function(_13){
if(_13){
this.loadedModels=_6.isArray(_13)?_13[0]:_13;
}
this._startup();
}),function(err){
throw new Error("Error creating models: "+err.message);
});
}else{
this._startup();
}
},_startup:function(){
this._initViewHidden();
this._needsResize=true;
this._startLayout();
},_initViewHidden:function(){
_4.set(this.domNode,"visibility","hidden");
},_startLayout:function(){
this.app.log("  > in app/ViewBase _startLayout firing layout for name=[",this.name,"], parent.name=[",this.parent.name,"]");
if(!this.hasOwnProperty("constraint")){
this.constraint=_3.get(this.domNode,"data-app-constraint")||"center";
}
_9.register(this.constraint);
this.app.emit("app-initLayout",{"view":this,"callback":_6.hitch(this,function(){
this.startup();
this.app.log("  > in app/ViewBase calling init() name=[",this.name,"], parent.name=[",this.parent.name,"]");
this.init();
this._started=true;
if(this._startDef){
this._startDef.resolve(this);
}
})});
},_loadViewController:function(){
var _14=new _7();
var _15;
if(!this.controller){
this.app.log("  > in app/ViewBase _loadViewController no controller set for view name=[",this.name,"], parent.name=[",this.parent.name,"]");
_14.resolve(true);
return _14;
}else{
_15=this.controller.replace(/(\.js)$/,"");
}
var _16;
try{
var _17=_15;
var _18=_17.indexOf("./");
if(_18>=0){
_17=_15.substring(_18+2);
}
_16=_1.on?_1.on("error",function(_19){
if(_14.isResolved()||_14.isRejected()){
return;
}
if(_19.info[0]&&(_19.info[0].indexOf(_17)>=0)){
_14.resolve(false);
if(_16){
_16.remove();
}
}
}):null;
if(_15.indexOf("./")==0){
_15="app/"+_15;
}
_1([_15],function(_1a){
_14.resolve(_1a);
if(_16){
_16.remove();
}
});
}
catch(e){
_14.reject(e);
if(_16){
_16.remove();
}
}
return _14;
},init:function(){
},beforeActivate:function(){
},afterActivate:function(){
},beforeDeactivate:function(){
},afterDeactivate:function(){
},destroy:function(){
}});
});

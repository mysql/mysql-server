//>>built
define("dojox/app/View",["dojo/_base/declare","dojo/_base/lang","dojo/Deferred","dojo/when","require","dojo/dom-attr","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","./model"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _1("dojox.app.View",null,{constructor:function(_a){
this.id="";
this.name="";
this.templateString="";
this.template="";
this.definition="";
this.parent=null;
this.children={};
this.selectedChild=null;
this._started=false;
this._definition=null;
_2.mixin(this,_a);
if(this.parent.views){
_2.mixin(this,this.parent.views[this.name]);
}
},_loadViewDefinition:function(){
var _b=new _3();
var _c;
if(this.definition&&(this.definition==="none")){
_b.resolve(true);
return _b;
}else{
if(this.definition){
_c=this.definition.replace(/(\.js)$/,"");
}else{
_c=this.id.split("_");
_c.shift();
_c=_c.join("/");
_c="./views/"+_c;
}
}
var _d;
try{
var _e=_c;
var _f=_e.indexOf("./");
if(_f>=0){
_e=_c.substring(_f+2);
}
_d=_5.on("error",function(_10){
if(_b.isResolved()||_b.isRejected()){
return;
}
if(_10.info[0]&&(_10.info[0].indexOf(_e)>=0)){
_b.resolve(false);
_d.remove();
}
});
if(_c.indexOf("./")==0){
_c="app/"+_c;
}
_5([_c],function(_11){
_b.resolve(_11);
_d.remove();
});
}
catch(ex){
_b.resolve(false);
_d.remove();
}
return _b;
},_loadViewTemplate:function(){
if(this.templateString){
return true;
}else{
if(!this.dependencies){
this.dependencies=[];
}
var tpl=this.template;
if(tpl.indexOf("./")==0){
tpl="app/"+tpl;
}
var _12=this.template?this.dependencies.concat(["dojo/text!"+tpl]):this.dependencies.concat([]);
var def=new _3();
if(_12.length>0){
var _13;
try{
_13=_5.on("error",_2.hitch(this,function(_14){
if(def.isResolved()||def.isRejected()){
return;
}
if(_14.info[0]&&_14.info[0].indexOf(this.template)>=0){
def.resolve(false);
_13.remove();
}
}));
_5(_12,function(){
def.resolve.call(def,arguments);
_13.remove();
});
}
catch(ex){
def.resolve(false);
_13.remove();
}
}else{
def.resolve(true);
}
var _15=new _3();
_4(def,_2.hitch(this,function(){
this.templateString=this.template?arguments[0][arguments[0].length-1]:"<div></div>";
_15.resolve(this);
}));
return _15;
}
},start:function(){
if(this._started){
return this;
}
var _16=this._loadViewDefinition();
var _17=this._loadViewTemplate();
this._startDef=new _3();
_4(_16,_2.hitch(this,function(_18){
this._definition=_18;
_4(_17,_2.hitch(this,function(){
this._setupModel();
}));
}));
return this._startDef;
},_setupModel:function(){
if(!this.loadedModels){
var _19=new _3();
var _1a;
try{
_1a=_9(this.models,this.parent,this.app);
}
catch(ex){
_19.reject("load model error.");
return _19.promise;
}
if(_1a.then){
_4(_1a,_2.hitch(this,function(_1b){
if(_1b){
this.loadedModels=_1b;
}
this._startup();
}),function(){
_19.reject("load model error.");
});
}else{
this.loadedModels=_1a;
this._startup();
}
}else{
this._startup();
}
},_startup:function(){
this._widget=this.render(this.templateString);
this.domNode=this._widget.domNode;
this.parent.domNode.appendChild(this.domNode);
this._widget.startup();
_6.set(this.domNode,"id",this.id);
_6.set(this.domNode,"data-app-region","center");
_6.set(this.domNode,"style","width:100%; height:100%");
this._widget.region="center";
if(this._definition){
_2.mixin(this,this._definition);
}
this.app.log("  > in app/View calling init() name=[",this.name,"], parent.name=[",this.parent.name,"]");
this.init();
this._started=true;
if(this._startDef){
this._startDef.resolve(this);
}
},render:function(_1c){
var _1d=new _7();
var _1e=new _8();
if(this.loadedModels){
_1e.loadedModels=this.loadedModels;
}
_2.mixin(_1d,_1e);
_1d.templateString=_1c;
_1d.buildRendering();
return _1d;
},init:function(){
},beforeActivate:function(){
},afterActivate:function(){
},beforeDeactivate:function(){
},afterDeactivate:function(){
},destroy:function(){
}});
});

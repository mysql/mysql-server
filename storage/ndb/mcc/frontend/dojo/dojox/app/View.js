//>>built
define("dojox/app/View",["require","dojo/when","dojo/on","dojo/_base/declare","dojo/_base/lang","dojo/Deferred","dijit/Destroyable","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","./ViewBase","./utils/nls"],function(_1,_2,on,_3,_4,_5,_6,_7,_8,_9,_a){
return _3("dojox.app.View",[_7,_8,_6,_9],{constructor:function(_b){
},connect:function(_c,_d,_e){
return this.own(on(_c,_d,_4.hitch(this,_e)))[0];
},_loadTemplate:function(){
if(this.templateString){
return true;
}else{
var _f=this.template;
var _10=this.dependencies?this.dependencies:[];
if(_f){
if(_f.indexOf("./")==0){
_f="app/"+_f;
}
_10=_10.concat(["dojo/text!"+_f]);
}
var def=new _5();
if(_10.length>0){
var _11;
try{
_11=_1.on?_1.on("error",_4.hitch(this,function(_12){
if(def.isResolved()||def.isRejected()){
return;
}
if(_12.info[0]&&_12.info[0].indexOf(this.template)>=0){
def.resolve(false);
if(_11){
_11.remove();
}
}
})):null;
_1(_10,function(){
def.resolve.call(def,arguments);
if(_11){
_11.remove();
}
});
}
catch(e){
def.resolve(false);
if(_11){
_11.remove();
}
}
}else{
def.resolve(true);
}
var _13=new _5();
_2(def,_4.hitch(this,function(){
this.templateString=this.template?arguments[0][arguments[0].length-1]:"<div></div>";
_13.resolve(this);
}));
return _13;
}
},load:function(){
var _14=new _5();
var _15=this.inherited(arguments);
var _16=_a(this);
_2(_15,_4.hitch(this,function(){
_2(_16,_4.hitch(this,function(nls){
this.nls=_4.mixin({},this.parent.nls);
if(nls){
_4.mixin(this.nls,nls);
}
_2(this._loadTemplate(),function(_17){
_14.resolve(_17);
});
}));
}));
return _14;
},_startup:function(){
this.buildRendering();
this.inherited(arguments);
}});
});

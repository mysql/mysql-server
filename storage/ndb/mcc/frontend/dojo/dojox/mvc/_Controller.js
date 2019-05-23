//>>built
define("dojox/mvc/_Controller",["dojo/_base/declare","dojo/_base/lang","dojo/Stateful","./_atBindingMixin"],function(_1,_2,_3,_4){
return _1("dojox.mvc._Controller",[_3,_4],{postscript:function(_5,_6){
if(this._applyAttributes){
this.inherited(arguments);
}
this._dbpostscript(_5,_6);
if(_5){
this.params=_5;
for(var s in _5){
this.set(s,_5[s]);
}
}
var _7;
try{
_7=require("dijit/registry");
this.id=this.id||(_6||{}).id||_7.getUniqueId(this.declaredClass.replace(/\./g,"_"));
_7.add(this);
}
catch(e){
}
if(!_6){
this.startup();
}else{
_6.setAttribute("widgetId",this.id);
}
},startup:function(){
if(!this._applyAttributes){
this._startAtWatchHandles();
}
this.inherited(arguments);
},destroy:function(){
this._beingDestroyed=true;
if(!this._applyAttributes){
this._stopAtWatchHandles();
}
this.inherited(arguments);
if(!this._applyAttributes){
try{
require("dijit/registry").remove(this.id);
}
catch(e){
}
}
this._destroyed=true;
},set:function(_8,_9){
if(typeof _8==="object"){
for(var x in _8){
if(_8.hasOwnProperty(x)){
this.set(x,_8[x]);
}
}
return this;
}
if(!this._applyAttributes){
if((_9||{}).atsignature=="dojox.mvc.at"){
return this._setAtWatchHandle(_8,_9);
}else{
var _a="_set"+_8.replace(/^[a-z]/,function(c){
return c.toUpperCase();
})+"Attr";
if(this[_a]){
this[_a](_9);
}else{
this._set(_8,_9);
}
return this;
}
}
return this.inherited(arguments);
},_set:function(_b,_c){
if(!this._applyAttributes){
return this._changeAttrValue(_b,_c);
}
return this.inherited(arguments);
}});
});

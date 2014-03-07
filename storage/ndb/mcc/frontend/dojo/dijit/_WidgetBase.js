//>>built
define("dijit/_WidgetBase",["require","dojo/_base/array","dojo/aspect","dojo/_base/config","dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/kernel","dojo/_base/lang","dojo/on","dojo/ready","dojo/Stateful","dojo/topic","dojo/_base/window","./registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,on,_f,_10,_11,win,_12){
if(!_d.isAsync){
_f(0,function(){
var _13=["dijit/_base/manager"];
_1(_13);
});
}
var _14={};
function _15(obj){
var ret={};
for(var _16 in obj){
ret[_16.toLowerCase()]=true;
}
return ret;
};
function _17(_18){
return function(val){
_8[val?"set":"remove"](this.domNode,_18,val);
this._set(_18,val);
};
};
return _6("dijit._WidgetBase",_10,{id:"",_setIdAttr:"domNode",lang:"",_setLangAttr:_17("lang"),dir:"",_setDirAttr:_17("dir"),textDir:"","class":"",_setClassAttr:{node:"domNode",type:"class"},style:"",title:"",tooltip:"",baseClass:"",srcNodeRef:null,domNode:null,containerNode:null,attributeMap:{},_blankGif:_4.blankGif||_1.toUrl("dojo/resources/blank.gif"),postscript:function(_19,_1a){
this.create(_19,_1a);
},create:function(_1b,_1c){
this.srcNodeRef=_7.byId(_1c);
this._connects=[];
this._supportingWidgets=[];
if(this.srcNodeRef&&(typeof this.srcNodeRef.id=="string")){
this.id=this.srcNodeRef.id;
}
if(_1b){
this.params=_1b;
_e.mixin(this,_1b);
}
this.postMixInProperties();
if(!this.id){
this.id=_12.getUniqueId(this.declaredClass.replace(/\./g,"_"));
}
_12.add(this);
this.buildRendering();
if(this.domNode){
this._applyAttributes();
var _1d=this.srcNodeRef;
if(_1d&&_1d.parentNode&&this.domNode!==_1d){
_1d.parentNode.replaceChild(this.domNode,_1d);
}
}
if(this.domNode){
this.domNode.setAttribute("widgetId",this.id);
}
this.postCreate();
if(this.srcNodeRef&&!this.srcNodeRef.parentNode){
delete this.srcNodeRef;
}
this._created=true;
},_applyAttributes:function(){
var _1e=this.constructor,_1f=_1e._setterAttrs;
if(!_1f){
_1f=(_1e._setterAttrs=[]);
for(var _20 in this.attributeMap){
_1f.push(_20);
}
var _21=_1e.prototype;
for(var _22 in _21){
if(_22 in this.attributeMap){
continue;
}
var _23="_set"+_22.replace(/^[a-z]|-[a-zA-Z]/g,function(c){
return c.charAt(c.length-1).toUpperCase();
})+"Attr";
if(_23 in _21){
_1f.push(_22);
}
}
}
_2.forEach(_1f,function(_24){
if(this.params&&_24 in this.params){
}else{
if(this[_24]){
this.set(_24,this[_24]);
}
}
},this);
for(var _25 in this.params){
this.set(_25,this[_25]);
}
},postMixInProperties:function(){
},buildRendering:function(){
if(!this.domNode){
this.domNode=this.srcNodeRef||_a.create("div");
}
if(this.baseClass){
var _26=this.baseClass.split(" ");
if(!this.isLeftToRight()){
_26=_26.concat(_2.map(_26,function(_27){
return _27+"Rtl";
}));
}
_9.add(this.domNode,_26);
}
},postCreate:function(){
},startup:function(){
if(this._started){
return;
}
this._started=true;
_2.forEach(this.getChildren(),function(obj){
if(!obj._started&&!obj._destroyed&&_e.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
});
},destroyRecursive:function(_28){
this._beingDestroyed=true;
this.destroyDescendants(_28);
this.destroy(_28);
},destroy:function(_29){
this._beingDestroyed=true;
this.uninitialize();
var c;
while(c=this._connects.pop()){
c.remove();
}
var w;
while(w=this._supportingWidgets.pop()){
if(w.destroyRecursive){
w.destroyRecursive();
}else{
if(w.destroy){
w.destroy();
}
}
}
this.destroyRendering(_29);
_12.remove(this.id);
this._destroyed=true;
},destroyRendering:function(_2a){
if(this.bgIframe){
this.bgIframe.destroy(_2a);
delete this.bgIframe;
}
if(this.domNode){
if(_2a){
_8.remove(this.domNode,"widgetId");
}else{
_a.destroy(this.domNode);
}
delete this.domNode;
}
if(this.srcNodeRef){
if(!_2a){
_a.destroy(this.srcNodeRef);
}
delete this.srcNodeRef;
}
},destroyDescendants:function(_2b){
_2.forEach(this.getChildren(),function(_2c){
if(_2c.destroyRecursive){
_2c.destroyRecursive(_2b);
}
});
},uninitialize:function(){
return false;
},_setStyleAttr:function(_2d){
var _2e=this.domNode;
if(_e.isObject(_2d)){
_c.set(_2e,_2d);
}else{
if(_2e.style.cssText){
_2e.style.cssText+="; "+_2d;
}else{
_2e.style.cssText=_2d;
}
}
this._set("style",_2d);
},_attrToDom:function(_2f,_30,_31){
_31=arguments.length>=3?_31:this.attributeMap[_2f];
_2.forEach(_e.isArray(_31)?_31:[_31],function(_32){
var _33=this[_32.node||_32||"domNode"];
var _34=_32.type||"attribute";
switch(_34){
case "attribute":
if(_e.isFunction(_30)){
_30=_e.hitch(this,_30);
}
var _35=_32.attribute?_32.attribute:(/^on[A-Z][a-zA-Z]*$/.test(_2f)?_2f.toLowerCase():_2f);
_8.set(_33,_35,_30);
break;
case "innerText":
_33.innerHTML="";
_33.appendChild(win.doc.createTextNode(_30));
break;
case "innerHTML":
_33.innerHTML=_30;
break;
case "class":
_9.replace(_33,_30,this[_2f]);
break;
}
},this);
},get:function(_36){
var _37=this._getAttrNames(_36);
return this[_37.g]?this[_37.g]():this[_36];
},set:function(_38,_39){
if(typeof _38==="object"){
for(var x in _38){
this.set(x,_38[x]);
}
return this;
}
var _3a=this._getAttrNames(_38),_3b=this[_3a.s];
if(_e.isFunction(_3b)){
var _3c=_3b.apply(this,Array.prototype.slice.call(arguments,1));
}else{
var _3d=this.focusNode&&!_e.isFunction(this.focusNode)?"focusNode":"domNode",tag=this[_3d].tagName,_3e=_14[tag]||(_14[tag]=_15(this[_3d])),map=_38 in this.attributeMap?this.attributeMap[_38]:_3a.s in this?this[_3a.s]:((_3a.l in _3e&&typeof _39!="function")||/^aria-|^data-|^role$/.test(_38))?_3d:null;
if(map!=null){
this._attrToDom(_38,_39,map);
}
this._set(_38,_39);
}
return _3c||this;
},_attrPairNames:{},_getAttrNames:function(_3f){
var apn=this._attrPairNames;
if(apn[_3f]){
return apn[_3f];
}
var uc=_3f.replace(/^[a-z]|-[a-zA-Z]/g,function(c){
return c.charAt(c.length-1).toUpperCase();
});
return (apn[_3f]={n:_3f+"Node",s:"_set"+uc+"Attr",g:"_get"+uc+"Attr",l:uc.toLowerCase()});
},_set:function(_40,_41){
var _42=this[_40];
this[_40]=_41;
if(this._watchCallbacks&&this._created&&_41!==_42){
this._watchCallbacks(_40,_42,_41);
}
},on:function(_43,_44){
return _3.after(this,this._onMap(_43),_44,true);
},_onMap:function(_45){
var _46=this.constructor,map=_46._onMap;
if(!map){
map=(_46._onMap={});
for(var _47 in _46.prototype){
if(/^on/.test(_47)){
map[_47.replace(/^on/,"").toLowerCase()]=_47;
}
}
}
return map[_45.toLowerCase()];
},toString:function(){
return "[Widget "+this.declaredClass+", "+(this.id||"NO ID")+"]";
},getChildren:function(){
return this.containerNode?_12.findWidgets(this.containerNode):[];
},getParent:function(){
return _12.getEnclosingWidget(this.domNode.parentNode);
},connect:function(obj,_48,_49){
var _4a=_5.connect(obj,_48,this,_49);
this._connects.push(_4a);
return _4a;
},disconnect:function(_4b){
var i=_2.indexOf(this._connects,_4b);
if(i!=-1){
_4b.remove();
this._connects.splice(i,1);
}
},subscribe:function(t,_4c){
var _4d=_11.subscribe(t,_e.hitch(this,_4c));
this._connects.push(_4d);
return _4d;
},unsubscribe:function(_4e){
this.disconnect(_4e);
},isLeftToRight:function(){
return this.dir?(this.dir=="ltr"):_b.isBodyLtr();
},isFocusable:function(){
return this.focus&&(_c.get(this.domNode,"display")!="none");
},placeAt:function(_4f,_50){
if(_4f.declaredClass&&_4f.addChild){
_4f.addChild(this,_50);
}else{
_a.place(this.domNode,_4f,_50);
}
return this;
},getTextDir:function(_51,_52){
return _52;
},applyTextDir:function(){
}});
});

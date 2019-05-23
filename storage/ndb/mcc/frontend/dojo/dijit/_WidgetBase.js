//>>built
define("dijit/_WidgetBase",["require","dojo/_base/array","dojo/aspect","dojo/_base/config","dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/has","dojo/_base/kernel","dojo/_base/lang","dojo/on","dojo/ready","dojo/Stateful","dojo/topic","dojo/_base/window","./Destroyable","./registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,on,_10,_11,_12,win,_13,_14){
_d.add("dijit-legacy-requires",!_e.isAsync);
if(_d("dijit-legacy-requires")){
_10(0,function(){
var _15=["dijit/_base/manager"];
_1(_15);
});
}
var _16={};
function _17(obj){
var ret={};
for(var _18 in obj){
ret[_18.toLowerCase()]=true;
}
return ret;
};
function _19(_1a){
return function(val){
_8[val?"set":"remove"](this.domNode,_1a,val);
this._set(_1a,val);
};
};
function _1b(a,b){
return a===b||(a!==a&&b!==b);
};
return _6("dijit._WidgetBase",[_11,_13],{id:"",_setIdAttr:"domNode",lang:"",_setLangAttr:_19("lang"),dir:"",_setDirAttr:_19("dir"),textDir:"","class":"",_setClassAttr:{node:"domNode",type:"class"},style:"",title:"",tooltip:"",baseClass:"",srcNodeRef:null,domNode:null,containerNode:null,ownerDocument:null,_setOwnerDocumentAttr:function(val){
this._set("ownerDocument",val);
},attributeMap:{},_blankGif:_4.blankGif||_1.toUrl("dojo/resources/blank.gif"),postscript:function(_1c,_1d){
this.create(_1c,_1d);
},create:function(_1e,_1f){
this.srcNodeRef=_7.byId(_1f);
this._connects=[];
this._supportingWidgets=[];
if(this.srcNodeRef&&(typeof this.srcNodeRef.id=="string")){
this.id=this.srcNodeRef.id;
}
if(_1e){
this.params=_1e;
_f.mixin(this,_1e);
}
this.postMixInProperties();
if(!this.id){
this.id=_14.getUniqueId(this.declaredClass.replace(/\./g,"_"));
if(this.params){
delete this.params.id;
}
}
this.ownerDocument=this.ownerDocument||(this.srcNodeRef?this.srcNodeRef.ownerDocument:win.doc);
this.ownerDocumentBody=win.body(this.ownerDocument);
_14.add(this);
this.buildRendering();
var _20;
if(this.domNode){
this._applyAttributes();
var _21=this.srcNodeRef;
if(_21&&_21.parentNode&&this.domNode!==_21){
_21.parentNode.replaceChild(this.domNode,_21);
_20=true;
}
this.domNode.setAttribute("widgetId",this.id);
}
this.postCreate();
if(_20){
delete this.srcNodeRef;
}
this._created=true;
},_applyAttributes:function(){
var _22=this.constructor,_23=_22._setterAttrs;
if(!_23){
_23=(_22._setterAttrs=[]);
for(var _24 in this.attributeMap){
_23.push(_24);
}
var _25=_22.prototype;
for(var _26 in _25){
if(_26 in this.attributeMap){
continue;
}
var _27="_set"+_26.replace(/^[a-z]|-[a-zA-Z]/g,function(c){
return c.charAt(c.length-1).toUpperCase();
})+"Attr";
if(_27 in _25){
_23.push(_26);
}
}
}
var _28={};
for(var key in this.params||{}){
_28[key]=this[key];
}
_2.forEach(_23,function(_29){
if(_29 in _28){
}else{
if(this[_29]){
this.set(_29,this[_29]);
}
}
},this);
for(key in _28){
this.set(key,_28[key]);
}
},postMixInProperties:function(){
},buildRendering:function(){
if(!this.domNode){
this.domNode=this.srcNodeRef||this.ownerDocument.createElement("div");
}
if(this.baseClass){
var _2a=this.baseClass.split(" ");
if(!this.isLeftToRight()){
_2a=_2a.concat(_2.map(_2a,function(_2b){
return _2b+"Rtl";
}));
}
_9.add(this.domNode,_2a);
}
},postCreate:function(){
},startup:function(){
if(this._started){
return;
}
this._started=true;
_2.forEach(this.getChildren(),function(obj){
if(!obj._started&&!obj._destroyed&&_f.isFunction(obj.startup)){
obj.startup();
obj._started=true;
}
});
},destroyRecursive:function(_2c){
this._beingDestroyed=true;
this.destroyDescendants(_2c);
this.destroy(_2c);
},destroy:function(_2d){
this._beingDestroyed=true;
this.uninitialize();
function _2e(w){
if(w.destroyRecursive){
w.destroyRecursive(_2d);
}else{
if(w.destroy){
w.destroy(_2d);
}
}
};
_2.forEach(this._connects,_f.hitch(this,"disconnect"));
_2.forEach(this._supportingWidgets,_2e);
if(this.domNode){
_2.forEach(_14.findWidgets(this.domNode,this.containerNode),_2e);
}
this.destroyRendering(_2d);
_14.remove(this.id);
this._destroyed=true;
},destroyRendering:function(_2f){
if(this.bgIframe){
this.bgIframe.destroy(_2f);
delete this.bgIframe;
}
if(this.domNode){
if(_2f){
_8.remove(this.domNode,"widgetId");
}else{
_a.destroy(this.domNode);
}
delete this.domNode;
}
if(this.srcNodeRef){
if(!_2f){
_a.destroy(this.srcNodeRef);
}
delete this.srcNodeRef;
}
},destroyDescendants:function(_30){
_2.forEach(this.getChildren(),function(_31){
if(_31.destroyRecursive){
_31.destroyRecursive(_30);
}
});
},uninitialize:function(){
return false;
},_setStyleAttr:function(_32){
var _33=this.domNode;
if(_f.isObject(_32)){
_c.set(_33,_32);
}else{
if(_33.style.cssText){
_33.style.cssText+="; "+_32;
}else{
_33.style.cssText=_32;
}
}
this._set("style",_32);
},_attrToDom:function(_34,_35,_36){
_36=arguments.length>=3?_36:this.attributeMap[_34];
_2.forEach(_f.isArray(_36)?_36:[_36],function(_37){
var _38=this[_37.node||_37||"domNode"];
var _39=_37.type||"attribute";
switch(_39){
case "attribute":
if(_f.isFunction(_35)){
_35=_f.hitch(this,_35);
}
var _3a=_37.attribute?_37.attribute:(/^on[A-Z][a-zA-Z]*$/.test(_34)?_34.toLowerCase():_34);
if(_38.tagName){
_8.set(_38,_3a,_35);
}else{
_38.set(_3a,_35);
}
break;
case "innerText":
_38.innerHTML="";
_38.appendChild(this.ownerDocument.createTextNode(_35));
break;
case "innerHTML":
_38.innerHTML=_35;
break;
case "class":
_9.replace(_38,_35,this[_34]);
break;
}
},this);
},get:function(_3b){
var _3c=this._getAttrNames(_3b);
return this[_3c.g]?this[_3c.g]():this[_3b];
},set:function(_3d,_3e){
if(typeof _3d==="object"){
for(var x in _3d){
this.set(x,_3d[x]);
}
return this;
}
var _3f=this._getAttrNames(_3d),_40=this[_3f.s];
if(_f.isFunction(_40)){
var _41=_40.apply(this,Array.prototype.slice.call(arguments,1));
}else{
var _42=this.focusNode&&!_f.isFunction(this.focusNode)?"focusNode":"domNode",tag=this[_42].tagName,_43=_16[tag]||(_16[tag]=_17(this[_42])),map=_3d in this.attributeMap?this.attributeMap[_3d]:_3f.s in this?this[_3f.s]:((_3f.l in _43&&typeof _3e!="function")||/^aria-|^data-|^role$/.test(_3d))?_42:null;
if(map!=null){
this._attrToDom(_3d,_3e,map);
}
this._set(_3d,_3e);
}
return _41||this;
},_attrPairNames:{},_getAttrNames:function(_44){
var apn=this._attrPairNames;
if(apn[_44]){
return apn[_44];
}
var uc=_44.replace(/^[a-z]|-[a-zA-Z]/g,function(c){
return c.charAt(c.length-1).toUpperCase();
});
return (apn[_44]={n:_44+"Node",s:"_set"+uc+"Attr",g:"_get"+uc+"Attr",l:uc.toLowerCase()});
},_set:function(_45,_46){
var _47=this[_45];
this[_45]=_46;
if(this._created&&!_1b(_46,_47)){
if(this._watchCallbacks){
this._watchCallbacks(_45,_47,_46);
}
this.emit("attrmodified-"+_45,{detail:{prevValue:_47,newValue:_46}});
}
},emit:function(_48,_49,_4a){
_49=_49||{};
if(_49.bubbles===undefined){
_49.bubbles=true;
}
if(_49.cancelable===undefined){
_49.cancelable=true;
}
if(!_49.detail){
_49.detail={};
}
_49.detail.widget=this;
var ret,_4b=this["on"+_48];
if(_4b){
ret=_4b.apply(this,_4a?_4a:[_49]);
}
if(this._started&&!this._beingDestroyed){
on.emit(this.domNode,_48.toLowerCase(),_49);
}
return ret;
},on:function(_4c,_4d){
var _4e=this._onMap(_4c);
if(_4e){
return _3.after(this,_4e,_4d,true);
}
return this.own(on(this.domNode,_4c,_4d))[0];
},_onMap:function(_4f){
var _50=this.constructor,map=_50._onMap;
if(!map){
map=(_50._onMap={});
for(var _51 in _50.prototype){
if(/^on/.test(_51)){
map[_51.replace(/^on/,"").toLowerCase()]=_51;
}
}
}
return map[typeof _4f=="string"&&_4f.toLowerCase()];
},toString:function(){
return "[Widget "+this.declaredClass+", "+(this.id||"NO ID")+"]";
},getChildren:function(){
return this.containerNode?_14.findWidgets(this.containerNode):[];
},getParent:function(){
return _14.getEnclosingWidget(this.domNode.parentNode);
},connect:function(obj,_52,_53){
return this.own(_5.connect(obj,_52,this,_53))[0];
},disconnect:function(_54){
_54.remove();
},subscribe:function(t,_55){
return this.own(_12.subscribe(t,_f.hitch(this,_55)))[0];
},unsubscribe:function(_56){
_56.remove();
},isLeftToRight:function(){
return this.dir?(this.dir=="ltr"):_b.isBodyLtr(this.ownerDocument);
},isFocusable:function(){
return this.focus&&(_c.get(this.domNode,"display")!="none");
},placeAt:function(_57,_58){
var _59=!_57.tagName&&_14.byId(_57);
if(_59&&_59.addChild&&(!_58||typeof _58==="number")){
_59.addChild(this,_58);
}else{
var ref=_59?(_59.containerNode&&!/after|before|replace/.test(_58||"")?_59.containerNode:_59.domNode):_7.byId(_57,this.ownerDocument);
_a.place(this.domNode,ref,_58);
if(!this._started&&(this.getParent()||{})._started){
this.startup();
}
}
return this;
},getTextDir:function(_5a,_5b){
return _5b;
},applyTextDir:function(){
},defer:function(fcn,_5c){
var _5d=setTimeout(_f.hitch(this,function(){
if(!_5d){
return;
}
_5d=null;
if(!this._destroyed){
_f.hitch(this,fcn)();
}
}),_5c||0);
return {remove:function(){
if(_5d){
clearTimeout(_5d);
_5d=null;
}
return null;
}};
}});
});

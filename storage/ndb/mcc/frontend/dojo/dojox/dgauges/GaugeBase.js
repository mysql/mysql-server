//>>built
define("dojox/dgauges/GaugeBase",["dojo/_base/lang","dojo/_base/declare","dojo/dom-geometry","dijit/registry","dijit/_WidgetBase","dojo/_base/html","dojo/_base/event","dojox/gfx","dojox/widget/_Invalidating","./ScaleBase","dojox/gfx/matrix"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _2("dojox.dgauges.GaugeBase",[_5,_9],{_elements:null,_scales:null,_elementsIndex:null,_elementsRenderers:null,_gfxGroup:null,_mouseShield:null,_widgetBox:null,_node:null,value:0,_mainIndicator:null,_getValueAttr:function(){
if(this._mainIndicator){
return this._mainIndicator.get("value");
}else{
this._setMainIndicator();
if(this._mainIndicator){
return this._mainIndicator.get("value");
}
}
return this.value;
},_setValueAttr:function(_c){
this._set("value",_c);
if(this._mainIndicator){
this._mainIndicator.set("value",_c);
}else{
this._setMainIndicator();
if(this._mainIndicator){
this._mainIndicator.set("value",_c);
}
}
},_setMainIndicator:function(){
var _d;
for(var i=0;i<this._scales.length;i++){
_d=this._scales[i].getIndicator("indicator");
if(_d){
this._mainIndicator=_d;
}
}
},_resetMainIndicator:function(){
this._mainIndicator=null;
},font:null,constructor:function(_e,_f){
this.font={family:"Helvetica",style:"normal",variant:"small-caps",weight:"bold",size:"10pt",color:"black"};
this._elements=[];
this._scales=[];
this._elementsIndex={};
this._elementsRenderers={};
this._node=_4.byId(_f);
var box=_6.getMarginBox(_f);
this.surface=_8.createSurface(this._node,box.w||1,box.h||1);
this._widgetBox=box;
this._baseGroup=this.surface.createGroup();
this._mouseShield=this._baseGroup.createGroup();
this._gfxGroup=this._baseGroup.createGroup();
},_setCursor:function(_10){
if(this._node){
this._node.style.cursor=_10;
}
},_computeBoundingBox:function(_11){
return _11?_11.getBoundingBox():{x:0,y:0,width:0,height:0};
},destroy:function(){
this.surface.destroy();
this.inherited(arguments);
},resize:function(_12,_13){
switch(arguments.length){
case 1:
_3.setMarginBox(this._node,_12);
break;
case 2:
_3.setMarginBox(this._node,{w:_12,h:_13});
break;
}
var box=_3.getMarginBox(this._node);
this._widgetBox=box;
var d=this.surface.getDimensions();
if(d.width!=box.w||d.height!=box.h){
this.surface.setDimensions(box.w,box.h);
this._mouseShield.clear();
this._mouseShield.createRect({x:0,y:0,width:box.w,height:box.h}).setFill([0,0,0,0]);
return this.invalidateRendering();
}else{
return this;
}
},addElement:function(_14,_15){
if(this._elementsIndex[_14]&&this._elementsIndex[_14]!=_15){
this.removeElement(_14);
}
if(_1.isFunction(_15)){
var _16={};
_1.mixin(_16,new _9());
_16._name=_14;
_16._gfxGroup=this._gfxGroup.createGroup();
_16.width=0;
_16.height=0;
_16._isGFX=true;
_16.refreshRendering=function(){
_16._gfxGroup.clear();
return _15(_16._gfxGroup,_16.width,_16.height);
};
this._elements.push(_16);
this._elementsIndex[_14]=_16;
}else{
_15._name=_14;
_15._gfxGroup=this._gfxGroup.createGroup();
_15._gauge=this;
this._elements.push(_15);
this._elementsIndex[_14]=_15;
if(_15 instanceof _a){
this._scales.push(_15);
}
}
return this.invalidateRendering();
},removeElement:function(_17){
var _18=this._elementsIndex[_17];
if(_18){
_18._gfxGroup.removeShape();
var idx=this._elements.indexOf(_18);
this._elements.splice(idx,1);
if(_18 instanceof _a){
var _19=this._scales.indexOf(_18);
this._scales.splice(_19,1);
this._resetMainIndicator();
}
delete this._elementsIndex[_17];
delete this._elementsRenderers[_17];
}
this.invalidateRendering();
return _18;
},getElement:function(_1a){
return this._elementsIndex[_1a];
},getElementRenderer:function(_1b){
return this._elementsRenderers[_1b];
},onStartEditing:function(_1c){
},onEndEditing:function(_1d){
}});
});

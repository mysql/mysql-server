//>>built
define("dojox/gfx/svg_mask",["dojo/_base/declare","dojo/_base/lang","./_base","./shape","./svg"],function(_1,_2,_3,_4,_5){
_2.extend(_5.Shape,{mask:null,setMask:function(_6){
var _7=this.rawNode;
if(_6){
_7.setAttribute("mask","url(#"+_6.shape.id+")");
this.mask=_6;
}else{
_7.removeAttribute("mask");
this.mask=null;
}
return this;
},getMask:function(){
return this.mask;
}});
var _8=_5.Mask=_1("dojox.gfx.svg.Mask",_5.Shape,{constructor:function(){
_4.Container._init.call(this);
this.shape=_8.defaultMask;
},setRawNode:function(_9){
this.rawNode=_9;
},setShape:function(_a){
if(!_a.id){
_a=_2.mixin({id:_3._base._getUniqueId()},_a);
}
this.inherited(arguments,[_a]);
}});
_8.nodeType="mask";
_8.defaultMask={id:null,x:0,y:0,width:1,height:1,maskUnits:"objectBoundingBox",maskContentUnits:"userSpaceOnUse"};
_2.extend(_8,_5.Container);
_2.extend(_8,_4.Creator);
_2.extend(_8,_5.Creator);
var _b=_5.Surface,_c=_b.prototype.add,_d=_b.prototype.remove;
_2.extend(_b,{createMask:function(_e){
return this.createObject(_8,_e);
},add:function(_f){
if(_f instanceof _8){
this.defNode.appendChild(_f.rawNode);
_f.parent=this;
}else{
_c.apply(this,arguments);
}
return this;
},remove:function(_10,_11){
if(_10 instanceof _8&&this.defNode==_10.rawNode.parentNode){
this.defNode.removeChild(_10.rawNode);
_10.parent=null;
}else{
_d.apply(this,arguments);
}
return this;
}});
});

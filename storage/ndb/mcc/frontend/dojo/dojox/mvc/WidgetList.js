//>>built
define("dojox/mvc/WidgetList",["require","dojo/_base/array","dojo/_base/lang","dojo/_base/declare","dijit/_Container","dijit/_WidgetBase","dojox/mvc/Templated"],function(_1,_2,_3,_4,_5,_6,_7){
var _8="data-mvc-child-type",_9="data-mvc-child-mixins",_a="data-mvc-child-props",_b="data-mvc-child-bindings",_c;
function _d(_e){
return eval("({"+_e+"})");
};
function _f(w){
for(var h=null;h=(w._handles||[]).pop();){
h.unwatch();
}
};
var _10=_4("dojox.mvc.WidgetList",[_6,_5],{childClz:null,childType:"",childMixins:"",childParams:null,childBindings:null,children:null,templateString:"",partialRebuild:false,_relTargetProp:"children",postMixInProperties:function(){
this.inherited(arguments);
if(this[_8]){
this.childType=this[_8];
}
if(this[_9]){
this.childMixins=this[_9];
}
},startup:function(){
this.inherited(arguments);
this._setChildrenAttr(this.children);
},_setChildrenAttr:function(_11){
var _12=this.children;
this._set("children",_11);
if(this._started&&(!this._builtOnce||_12!=_11)){
_f(this);
this._builtOnce=true;
this._buildChildren(_11);
if(_3.isArray(_11)){
var _13=this;
!this.partialRebuild&&_3.isFunction(_11.watchElements)&&(this._handles=this._handles||[]).push(_11.watchElements(function(idx,_14,_15){
_13._buildChildren(_11);
}));
_11.watch!=={}.watch&&(this._handles=this._handles||[]).push(_11.watch(function(_16,old,_17){
if(!isNaN(_16)){
var w=_13.getChildren()[_16-0];
w&&w.set(w._relTargetProp||"target",_17);
}
}));
}
}
},_buildChildren:function(_18){
for(var cw=this.getChildren(),w=null;w=cw.pop();){
this.removeChild(w);
w.destroy();
}
if(!_3.isArray(_18)){
return;
}
var _19=_3.hitch(this,function(seq){
if(this._buildChildrenSeq>seq){
return;
}
var clz=_4([].slice.call(arguments,1),{}),_1a=this;
function _1b(_1c,_1d){
_2.forEach(_2.map(_1c,function(_1e,idx){
var _1f={ownerDocument:_1a.ownerDocument,parent:_1a,indexAtStartup:_1d+idx};
_1f[(_1a.childParams||_1a[_a]&&_d.call(_1f,_1a[_a])||{})._relTargetProp||clz.prototype._relTargetProp||"target"]=_1e;
var _20=_1a.childParams||_1a[_a]&&_d.call(_1f,_1a[_a]),_21=_1a.childBindings||_1a[_b]&&_d.call(_1f,_1a[_b]);
if(_1a.templateString&&!_1f.templateString&&!clz.prototype.templateString){
_1f.templateString=_1a.templateString;
}
if(_21&&!_1f.bindings&&!clz.prototype.bindings){
_1f.bindings=_21;
}
return new clz(_3.mixin(_1f,_20));
}),function(_22,idx){
_1a.addChild(_22,_1d+idx);
});
};
_1b(_18,0);
if(this.partialRebuild){
_3.isFunction(_18.watchElements)&&(this._handles=this._handles||[]).push(_18.watchElements(function(idx,_23,_24){
for(var i=0,l=(_23||[]).length;i<l;++i){
_1a.removeChild(idx);
}
_1b(_24,idx);
}));
}
},this._buildChildrenSeq=(this._buildChildrenSeq||0)+1);
if(this.childClz){
_19(this.childClz);
}else{
if(this.childType){
var _25=[this.childType].concat(this.childMixins&&this.childMixins.split(",")||[]),_26=_2.filter(_2.map(_25,function(_27){
return _3.getObject(_27)?_c:_27;
}),function(mid){
return mid!==_c;
}),_28=this;
_1(_26,function(){
if(!_28._beingDestroyed){
_19.apply(this,_2.map(_25,function(_29){
return _3.getObject(_29)||_1(_29);
}));
}
});
}else{
_19(_7);
}
}
},destroy:function(){
_f(this);
this.inherited(arguments);
}});
_10.prototype[_8]=_10.prototype[_9]=_10.prototype[_a]=_10.prototype[_b]="";
return _10;
});

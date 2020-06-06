//>>built
define("dojox/app/controllers/Layout",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/window","dojo/query","dojo/dom-geometry","dojo/dom-attr","dojo/dom-style","dijit/registry","./LayoutBase","../utils/layout","../utils/constraints","dojo/sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
return _1("dojox.app.controllers.Layout",_a,{constructor:function(_e,_f){
},onResize:function(){
this._doResize(this.app);
this.resizeSelectedChildren(this.app);
},resizeSelectedChildren:function(w){
for(var _10 in w.selectedChildren){
if(w.selectedChildren[_10]&&w.selectedChildren[_10].domNode){
this.app.log("in Layout resizeSelectedChildren calling resizeSelectedChildren calling _doResize for w.selectedChildren[hash].id="+w.selectedChildren[_10].id);
this._doResize(w.selectedChildren[_10]);
_3.forEach(w.selectedChildren[_10].domNode.children,function(_11){
if(_9.byId(_11.id)&&_9.byId(_11.id).resize){
_9.byId(_11.id).resize();
}
});
this.resizeSelectedChildren(w.selectedChildren[_10]);
}
}
},initLayout:function(_12){
this.app.log("in app/controllers/Layout.initLayout event=",_12);
this.app.log("in app/controllers/Layout.initLayout event.view.parent.name=[",_12.view.parent.name,"]");
if(!_12.view.domNode.parentNode||(_d("ie")==8&&!_12.view.domNode.parentElement)){
if(this.app.useConfigOrder){
_12.view.parent.domNode.appendChild(_12.view.domNode);
}else{
this.addViewToParentDomByConstraint(_12);
}
}
_7.set(_12.view.domNode,"data-app-constraint",_12.view.constraint);
this.inherited(arguments);
},addViewToParentDomByConstraint:function(_13){
var _14=_13.view.constraint;
if(_14==="bottom"){
_13.view.parent.domNode.appendChild(_13.view.domNode);
}else{
if(_14==="top"){
_13.view.parent.domNode.insertBefore(_13.view.domNode,_13.view.parent.domNode.firstChild);
}else{
if(_13.view.parent.domNode.children.length>0){
for(var _15 in _13.view.parent.domNode.children){
var _16=_13.view.parent.domNode.children[_15];
var dir=_8.get(_13.view.parent.domNode,"direction");
var _17=(dir==="ltr");
var _18=_17?"left":"right";
var _19=_17?"right":"left";
if(_16.getAttribute&&_16.getAttribute("data-app-constraint")){
var _1a=_16.getAttribute("data-app-constraint");
if(_1a==="bottom"||(_1a===_19)||(_1a!=="top"&&(_14===_18))){
_13.view.parent.domNode.insertBefore(_13.view.domNode,_16);
break;
}
}
}
}
}
}
if(!_13.view.domNode.parentNode||(_d("ie")==8&&!_13.view.domNode.parentElement)){
_13.view.parent.domNode.appendChild(_13.view.domNode);
}
},_doResize:function(_1b){
var _1c=_1b.domNode;
if(!_1c){
this.app.log("Warning - View has not been loaded, in Layout _doResize view.domNode is not set for view.id="+_1b.id+" view=",_1b);
return;
}
var mb={};
if(!("h" in mb)||!("w" in mb)){
mb=_2.mixin(_6.getMarginBox(_1c),mb);
}
if(_1b!==this.app){
var cs=_8.getComputedStyle(_1c);
var me=_6.getMarginExtents(_1c,cs);
var be=_6.getBorderExtents(_1c,cs);
var bb=(_1b._borderBox={w:mb.w-(me.w+be.w),h:mb.h-(me.h+be.h)});
var pe=_6.getPadExtents(_1c,cs);
_1b._contentBox={l:_8.toPixelValue(_1c,cs.paddingLeft),t:_8.toPixelValue(_1c,cs.paddingTop),w:bb.w-pe.w,h:bb.h-pe.h};
}else{
_1b._contentBox={l:0,t:0,h:_4.global.innerHeight||_4.doc.documentElement.clientHeight,w:_4.global.innerWidth||_4.doc.documentElement.clientWidth};
}
this.inherited(arguments);
},layoutView:function(_1d){
if(_1d.view){
this.inherited(arguments);
if(_1d.doResize){
this._doResize(_1d.parent||this.app);
this._doResize(_1d.view);
}
}
},_doLayout:function(_1e){
if(!_1e){
console.warn("layout empty view.");
return;
}
this.app.log("in Layout _doLayout called for view.id="+_1e.id+" view=",_1e);
var _1f;
var _20=_c.getSelectedChild(_1e,_1e.constraint);
if(_20&&_20.isFullScreen){
console.warn("fullscreen sceen layout");
}else{
_1f=_5("> [data-app-constraint]",_1e.domNode).map(function(_21){
var w=_9.getEnclosingWidget(_21);
if(w){
w._constraint=_7.get(_21,"data-app-constraint");
return w;
}
return {domNode:_21,_constraint:_7.get(_21,"data-app-constraint")};
});
if(_20){
_1f=_3.filter(_1f,function(c){
return c.domNode&&c._constraint;
},_1e);
}
}
if(_1e._contentBox){
_b.layoutChildren(_1e.domNode,_1e._contentBox,_1f);
}
}});
});

//>>built
define("dijit/layout/_ContentPaneResizeMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/dom-geometry","dojo/_base/lang","dojo/query","dojo/_base/sniff","dojo/_base/window","../registry","./utils","../_Contained"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _2("dijit.layout._ContentPaneResizeMixin",null,{doLayout:true,isLayoutContainer:true,startup:function(){
if(this._started){
return;
}
var _d=this.getParent();
this._childOfLayoutWidget=_d&&_d.isLayoutContainer;
this._needLayout=!this._childOfLayoutWidget;
this.inherited(arguments);
if(this._isShown()){
this._onShow();
}
if(!this._childOfLayoutWidget){
this.connect(_8("ie")?this.domNode:_9.global,"onresize",function(){
this._needLayout=!this._childOfLayoutWidget;
this.resize();
});
}
},_checkIfSingleChild:function(){
var _e=_7("> *",this.containerNode).filter(function(_f){
return _f.tagName!=="SCRIPT";
}),_10=_e.filter(function(_11){
return _3.has(_11,"data-dojo-type")||_3.has(_11,"dojoType")||_3.has(_11,"widgetId");
}),_12=_1.filter(_10.map(_a.byNode),function(_13){
return _13&&_13.domNode&&_13.resize;
});
if(_e.length==_10.length&&_12.length==1){
this._singleChild=_12[0];
}else{
delete this._singleChild;
}
_4.toggle(this.containerNode,this.baseClass+"SingleChild",!!this._singleChild);
},resize:function(_14,_15){
if(!this._wasShown&&this.open!==false){
this._onShow();
}
this._resizeCalled=true;
this._scheduleLayout(_14,_15);
},_scheduleLayout:function(_16,_17){
if(this._isShown()){
this._layout(_16,_17);
}else{
this._needLayout=true;
this._changeSize=_16;
this._resultSize=_17;
}
},_layout:function(_18,_19){
if(_18){
_5.setMarginBox(this.domNode,_18);
}
var cn=this.containerNode;
if(cn===this.domNode){
var mb=_19||{};
_6.mixin(mb,_18||{});
if(!("h" in mb)||!("w" in mb)){
mb=_6.mixin(_5.getMarginBox(cn),mb);
}
this._contentBox=_b.marginBox2contentBox(cn,mb);
}else{
this._contentBox=_5.getContentBox(cn);
}
this._layoutChildren();
delete this._needLayout;
},_layoutChildren:function(){
if(this.doLayout){
this._checkIfSingleChild();
}
if(this._singleChild&&this._singleChild.resize){
var cb=this._contentBox||_5.getContentBox(this.containerNode);
this._singleChild.resize({w:cb.w,h:cb.h});
}else{
_1.forEach(this.getChildren(),function(_1a){
if(_1a.resize){
_1a.resize();
}
});
}
},_isShown:function(){
if(this._childOfLayoutWidget){
if(this._resizeCalled&&"open" in this){
return this.open;
}
return this._resizeCalled;
}else{
if("open" in this){
return this.open;
}else{
var _1b=this.domNode,_1c=this.domNode.parentNode;
return (_1b.style.display!="none")&&(_1b.style.visibility!="hidden")&&!_4.contains(_1b,"dijitHidden")&&_1c&&_1c.style&&(_1c.style.display!="none");
}
}
},_onShow:function(){
if(this._needLayout){
this._layout(this._changeSize,this._resultSize);
}
this.inherited(arguments);
this._wasShown=true;
}});
});

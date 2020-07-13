//>>built
define("dijit/layout/_ContentPaneResizeMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/lang","dojo/query","../registry","../Viewport","./utils"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dijit.layout._ContentPaneResizeMixin",null,{doLayout:true,isLayoutContainer:true,startup:function(){
if(this._started){
return;
}
var _b=this.getParent();
this._childOfLayoutWidget=_b&&_b.isLayoutContainer;
this._needLayout=!this._childOfLayoutWidget;
this.inherited(arguments);
if(this._isShown()){
this._onShow();
}
if(!this._childOfLayoutWidget){
this.own(_9.on("resize",_6.hitch(this,"resize")));
}
},_checkIfSingleChild:function(){
if(!this.doLayout){
return;
}
var _c=[],_d=false;
_7("> *",this.containerNode).some(function(_e){
var _f=_8.byNode(_e);
if(_f&&_f.resize){
_c.push(_f);
}else{
if(!/script|link|style/i.test(_e.nodeName)&&_e.offsetHeight){
_d=true;
}
}
});
this._singleChild=_c.length==1&&!_d?_c[0]:null;
_3.toggle(this.containerNode,this.baseClass+"SingleChild",!!this._singleChild);
},resize:function(_10,_11){
this._resizeCalled=true;
this._scheduleLayout(_10,_11);
},_scheduleLayout:function(_12,_13){
if(this._isShown()){
this._layout(_12,_13);
}else{
this._needLayout=true;
this._changeSize=_12;
this._resultSize=_13;
}
},_layout:function(_14,_15){
delete this._needLayout;
if(!this._wasShown&&this.open!==false){
this._onShow();
}
if(_14){
_4.setMarginBox(this.domNode,_14);
}
var cn=this.containerNode;
if(cn===this.domNode){
var mb=_15||{};
_6.mixin(mb,_14||{});
if(!("h" in mb)||!("w" in mb)){
mb=_6.mixin(_4.getMarginBox(cn),mb);
}
this._contentBox=_a.marginBox2contentBox(cn,mb);
}else{
this._contentBox=_4.getContentBox(cn);
}
this._layoutChildren();
},_layoutChildren:function(){
this._checkIfSingleChild();
if(this._singleChild&&this._singleChild.resize){
var cb=this._contentBox||_4.getContentBox(this.containerNode);
this._singleChild.resize({w:cb.w,h:cb.h});
}else{
var _16=this.getChildren(),_17,i=0;
while(_17=_16[i++]){
if(_17.resize){
_17.resize();
}
}
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
var _18=this.domNode,_19=this.domNode.parentNode;
return (_18.style.display!="none")&&(_18.style.visibility!="hidden")&&!_3.contains(_18,"dijitHidden")&&_19&&_19.style&&(_19.style.display!="none");
}
}
},_onShow:function(){
this._wasShown=true;
if(this._needLayout){
this._layout(this._changeSize,this._resultSize);
}
this.inherited(arguments);
}});
});

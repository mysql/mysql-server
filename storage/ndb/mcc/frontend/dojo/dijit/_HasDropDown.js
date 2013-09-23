//>>built
define("dijit/_HasDropDown",["dojo/_base/declare","dojo/_base/Deferred","dojo/_base/event","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/has","dojo/keys","dojo/_base/lang","dojo/touch","dojo/_base/window","dojo/window","./registry","./focus","./popup","./_FocusMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
return _1("dijit._HasDropDown",_12,{_buttonNode:null,_arrowWrapperNode:null,_popupStateNode:null,_aroundNode:null,dropDown:null,autoWidth:true,forceWidth:false,maxHeight:0,dropDownPosition:["below","above"],_stopClickEvents:true,_onDropDownMouseDown:function(e){
if(this.disabled||this.readOnly){
return;
}
_3.stop(e);
this._docHandler=this.connect(_d.doc,_c.release,"_onDropDownMouseUp");
this.toggleDropDown();
},_onDropDownMouseUp:function(e){
if(e&&this._docHandler){
this.disconnect(this._docHandler);
}
var _13=this.dropDown,_14=false;
if(e&&this._opened){
var c=_7.position(this._buttonNode,true);
if(!(e.pageX>=c.x&&e.pageX<=c.x+c.w)||!(e.pageY>=c.y&&e.pageY<=c.y+c.h)){
var t=e.target;
while(t&&!_14){
if(_6.contains(t,"dijitPopup")){
_14=true;
}else{
t=t.parentNode;
}
}
if(_14){
t=e.target;
if(_13.onItemClick){
var _15;
while(t&&!(_15=_f.byNode(t))){
t=t.parentNode;
}
if(_15&&_15.onClick&&_15.getParent){
_15.getParent().onItemClick(_15,e);
}
}
return;
}
}
}
if(this._opened){
if(_13.focus&&_13.autoFocus!==false){
window.setTimeout(_b.hitch(_13,"focus"),1);
}
}else{
setTimeout(_b.hitch(this,"focus"),0);
}
if(_9("ios")){
this._justGotMouseUp=true;
setTimeout(_b.hitch(this,function(){
this._justGotMouseUp=false;
}),0);
}
},_onDropDownClick:function(e){
if(_9("ios")&&!this._justGotMouseUp){
this._onDropDownMouseDown(e);
this._onDropDownMouseUp(e);
}
if(this._stopClickEvents){
_3.stop(e);
}
},buildRendering:function(){
this.inherited(arguments);
this._buttonNode=this._buttonNode||this.focusNode||this.domNode;
this._popupStateNode=this._popupStateNode||this.focusNode||this._buttonNode;
var _16={"after":this.isLeftToRight()?"Right":"Left","before":this.isLeftToRight()?"Left":"Right","above":"Up","below":"Down","left":"Left","right":"Right"}[this.dropDownPosition[0]]||this.dropDownPosition[0]||"Down";
_6.add(this._arrowWrapperNode||this._buttonNode,"dijit"+_16+"ArrowButton");
},postCreate:function(){
this.inherited(arguments);
this.connect(this._buttonNode,_c.press,"_onDropDownMouseDown");
this.connect(this._buttonNode,"onclick","_onDropDownClick");
this.connect(this.focusNode,"onkeypress","_onKey");
this.connect(this.focusNode,"onkeyup","_onKeyUp");
},destroy:function(){
if(this.dropDown){
if(!this.dropDown._destroyed){
this.dropDown.destroyRecursive();
}
delete this.dropDown;
}
this.inherited(arguments);
},_onKey:function(e){
if(this.disabled||this.readOnly){
return;
}
var d=this.dropDown,_17=e.target;
if(d&&this._opened&&d.handleKey){
if(d.handleKey(e)===false){
_3.stop(e);
return;
}
}
if(d&&this._opened&&e.charOrCode==_a.ESCAPE){
this.closeDropDown();
_3.stop(e);
}else{
if(!this._opened&&(e.charOrCode==_a.DOWN_ARROW||((e.charOrCode==_a.ENTER||e.charOrCode==" ")&&((_17.tagName||"").toLowerCase()!=="input"||(_17.type&&_17.type.toLowerCase()!=="text"))))){
this._toggleOnKeyUp=true;
_3.stop(e);
}
}
},_onKeyUp:function(){
if(this._toggleOnKeyUp){
delete this._toggleOnKeyUp;
this.toggleDropDown();
var d=this.dropDown;
if(d&&d.focus){
setTimeout(_b.hitch(d,"focus"),1);
}
}
},_onBlur:function(){
var _18=_10.curNode&&this.dropDown&&_4.isDescendant(_10.curNode,this.dropDown.domNode);
this.closeDropDown(_18);
this.inherited(arguments);
},isLoaded:function(){
return true;
},loadDropDown:function(_19){
_19();
},loadAndOpenDropDown:function(){
var d=new _2(),_1a=_b.hitch(this,function(){
this.openDropDown();
d.resolve(this.dropDown);
});
if(!this.isLoaded()){
this.loadDropDown(_1a);
}else{
_1a();
}
return d;
},toggleDropDown:function(){
if(this.disabled||this.readOnly){
return;
}
if(!this._opened){
this.loadAndOpenDropDown();
}else{
this.closeDropDown();
}
},openDropDown:function(){
var _1b=this.dropDown,_1c=_1b.domNode,_1d=this._aroundNode||this.domNode,_1e=this;
if(!this._preparedNode){
this._preparedNode=true;
if(_1c.style.width){
this._explicitDDWidth=true;
}
if(_1c.style.height){
this._explicitDDHeight=true;
}
}
if(this.maxHeight||this.forceWidth||this.autoWidth){
var _1f={display:"",visibility:"hidden"};
if(!this._explicitDDWidth){
_1f.width="";
}
if(!this._explicitDDHeight){
_1f.height="";
}
_8.set(_1c,_1f);
var _20=this.maxHeight;
if(_20==-1){
var _21=_e.getBox(),_22=_7.position(_1d,false);
_20=Math.floor(Math.max(_22.y,_21.h-(_22.y+_22.h)));
}
_11.moveOffScreen(_1b);
if(_1b.startup&&!_1b._started){
_1b.startup();
}
var mb=_7.getMarginSize(_1c);
var _23=(_20&&mb.h>_20);
_8.set(_1c,{overflowX:"hidden",overflowY:_23?"auto":"hidden"});
if(_23){
mb.h=_20;
if("w" in mb){
mb.w+=16;
}
}else{
delete mb.h;
}
if(this.forceWidth){
mb.w=_1d.offsetWidth;
}else{
if(this.autoWidth){
mb.w=Math.max(mb.w,_1d.offsetWidth);
}else{
delete mb.w;
}
}
if(_b.isFunction(_1b.resize)){
_1b.resize(mb);
}else{
_7.setMarginBox(_1c,mb);
}
}
var _24=_11.open({parent:this,popup:_1b,around:_1d,orient:this.dropDownPosition,onExecute:function(){
_1e.closeDropDown(true);
},onCancel:function(){
_1e.closeDropDown(true);
},onClose:function(){
_5.set(_1e._popupStateNode,"popupActive",false);
_6.remove(_1e._popupStateNode,"dijitHasDropDownOpen");
_1e._opened=false;
}});
_5.set(this._popupStateNode,"popupActive","true");
_6.add(_1e._popupStateNode,"dijitHasDropDownOpen");
this._opened=true;
return _24;
},closeDropDown:function(_25){
if(this._opened){
if(_25){
this.focus();
}
_11.close(this.dropDown);
this._opened=false;
}
}});
});

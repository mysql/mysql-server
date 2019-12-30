//>>built
define("dijit/_HasDropDown",["dojo/_base/declare","dojo/_base/Deferred","dojo/_base/event","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/has","dojo/keys","dojo/_base/lang","dojo/on","./registry","./focus","./popup","./_FocusMixin","./Viewport"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,on,_c,_d,_e,_f,_10){
return _1("dijit._HasDropDown",_f,{_buttonNode:null,_arrowWrapperNode:null,_popupStateNode:null,_aroundNode:null,dropDown:null,autoWidth:true,forceWidth:false,maxHeight:0,dropDownPosition:["below","above"],_stopClickEvents:true,_onDropDownMouseDown:function(e){
if(this.disabled||this.readOnly){
return;
}
if(e.type!="MSPointerDown"){
e.preventDefault();
}
this._docHandler=this.connect(this.ownerDocument,"mouseup","_onDropDownMouseUp");
this.toggleDropDown();
},_onDropDownMouseUp:function(e){
if(e&&this._docHandler){
this.disconnect(this._docHandler);
}
var _11=this.dropDown,_12=false;
if(e&&this._opened){
var c=_7.position(this._buttonNode,true);
if(!(e.pageX>=c.x&&e.pageX<=c.x+c.w)||!(e.pageY>=c.y&&e.pageY<=c.y+c.h)){
var t=e.target;
while(t&&!_12){
if(_6.contains(t,"dijitPopup")){
_12=true;
}else{
t=t.parentNode;
}
}
if(_12){
t=e.target;
if(_11.onItemClick){
var _13;
while(t&&!(_13=_c.byNode(t))){
t=t.parentNode;
}
if(_13&&_13.onClick&&_13.getParent){
_13.getParent().onItemClick(_13,e);
}
}
return;
}
}
}
if(this._opened){
if(_11.focus&&(_11.autoFocus!==false||(e.type=="mouseup"&&!this.hovering))){
this._focusDropDownTimer=this.defer(function(){
_11.focus();
delete this._focusDropDownTimer;
});
}
}else{
if(this.focus){
this.defer("focus");
}
}
if(_9("touch")){
this._justGotMouseUp=true;
this.defer(function(){
this._justGotMouseUp=false;
});
}
},_onDropDownClick:function(e){
if(_9("touch")&&!this._justGotMouseUp){
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
var _14={"after":this.isLeftToRight()?"Right":"Left","before":this.isLeftToRight()?"Left":"Right","above":"Up","below":"Down","left":"Left","right":"Right"}[this.dropDownPosition[0]]||this.dropDownPosition[0]||"Down";
_6.add(this._arrowWrapperNode||this._buttonNode,"dijit"+_14+"ArrowButton");
},postCreate:function(){
this.inherited(arguments);
var _15=this.focusNode||this.domNode;
this.own(on(this._buttonNode,"mousedown",_b.hitch(this,"_onDropDownMouseDown")),on(this._buttonNode,"click",_b.hitch(this,"_onDropDownClick")),on(_15,"keydown",_b.hitch(this,"_onKey")),on(_15,"keyup",_b.hitch(this,"_onKeyUp")));
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
var d=this.dropDown,_16=e.target;
if(d&&this._opened&&d.handleKey){
if(d.handleKey(e)===false){
_3.stop(e);
return;
}
}
if(d&&this._opened&&e.keyCode==_a.ESCAPE){
this.closeDropDown();
_3.stop(e);
}else{
if(!this._opened&&(e.keyCode==_a.DOWN_ARROW||((e.keyCode==_a.ENTER||e.keyCode==_a.SPACE)&&((_16.tagName||"").toLowerCase()!=="input"||(_16.type&&_16.type.toLowerCase()!=="text"))))){
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
this.defer(_b.hitch(d,"focus"),1);
}
}
},_onBlur:function(){
var _17=_d.curNode&&this.dropDown&&_4.isDescendant(_d.curNode,this.dropDown.domNode);
this.closeDropDown(_17);
this.inherited(arguments);
},isLoaded:function(){
return true;
},loadDropDown:function(_18){
_18();
},loadAndOpenDropDown:function(){
var d=new _2(),_19=_b.hitch(this,function(){
this.openDropDown();
d.resolve(this.dropDown);
});
if(!this.isLoaded()){
this.loadDropDown(_19);
}else{
_19();
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
var _1a=this.dropDown,_1b=_1a.domNode,_1c=this._aroundNode||this.domNode,_1d=this;
if(!this._preparedNode){
this._preparedNode=true;
if(_1b.style.width){
this._explicitDDWidth=true;
}
if(_1b.style.height){
this._explicitDDHeight=true;
}
}
if(this.maxHeight||this.forceWidth||this.autoWidth){
var _1e={display:"",visibility:"hidden"};
if(!this._explicitDDWidth){
_1e.width="";
}
if(!this._explicitDDHeight){
_1e.height="";
}
_8.set(_1b,_1e);
var _1f=this.maxHeight;
if(_1f==-1){
var _20=_10.getEffectiveBox(this.ownerDocument),_21=_7.position(_1c,false);
_1f=Math.floor(Math.max(_21.y,_20.h-(_21.y+_21.h)));
}
_e.moveOffScreen(_1a);
if(_1a.startup&&!_1a._started){
_1a.startup();
}
var mb=_7.getMarginSize(_1b);
var _22=(_1f&&mb.h>_1f);
_8.set(_1b,{overflow:_22?"auto":"visible"});
if(_22){
mb.h=_1f;
if("w" in mb){
mb.w+=16;
}
}else{
delete mb.h;
}
if(this.forceWidth){
mb.w=_1c.offsetWidth;
}else{
if(this.autoWidth){
mb.w=Math.max(mb.w,_1c.offsetWidth);
}else{
delete mb.w;
}
}
if(_b.isFunction(_1a.resize)){
_1a.resize(mb);
}else{
_7.setMarginBox(_1b,mb);
}
}
var _23=_e.open({parent:this,popup:_1a,around:_1c,orient:this.dropDownPosition,onExecute:function(){
_1d.closeDropDown(true);
},onCancel:function(){
_1d.closeDropDown(true);
},onClose:function(){
_5.set(_1d._popupStateNode,"popupActive",false);
_6.remove(_1d._popupStateNode,"dijitHasDropDownOpen");
_1d._set("_opened",false);
}});
_5.set(this._popupStateNode,"popupActive","true");
_6.add(this._popupStateNode,"dijitHasDropDownOpen");
this._set("_opened",true);
this._popupStateNode.setAttribute("aria-expanded","true");
this._popupStateNode.setAttribute("aria-owns",_1a.id);
if(_1b.getAttribute("role")!=="presentation"&&!_1b.getAttribute("aria-labelledby")){
_1b.setAttribute("aria-labelledby",this.id);
}
return _23;
},closeDropDown:function(_24){
if(this._focusDropDownTimer){
this._focusDropDownTimer.remove();
delete this._focusDropDownTimer;
}
if(this._opened){
this._popupStateNode.setAttribute("aria-expanded","false");
if(_24){
this.focus();
}
_e.close(this.dropDown);
this._opened=false;
}
}});
});

//>>built
define("dojox/mobile/ComboBox",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-geometry","dojo/dom-style","dojo/window","dijit/form/_AutoCompleterMixin","dijit/popup","./_ComboBoxMenu","./TextBox","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
_1.experimental("dojox.mobile.ComboBox");
return _2("dojox.mobile.ComboBox",[_b,_8],{dropDownClass:"dojox.mobile._ComboBoxMenu",selectOnClick:false,autoComplete:false,dropDown:null,maxHeight:-1,dropDownPosition:["below","above"],_throttleOpenClose:function(){
if(this._throttleHandler){
clearTimeout(this._throttleHandler);
}
this._throttleHandler=setTimeout(_3.hitch(this,function(){
this._throttleHandler=null;
}),500);
},_onFocus:function(){
this.inherited(arguments);
if(!this._opened&&!this._throttleHandler){
this._startSearchAll();
}
},onInput:function(e){
this._onKey(e);
this.inherited(arguments);
},_setListAttr:function(v){
this._set("list",v);
},closeDropDown:function(){
this._throttleOpenClose();
if(this.startHandler){
this.disconnect(this.startHandler);
this.startHandler=null;
if(this.moveHandler){
this.disconnect(this.moveHandler);
}
if(this.endHandler){
this.disconnect(this.endHandler);
}
}
this.inherited(arguments);
_9.close(this.dropDown);
this._opened=false;
},openDropDown:function(){
var _d=!this._opened;
var _e=this.dropDown,_f=_e.domNode,_10=this.domNode,_11=this;
if(!this._preparedNode){
this._preparedNode=true;
if(_f.style.width){
this._explicitDDWidth=true;
}
if(_f.style.height){
this._explicitDDHeight=true;
}
}
var _12={display:"",overflow:"hidden",visibility:"hidden"};
if(!this._explicitDDWidth){
_12.width="";
}
if(!this._explicitDDHeight){
_12.height="";
}
_6.set(_f,_12);
var _13=this.maxHeight;
if(_13==-1){
var _14=_7.getBox(),_15=_5.position(_10,false);
_13=Math.floor(Math.max(_15.y,_14.h-(_15.y+_15.h)));
}
_9.moveOffScreen(_e);
if(_e.startup&&!_e._started){
_e.startup();
}
var mb=_5.position(this.dropDown.containerNode,false);
var _16=(_13&&mb.h>_13);
if(_16){
mb.h=_13;
}
mb.w=Math.max(mb.w,_10.offsetWidth);
_5.setMarginBox(_f,mb);
var _17=_9.open({parent:this,popup:_e,around:_10,orient:this.dropDownPosition,onExecute:function(){
_11.closeDropDown();
},onCancel:function(){
_11.closeDropDown();
},onClose:function(){
_11._opened=false;
}});
this._opened=true;
if(_d){
if(_17.aroundCorner.charAt(0)=="B"){
this.domNode.scrollIntoView(true);
}
this.startHandler=this.connect(_4.doc.documentElement,_c("touch")?"ontouchstart":"onmousedown",_3.hitch(this,function(){
var _18=false;
this.moveHandler=this.connect(_4.doc.documentElement,_c("touch")?"ontouchmove":"onmousemove",function(){
_18=true;
});
this.endHandler=this.connect(_4.doc.documentElement,_c("touch")?"ontouchend":"onmouseup",function(){
if(!_18){
this.closeDropDown();
}
});
}));
}
return _17;
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onclick","_onClick");
},_onClick:function(e){
if(!this._throttleHandler){
if(this.opened){
this.closeDropDown();
}else{
this._startSearchAll();
}
}
}});
});

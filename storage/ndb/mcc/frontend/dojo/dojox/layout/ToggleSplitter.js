//>>built
define("dojox/layout/ToggleSplitter",["dojo","dijit","dijit/layout/BorderContainer"],function(_1,_2){
_1.experimental("dojox.layout.ToggleSplitter");
_1.declare("dojox.layout.ToggleSplitter",_2.layout._Splitter,{state:"full",_closedSize:"0",baseClass:"dojoxToggleSplitter",templateString:"<div class=\"dijitSplitter dojoxToggleSplitter\" dojoAttachEvent=\"onkeypress:_onKeyPress,onmousedown:_startDrag,onmouseenter:_onMouse,onmouseleave:_onMouse\">"+"<div dojoAttachPoint=\"toggleNode\" class=\"dijitSplitterThumb dojoxToggleSplitterIcon\" tabIndex=\"0\" role=\"separator\" "+"dojoAttachEvent=\"onmousedown:_onToggleNodeMouseDown,onclick:_toggle,onmouseenter:_onToggleNodeMouseMove,onmouseleave:_onToggleNodeMouseMove,onfocus:_onToggleNodeMouseMove,onblur:_onToggleNodeMouseMove\">"+"<span class=\"dojoxToggleSplitterA11y\" dojoAttachPoint=\"a11yText\"></span></div>"+"</div>",postCreate:function(){
this.inherited(arguments);
var _3=this.region;
_1.addClass(this.domNode,this.baseClass+_3.charAt(0).toUpperCase()+_3.substring(1));
},startup:function(){
this.inherited(arguments);
var _4=this.child,_5=this.child.domNode,_6=_1.style(_5,(this.horizontal?"height":"width"));
this.domNode.setAttribute("aria-controls",_5.id);
_1.forEach(["toggleSplitterState","toggleSplitterFullSize","toggleSplitterCollapsedSize"],function(_7){
var _8=_7.substring("toggleSplitter".length);
_8=_8.charAt(0).toLowerCase()+_8.substring(1);
if(_7 in this.child){
this[_8]=this.child[_7];
}
},this);
if(!this.fullSize){
this.fullSize=this.state=="full"?_6+"px":"75px";
}
this._openStyleProps=this._getStyleProps(_5,"full");
this._started=true;
this.set("state",this.state);
return this;
},_onKeyPress:function(_9){
if(this.state=="full"){
this.inherited(arguments);
}
if(_9.charCode==_1.keys.SPACE||_9.keyCode==_1.keys.ENTER){
this._toggle(_9);
}
},_onToggleNodeMouseDown:function(_a){
_1.stopEvent(_a);
this.toggleNode.focus();
},_startDrag:function(e){
if(this.state=="full"){
this.inherited(arguments);
}
},_stopDrag:function(e){
this.inherited(arguments);
this.toggleNode.blur();
},_toggle:function(_b){
var _c;
switch(this.state){
case "full":
_c=this.collapsedSize?"collapsed":"closed";
break;
case "collapsed":
_c="closed";
break;
default:
_c="full";
}
this.set("state",_c);
},_onToggleNodeMouseMove:function(_d){
var _e=this.baseClass,_f=this.toggleNode,on=this.state=="full"||this.state=="collapsed",_10=_d.type=="mouseout"||_d.type=="blur";
_1.toggleClass(_f,_e+"IconOpen",_10&&on);
_1.toggleClass(_f,_e+"IconOpenHover",!_10&&on);
_1.toggleClass(_f,_e+"IconClosed",_10&&!on);
_1.toggleClass(_f,_e+"IconClosedHover",!_10&&!on);
},_handleOnChange:function(_11){
var _12=this.child.domNode,_13,_14,dim=this.horizontal?"height":"width";
if(this.state=="full"){
var _15=_1.mixin({display:"block",overflow:"auto",visibility:"visible"},this._openStyleProps);
_15[dim]=(this._openStyleProps&&this._openStyleProps[dim])?this._openStyleProps[dim]:this.fullSize;
_1.style(this.domNode,"cursor","");
_1.style(_12,_15);
}else{
if(this.state=="collapsed"){
_14=_1.getComputedStyle(_12);
_13=this._getStyleProps(_12,"full",_14);
this._openStyleProps=_13;
_1.style(this.domNode,"cursor","auto");
_1.style(_12,dim,this.collapsedSize);
}else{
if(!this.collapsedSize){
_14=_1.getComputedStyle(_12);
_13=this._getStyleProps(_12,"full",_14);
this._openStyleProps=_13;
}
var _16=this._getStyleProps(_12,"closed",_14);
_1.style(this.domNode,"cursor","auto");
_1.style(_12,_16);
}
}
this._setStateClass();
if(this.container._started){
this.container._layoutChildren(this.region);
}
},_getStyleProps:function(_17,_18,_19){
if(!_19){
_19=_1.getComputedStyle(_17);
}
var _1a={},dim=this.horizontal?"height":"width";
_1a["overflow"]=(_18!="closed")?_19["overflow"]:"hidden";
_1a["visibility"]=(_18!="closed")?_19["visibility"]:"hidden";
_1a[dim]=(_18!="closed")?_17.style[dim]||_19[dim]:this._closedSize;
var _1b=["Top","Right","Bottom","Left"];
_1.forEach(["padding","margin","border"],function(_1c){
for(var i=0;i<_1b.length;i++){
var _1d=_1c+_1b[i];
if(_1c=="border"){
_1d+="Width";
}
if(undefined!==_19[_1d]){
_1a[_1d]=(_18!="closed")?_19[_1d]:0;
}
}
});
return _1a;
},_setStateClass:function(){
var _1e="&#9652",_1f=this.region.toLowerCase(),_20=this.baseClass,_21=this.toggleNode,on=this.state=="full"||this.state=="collapsed",_22=this.focused;
_1.toggleClass(_21,_20+"IconOpen",on&&!_22);
_1.toggleClass(_21,_20+"IconClosed",!on&&!_22);
_1.toggleClass(_21,_20+"IconOpenHover",on&&_22);
_1.toggleClass(_21,_20+"IconClosedHover",!on&&_22);
if(_1f=="top"&&on||_1f=="bottom"&&!on){
_1e="&#9650";
}else{
if(_1f=="top"&&!on||_1f=="bottom"&&on){
_1e="&#9660";
}else{
if(_1f=="right"&&on||_1f=="left"&&!on){
_1e="&#9654";
}else{
if(_1f=="right"&&!on||_1f=="left"&&on){
_1e="&#9664";
}
}
}
}
this.a11yText.innerHTML=_1e;
},_setStateAttr:function(_23){
if(!this._started){
return;
}
var _24=this.state;
this.state=_23;
this._handleOnChange(_24);
var _25;
switch(_23){
case "full":
this.domNode.setAttribute("aria-expanded",true);
_25="onOpen";
break;
case "collapsed":
this.domNode.setAttribute("aria-expanded",true);
_25="onCollapsed";
break;
default:
this.domNode.setAttribute("aria-expanded",false);
_25="onClosed";
}
this[_25](this.child);
},onOpen:function(_26){
},onCollapsed:function(_27){
},onClosed:function(_28){
}});
_1.extend(_2._Widget,{toggleSplitterState:"full",toggleSplitterFullSize:"",toggleSplitterCollapsedSize:""});
});

//>>built
define("dojox/layout/ToggleSplitter",["dojo","dijit","dijit/layout/BorderContainer"],function(_1,_2){
_1.experimental("dojox.layout.ToggleSplitter");
var _3=_1.declare("dojox.layout.ToggleSplitter",_2.layout._Splitter,{container:null,child:null,region:null,state:"full",_closedSize:"0",baseClass:"dojoxToggleSplitter",templateString:"<div class=\"dijitSplitter dojoxToggleSplitter\" dojoAttachEvent=\"onkeypress:_onKeyPress,onmousedown:_startDrag,onmouseenter:_onMouse,onmouseleave:_onMouse\">"+"<div dojoAttachPoint=\"toggleNode\" class=\"dijitSplitterThumb dojoxToggleSplitterIcon\" tabIndex=\"0\" role=\"separator\" "+"dojoAttachEvent=\"onmousedown:_onToggleNodeMouseDown,onclick:_toggle,onmouseenter:_onToggleNodeMouseMove,onmouseleave:_onToggleNodeMouseMove,onfocus:_onToggleNodeMouseMove,onblur:_onToggleNodeMouseMove\">"+"<span class=\"dojoxToggleSplitterA11y\" dojoAttachPoint=\"a11yText\"></span></div>"+"</div>",postCreate:function(){
this.inherited(arguments);
var _4=this.region;
_1.addClass(this.domNode,this.baseClass+_4.charAt(0).toUpperCase()+_4.substring(1));
},startup:function(){
this.inherited(arguments);
var _5=this.child,_6=this.child.domNode,_7=_1.style(_6,(this.horizontal?"height":"width"));
this.domNode.setAttribute("aria-controls",_6.id);
_1.forEach(["toggleSplitterState","toggleSplitterFullSize","toggleSplitterCollapsedSize"],function(_8){
var _9=_8.substring("toggleSplitter".length);
_9=_9.charAt(0).toLowerCase()+_9.substring(1);
if(_8 in this.child){
this[_9]=this.child[_8];
}
},this);
if(!this.fullSize){
this.fullSize=this.state=="full"?_7+"px":"75px";
}
this._openStyleProps=this._getStyleProps(_6,"full");
this._started=true;
this.set("state",this.state);
return this;
},_onKeyPress:function(_a){
if(this.state=="full"){
this.inherited(arguments);
}
if(_a.charCode==_1.keys.SPACE||_a.keyCode==_1.keys.ENTER){
this._toggle(_a);
_1.stopEvent(_a);
}
},_onToggleNodeMouseDown:function(_b){
_1.stopEvent(_b);
this.toggleNode.focus();
},_startDrag:function(e){
if(this.state=="full"){
this.inherited(arguments);
}
},_stopDrag:function(e){
this.inherited(arguments);
this.toggleNode.blur();
},_toggle:function(_c){
var _d;
switch(this.state){
case "full":
_d=this.collapsedSize?"collapsed":"closed";
break;
case "collapsed":
_d="closed";
break;
default:
_d="full";
}
this.set("state",_d);
},_onToggleNodeMouseMove:function(_e){
var _f=this.baseClass,_10=this.toggleNode,on=this.state=="full"||this.state=="collapsed",_11=_e.type=="mouseout"||_e.type=="blur";
_1.toggleClass(_10,_f+"IconOpen",_11&&on);
_1.toggleClass(_10,_f+"IconOpenHover",!_11&&on);
_1.toggleClass(_10,_f+"IconClosed",_11&&!on);
_1.toggleClass(_10,_f+"IconClosedHover",!_11&&!on);
},_handleOnChange:function(_12){
var _13=this.child.domNode,_14,_15,dim=this.horizontal?"height":"width";
if(this.state=="full"){
var _16=_1.mixin({display:"block",overflow:"auto",visibility:"visible"},this._openStyleProps);
_16[dim]=(this._openStyleProps&&this._openStyleProps[dim])?this._openStyleProps[dim]:this.fullSize;
_1.style(this.domNode,"cursor","");
_1.style(_13,_16);
}else{
if(this.state=="collapsed"){
_15=_1.getComputedStyle(_13);
_14=this._getStyleProps(_13,"full",_15);
this._openStyleProps=_14;
_1.style(this.domNode,"cursor","auto");
_1.style(_13,dim,this.collapsedSize);
}else{
if(!this.collapsedSize){
_15=_1.getComputedStyle(_13);
_14=this._getStyleProps(_13,"full",_15);
this._openStyleProps=_14;
}
var _17=this._getStyleProps(_13,"closed",_15);
_1.style(this.domNode,"cursor","auto");
_1.style(_13,_17);
}
}
this._setStateClass();
if(this.container._started){
this.container._layoutChildren(this.region);
}
},_getStyleProps:function(_18,_19,_1a){
if(!_1a){
_1a=_1.getComputedStyle(_18);
}
var _1b={},dim=this.horizontal?"height":"width";
_1b["overflow"]=(_19!="closed")?_1a["overflow"]:"hidden";
_1b["visibility"]=(_19!="closed")?_1a["visibility"]:"hidden";
_1b[dim]=(_19!="closed")?_18.style[dim]||_1a[dim]:this._closedSize;
var _1c=["Top","Right","Bottom","Left"];
_1.forEach(["padding","margin","border"],function(_1d){
for(var i=0;i<_1c.length;i++){
var _1e=_1d+_1c[i];
if(_1d=="border"){
_1e+="Width";
}
if(undefined!==_1a[_1e]){
_1b[_1e]=(_19!="closed")?_1a[_1e]:0;
}
}
});
return _1b;
},_setStateClass:function(){
var _1f="&#9652",_20=this.region.toLowerCase(),_21=this.baseClass,_22=this.toggleNode,on=this.state=="full"||this.state=="collapsed",_23=this.focused;
_1.toggleClass(_22,_21+"IconOpen",on&&!_23);
_1.toggleClass(_22,_21+"IconClosed",!on&&!_23);
_1.toggleClass(_22,_21+"IconOpenHover",on&&_23);
_1.toggleClass(_22,_21+"IconClosedHover",!on&&_23);
if(_20=="top"&&on||_20=="bottom"&&!on){
_1f="&#9650";
}else{
if(_20=="top"&&!on||_20=="bottom"&&on){
_1f="&#9660";
}else{
if(_20=="right"&&on||_20=="left"&&!on){
_1f="&#9654";
}else{
if(_20=="right"&&!on||_20=="left"&&on){
_1f="&#9664";
}
}
}
}
this.a11yText.innerHTML=_1f;
},_setStateAttr:function(_24){
if(!this._started){
return;
}
var _25=this.state;
this.state=_24;
this._handleOnChange(_25);
var _26;
switch(_24){
case "full":
this.domNode.setAttribute("aria-expanded",true);
_26="onOpen";
break;
case "collapsed":
this.domNode.setAttribute("aria-expanded",true);
_26="onCollapsed";
break;
default:
this.domNode.setAttribute("aria-expanded",false);
_26="onClosed";
}
this[_26](this.child);
},onOpen:function(_27){
},onCollapsed:function(_28){
},onClosed:function(_29){
}});
_1.extend(_2._Widget,{toggleSplitterState:"full",toggleSplitterFullSize:"",toggleSplitterCollapsedSize:""});
return _3;
});

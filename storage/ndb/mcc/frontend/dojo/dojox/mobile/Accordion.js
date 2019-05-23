//>>built
define("dojox/mobile/Accordion",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/dom","dojo/dom-class","dojo/dom-construct","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./iconUtils","./lazyLoadUtils","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e=_2([_a,_8],{label:"Label",icon1:"",icon2:"",iconPos1:"",iconPos2:"",selected:false,baseClass:"mblAccordionTitle",buildRendering:function(){
this.inherited(arguments);
var a=this.anchorNode=_7.create("a",{className:"mblAccordionTitleAnchor"},this.domNode);
a.href="javascript:void(0)";
this.textBoxNode=_7.create("div",{className:"mblAccordionTitleTextBox"},a);
this.labelNode=_7.create("span",{className:"mblAccordionTitleLabel",innerHTML:this._cv?this._cv(this.label):this.label},this.textBoxNode);
this._isOnLine=this.inheritParams();
},postCreate:function(){
this._clickHandle=this.connect(this.domNode,"onclick","_onClick");
_5.setSelectable(this.domNode,false);
},inheritParams:function(){
var _f=this.getParent();
if(_f){
if(this.icon1&&_f.iconBase&&_f.iconBase.charAt(_f.iconBase.length-1)==="/"){
this.icon1=_f.iconBase+this.icon1;
}
if(!this.icon1){
this.icon1=_f.iconBase;
}
if(!this.iconPos1){
this.iconPos1=_f.iconPos;
}
if(this.icon2&&_f.iconBase&&_f.iconBase.charAt(_f.iconBase.length-1)==="/"){
this.icon2=_f.iconBase+this.icon2;
}
if(!this.icon2){
this.icon2=_f.iconBase||this.icon1;
}
if(!this.iconPos2){
this.iconPos2=_f.iconPos||this.iconPos1;
}
}
return !!_f;
},_setIcon:function(_10,n){
if(!this.getParent()){
return;
}
this._set("icon"+n,_10);
if(!this["iconParentNode"+n]){
this["iconParentNode"+n]=_7.create("div",{className:"mblAccordionIconParent mblAccordionIconParent"+n},this.anchorNode,"first");
}
this["iconNode"+n]=_b.setIcon(_10,this["iconPos"+n],this["iconNode"+n],this.alt,this["iconParentNode"+n]);
this["icon"+n]=_10;
_6.toggle(this.domNode,"mblAccordionHasIcon",_10&&_10!=="none");
},_setIcon1Attr:function(_11){
this._setIcon(_11,1);
},_setIcon2Attr:function(_12){
this._setIcon(_12,2);
},startup:function(){
if(this._started){
return;
}
if(!this._isOnLine){
this.inheritParams();
}
if(!this._isOnLine){
this.set({icon1:this.icon1,icon2:this.icon2});
}
this.inherited(arguments);
},_onClick:function(e){
if(this.onClick(e)===false){
return;
}
var p=this.getParent();
if(!p.fixedHeight&&this.contentWidget.domNode.style.display!=="none"){
p.collapse(this.contentWidget,!p.animation);
}else{
p.expand(this.contentWidget,!p.animation);
}
},onClick:function(){
},_setSelectedAttr:function(_13){
_6.toggle(this.domNode,"mblAccordionTitleSelected",_13);
this._set("selected",_13);
}});
var _14=_2("dojox.mobile.Accordion",[_a,_9,_8],{iconBase:"",iconPos:"",fixedHeight:false,singleOpen:false,animation:true,roundRect:false,duration:0.3,baseClass:"mblAccordion",_openSpace:1,startup:function(){
if(this._started){
return;
}
if(_6.contains(this.domNode,"mblAccordionRoundRect")){
this.roundRect=true;
}else{
if(this.roundRect){
_6.add(this.domNode,"mblAccordionRoundRect");
}
}
if(this.fixedHeight){
this.singleOpen=true;
}
var _15=this.getChildren();
_1.forEach(_15,this._setupChild,this);
var sel;
_1.forEach(_15,function(_16){
_16.startup();
_16._at.startup();
this.collapse(_16,true);
if(_16.selected){
sel=_16;
}
},this);
if(!sel&&this.fixedHeight){
sel=_15[_15.length-1];
}
if(sel){
this.expand(sel,true);
}else{
this._updateLast();
}
setTimeout(_3.hitch(this,function(){
this.resize();
}),0);
this._started=true;
},_setupChild:function(_17){
if(_17.domNode.style.overflow!="hidden"){
_17.domNode.style.overflow=this.fixedHeight?"auto":"hidden";
}
_17._at=new _e({label:_17.label,alt:_17.alt,icon1:_17.icon1,icon2:_17.icon2,iconPos1:_17.iconPos1,iconPos2:_17.iconPos2,contentWidget:_17});
_7.place(_17._at.domNode,_17.domNode,"before");
_6.add(_17.domNode,"mblAccordionPane");
},addChild:function(_18,_19){
this.inherited(arguments);
if(this._started){
this._setupChild(_18);
_18._at.startup();
if(_18.selected){
this.expand(_18,true);
setTimeout(function(){
_18.domNode.style.height="";
},0);
}else{
this.collapse(_18);
}
}
},removeChild:function(_1a){
if(typeof _1a=="number"){
_1a=this.getChildren()[_1a];
}
if(_1a){
_1a._at.destroy();
}
this.inherited(arguments);
},getChildren:function(){
return _1.filter(this.inherited(arguments),function(_1b){
return !(_1b instanceof _e);
});
},getSelectedPanes:function(){
return _1.filter(this.getChildren(),function(_1c){
return _1c.domNode.style.display!="none";
});
},resize:function(){
if(this.fixedHeight){
var _1d=_1.filter(this.getChildren(),function(_1e){
return _1e._at.domNode.style.display!="none";
});
var _1f=this.domNode.clientHeight;
_1.forEach(_1d,function(_20){
_1f-=_20._at.domNode.offsetHeight;
});
this._openSpace=_1f>0?_1f:0;
var sel=this.getSelectedPanes()[0];
sel.domNode.style.webkitTransition="";
sel.domNode.style.height=this._openSpace+"px";
}
},_updateLast:function(){
var _21=this.getChildren();
_1.forEach(_21,function(c,i){
_6.toggle(c._at.domNode,"mblAccordionTitleLast",i===_21.length-1&&!_6.contains(c._at.domNode,"mblAccordionTitleSelected"));
},this);
},expand:function(_22,_23){
if(_22.lazy){
_c.instantiateLazyWidgets(_22.containerNode,_22.requires);
_22.lazy=false;
}
var _24=this.getChildren();
_1.forEach(_24,function(c,i){
c.domNode.style.webkitTransition=_23?"":"height "+this.duration+"s linear";
if(c===_22){
c.domNode.style.display="";
var h;
if(this.fixedHeight){
h=this._openSpace;
}else{
h=parseInt(c.height||c.domNode.getAttribute("height"));
if(!h){
c.domNode.style.height="";
h=c.domNode.offsetHeight;
c.domNode.style.height="0px";
}
}
setTimeout(function(){
c.domNode.style.height=h+"px";
},0);
this.select(_22);
}else{
if(this.singleOpen){
this.collapse(c,_23);
}
}
},this);
this._updateLast();
},collapse:function(_25,_26){
if(_25.domNode.style.display==="none"){
return;
}
_25.domNode.style.webkitTransition=_26?"":"height "+this.duration+"s linear";
_25.domNode.style.height="0px";
if(!_4("webkit")||_26){
_25.domNode.style.display="none";
this._updateLast();
}else{
var _27=this;
setTimeout(function(){
_25.domNode.style.display="none";
_27._updateLast();
if(!_27.fixedHeight&&_27.singleOpen){
for(var v=_27.getParent();v;v=v.getParent()){
if(_6.contains(v.domNode,"mblView")){
if(v&&v.resize){
v.resize();
}
break;
}
}
}
},this.duration*1000);
}
this.deselect(_25);
},select:function(_28){
_28._at.set("selected",true);
},deselect:function(_29){
_29._at.set("selected",false);
}});
_14.ChildWidgetProperties={alt:"",label:"",icon1:"",icon2:"",iconPos1:"",iconPos2:"",selected:false,lazy:false};
_3.extend(_a,_14.ChildWidgetProperties);
return _14;
});

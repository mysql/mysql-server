//>>built
define("dojox/mobile/Accordion",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/sniff","dojo/dom-class","dojo/dom-construct","dojo/dom-attr","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./iconUtils","./lazyLoadUtils","./_css3","./common","require","dojo/has!dojo-bidi?dojox/mobile/bidi/Accordion"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_2([_a,_8],{label:"Label",icon1:"",icon2:"",iconPos1:"",iconPos2:"",selected:false,baseClass:"mblAccordionTitle",buildRendering:function(){
this.inherited(arguments);
var a=this.anchorNode=_6.create("a",{className:"mblAccordionTitleAnchor",role:"presentation"},this.domNode);
this.textBoxNode=_6.create("div",{className:"mblAccordionTitleTextBox"},a);
this.labelNode=_6.create("span",{className:"mblAccordionTitleLabel",innerHTML:this._cv?this._cv(this.label):this.label},this.textBoxNode);
this._isOnLine=this.inheritParams();
_7.set(this.textBoxNode,"role","tab");
_7.set(this.textBoxNode,"tabindex","0");
},postCreate:function(){
this.connect(this.domNode,"onclick","_onClick");
_e.setSelectable(this.domNode,false);
},inheritParams:function(){
var _12=this.getParent();
if(_12){
if(this.icon1&&_12.iconBase&&_12.iconBase.charAt(_12.iconBase.length-1)==="/"){
this.icon1=_12.iconBase+this.icon1;
}
if(!this.icon1){
this.icon1=_12.iconBase;
}
if(!this.iconPos1){
this.iconPos1=_12.iconPos;
}
if(this.icon2&&_12.iconBase&&_12.iconBase.charAt(_12.iconBase.length-1)==="/"){
this.icon2=_12.iconBase+this.icon2;
}
if(!this.icon2){
this.icon2=_12.iconBase||this.icon1;
}
if(!this.iconPos2){
this.iconPos2=_12.iconPos||this.iconPos1;
}
}
return !!_12;
},_setIcon:function(_13,n){
if(!this.getParent()){
return;
}
this._set("icon"+n,_13);
if(!this["iconParentNode"+n]){
this["iconParentNode"+n]=_6.create("div",{className:"mblAccordionIconParent mblAccordionIconParent"+n},this.anchorNode,"first");
}
this["iconNode"+n]=_b.setIcon(_13,this["iconPos"+n],this["iconNode"+n],this.alt,this["iconParentNode"+n]);
this["icon"+n]=_13;
_5.toggle(this.domNode,"mblAccordionHasIcon",_13&&_13!=="none");
if(_4("dojo-bidi")&&!this.getParent().isLeftToRight()){
this.getParent()._setIconDir(this["iconParentNode"+n]);
}
},_setIcon1Attr:function(_14){
this._setIcon(_14,1);
},_setIcon2Attr:function(_15){
this._setIcon(_15,2);
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
},_setSelectedAttr:function(_16){
_5.toggle(this.domNode,"mblAccordionTitleSelected",_16);
this._set("selected",_16);
}});
var _17=_2(_4("dojo-bidi")?"dojox.mobile.NonBidiAccordion":"dojox.mobile.Accordion",[_a,_9,_8],{iconBase:"",iconPos:"",fixedHeight:false,singleOpen:false,animation:true,roundRect:false,duration:0.3,baseClass:"mblAccordion",_openSpace:1,buildRendering:function(){
this.inherited(arguments);
_7.set(this.domNode,"role","tablist");
_7.set(this.domNode,"aria-multiselectable",!this.singleOpen);
},startup:function(){
if(this._started){
return;
}
if(_5.contains(this.domNode,"mblAccordionRoundRect")){
this.roundRect=true;
}else{
if(this.roundRect){
_5.add(this.domNode,"mblAccordionRoundRect");
}
}
if(this.fixedHeight){
this.singleOpen=true;
}
var _18=this.getChildren();
_1.forEach(_18,this._setupChild,this);
var sel;
var _19=1;
_1.forEach(_18,function(_1a){
_1a.startup();
_1a._at.startup();
this.collapse(_1a,true);
_7.set(_1a._at.textBoxNode,"aria-setsize",_18.length);
_7.set(_1a._at.textBoxNode,"aria-posinset",_19++);
if(_1a.selected){
sel=_1a;
}
},this);
if(!sel&&this.fixedHeight){
sel=_18[_18.length-1];
}
if(sel){
this.expand(sel,true);
}else{
this._updateLast();
}
this.defer(function(){
this.resize();
});
this._started=true;
},_setupChild:function(_1b){
if(_1b.domNode.style.overflow!="hidden"){
_1b.domNode.style.overflow=this.fixedHeight?"auto":"hidden";
}
_1b._at=new _11({label:_1b.label,alt:_1b.alt,icon1:_1b.icon1,icon2:_1b.icon2,iconPos1:_1b.iconPos1,iconPos2:_1b.iconPos2,contentWidget:_1b});
_6.place(_1b._at.domNode,_1b.domNode,"before");
_5.add(_1b.domNode,"mblAccordionPane");
_7.set(_1b._at.textBoxNode,"aria-controls",_1b.domNode.id);
_7.set(_1b.domNode,"role","tabpanel");
_7.set(_1b.domNode,"aria-labelledby",_1b._at.id);
},addChild:function(_1c,_1d){
this.inherited(arguments);
if(this._started){
this._setupChild(_1c);
_1c._at.startup();
if(_1c.selected){
this.expand(_1c,true);
this.defer(function(){
_1c.domNode.style.height="";
});
}else{
this.collapse(_1c);
}
this._addChildAriaAttrs();
}
},removeChild:function(_1e){
if(typeof _1e=="number"){
_1e=this.getChildren()[_1e];
}
if(_1e){
_1e._at.destroy();
}
this.inherited(arguments);
this._addChildAriaAttrs();
},_addChildAriaAttrs:function(){
var _1f=1;
var _20=this.getChildren();
_1.forEach(_20,function(_21){
_7.set(_21._at.textBoxNode,"aria-posinset",_1f++);
_7.set(_21._at.textBoxNode,"aria-setsize",_20.length);
});
},getChildren:function(){
return _1.filter(this.inherited(arguments),function(_22){
return !(_22 instanceof _11);
});
},getSelectedPanes:function(){
return _1.filter(this.getChildren(),function(_23){
return _23.domNode.style.display!="none";
});
},resize:function(){
if(this.fixedHeight){
var _24=_1.filter(this.getChildren(),function(_25){
return _25._at.domNode.style.display!="none";
});
var _26=this.domNode.clientHeight;
_1.forEach(_24,function(_27){
_26-=_27._at.domNode.offsetHeight;
});
this._openSpace=_26>0?_26:0;
var sel=this.getSelectedPanes()[0];
sel.domNode.style[_d.name("transition")]="";
sel.domNode.style.height=this._openSpace+"px";
}
},_updateLast:function(){
var _28=this.getChildren();
_1.forEach(_28,function(c,i){
_5.toggle(c._at.domNode,"mblAccordionTitleLast",i===_28.length-1&&!_5.contains(c._at.domNode,"mblAccordionTitleSelected"));
},this);
},expand:function(_29,_2a){
if(_29.lazy){
_c.instantiateLazyWidgets(_29.containerNode,_29.requires);
_29.lazy=false;
}
var _2b=this.getChildren();
_1.forEach(_2b,function(c,i){
c.domNode.style[_d.name("transition")]=_2a?"":"height "+this.duration+"s linear";
if(c===_29){
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
this.defer(function(){
c.domNode.style.height=h+"px";
});
this.select(_29);
}else{
if(this.singleOpen){
this.collapse(c,_2a);
}
}
},this);
this._updateLast();
_7.set(_29.domNode,"aria-expanded","true");
_7.set(_29.domNode,"aria-hidden","false");
},collapse:function(_2c,_2d){
if(_2c.domNode.style.display==="none"){
return;
}
_2c.domNode.style[_d.name("transition")]=_2d?"":"height "+this.duration+"s linear";
_2c.domNode.style.height="0px";
if(!_4("css3-animations")||_2d){
_2c.domNode.style.display="none";
this._updateLast();
}else{
var _2e=this;
_2e.defer(function(){
_2c.domNode.style.display="none";
_2e._updateLast();
if(!_2e.fixedHeight&&_2e.singleOpen){
for(var v=_2e.getParent();v;v=v.getParent()){
if(_5.contains(v.domNode,"mblView")){
if(v&&v.resize){
v.resize();
}
break;
}
}
}
},this.duration*1000);
}
this.deselect(_2c);
_7.set(_2c.domNode,"aria-expanded","false");
_7.set(_2c.domNode,"aria-hidden","true");
},select:function(_2f){
_2f._at.set("selected",true);
_7.set(_2f._at.textBoxNode,"aria-selected","true");
},deselect:function(_30){
_30._at.set("selected",false);
_7.set(_30._at.textBoxNode,"aria-selected","false");
}});
_17.ChildWidgetProperties={alt:"",label:"",icon1:"",icon2:"",iconPos1:"",iconPos2:"",selected:false,lazy:false};
_3.extend(_a,_17.ChildWidgetProperties);
return _4("dojo-bidi")?_2("dojox.mobile.Accordion",[_17,_10]):_17;
});

//>>built
define("dojox/calendar/_RendererMixin",["dojo/_base/declare","dojo/_base/lang","dojo/dom-style","dojo/dom-class","dojo/Stateful"],function(_1,_2,_3,_4,_5){
return _1("dojox.calendar._RendererMixin",_5,{item:null,owner:null,edited:false,focused:false,hovered:false,selected:false,storeState:false,moveEnabled:true,resizeEnabled:true,_orientation:"vertical",_displayValue:"block",_displayValueMap:{},visibilityLimits:{resizeStartHandle:50,resizeEndHandle:-1,summaryLabel:15,startTimeLabel:45,endTimeLabel:50},_setSelectedAttr:function(_6){
this._setState("selected",_6,"Selected");
},_setFocusedAttr:function(_7){
this._setState("focused",_7,"Focused");
},_setEditedAttr:function(_8){
this._setState("edited",_8,"Edited");
},_setHoveredAttr:function(_9){
this._setState("hovered",_9,"Hovered");
},_setStoreStateAttr:function(_a){
var _b=null;
switch(_a){
case "storing":
_b="Storing";
break;
case "unstored":
_b="Unstored";
break;
default:
_b=null;
}
var tn=this.stateNode||this.domNode;
_4.remove(tn,"Storing");
_4.remove(tn,"Unstored");
this._set("storeState",_a);
if(_b!=null){
_4.add(tn,_b);
}
},_setState:function(_c,_d,_e){
if(this[_c]!=_d){
var tn=this.stateNode||this.domNode;
_4[_d?"add":"remove"](tn,_e);
this._set(_c,_d);
}
},_setItemAttr:function(_f){
if(_f==null){
if(this.item&&this.item.cssClass){
_4.remove(this.domNode,this.item.cssClass);
}
this.item=null;
}else{
if(this.item!=null){
if(this.item.cssClass!=_f.cssClass){
if(this.item.cssClass){
_4.remove(this.domNode,this.item.cssClass);
}
}
this.item=_2.mixin(this.item,_f);
if(_f.cssClass){
_4.add(this.domNode,_f.cssClass);
}
}else{
this.item=_f;
if(_f.cssClass){
_4.add(this.domNode,_f.cssClass);
}
}
}
},_setText:function(_10,_11,_12){
if(this.owner){
this.owner._setText(_10,_11,_12);
}
},_isElementVisible:function(elt,_13,_14,_15){
var _16;
var _17=this.visibilityLimits[elt];
switch(elt){
case "moveHandle":
_16=this.moveEnabled;
break;
case "resizeStartHandle":
if(this.mobile){
_16=this.resizeEnabled&&!_13&&this.edited&&(_17==-1||_15>_17);
}else{
_16=this.resizeEnabled&&!_13&&(_17==-1||_15>_17);
}
break;
case "resizeEndHandle":
if(this.mobile){
_16=this.resizeEnabled&&!_14&&this.edited&&(_17==-1||_15>_17);
}else{
_16=this.resizeEnabled&&!_14&&(_17==-1||_15>_17);
}
break;
case "startTimeLabel":
if(this.mobile){
_16=!_13&&(!this.edited||this.edited&&(_17==-1||_15>_17));
}else{
_16=!_13&&(_17==-1||_15>_17);
}
break;
case "endTimeLabel":
_16=this.edited&&!_14&&(_17==-1||_15>_17);
break;
case "summaryLabel":
if(this.mobile){
_16=!this.edited||this.edited&&(_17==-1||_15>_17);
}else{
_16=_17==-1||_15>_17;
}
break;
}
return _16;
},_formatTime:function(rd,d){
if(this.owner){
var f=this.owner.get("formatItemTimeFunc");
if(f!=null&&typeof f==="function"){
return f(d,rd,this.owner,this.item);
}
}
return rd.dateLocaleModule.format(d,{selector:"time"});
},getDisplayValue:function(_18){
return this._displayValue;
},updateRendering:function(w,h){
h=h||this.item.h;
w=w||this.item.w;
if(!h&&!w){
return;
}
this.item.h=h;
this.item.w=w;
var _19=this._orientation=="vertical"?h:w;
var rd=this.owner.renderData;
var _1a=rd.dateModule.compare(this.item.range[0],this.item.startTime)!=0;
var _1b=rd.dateModule.compare(this.item.range[1],this.item.endTime)!=0;
var _1c;
if(this.beforeIcon!=null){
_1c=this._orientation!="horizontal"||this.isLeftToRight()?_1a:_1b;
_3.set(this.beforeIcon,"display",_1c?this.getDisplayValue("beforeIcon"):"none");
}
if(this.afterIcon!=null){
_1c=this._orientation!="horizontal"||this.isLeftToRight()?_1b:_1a;
_3.set(this.afterIcon,"display",_1c?this.getDisplayValue("afterIcon"):"none");
}
if(this.moveHandle){
_1c=this._isElementVisible("moveHandle",_1a,_1b,_19);
_3.set(this.moveHandle,"display",_1c?this.getDisplayValue("moveHandle"):"none");
}
if(this.resizeStartHandle){
_1c=this._isElementVisible("resizeStartHandle",_1a,_1b,_19);
_3.set(this.resizeStartHandle,"display",_1c?this.getDisplayValue("resizeStartHandle"):"none");
}
if(this.resizeEndHandle){
_1c=this._isElementVisible("resizeEndHandle",_1a,_1b,_19);
_3.set(this.resizeEndHandle,"display",_1c?this.getDisplayValue("resizeEndHandle"):"none");
}
if(this.startTimeLabel){
_1c=this._isElementVisible("startTimeLabel",_1a,_1b,_19);
_3.set(this.startTimeLabel,"display",_1c?this.getDisplayValue("startTimeLabel"):"none");
if(_1c){
this._setText(this.startTimeLabel,this._formatTime(rd,this.item.startTime));
}
}
if(this.endTimeLabel){
_1c=this._isElementVisible("endTimeLabel",_1a,_1b,_19);
_3.set(this.endTimeLabel,"display",_1c?this.getDisplayValue("endTimeLabel"):"none");
if(_1c){
this._setText(this.endTimeLabel,this._formatTime(rd,this.item.endTime));
}
}
if(this.summaryLabel){
_1c=this._isElementVisible("summaryLabel",_1a,_1b,_19);
_3.set(this.summaryLabel,"display",_1c?this.getDisplayValue("summaryLabel"):"none");
if(_1c){
this._setText(this.summaryLabel,this.item.summary,true);
}
}
}});
});

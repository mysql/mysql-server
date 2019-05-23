//>>built
define("dojox/calendar/_RendererMixin",["dojo/_base/declare","dojo/_base/lang","dojo/dom-style","dojo/dom-class","dojo/Stateful"],function(_1,_2,_3,_4,_5){
return _1("dojox.calendar._RendererMixin",_5,{item:null,owner:null,edited:false,focused:false,hovered:false,selected:false,moveEnabled:true,resizeEnabled:true,_orientation:"vertical",_displayValue:"block",_displayValueMap:{},visibilityLimits:{resizeStartHandle:50,resizeEndHandle:-1,summaryLabel:15,startTimeLabel:45,endTimeLabel:50},_setSelectedAttr:function(_6){
this._setState("selected",_6,"Selected");
},_setFocusedAttr:function(_7){
this._setState("focused",_7,"Focused");
},_setEditedAttr:function(_8){
this._setState("edited",_8,"Edited");
},_setHoveredAttr:function(_9){
this._setState("hovered",_9,"Hovered");
},_setState:function(_a,_b,_c){
if(this[_a]!=_b){
var tn=this.stateNode||this.domNode;
_4[_b?"add":"remove"](tn,_c);
this._set(_a,_b);
}
},_setItemAttr:function(_d){
if(_d==null){
if(this.item&&this.item.cssClass){
_4.remove(this.domNode,this.item.cssClass);
}
this.item=null;
}else{
if(this.item!=null){
if(this.item.cssClass!=_d.cssClass){
if(this.item.cssClass){
_4.remove(this.domNode,this.item.cssClass);
}
}
this.item=_2.mixin(this.item,_d);
if(_d.cssClass){
_4.add(this.domNode,_d.cssClass);
}
}else{
this.item=_d;
if(_d.cssClass){
_4.add(this.domNode,_d.cssClass);
}
}
}
},_setText:function(_e,_f,_10){
if(this.owner){
this.owner._setText(_e,_f,_10);
}
},_isElementVisible:function(elt,_11,_12,_13){
var _14=true;
var _15=this.visibilityLimits[elt];
switch(elt){
case "moveHandle":
_14=this.moveEnabled;
break;
case "resizeStartHandle":
if(this.mobile){
_14=this.resizeEnabled&&!_11&&this.edited&&(_15==-1||_13>_15);
}else{
_14=this.resizeEnabled&&!_11&&(_15==-1||_13>_15);
}
break;
case "resizeEndHandle":
if(this.mobile){
_14=this.resizeEnabled&&!_12&&this.edited&&(_15==-1||_13>_15);
}else{
_14=this.resizeEnabled&&!_12&&(_15==-1||_13>_15);
}
break;
case "startTimeLabel":
if(this.mobile){
_14=!_11&&(!this.edited||this.edited&&(_15==-1||_13>_15));
}else{
_14=!_11&&(_15==-1||_13>_15);
}
break;
case "endTimeLabel":
_14=this.edited&&!_12&&(_15==-1||_13>_15);
break;
case "summaryLabel":
if(this.mobile){
_14=!this.edited||this.edited&&(_15==-1||_13>_15);
}else{
_14=_15==-1||_13>_15;
}
break;
}
return _14;
},_formatTime:function(rd,d){
if(this.owner){
var f=this.owner.get("formatItemTimeFunc");
if(f!=null){
return this.owner.formatItemTimeFunc(d,rd);
}
}
return rd.dateLocaleModule.format(d,{selector:"time"});
},getDisplayValue:function(_16){
return this._displayValue;
},updateRendering:function(w,h){
h=h||this.item.h;
w=w||this.item.w;
if(!h&&!w){
return;
}
this.item.h=h;
this.item.w=w;
var _17=this._orientation=="vertical"?h:w;
var rd=this.owner.renderData;
var _18=rd.dateModule.compare(this.item.range[0],this.item.startTime)!=0;
var _19=rd.dateModule.compare(this.item.range[1],this.item.endTime)!=0;
var _1a;
if(this.beforeIcon!=null){
_1a=this._orientation!="horizontal"||this.isLeftToRight()?_18:_19;
_3.set(this.beforeIcon,"display",_1a?this.getDisplayValue("beforeIcon"):"none");
}
if(this.afterIcon!=null){
_1a=this._orientation!="horizontal"||this.isLeftToRight()?_19:_18;
_3.set(this.afterIcon,"display",_1a?this.getDisplayValue("afterIcon"):"none");
}
if(this.moveHandle){
_1a=this._isElementVisible("moveHandle",_18,_19,_17);
_3.set(this.moveHandle,"display",_1a?this.getDisplayValue("moveHandle"):"none");
}
if(this.resizeStartHandle){
_1a=this._isElementVisible("resizeStartHandle",_18,_19,_17);
_3.set(this.resizeStartHandle,"display",_1a?this.getDisplayValue("resizeStartHandle"):"none");
}
if(this.resizeEndHandle){
_1a=this._isElementVisible("resizeEndHandle",_18,_19,_17);
_3.set(this.resizeEndHandle,"display",_1a?this.getDisplayValue("resizeEndHandle"):"none");
}
if(this.startTimeLabel){
_1a=this._isElementVisible("startTimeLabel",_18,_19,_17);
_3.set(this.startTimeLabel,"display",_1a?this.getDisplayValue("startTimeLabel"):"none");
if(_1a){
this._setText(this.startTimeLabel,this._formatTime(rd,this.item.startTime));
}
}
if(this.endTimeLabel){
_1a=this._isElementVisible("endTimeLabel",_18,_19,_17);
_3.set(this.endTimeLabel,"display",_1a?this.getDisplayValue("endTimeLabel"):"none");
if(_1a){
this._setText(this.endTimeLabel,this._formatTime(rd,this.item.endTime));
}
}
if(this.summaryLabel){
_1a=this._isElementVisible("summaryLabel",_18,_19,_17);
_3.set(this.summaryLabel,"display",_1a?this.getDisplayValue("summaryLabel"):"none");
if(_1a){
this._setText(this.summaryLabel,this.item.summary,true);
}
}
}});
});

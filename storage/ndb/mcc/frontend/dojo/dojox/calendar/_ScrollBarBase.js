//>>built
define("dojox/calendar/_ScrollBarBase",["dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/on","dojo/dom-style","dojo/sniff","dijit/_WidgetBase","dojox/html/metrics"],function(_1,_2,_3,on,_4,_5,_6,_7){
return _1("dojox.calendar._ScrollBarBase",_6,{value:0,minimum:0,maximum:100,direction:"vertical",_vertical:true,_scrollHandle:null,containerSize:0,buildRendering:function(){
this.inherited(arguments);
this.own(on(this.domNode,"scroll",_3.hitch(this,function(_8){
this.value=this._getDomScrollerValue();
this.onChange(this.value);
this.onScroll(this.value);
})));
},_getDomScrollerValue:function(){
if(this._vertical){
return this.domNode.scrollTop;
}
var _9=!this.isLeftToRight();
if(_9){
if(_5("webkit")||_5("ie")==7){
if(this._scW==undefined){
this._scW=_7.getScrollbar().w;
}
return this.maximum-this.domNode.scrollLeft-this.containerSize+this._scW;
}
if(_5("mozilla")){
return -this.domNode.scrollLeft;
}
}
return this.domNode.scrollLeft;
},_setDomScrollerValue:function(_a){
this.domNode[this._vertical?"scrollTop":"scrollLeft"]=_a;
},_setValueAttr:function(_b){
_b=Math.min(this.maximum,_b);
_b=Math.max(this.minimum,_b);
if(this.value!=_b){
this.value=_b;
this.onChange(_b);
this._setDomScrollerValue(_b);
}
},onChange:function(_c){
},onScroll:function(_d){
},_setMinimumAttr:function(_e){
_e=Math.min(_e,this.maximum);
this.minimum=_e;
},_setMaximumAttr:function(_f){
_f=Math.max(_f,this.minimum);
this.maximum=_f;
_4.set(this.content,this._vertical?"height":"width",_f+"px");
},_setDirectionAttr:function(_10){
if(_10=="vertical"){
_10="vertical";
this._vertical=true;
}else{
_10="horizontal";
this._vertical=false;
}
this._set("direction",_10);
}});
});

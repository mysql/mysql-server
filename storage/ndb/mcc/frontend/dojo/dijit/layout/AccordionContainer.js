//>>built
require({cache:{"url:dijit/layout/templates/AccordionButton.html":"<div data-dojo-attach-event='onclick:_onTitleClick' class='dijitAccordionTitle' role=\"presentation\">\n\t<div data-dojo-attach-point='titleNode,focusNode' data-dojo-attach-event='onkeypress:_onTitleKeyPress'\n\t\t\tclass='dijitAccordionTitleFocus' role=\"tab\" aria-expanded=\"false\"\n\t\t><span class='dijitInline dijitAccordionArrow' role=\"presentation\"></span\n\t\t><span class='arrowTextUp' role=\"presentation\">+</span\n\t\t><span class='arrowTextDown' role=\"presentation\">-</span\n\t\t><img src=\"${_blankGif}\" alt=\"\" class=\"dijitIcon\" data-dojo-attach-point='iconNode' style=\"vertical-align: middle\" role=\"presentation\"/>\n\t\t<span role=\"presentation\" data-dojo-attach-point='titleTextNode' class='dijitAccordionText'></span>\n\t</div>\n</div>\n"}});
define("dijit/layout/AccordionContainer",["require","dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/fx","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/_base/sniff","dojo/topic","../focus","../_base/manager","dojo/ready","../_Widget","../_Container","../_TemplatedMixin","../_CssStateMixin","./StackContainer","./ContentPane","dojo/text!./templates/AccordionButton.html"],function(_1,_2,_3,_4,fx,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18){
var _19=_3("dijit.layout._AccordionButton",[_12,_14,_15],{templateString:_18,label:"",_setLabelAttr:{node:"titleTextNode",type:"innerHTML"},title:"",_setTitleAttr:{node:"titleTextNode",type:"attribute",attribute:"title"},iconClassAttr:"",_setIconClassAttr:{node:"iconNode",type:"class"},baseClass:"dijitAccordionTitle",getParent:function(){
return this.parent;
},buildRendering:function(){
this.inherited(arguments);
var _1a=this.id.replace(" ","_");
_6.set(this.titleTextNode,"id",_1a+"_title");
this.focusNode.setAttribute("aria-labelledby",_6.get(this.titleTextNode,"id"));
_5.setSelectable(this.domNode,false);
},getTitleHeight:function(){
return _9.getMarginSize(this.domNode).h;
},_onTitleClick:function(){
var _1b=this.getParent();
_1b.selectChild(this.contentWidget,true);
_f.focus(this.focusNode);
},_onTitleKeyPress:function(evt){
return this.getParent()._onKeyPress(evt,this.contentWidget);
},_setSelectedAttr:function(_1c){
this._set("selected",_1c);
this.focusNode.setAttribute("aria-expanded",_1c);
this.focusNode.setAttribute("aria-selected",_1c);
this.focusNode.setAttribute("tabIndex",_1c?"0":"-1");
}});
var _1d=_3("dijit.layout._AccordionInnerContainer",[_12,_15],{baseClass:"dijitAccordionInnerContainer",isLayoutContainer:true,buildRendering:function(){
this.domNode=_8.place("<div class='"+this.baseClass+"' role='presentation'>",this.contentWidget.domNode,"after");
var _1e=this.contentWidget,cls=_c.isString(this.buttonWidget)?_c.getObject(this.buttonWidget):this.buttonWidget;
this.button=_1e._buttonWidget=(new cls({contentWidget:_1e,label:_1e.title,title:_1e.tooltip,dir:_1e.dir,lang:_1e.lang,textDir:_1e.textDir,iconClass:_1e.iconClass,id:_1e.id+"_button",parent:this.parent})).placeAt(this.domNode);
this.containerNode=_8.place("<div class='dijitAccordionChildWrapper' style='display:none'>",this.domNode);
_8.place(this.contentWidget.domNode,this.containerNode);
},postCreate:function(){
this.inherited(arguments);
var _1f=this.button;
this._contentWidgetWatches=[this.contentWidget.watch("title",_c.hitch(this,function(_20,_21,_22){
_1f.set("label",_22);
})),this.contentWidget.watch("tooltip",_c.hitch(this,function(_23,_24,_25){
_1f.set("title",_25);
})),this.contentWidget.watch("iconClass",_c.hitch(this,function(_26,_27,_28){
_1f.set("iconClass",_28);
}))];
},_setSelectedAttr:function(_29){
this._set("selected",_29);
this.button.set("selected",_29);
if(_29){
var cw=this.contentWidget;
if(cw.onSelected){
cw.onSelected();
}
}
},startup:function(){
this.contentWidget.startup();
},destroy:function(){
this.button.destroyRecursive();
_2.forEach(this._contentWidgetWatches||[],function(w){
w.unwatch();
});
delete this.contentWidget._buttonWidget;
delete this.contentWidget._wrapperWidget;
this.inherited(arguments);
},destroyDescendants:function(_2a){
this.contentWidget.destroyRecursive(_2a);
}});
var _2b=_3("dijit.layout.AccordionContainer",_16,{duration:_10.defaultDuration,buttonWidget:_19,baseClass:"dijitAccordionContainer",buildRendering:function(){
this.inherited(arguments);
this.domNode.style.overflow="hidden";
this.domNode.setAttribute("role","tablist");
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(this.selectedChildWidget){
var _2c=this.selectedChildWidget.containerNode.style;
_2c.display="";
_2c.overflow="auto";
this.selectedChildWidget._wrapperWidget.set("selected",true);
}
},layout:function(){
var _2d=this.selectedChildWidget;
if(!_2d){
return;
}
var _2e=_2d._wrapperWidget.domNode,_2f=_9.getMarginExtents(_2e),_30=_9.getPadBorderExtents(_2e),_31=_2d._wrapperWidget.containerNode,_32=_9.getMarginExtents(_31),_33=_9.getPadBorderExtents(_31),_34=this._contentBox;
var _35=0;
_2.forEach(this.getChildren(),function(_36){
if(_36!=_2d){
_35+=_9.getMarginSize(_36._wrapperWidget.domNode).h;
}
});
this._verticalSpace=_34.h-_35-_2f.h-_30.h-_32.h-_33.h-_2d._buttonWidget.getTitleHeight();
this._containerContentBox={h:this._verticalSpace,w:this._contentBox.w-_2f.w-_30.w-_32.w-_33.w};
if(_2d){
_2d.resize(this._containerContentBox);
}
},_setupChild:function(_37){
_37._wrapperWidget=_1d({contentWidget:_37,buttonWidget:this.buttonWidget,id:_37.id+"_wrapper",dir:_37.dir,lang:_37.lang,textDir:_37.textDir,parent:this});
this.inherited(arguments);
},addChild:function(_38,_39){
if(this._started){
var _3a=this.containerNode;
if(_39&&typeof _39=="number"){
var _3b=_12.prototype.getChildren.call(this);
if(_3b&&_3b.length>=_39){
_3a=_3b[_39-1].domNode;
_39="after";
}
}
_8.place(_38.domNode,_3a,_39);
if(!_38._started){
_38.startup();
}
this._setupChild(_38);
_e.publish(this.id+"-addChild",_38,_39);
this.layout();
if(!this.selectedChildWidget){
this.selectChild(_38);
}
}else{
this.inherited(arguments);
}
},removeChild:function(_3c){
if(_3c._wrapperWidget){
_8.place(_3c.domNode,_3c._wrapperWidget.domNode,"after");
_3c._wrapperWidget.destroy();
delete _3c._wrapperWidget;
}
_7.remove(_3c.domNode,"dijitHidden");
this.inherited(arguments);
},getChildren:function(){
return _2.map(this.inherited(arguments),function(_3d){
return _3d.declaredClass=="dijit.layout._AccordionInnerContainer"?_3d.contentWidget:_3d;
},this);
},destroy:function(){
if(this._animation){
this._animation.stop();
}
_2.forEach(this.getChildren(),function(_3e){
if(_3e._wrapperWidget){
_3e._wrapperWidget.destroy();
}else{
_3e.destroyRecursive();
}
});
this.inherited(arguments);
},_showChild:function(_3f){
_3f._wrapperWidget.containerNode.style.display="block";
return this.inherited(arguments);
},_hideChild:function(_40){
_40._wrapperWidget.containerNode.style.display="none";
this.inherited(arguments);
},_transition:function(_41,_42,_43){
if(_d("ie")<8){
_43=false;
}
if(this._animation){
this._animation.stop(true);
delete this._animation;
}
var _44=this;
if(_41){
_41._wrapperWidget.set("selected",true);
var d=this._showChild(_41);
if(this.doLayout&&_41.resize){
_41.resize(this._containerContentBox);
}
}
if(_42){
_42._wrapperWidget.set("selected",false);
if(!_43){
this._hideChild(_42);
}
}
if(_43){
var _45=_41._wrapperWidget.containerNode,_46=_42._wrapperWidget.containerNode;
var _47=_41._wrapperWidget.containerNode,_48=_9.getMarginExtents(_47),_49=_9.getPadBorderExtents(_47),_4a=_48.h+_49.h;
_46.style.height=(_44._verticalSpace-_4a)+"px";
this._animation=new fx.Animation({node:_45,duration:this.duration,curve:[1,this._verticalSpace-_4a-1],onAnimate:function(_4b){
_4b=Math.floor(_4b);
_45.style.height=_4b+"px";
_46.style.height=(_44._verticalSpace-_4a-_4b)+"px";
},onEnd:function(){
delete _44._animation;
_45.style.height="auto";
_42._wrapperWidget.containerNode.style.display="none";
_46.style.height="auto";
_44._hideChild(_42);
}});
this._animation.onStop=this._animation.onEnd;
this._animation.play();
}
return d;
},_onKeyPress:function(e,_4c){
if(this.disabled||e.altKey||!(_4c||e.ctrlKey)){
return;
}
var c=e.charOrCode;
if((_4c&&(c==_b.LEFT_ARROW||c==_b.UP_ARROW))||(e.ctrlKey&&c==_b.PAGE_UP)){
this._adjacent(false)._buttonWidget._onTitleClick();
_4.stop(e);
}else{
if((_4c&&(c==_b.RIGHT_ARROW||c==_b.DOWN_ARROW))||(e.ctrlKey&&(c==_b.PAGE_DOWN||c==_b.TAB))){
this._adjacent(true)._buttonWidget._onTitleClick();
_4.stop(e);
}
}
}});
if(!_a.isAsync){
_11(0,function(){
var _4d=["dijit/layout/AccordionPane"];
_1(_4d);
});
}
_2b._InnerContainer=_1d;
_2b._Button=_19;
return _2b;
});

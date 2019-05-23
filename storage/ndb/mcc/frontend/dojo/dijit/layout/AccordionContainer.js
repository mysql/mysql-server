//>>built
require({cache:{"url:dijit/layout/templates/AccordionButton.html":"<div data-dojo-attach-event='onclick:_onTitleClick' class='dijitAccordionTitle' role=\"presentation\">\n\t<div data-dojo-attach-point='titleNode,focusNode' data-dojo-attach-event='onkeypress:_onTitleKeyPress'\n\t\t\tclass='dijitAccordionTitleFocus' role=\"tab\" aria-expanded=\"false\"\n\t\t><span class='dijitInline dijitAccordionArrow' role=\"presentation\"></span\n\t\t><span class='arrowTextUp' role=\"presentation\">+</span\n\t\t><span class='arrowTextDown' role=\"presentation\">-</span\n\t\t><img src=\"${_blankGif}\" alt=\"\" class=\"dijitIcon\" data-dojo-attach-point='iconNode' style=\"vertical-align: middle\" role=\"presentation\"/>\n\t\t<span role=\"presentation\" data-dojo-attach-point='titleTextNode' class='dijitAccordionText'></span>\n\t</div>\n</div>\n"}});
define("dijit/layout/AccordionContainer",["require","dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/fx","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/topic","../focus","../_base/manager","dojo/ready","../_Widget","../_Container","../_TemplatedMixin","../_CssStateMixin","./StackContainer","./ContentPane","dojo/text!./templates/AccordionButton.html"],function(_1,_2,_3,_4,fx,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17){
var _18=_3("dijit.layout._AccordionButton",[_11,_13,_14],{templateString:_17,label:"",_setLabelAttr:{node:"titleTextNode",type:"innerHTML"},title:"",_setTitleAttr:{node:"titleTextNode",type:"attribute",attribute:"title"},iconClassAttr:"",_setIconClassAttr:{node:"iconNode",type:"class"},baseClass:"dijitAccordionTitle",getParent:function(){
return this.parent;
},buildRendering:function(){
this.inherited(arguments);
var _19=this.id.replace(" ","_");
_6.set(this.titleTextNode,"id",_19+"_title");
this.focusNode.setAttribute("aria-labelledby",_6.get(this.titleTextNode,"id"));
_5.setSelectable(this.domNode,false);
},getTitleHeight:function(){
return _9.getMarginSize(this.domNode).h;
},_onTitleClick:function(){
var _1a=this.getParent();
_1a.selectChild(this.contentWidget,true);
_e.focus(this.focusNode);
},_onTitleKeyPress:function(evt){
return this.getParent()._onKeyPress(evt,this.contentWidget);
},_setSelectedAttr:function(_1b){
this._set("selected",_1b);
this.focusNode.setAttribute("aria-expanded",_1b?"true":"false");
this.focusNode.setAttribute("aria-selected",_1b?"true":"false");
this.focusNode.setAttribute("tabIndex",_1b?"0":"-1");
}});
var _1c=_3("dijit.layout._AccordionInnerContainer",[_11,_14],{baseClass:"dijitAccordionInnerContainer",isLayoutContainer:true,buildRendering:function(){
this.domNode=_8.place("<div class='"+this.baseClass+"' role='presentation'>",this.contentWidget.domNode,"after");
var _1d=this.contentWidget,cls=_b.isString(this.buttonWidget)?_b.getObject(this.buttonWidget):this.buttonWidget;
this.button=_1d._buttonWidget=(new cls({contentWidget:_1d,label:_1d.title,title:_1d.tooltip,dir:_1d.dir,lang:_1d.lang,textDir:_1d.textDir,iconClass:_1d.iconClass,id:_1d.id+"_button",parent:this.parent})).placeAt(this.domNode);
this.containerNode=_8.place("<div class='dijitAccordionChildWrapper' role='tabpanel' style='display:none'>",this.domNode);
this.containerNode.setAttribute("aria-labelledby",this.button.id);
_8.place(this.contentWidget.domNode,this.containerNode);
},postCreate:function(){
this.inherited(arguments);
var _1e=this.button;
this._contentWidgetWatches=[this.contentWidget.watch("title",_b.hitch(this,function(_1f,_20,_21){
_1e.set("label",_21);
})),this.contentWidget.watch("tooltip",_b.hitch(this,function(_22,_23,_24){
_1e.set("title",_24);
})),this.contentWidget.watch("iconClass",_b.hitch(this,function(_25,_26,_27){
_1e.set("iconClass",_27);
}))];
},_setSelectedAttr:function(_28){
this._set("selected",_28);
this.button.set("selected",_28);
if(_28){
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
},destroyDescendants:function(_29){
this.contentWidget.destroyRecursive(_29);
}});
var _2a=_3("dijit.layout.AccordionContainer",_15,{duration:_f.defaultDuration,buttonWidget:_18,baseClass:"dijitAccordionContainer",buildRendering:function(){
this.inherited(arguments);
this.domNode.style.overflow="hidden";
this.domNode.setAttribute("role","tablist");
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(this.selectedChildWidget){
this.selectedChildWidget._wrapperWidget.set("selected",true);
}
},layout:function(){
var _2b=this.selectedChildWidget;
if(!_2b){
return;
}
var _2c=_2b._wrapperWidget.domNode,_2d=_9.getMarginExtents(_2c),_2e=_9.getPadBorderExtents(_2c),_2f=_2b._wrapperWidget.containerNode,_30=_9.getMarginExtents(_2f),_31=_9.getPadBorderExtents(_2f),_32=this._contentBox;
var _33=0;
_2.forEach(this.getChildren(),function(_34){
if(_34!=_2b){
_33+=_9.getMarginSize(_34._wrapperWidget.domNode).h;
}
});
this._verticalSpace=_32.h-_33-_2d.h-_2e.h-_30.h-_31.h-_2b._buttonWidget.getTitleHeight();
this._containerContentBox={h:this._verticalSpace,w:this._contentBox.w-_2d.w-_2e.w-_30.w-_31.w};
if(_2b){
_2b.resize(this._containerContentBox);
}
},_setupChild:function(_35){
_35._wrapperWidget=_1c({contentWidget:_35,buttonWidget:this.buttonWidget,id:_35.id+"_wrapper",dir:_35.dir,lang:_35.lang,textDir:_35.textDir,parent:this});
this.inherited(arguments);
},addChild:function(_36,_37){
if(this._started){
var _38=this.containerNode;
if(_37&&typeof _37=="number"){
var _39=_11.prototype.getChildren.call(this);
if(_39&&_39.length>=_37){
_38=_39[_37-1].domNode;
_37="after";
}
}
_8.place(_36.domNode,_38,_37);
if(!_36._started){
_36.startup();
}
this._setupChild(_36);
_d.publish(this.id+"-addChild",_36,_37);
this.layout();
if(!this.selectedChildWidget){
this.selectChild(_36);
}
}else{
this.inherited(arguments);
}
},removeChild:function(_3a){
if(_3a._wrapperWidget){
_8.place(_3a.domNode,_3a._wrapperWidget.domNode,"after");
_3a._wrapperWidget.destroy();
delete _3a._wrapperWidget;
}
_7.remove(_3a.domNode,"dijitHidden");
this.inherited(arguments);
},getChildren:function(){
return _2.map(this.inherited(arguments),function(_3b){
return _3b.declaredClass=="dijit.layout._AccordionInnerContainer"?_3b.contentWidget:_3b;
},this);
},destroy:function(){
if(this._animation){
this._animation.stop();
}
_2.forEach(this.getChildren(),function(_3c){
if(_3c._wrapperWidget){
_3c._wrapperWidget.destroy();
}else{
_3c.destroyRecursive();
}
});
this.inherited(arguments);
},_showChild:function(_3d){
_3d._wrapperWidget.containerNode.style.display="block";
return this.inherited(arguments);
},_hideChild:function(_3e){
_3e._wrapperWidget.containerNode.style.display="none";
this.inherited(arguments);
},_transition:function(_3f,_40,_41){
if(_c("ie")<8){
_41=false;
}
if(this._animation){
this._animation.stop(true);
delete this._animation;
}
var _42=this;
if(_3f){
_3f._wrapperWidget.set("selected",true);
var d=this._showChild(_3f);
if(this.doLayout&&_3f.resize){
_3f.resize(this._containerContentBox);
}
}
if(_40){
_40._wrapperWidget.set("selected",false);
if(!_41){
this._hideChild(_40);
}
}
if(_41){
var _43=_3f._wrapperWidget.containerNode,_44=_40._wrapperWidget.containerNode;
var _45=_3f._wrapperWidget.containerNode,_46=_9.getMarginExtents(_45),_47=_9.getPadBorderExtents(_45),_48=_46.h+_47.h;
_44.style.height=(_42._verticalSpace-_48)+"px";
this._animation=new fx.Animation({node:_43,duration:this.duration,curve:[1,this._verticalSpace-_48-1],onAnimate:function(_49){
_49=Math.floor(_49);
_43.style.height=_49+"px";
_44.style.height=(_42._verticalSpace-_48-_49)+"px";
},onEnd:function(){
delete _42._animation;
_43.style.height="auto";
_40._wrapperWidget.containerNode.style.display="none";
_44.style.height="auto";
_42._hideChild(_40);
}});
this._animation.onStop=this._animation.onEnd;
this._animation.play();
}
return d;
},_onKeyPress:function(e,_4a){
if(this.disabled||e.altKey||!(_4a||e.ctrlKey)){
return;
}
var c=e.charOrCode;
if((_4a&&(c==_a.LEFT_ARROW||c==_a.UP_ARROW))||(e.ctrlKey&&c==_a.PAGE_UP)){
this._adjacent(false)._buttonWidget._onTitleClick();
_4.stop(e);
}else{
if((_4a&&(c==_a.RIGHT_ARROW||c==_a.DOWN_ARROW))||(e.ctrlKey&&(c==_a.PAGE_DOWN||c==_a.TAB))){
this._adjacent(true)._buttonWidget._onTitleClick();
_4.stop(e);
}
}
}});
if(_c("dijit-legacy-requires")){
_10(0,function(){
var _4b=["dijit/layout/AccordionPane"];
_1(_4b);
});
}
_2a._InnerContainer=_1c;
_2a._Button=_18;
return _2a;
});

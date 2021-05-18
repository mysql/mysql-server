//>>built
require({cache:{"url:dijit/layout/templates/AccordionButton.html":"<div data-dojo-attach-event='ondijitclick:_onTitleClick' class='dijitAccordionTitle' role=\"presentation\">\n\t<div data-dojo-attach-point='titleNode,focusNode' data-dojo-attach-event='onkeydown:_onTitleKeyDown'\n\t\t\tclass='dijitAccordionTitleFocus' role=\"tab\" aria-expanded=\"false\"\n\t\t><span class='dijitInline dijitAccordionArrow' role=\"presentation\"></span\n\t\t><span class='arrowTextUp' role=\"presentation\">+</span\n\t\t><span class='arrowTextDown' role=\"presentation\">-</span\n\t\t><span role=\"presentation\" class=\"dijitInline dijitIcon\" data-dojo-attach-point=\"iconNode\"></span>\n\t\t<span role=\"presentation\" data-dojo-attach-point='titleTextNode, textDirNode' class='dijitAccordionText'></span>\n\t</div>\n</div>\n"}});
define("dijit/layout/AccordionContainer",["require","dojo/_base/array","dojo/_base/declare","dojo/_base/fx","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/topic","../focus","../_base/manager","dojo/ready","../_Widget","../_Container","../_TemplatedMixin","../_CssStateMixin","./StackContainer","./ContentPane","dojo/text!./templates/AccordionButton.html","../a11yclick"],function(_1,_2,_3,fx,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16){
function _17(_18,dim){
_18.resize?_18.resize(dim):_8.setMarginBox(_18.domNode,dim);
};
var _19=_3("dijit.layout._AccordionButton",[_10,_12,_13],{templateString:_16,label:"",_setLabelAttr:{node:"titleTextNode",type:"innerHTML"},title:"",_setTitleAttr:{node:"titleTextNode",type:"attribute",attribute:"title"},iconClassAttr:"",_setIconClassAttr:{node:"iconNode",type:"class"},baseClass:"dijitAccordionTitle",getParent:function(){
return this.parent;
},buildRendering:function(){
this.inherited(arguments);
var _1a=this.id.replace(" ","_");
_5.set(this.titleTextNode,"id",_1a+"_title");
this.focusNode.setAttribute("aria-labelledby",_5.get(this.titleTextNode,"id"));
_4.setSelectable(this.domNode,false);
},getTitleHeight:function(){
return _8.getMarginSize(this.domNode).h;
},_onTitleClick:function(){
var _1b=this.getParent();
_1b.selectChild(this.contentWidget,true);
_d.focus(this.focusNode);
},_onTitleKeyDown:function(evt){
return this.getParent()._onKeyDown(evt,this.contentWidget);
},_setSelectedAttr:function(_1c){
this._set("selected",_1c);
this.focusNode.setAttribute("aria-expanded",_1c?"true":"false");
this.focusNode.setAttribute("aria-selected",_1c?"true":"false");
this.focusNode.setAttribute("tabIndex",_1c?"0":"-1");
}});
if(_b("dojo-bidi")){
_19.extend({_setLabelAttr:function(_1d){
this._set("label",_1d);
_5.set(this.titleTextNode,"innerHTML",_1d);
this.applyTextDir(this.titleTextNode);
},_setTitleAttr:function(_1e){
this._set("title",_1e);
_5.set(this.titleTextNode,"title",_1e);
this.applyTextDir(this.titleTextNode);
}});
}
var _1f=_3("dijit.layout._AccordionInnerContainer"+(_b("dojo-bidi")?"_NoBidi":""),[_10,_13],{baseClass:"dijitAccordionInnerContainer",isLayoutContainer:true,buildRendering:function(){
this.domNode=_7.place("<div class='"+this.baseClass+"' role='presentation'>",this.contentWidget.domNode,"after");
var _20=this.contentWidget,cls=_a.isString(this.buttonWidget)?_a.getObject(this.buttonWidget):this.buttonWidget;
this.button=_20._buttonWidget=(new cls({contentWidget:_20,label:_20.title,title:_20.tooltip,dir:_20.dir,lang:_20.lang,textDir:_20.textDir||this.textDir,iconClass:_20.iconClass,id:_20.id+"_button",parent:this.parent})).placeAt(this.domNode);
this.containerNode=_7.place("<div class='dijitAccordionChildWrapper' role='tabpanel' style='display:none'>",this.domNode);
this.containerNode.setAttribute("aria-labelledby",this.button.id);
_7.place(this.contentWidget.domNode,this.containerNode);
},postCreate:function(){
this.inherited(arguments);
var _21=this.button,cw=this.contentWidget;
this._contentWidgetWatches=[cw.watch("title",_a.hitch(this,function(_22,_23,_24){
_21.set("label",_24);
})),cw.watch("tooltip",_a.hitch(this,function(_25,_26,_27){
_21.set("title",_27);
})),cw.watch("iconClass",_a.hitch(this,function(_28,_29,_2a){
_21.set("iconClass",_2a);
}))];
},_setSelectedAttr:function(_2b){
this._set("selected",_2b);
this.button.set("selected",_2b);
if(_2b){
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
},destroyDescendants:function(_2c){
this.contentWidget.destroyRecursive(_2c);
}});
if(_b("dojo-bidi")){
_1f=_3("dijit.layout._AccordionInnerContainer",_1f,{postCreate:function(){
this.inherited(arguments);
var _2d=this.button;
this._contentWidgetWatches.push(this.contentWidget.watch("textDir",function(_2e,_2f,_30){
_2d.set("textDir",_30);
}));
}});
}
var _31=_3("dijit.layout.AccordionContainer",_14,{duration:_e.defaultDuration,buttonWidget:_19,baseClass:"dijitAccordionContainer",buildRendering:function(){
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
var _32=this.selectedChildWidget;
if(!_32){
return;
}
var _33=_32._wrapperWidget.domNode,_34=_8.getMarginExtents(_33),_35=_8.getPadBorderExtents(_33),_36=_32._wrapperWidget.containerNode,_37=_8.getMarginExtents(_36),_38=_8.getPadBorderExtents(_36),_39=this._contentBox;
var _3a=0;
_2.forEach(this.getChildren(),function(_3b){
if(_3b!=_32){
_3a+=_8.getMarginSize(_3b._wrapperWidget.domNode).h;
}
});
this._verticalSpace=_39.h-_3a-_34.h-_35.h-_37.h-_38.h-_32._buttonWidget.getTitleHeight();
this._containerContentBox={h:this._verticalSpace,w:this._contentBox.w-_34.w-_35.w-_37.w-_38.w};
if(_32){
_17(_32,this._containerContentBox);
}
},_setupChild:function(_3c){
_3c._wrapperWidget=_1f({contentWidget:_3c,buttonWidget:this.buttonWidget,id:_3c.id+"_wrapper",dir:_3c.dir,lang:_3c.lang,textDir:_3c.textDir||this.textDir,parent:this});
this.inherited(arguments);
_7.place(_3c.domNode,_3c._wrapper,"replace");
},removeChild:function(_3d){
if(_3d._wrapperWidget){
_7.place(_3d.domNode,_3d._wrapperWidget.domNode,"after");
_3d._wrapperWidget.destroy();
delete _3d._wrapperWidget;
}
_6.remove(_3d.domNode,"dijitHidden");
this.inherited(arguments);
},getChildren:function(){
return _2.map(this.inherited(arguments),function(_3e){
return _3e.declaredClass=="dijit.layout._AccordionInnerContainer"?_3e.contentWidget:_3e;
},this);
},destroy:function(){
if(this._animation){
this._animation.stop();
}
_2.forEach(this.getChildren(),function(_3f){
if(_3f._wrapperWidget){
_3f._wrapperWidget.destroy();
}else{
_3f.destroyRecursive();
}
});
this.inherited(arguments);
},_showChild:function(_40){
_40._wrapperWidget.containerNode.style.display="block";
return this.inherited(arguments);
},_hideChild:function(_41){
_41._wrapperWidget.containerNode.style.display="none";
this.inherited(arguments);
},_transition:function(_42,_43,_44){
if(_b("ie")<8){
_44=false;
}
if(this._animation){
this._animation.stop(true);
delete this._animation;
}
var _45=this;
if(_42){
_42._wrapperWidget.set("selected",true);
var d=this._showChild(_42);
if(this.doLayout){
_17(_42,this._containerContentBox);
}
}
if(_43){
_43._wrapperWidget.set("selected",false);
if(!_44){
this._hideChild(_43);
}
}
if(_44){
var _46=_42._wrapperWidget.containerNode,_47=_43._wrapperWidget.containerNode;
var _48=_42._wrapperWidget.containerNode,_49=_8.getMarginExtents(_48),_4a=_8.getPadBorderExtents(_48),_4b=_49.h+_4a.h;
_47.style.height=(_45._verticalSpace-_4b)+"px";
this._animation=new fx.Animation({node:_46,duration:this.duration,curve:[1,this._verticalSpace-_4b-1],onAnimate:function(_4c){
_4c=Math.floor(_4c);
_46.style.height=_4c+"px";
_47.style.height=(_45._verticalSpace-_4b-_4c)+"px";
},onEnd:function(){
delete _45._animation;
_46.style.height="auto";
_43._wrapperWidget.containerNode.style.display="none";
_47.style.height="auto";
_45._hideChild(_43);
}});
this._animation.onStop=this._animation.onEnd;
this._animation.play();
}
return d;
},_onKeyDown:function(e,_4d){
if(this.disabled||e.altKey||!(_4d||e.ctrlKey)){
return;
}
var c=e.keyCode;
if((_4d&&(c==_9.LEFT_ARROW||c==_9.UP_ARROW))||(e.ctrlKey&&c==_9.PAGE_UP)){
this._adjacent(false)._buttonWidget._onTitleClick();
e.stopPropagation();
e.preventDefault();
}else{
if((_4d&&(c==_9.RIGHT_ARROW||c==_9.DOWN_ARROW))||(e.ctrlKey&&(c==_9.PAGE_DOWN||c==_9.TAB))){
this._adjacent(true)._buttonWidget._onTitleClick();
e.stopPropagation();
e.preventDefault();
}
}
}});
if(_b("dijit-legacy-requires")){
_f(0,function(){
var _4e=["dijit/layout/AccordionPane"];
_1(_4e);
});
}
_31._InnerContainer=_1f;
_31._Button=_19;
return _31;
});

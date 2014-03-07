//>>built
define("dijit/_editor/plugins/FullScreen",["dojo/aspect","dojo/_base/declare","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/i18n","dojo/keys","dojo/_base/lang","dojo/on","dojo/_base/sniff","dojo/_base/window","dojo/window","../../focus","../_Plugin","../../form/ToggleButton","../../registry","dojo/i18n!../nls/commands"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,on,_a,_b,_c,_d,_e,_f,_10){
var _11=_2("dijit._editor.plugins.FullScreen",_e,{zIndex:500,_origState:null,_origiFrameState:null,_resizeHandle:null,isFullscreen:false,toggle:function(){
this.button.set("checked",!this.button.get("checked"));
},_initButton:function(){
var _12=_7.getLocalization("dijit._editor","commands"),_13=this.editor;
this.button=new _f({label:_12["fullScreen"],dir:_13.dir,lang:_13.lang,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"FullScreen",tabIndex:"-1",onChange:_9.hitch(this,"_setFullScreen")});
},setEditor:function(_14){
this.editor=_14;
this._initButton();
this.editor.addKeyHandler(_8.F11,true,true,_9.hitch(this,function(e){
this.toggle();
_6.stop(e);
setTimeout(_9.hitch(this,function(){
this.editor.focus();
}),250);
return true;
}));
this.connect(this.editor.domNode,"onkeydown","_containFocus");
},_containFocus:function(e){
if(this.isFullscreen){
var ed=this.editor;
if(!ed.isTabIndent&&ed._fullscreen_oldOnKeyDown&&e.keyCode===_8.TAB){
var f=_d.curNode;
var avn=this._getAltViewNode();
if(f==ed.iframe||(avn&&f===avn)){
setTimeout(_9.hitch(this,function(){
ed.toolbar.focus();
}),10);
}else{
if(avn&&_5.get(ed.iframe,"display")==="none"){
setTimeout(_9.hitch(this,function(){
_d.focus(avn);
}),10);
}else{
setTimeout(_9.hitch(this,function(){
ed.focus();
}),10);
}
}
_6.stop(e);
}else{
if(ed._fullscreen_oldOnKeyDown){
ed._fullscreen_oldOnKeyDown(e);
}
}
}
},_resizeEditor:function(){
var vp=_c.getBox();
_4.setMarginBox(this.editor.domNode,{w:vp.w,h:vp.h});
var _15=this.editor.getHeaderHeight();
var _16=this.editor.getFooterHeight();
var _17=_4.getPadBorderExtents(this.editor.domNode);
var _18=_4.getPadBorderExtents(this.editor.iframe.parentNode);
var _19=_4.getMarginExtents(this.editor.iframe.parentNode);
var _1a=vp.h-(_15+_17.h+_16);
_4.setMarginBox(this.editor.iframe.parentNode,{h:_1a,w:vp.w});
_4.setMarginBox(this.editor.iframe,{h:_1a-(_18.h+_19.h)});
},_getAltViewNode:function(){
},_setFullScreen:function(_1b){
var vp=_c.getBox();
var ed=this.editor;
var _1c=_b.body();
var _1d=ed.domNode.parentNode;
this.isFullscreen=_1b;
if(_1b){
while(_1d&&_1d!==_b.body()){
_3.add(_1d,"dijitForceStatic");
_1d=_1d.parentNode;
}
this._editorResizeHolder=this.editor.resize;
ed.resize=function(){
};
ed._fullscreen_oldOnKeyDown=ed.onKeyDown;
ed.onKeyDown=_9.hitch(this,this._containFocus);
this._origState={};
this._origiFrameState={};
var _1e=ed.domNode,_1f=_1e&&_1e.style||{};
this._origState={width:_1f.width||"",height:_1f.height||"",top:_5.get(_1e,"top")||"",left:_5.get(_1e,"left")||"",position:_5.get(_1e,"position")||"static",marginBox:_4.getMarginBox(ed.domNode)};
var _20=ed.iframe,_21=_20&&_20.style||{};
var bc=_5.get(ed.iframe,"backgroundColor");
this._origiFrameState={backgroundColor:bc||"transparent",width:_21.width||"auto",height:_21.height||"auto",zIndex:_21.zIndex||""};
_5.set(ed.domNode,{position:"absolute",top:"0px",left:"0px",zIndex:this.zIndex,width:vp.w+"px",height:vp.h+"px"});
_5.set(ed.iframe,{height:"100%",width:"100%",zIndex:this.zIndex,backgroundColor:bc!=="transparent"&&bc!=="rgba(0, 0, 0, 0)"?bc:"white"});
_5.set(ed.iframe.parentNode,{height:"95%",width:"100%"});
if(_1c.style&&_1c.style.overflow){
this._oldOverflow=_5.get(_1c,"overflow");
}else{
this._oldOverflow="";
}
if(_a("ie")&&!_a("quirks")){
if(_1c.parentNode&&_1c.parentNode.style&&_1c.parentNode.style.overflow){
this._oldBodyParentOverflow=_1c.parentNode.style.overflow;
}else{
try{
this._oldBodyParentOverflow=_5.get(_1c.parentNode,"overflow");
}
catch(e){
this._oldBodyParentOverflow="scroll";
}
}
_5.set(_1c.parentNode,"overflow","hidden");
}
_5.set(_1c,"overflow","hidden");
var _22=function(){
var vp=_c.getBox();
if("_prevW" in this&&"_prevH" in this){
if(vp.w===this._prevW&&vp.h===this._prevH){
return;
}
}else{
this._prevW=vp.w;
this._prevH=vp.h;
}
if(this._resizer){
clearTimeout(this._resizer);
delete this._resizer;
}
this._resizer=setTimeout(_9.hitch(this,function(){
delete this._resizer;
this._resizeEditor();
}),10);
};
this._resizeHandle=on(window,"resize",_9.hitch(this,_22));
this._resizeHandle2=_1.after(ed,"onResize",_9.hitch(this,function(){
if(this._resizer){
clearTimeout(this._resizer);
delete this._resizer;
}
this._resizer=setTimeout(_9.hitch(this,function(){
delete this._resizer;
this._resizeEditor();
}),10);
}));
this._resizeEditor();
var dn=this.editor.toolbar.domNode;
setTimeout(function(){
_c.scrollIntoView(dn);
},250);
}else{
if(this._resizeHandle){
this._resizeHandle.remove();
this._resizeHandle=null;
}
if(this._resizeHandle2){
this._resizeHandle2.remove();
this._resizeHandle2=null;
}
if(this._rst){
clearTimeout(this._rst);
this._rst=null;
}
while(_1d&&_1d!==_b.body()){
_3.remove(_1d,"dijitForceStatic");
_1d=_1d.parentNode;
}
if(this._editorResizeHolder){
this.editor.resize=this._editorResizeHolder;
}
if(!this._origState&&!this._origiFrameState){
return;
}
if(ed._fullscreen_oldOnKeyDown){
ed.onKeyDown=ed._fullscreen_oldOnKeyDown;
delete ed._fullscreen_oldOnKeyDown;
}
var _23=this;
setTimeout(function(){
var mb=_23._origState.marginBox;
var oh=_23._origState.height;
if(_a("ie")&&!_a("quirks")){
_1c.parentNode.style.overflow=_23._oldBodyParentOverflow;
delete _23._oldBodyParentOverflow;
}
_5.set(_1c,"overflow",_23._oldOverflow);
delete _23._oldOverflow;
_5.set(ed.domNode,_23._origState);
_5.set(ed.iframe.parentNode,{height:"",width:""});
_5.set(ed.iframe,_23._origiFrameState);
delete _23._origState;
delete _23._origiFrameState;
var _24=_10.getEnclosingWidget(ed.domNode.parentNode);
if(_24&&_24.resize){
_24.resize();
}else{
if(!oh||oh.indexOf("%")<0){
setTimeout(_9.hitch(this,function(){
ed.resize({h:mb.h});
}),0);
}
}
_c.scrollIntoView(_23.editor.toolbar.domNode);
},100);
}
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},destroy:function(){
if(this._resizeHandle){
this._resizeHandle.remove();
this._resizeHandle=null;
}
if(this._resizeHandle2){
this._resizeHandle2.remove();
this._resizeHandle2=null;
}
if(this._resizer){
clearTimeout(this._resizer);
this._resizer=null;
}
this.inherited(arguments);
}});
_e.registry["fullScreen"]=_e.registry["fullscreen"]=function(_25){
return new _11({zIndex:("zIndex" in _25)?_25.zIndex:500});
};
return _11;
});

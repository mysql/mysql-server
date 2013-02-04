//>>built
define("dijit/_editor/plugins/ViewSource",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/i18n","dojo/keys","dojo/_base/lang","dojo/on","dojo/_base/sniff","dojo/_base/window","dojo/window","../../focus","../_Plugin","../../form/ToggleButton","../..","../../registry","dojo/i18n!../nls/commands"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f,_10,_11,_12){
var _13=_2("dijit._editor.plugins.ViewSource",_f,{stripScripts:true,stripComments:true,stripIFrames:true,readOnly:false,_fsPlugin:null,toggle:function(){
if(_b("webkit")){
this._vsFocused=true;
}
this.button.set("checked",!this.button.get("checked"));
},_initButton:function(){
var _14=_8.getLocalization("dijit._editor","commands"),_15=this.editor;
this.button=new _10({label:_14["viewSource"],dir:_15.dir,lang:_15.lang,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"ViewSource",tabIndex:"-1",onChange:_a.hitch(this,"_showSource")});
if(_b("ie")==7){
this._ieFixNode=_4.create("div",{style:{opacity:"0",zIndex:"-1000",position:"absolute",top:"-1000px"}},_c.body());
}
this.button.set("readOnly",false);
},setEditor:function(_16){
this.editor=_16;
this._initButton();
this.editor.addKeyHandler(_9.F12,true,true,_a.hitch(this,function(e){
this.button.focus();
this.toggle();
_7.stop(e);
setTimeout(_a.hitch(this,function(){
this.editor.focus();
}),100);
}));
},_showSource:function(_17){
var ed=this.editor;
var _18=ed._plugins;
var _19;
this._sourceShown=_17;
var _1a=this;
try{
if(!this.sourceArea){
this._createSourceView();
}
if(_17){
ed._sourceQueryCommandEnabled=ed.queryCommandEnabled;
ed.queryCommandEnabled=function(cmd){
return cmd.toLowerCase()==="viewsource";
};
this.editor.onDisplayChanged();
_19=ed.get("value");
_19=this._filter(_19);
ed.set("value",_19);
_1.forEach(_18,function(p){
if(!(p instanceof _13)){
p.set("disabled",true);
}
});
if(this._fsPlugin){
this._fsPlugin._getAltViewNode=function(){
return _1a.sourceArea;
};
}
this.sourceArea.value=_19;
this.sourceArea.style.height=ed.iframe.style.height;
this.sourceArea.style.width=ed.iframe.style.width;
_6.set(ed.iframe,"display","none");
_6.set(this.sourceArea,{display:"block"});
var _1b=function(){
var vp=_d.getBox();
if("_prevW" in this&&"_prevH" in this){
if(vp.w===this._prevW&&vp.h===this._prevH){
return;
}else{
this._prevW=vp.w;
this._prevH=vp.h;
}
}else{
this._prevW=vp.w;
this._prevH=vp.h;
}
if(this._resizer){
clearTimeout(this._resizer);
delete this._resizer;
}
this._resizer=setTimeout(_a.hitch(this,function(){
delete this._resizer;
this._resize();
}),10);
};
this._resizeHandle=on(window,"resize",_a.hitch(this,_1b));
setTimeout(_a.hitch(this,this._resize),100);
this.editor.onNormalizedDisplayChanged();
this.editor.__oldGetValue=this.editor.getValue;
this.editor.getValue=_a.hitch(this,function(){
var txt=this.sourceArea.value;
txt=this._filter(txt);
return txt;
});
}else{
if(!ed._sourceQueryCommandEnabled){
return;
}
this._resizeHandle.remove();
delete this._resizeHandle;
if(this.editor.__oldGetValue){
this.editor.getValue=this.editor.__oldGetValue;
delete this.editor.__oldGetValue;
}
ed.queryCommandEnabled=ed._sourceQueryCommandEnabled;
if(!this._readOnly){
_19=this.sourceArea.value;
_19=this._filter(_19);
ed.beginEditing();
ed.set("value",_19);
ed.endEditing();
}
_1.forEach(_18,function(p){
p.set("disabled",false);
});
_6.set(this.sourceArea,"display","none");
_6.set(ed.iframe,"display","block");
delete ed._sourceQueryCommandEnabled;
this.editor.onDisplayChanged();
}
setTimeout(_a.hitch(this,function(){
var _1c=ed.domNode.parentNode;
if(_1c){
var _1d=_12.getEnclosingWidget(_1c);
if(_1d&&_1d.resize){
_1d.resize();
}
}
ed.resize();
}),300);
}
catch(e){
}
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},_resize:function(){
var ed=this.editor;
var tbH=ed.getHeaderHeight();
var fH=ed.getFooterHeight();
var eb=_5.position(ed.domNode);
var _1e=_5.getPadBorderExtents(ed.iframe.parentNode);
var _1f=_5.getMarginExtents(ed.iframe.parentNode);
var _20=_5.getPadBorderExtents(ed.domNode);
var edb={w:eb.w-_20.w,h:eb.h-(tbH+_20.h+ +fH)};
if(this._fsPlugin&&this._fsPlugin.isFullscreen){
var vp=_d.getBox();
edb.w=(vp.w-_20.w);
edb.h=(vp.h-(tbH+_20.h+fH));
}
if(_b("ie")){
edb.h-=2;
}
if(this._ieFixNode){
var _21=-this._ieFixNode.offsetTop/1000;
edb.w=Math.floor((edb.w+0.9)/_21);
edb.h=Math.floor((edb.h+0.9)/_21);
}
_5.setMarginBox(this.sourceArea,{w:edb.w-(_1e.w+_1f.w),h:edb.h-(_1e.h+_1f.h)});
_5.setMarginBox(ed.iframe.parentNode,{h:edb.h});
},_createSourceView:function(){
var ed=this.editor;
var _22=ed._plugins;
this.sourceArea=_4.create("textarea");
if(this.readOnly){
_3.set(this.sourceArea,"readOnly",true);
this._readOnly=true;
}
_6.set(this.sourceArea,{padding:"0px",margin:"0px",borderWidth:"0px",borderStyle:"none"});
_4.place(this.sourceArea,ed.iframe,"before");
if(_b("ie")&&ed.iframe.parentNode.lastChild!==ed.iframe){
_6.set(ed.iframe.parentNode.lastChild,{width:"0px",height:"0px",padding:"0px",margin:"0px",borderWidth:"0px",borderStyle:"none"});
}
ed._viewsource_oldFocus=ed.focus;
var _23=this;
ed.focus=function(){
if(_23._sourceShown){
_23.setSourceAreaCaret();
}else{
try{
if(this._vsFocused){
delete this._vsFocused;
_e.focus(ed.editNode);
}else{
ed._viewsource_oldFocus();
}
}
catch(e){
}
}
};
var i,p;
for(i=0;i<_22.length;i++){
p=_22[i];
if(p&&(p.declaredClass==="dijit._editor.plugins.FullScreen"||p.declaredClass===(_11._scopeName+"._editor.plugins.FullScreen"))){
this._fsPlugin=p;
break;
}
}
if(this._fsPlugin){
this._fsPlugin._viewsource_getAltViewNode=this._fsPlugin._getAltViewNode;
this._fsPlugin._getAltViewNode=function(){
return _23._sourceShown?_23.sourceArea:this._viewsource_getAltViewNode();
};
}
this.connect(this.sourceArea,"onkeydown",_a.hitch(this,function(e){
if(this._sourceShown&&e.keyCode==_9.F12&&e.ctrlKey&&e.shiftKey){
this.button.focus();
this.button.set("checked",false);
setTimeout(_a.hitch(this,function(){
ed.focus();
}),100);
_7.stop(e);
}
}));
},_stripScripts:function(_24){
if(_24){
_24=_24.replace(/<\s*script[^>]*>((.|\s)*?)<\\?\/\s*script\s*>/ig,"");
_24=_24.replace(/<\s*script\b([^<>]|\s)*>?/ig,"");
_24=_24.replace(/<[^>]*=(\s|)*[("|')]javascript:[^$1][(\s|.)]*[$1][^>]*>/ig,"");
}
return _24;
},_stripComments:function(_25){
if(_25){
_25=_25.replace(/<!--(.|\s){1,}?-->/g,"");
}
return _25;
},_stripIFrames:function(_26){
if(_26){
_26=_26.replace(/<\s*iframe[^>]*>((.|\s)*?)<\\?\/\s*iframe\s*>/ig,"");
}
return _26;
},_filter:function(_27){
if(_27){
if(this.stripScripts){
_27=this._stripScripts(_27);
}
if(this.stripComments){
_27=this._stripComments(_27);
}
if(this.stripIFrames){
_27=this._stripIFrames(_27);
}
}
return _27;
},setSourceAreaCaret:function(){
var _28=_c.global;
var _29=this.sourceArea;
_e.focus(_29);
if(this._sourceShown&&!this.readOnly){
if(_b("ie")){
if(this.sourceArea.createTextRange){
var _2a=_29.createTextRange();
_2a.collapse(true);
_2a.moveStart("character",-99999);
_2a.moveStart("character",0);
_2a.moveEnd("character",0);
_2a.select();
}
}else{
if(_28.getSelection){
if(_29.setSelectionRange){
_29.setSelectionRange(0,0);
}
}
}
}
},destroy:function(){
if(this._ieFixNode){
_c.body().removeChild(this._ieFixNode);
}
if(this._resizer){
clearTimeout(this._resizer);
delete this._resizer;
}
if(this._resizeHandle){
this._resizeHandle.remove();
delete this._resizeHandle;
}
this.inherited(arguments);
}});
_f.registry["viewSource"]=_f.registry["viewsource"]=function(_2b){
return new _13({readOnly:("readOnly" in _2b)?_2b.readOnly:false,stripComments:("stripComments" in _2b)?_2b.stripComments:true,stripScripts:("stripScripts" in _2b)?_2b.stripScripts:true,stripIFrames:("stripIFrames" in _2b)?_2b.stripIFrames:true});
};
return _13;
});

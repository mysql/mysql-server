//>>built
define("dijit/_editor/plugins/ViewSource",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/i18n","dojo/keys","dojo/_base/lang","dojo/on","dojo/sniff","dojo/_base/window","dojo/window","../../focus","../_Plugin","../../form/ToggleButton","../..","../../registry","dojo/aspect","dojo/i18n!../nls/commands"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f,_10,_11,_12,_13){
var _14=_2("dijit._editor.plugins.ViewSource",_f,{stripScripts:true,stripComments:true,stripIFrames:true,readOnly:false,_fsPlugin:null,toggle:function(){
if(_b("webkit")){
this._vsFocused=true;
}
this.button.set("checked",!this.button.get("checked"));
},_initButton:function(){
var _15=_8.getLocalization("dijit._editor","commands"),_16=this.editor;
this.button=new _10({label:_15["viewSource"],ownerDocument:_16.ownerDocument,dir:_16.dir,lang:_16.lang,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"ViewSource",tabIndex:"-1",onChange:_a.hitch(this,"_showSource")});
if(_b("ie")==7){
this._ieFixNode=_4.create("div",{style:{opacity:"0",zIndex:"-1000",position:"absolute",top:"-1000px"}},_16.ownerDocumentBody);
}
this.button.set("readOnly",false);
},setEditor:function(_17){
this.editor=_17;
this._initButton();
this.editor.addKeyHandler(_9.F12,true,true,_a.hitch(this,function(e){
this.button.focus();
this.toggle();
_7.stop(e);
setTimeout(_a.hitch(this,function(){
if(this.editor.focused){
this.editor.focus();
}
}),100);
}));
},_showSource:function(_18){
var ed=this.editor;
var _19=ed._plugins;
var _1a;
this._sourceShown=_18;
var _1b=this;
try{
if(!this.sourceArea){
this._createSourceView();
}
if(_18){
ed._sourceQueryCommandEnabled=ed.queryCommandEnabled;
ed.queryCommandEnabled=function(cmd){
return cmd.toLowerCase()==="viewsource";
};
this.editor.onDisplayChanged();
_1a=ed.get("value");
_1a=this._filter(_1a);
ed.set("value",_1a);
_1.forEach(_19,function(p){
if(p&&!(p instanceof _14)&&p.isInstanceOf(_f)){
p.set("disabled",true);
}
});
if(this._fsPlugin){
this._fsPlugin._getAltViewNode=function(){
return _1b.sourceArea;
};
}
this.sourceArea.value=_1a;
this.sourceArea.style.height=ed.iframe.style.height;
this.sourceArea.style.width=ed.iframe.style.width;
_6.set(ed.iframe,"display","none");
_6.set(this.sourceArea,{display:"block"});
var _1c=function(){
var vp=_d.getBox(ed.ownerDocument);
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
this._resizeHandle=on(window,"resize",_a.hitch(this,_1c));
setTimeout(_a.hitch(this,this._resize),100);
this.editor.onNormalizedDisplayChanged();
this.editor.__oldGetValue=this.editor.getValue;
this.editor.getValue=_a.hitch(this,function(){
var txt=this.sourceArea.value;
txt=this._filter(txt);
return txt;
});
this._setListener=_13.after(this.editor,"setValue",_a.hitch(this,function(_1d){
_1d=_1d||"";
_1d=this._filter(_1d);
this.sourceArea.value=_1d;
}),true);
}else{
if(!ed._sourceQueryCommandEnabled){
return;
}
this._setListener.remove();
delete this._setListener;
this._resizeHandle.remove();
delete this._resizeHandle;
if(this.editor.__oldGetValue){
this.editor.getValue=this.editor.__oldGetValue;
delete this.editor.__oldGetValue;
}
ed.queryCommandEnabled=ed._sourceQueryCommandEnabled;
if(!this._readOnly){
_1a=this.sourceArea.value;
_1a=this._filter(_1a);
ed.beginEditing();
ed.set("value",_1a);
ed.endEditing();
}
_1.forEach(_19,function(p){
if(p&&p.isInstanceOf(_f)){
p.set("disabled",false);
}
});
_6.set(this.sourceArea,"display","none");
_6.set(ed.iframe,"display","block");
delete ed._sourceQueryCommandEnabled;
this.editor.onDisplayChanged();
}
setTimeout(_a.hitch(this,function(){
var _1e=ed.domNode.parentNode;
if(_1e){
var _1f=_12.getEnclosingWidget(_1e);
if(_1f&&_1f.resize){
_1f.resize();
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
var _20=_5.getPadBorderExtents(ed.iframe.parentNode);
var _21=_5.getMarginExtents(ed.iframe.parentNode);
var _22=_5.getPadBorderExtents(ed.domNode);
var edb={w:eb.w-_22.w,h:eb.h-(tbH+_22.h+fH)};
if(this._fsPlugin&&this._fsPlugin.isFullscreen){
var vp=_d.getBox(ed.ownerDocument);
edb.w=(vp.w-_22.w);
edb.h=(vp.h-(tbH+_22.h+fH));
}
if(_b("ie")){
edb.h-=2;
}
if(this._ieFixNode){
var _23=-this._ieFixNode.offsetTop/1000;
edb.w=Math.floor((edb.w+0.9)/_23);
edb.h=Math.floor((edb.h+0.9)/_23);
}
_5.setMarginBox(this.sourceArea,{w:edb.w-(_20.w+_21.w),h:edb.h-(_20.h+_21.h)});
_5.setMarginBox(ed.iframe.parentNode,{h:edb.h});
},_createSourceView:function(){
var ed=this.editor;
var _24=ed._plugins;
this.sourceArea=_4.create("textarea");
if(this.readOnly){
_3.set(this.sourceArea,"readOnly",true);
this._readOnly=true;
}
_6.set(this.sourceArea,{padding:"0px",margin:"0px",borderWidth:"0px",borderStyle:"none"});
_3.set(this.sourceArea,"aria-label",this.editor.id);
_4.place(this.sourceArea,ed.iframe,"before");
if(_b("ie")&&ed.iframe.parentNode.lastChild!==ed.iframe){
_6.set(ed.iframe.parentNode.lastChild,{width:"0px",height:"0px",padding:"0px",margin:"0px",borderWidth:"0px",borderStyle:"none"});
}
ed._viewsource_oldFocus=ed.focus;
var _25=this;
ed.focus=function(){
if(_25._sourceShown){
_25.setSourceAreaCaret();
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
for(i=0;i<_24.length;i++){
p=_24[i];
if(p&&(p.declaredClass==="dijit._editor.plugins.FullScreen"||p.declaredClass===(_11._scopeName+"._editor.plugins.FullScreen"))){
this._fsPlugin=p;
break;
}
}
if(this._fsPlugin){
this._fsPlugin._viewsource_getAltViewNode=this._fsPlugin._getAltViewNode;
this._fsPlugin._getAltViewNode=function(){
return _25._sourceShown?_25.sourceArea:this._viewsource_getAltViewNode();
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
},_stripScripts:function(_26){
if(_26){
_26=_26.replace(/<\s*script[^>]*>((.|\s)*?)<\\?\/\s*script\s*>/ig,"");
_26=_26.replace(/<\s*script\b([^<>]|\s)*>?/ig,"");
_26=_26.replace(/<[^>]*=(\s|)*[("|')]javascript:[^$1][(\s|.)]*[$1][^>]*>/ig,"");
}
return _26;
},_stripComments:function(_27){
if(_27){
_27=_27.replace(/<!--(.|\s){1,}?-->/g,"");
}
return _27;
},_stripIFrames:function(_28){
if(_28){
_28=_28.replace(/<\s*iframe[^>]*>((.|\s)*?)<\\?\/\s*iframe\s*>/ig,"");
}
return _28;
},_filter:function(_29){
if(_29){
if(this.stripScripts){
_29=this._stripScripts(_29);
}
if(this.stripComments){
_29=this._stripComments(_29);
}
if(this.stripIFrames){
_29=this._stripIFrames(_29);
}
}
return _29;
},setSourceAreaCaret:function(){
var _2a=_c.global;
var _2b=this.sourceArea;
_e.focus(_2b);
if(this._sourceShown&&!this.readOnly){
if(_b("ie")){
if(this.sourceArea.createTextRange){
var _2c=_2b.createTextRange();
_2c.collapse(true);
_2c.moveStart("character",-99999);
_2c.moveStart("character",0);
_2c.moveEnd("character",0);
_2c.select();
}
}else{
if(_2a.getSelection){
if(_2b.setSelectionRange){
_2b.setSelectionRange(0,0);
}
}
}
}
},destroy:function(){
if(this._ieFixNode){
_4.destroy(this._ieFixNode);
}
if(this._resizer){
clearTimeout(this._resizer);
delete this._resizer;
}
if(this._resizeHandle){
this._resizeHandle.remove();
delete this._resizeHandle;
}
if(this._setListener){
this._setListener.remove();
delete this._setListener;
}
this.inherited(arguments);
}});
_f.registry["viewSource"]=_f.registry["viewsource"]=function(_2d){
return new _14({readOnly:("readOnly" in _2d)?_2d.readOnly:false,stripComments:("stripComments" in _2d)?_2d.stripComments:true,stripScripts:("stripScripts" in _2d)?_2d.stripScripts:true,stripIFrames:("stripIFrames" in _2d)?_2d.stripIFrames:true});
};
return _14;
});

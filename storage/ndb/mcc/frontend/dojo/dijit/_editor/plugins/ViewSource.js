//>>built
define("dijit/_editor/plugins/ViewSource",["dojo/_base/array","dojo/aspect","dojo/_base/declare","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/i18n","dojo/keys","dojo/_base/lang","dojo/on","dojo/sniff","dojo/window","../../focus","../_Plugin","../../form/ToggleButton","../..","../../registry","dojo/i18n!../nls/commands"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f,_10,_11){
var _12=_3("dijit._editor.plugins.ViewSource",_e,{stripScripts:true,stripComments:true,stripIFrames:true,stripEventHandlers:true,readOnly:false,_fsPlugin:null,toggle:function(){
if(_b("webkit")){
this._vsFocused=true;
}
this.button.set("checked",!this.button.get("checked"));
},_initButton:function(){
var _13=_8.getLocalization("dijit._editor","commands"),_14=this.editor;
this.button=new _f({label:_13["viewSource"],ownerDocument:_14.ownerDocument,dir:_14.dir,lang:_14.lang,showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"ViewSource",tabIndex:"-1",onChange:_a.hitch(this,"_showSource")});
this.button.set("readOnly",false);
},setEditor:function(_15){
this.editor=_15;
this._initButton();
this.removeValueFilterHandles();
this._setValueFilterHandle=_2.before(this.editor,"setValue",_a.hitch(this,function(_16){
return [this._filter(_16)];
}));
this._getValueFilterHandle=_2.after(this.editor,"getValue",_a.hitch(this,function(_17){
return this._filter(_17);
}));
this.editor.addKeyHandler(_9.F12,true,true,_a.hitch(this,function(e){
this.button.focus();
this.toggle();
e.stopPropagation();
e.preventDefault();
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
_1.forEach(_19,function(p){
if(p&&!(p instanceof _12)&&p.isInstanceOf(_e)){
p.set("disabled",true);
}
});
if(this._fsPlugin){
this._fsPlugin._getAltViewNode=function(){
return _1b.sourceArea;
};
}
this.sourceArea.value=ed.get("value");
this.sourceArea.style.height=ed.iframe.style.height;
this.sourceArea.style.width=ed.iframe.style.width;
ed.iframe.parentNode.style.position="relative";
_7.set(ed.iframe,{position:"absolute",top:0,visibility:"hidden"});
_7.set(this.sourceArea,{display:"block"});
var _1c=function(){
var vp=_c.getBox(ed.ownerDocument);
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
this._setListener=_2.after(this.editor,"setValue",_a.hitch(this,function(_1d){
_1d=_1d||"";
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
ed.beginEditing();
ed.set("value",_1a);
ed.endEditing();
}
_1.forEach(_19,function(p){
if(p&&p.isInstanceOf(_e)){
p.set("disabled",false);
}
});
_7.set(this.sourceArea,"display","none");
_7.set(ed.iframe,{position:"relative",visibility:"visible"});
delete ed._sourceQueryCommandEnabled;
this.editor.onDisplayChanged();
}
setTimeout(_a.hitch(this,function(){
var _1e=ed.domNode.parentNode;
if(_1e){
var _1f=_11.getEnclosingWidget(_1e);
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
var eb=_6.position(ed.domNode);
var _20=_6.getPadBorderExtents(ed.iframe.parentNode);
var _21=_6.getMarginExtents(ed.iframe.parentNode);
var _22=_6.getPadBorderExtents(ed.domNode);
var edb={w:eb.w-_22.w,h:eb.h-(tbH+_22.h+fH)};
if(this._fsPlugin&&this._fsPlugin.isFullscreen){
var vp=_c.getBox(ed.ownerDocument);
edb.w=(vp.w-_22.w);
edb.h=(vp.h-(tbH+_22.h+fH));
}
_6.setMarginBox(this.sourceArea,{w:Math.round(edb.w-(_20.w+_21.w)),h:Math.round(edb.h-(_20.h+_21.h))});
},_createSourceView:function(){
var ed=this.editor;
var _23=ed._plugins;
this.sourceArea=_5.create("textarea");
if(this.readOnly){
_4.set(this.sourceArea,"readOnly",true);
this._readOnly=true;
}
_7.set(this.sourceArea,{padding:"0px",margin:"0px",borderWidth:"0px",borderStyle:"none"});
_4.set(this.sourceArea,"aria-label",this.editor.id);
_5.place(this.sourceArea,ed.iframe,"before");
if(_b("ie")&&ed.iframe.parentNode.lastChild!==ed.iframe){
_7.set(ed.iframe.parentNode.lastChild,{width:"0px",height:"0px",padding:"0px",margin:"0px",borderWidth:"0px",borderStyle:"none"});
}
ed._viewsource_oldFocus=ed.focus;
var _24=this;
ed.focus=function(){
if(_24._sourceShown){
_24.setSourceAreaCaret();
}else{
try{
if(this._vsFocused){
delete this._vsFocused;
_d.focus(ed.editNode);
}else{
ed._viewsource_oldFocus();
}
}
catch(e){
}
}
};
var i,p;
for(i=0;i<_23.length;i++){
p=_23[i];
if(p&&(p.declaredClass==="dijit._editor.plugins.FullScreen"||p.declaredClass===(_10._scopeName+"._editor.plugins.FullScreen"))){
this._fsPlugin=p;
break;
}
}
if(this._fsPlugin){
this._fsPlugin._viewsource_getAltViewNode=this._fsPlugin._getAltViewNode;
this._fsPlugin._getAltViewNode=function(){
return _24._sourceShown?_24.sourceArea:this._viewsource_getAltViewNode();
};
}
this.own(on(this.sourceArea,"keydown",_a.hitch(this,function(e){
if(this._sourceShown&&e.keyCode==_9.F12&&e.ctrlKey&&e.shiftKey){
this.button.focus();
this.button.set("checked",false);
setTimeout(_a.hitch(this,function(){
ed.focus();
}),100);
e.stopPropagation();
e.preventDefault();
}
})));
},_stripScripts:function(_25){
if(_25){
_25=_25.replace(/<\s*script[^>]*>((.|\s)*?)<\\?\/\s*script\s*>/ig,"");
_25=_25.replace(/<\s*script\b([^<>]|\s)*>?/ig,"");
_25=_25.replace(/<[^>]*=(\s|)*[("|')]javascript:[^$1][(\s|.)]*[$1][^>]*>/ig,"");
}
return _25;
},_stripComments:function(_26){
if(_26){
_26=_26.replace(/<!--(.|\s){1,}?-->/g,"");
}
return _26;
},_stripIFrames:function(_27){
if(_27){
_27=_27.replace(/<\s*iframe[^>]*>((.|\s)*?)<\\?\/\s*iframe\s*>/ig,"");
}
return _27;
},_stripEventHandlers:function(_28){
if(_28){
var _29=_28.match(/<[a-z]+?\b(.*?on.*?(['"]).*?\2.*?)+>/gim);
if(_29){
for(var i=0,l=_29.length;i<l;i++){
var _2a=_29[i];
var _2b=_2a.replace(/\s+on[a-z]*\s*=\s*(['"])(.*?)\1/igm,"");
_28=_28.replace(_2a,_2b);
}
}
}
return _28;
},_filter:function(_2c){
if(_2c){
if(this.stripScripts){
_2c=this._stripScripts(_2c);
}
if(this.stripComments){
_2c=this._stripComments(_2c);
}
if(this.stripIFrames){
_2c=this._stripIFrames(_2c);
}
if(this.stripEventHandlers){
_2c=this._stripEventHandlers(_2c);
}
}
return _2c;
},removeValueFilterHandles:function(){
if(this._setValueFilterHandle){
this._setValueFilterHandle.remove();
delete this._setValueFilterHandle;
}
if(this._getValueFilterHandle){
this._getValueFilterHandle.remove();
delete this._getValueFilterHandle;
}
},setSourceAreaCaret:function(){
var _2d=this.sourceArea;
_d.focus(_2d);
if(this._sourceShown&&!this.readOnly){
if(_2d.setSelectionRange){
_2d.setSelectionRange(0,0);
}else{
if(this.sourceArea.createTextRange){
var _2e=_2d.createTextRange();
_2e.collapse(true);
_2e.moveStart("character",-99999);
_2e.moveStart("character",0);
_2e.moveEnd("character",0);
_2e.select();
}
}
}
},destroy:function(){
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
this.removeValueFilterHandles();
this.inherited(arguments);
}});
_e.registry["viewSource"]=_e.registry["viewsource"]=function(_2f){
return new _12({readOnly:("readOnly" in _2f)?_2f.readOnly:false,stripComments:("stripComments" in _2f)?_2f.stripComments:true,stripScripts:("stripScripts" in _2f)?_2f.stripScripts:true,stripIFrames:("stripIFrames" in _2f)?_2f.stripIFrames:true,stripEventHandlers:("stripEventHandlers" in _2f)?_2f.stripEventHandlers:true});
};
return _12;
});

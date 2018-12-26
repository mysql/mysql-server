//>>built
define("dojox/editor/plugins/StatusBar",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/layout/ResizeHandle"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.StatusBar");
_1.declare("dojox.editor.plugins._StatusBar",[_2._Widget,_2._TemplatedMixin],{templateString:"<div class=\"dojoxEditorStatusBar\">"+"<table><tbody><tr>"+"<td class=\"dojoxEditorStatusBarText\" tabindex=\"-1\" aria-role=\"presentation\" aria-live=\"aggressive\"><span dojoAttachPoint=\"barContent\">&nbsp;</span></td>"+"<td><span dojoAttachPoint=\"handle\"></span></td>"+"</tr></tbody><table>"+"</div>",_getValueAttr:function(){
return this.barContent.innerHTML;
},_setValueAttr:function(_4){
if(_4){
_4=_1.trim(_4);
if(!_4){
_4="&nbsp;";
}
}else{
_4="&nbsp;";
}
this.barContent.innerHTML=_4;
}});
_1.declare("dojox.editor.plugins.StatusBar",_2._editor._Plugin,{statusBar:null,resizer:true,setEditor:function(_5){
this.editor=_5;
this.statusBar=new _3.editor.plugins._StatusBar();
if(this.resizer){
this.resizeHandle=new _3.layout.ResizeHandle({targetId:this.editor,activeResize:true},this.statusBar.handle);
this.resizeHandle.startup();
}else{
_1.style(this.statusBar.handle.parentNode,"display","none");
}
var _6=null;
if(_5.footer.lastChild){
_6="after";
}
_1.place(this.statusBar.domNode,_5.footer.lastChild||_5.footer,_6);
this.statusBar.startup();
this.editor.statusBar=this;
this._msgListener=_1.subscribe(this.editor.id+"_statusBar",_1.hitch(this,this._setValueAttr));
},_getValueAttr:function(){
return this.statusBar.get("value");
},_setValueAttr:function(_7){
this.statusBar.set("value",_7);
},set:function(_8,_9){
if(_8){
var _a="_set"+_8.charAt(0).toUpperCase()+_8.substring(1,_8.length)+"Attr";
if(_1.isFunction(this[_a])){
this[_a](_9);
}else{
this[_8]=_9;
}
}
},get:function(_b){
if(_b){
var _c="_get"+_b.charAt(0).toUpperCase()+_b.substring(1,_b.length)+"Attr";
var f=this[_c];
if(_1.isFunction(f)){
return this[_c]();
}else{
return this[_b];
}
}
return null;
},destroy:function(){
if(this.statusBar){
this.statusBar.destroy();
delete this.statusBar;
}
if(this.resizeHandle){
this.resizeHandle.destroy();
delete this.resizeHandle;
}
if(this._msgListener){
_1.unsubscribe(this._msgListener);
delete this._msgListener;
}
delete this.editor.statusBar;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _d=o.args.name.toLowerCase();
if(_d==="statusbar"){
var _e=("resizer" in o.args)?o.args.resizer:true;
o.plugin=new _3.editor.plugins.StatusBar({resizer:_e});
}
});
return _3.editor.plugins.StatusBar;
});

//>>built
define("dojox/editor/plugins/StatusBar",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/layout/ResizeHandle"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.editor.plugins.StatusBar");
_1.declare("dojox.editor.plugins._StatusBar",[_4,_5],{templateString:"<div class=\"dojoxEditorStatusBar\">"+"<table><tbody><tr>"+"<td class=\"dojoxEditorStatusBarText\" tabindex=\"-1\" aria-role=\"presentation\" aria-live=\"aggressive\"><span dojoAttachPoint=\"barContent\">&nbsp;</span></td>"+"<td><span dojoAttachPoint=\"handle\"></span></td>"+"</tr></tbody><table>"+"</div>",_getValueAttr:function(){
return this.barContent.innerHTML;
},_setValueAttr:function(_7){
if(_7){
_7=_1.trim(_7);
if(!_7){
_7="&nbsp;";
}
}else{
_7="&nbsp;";
}
this.barContent.innerHTML=_7;
}});
_1.declare("dojox.editor.plugins.StatusBar",_6,{statusBar:null,resizer:true,setEditor:function(_8){
this.editor=_8;
this.statusBar=new _3.editor.plugins._StatusBar();
if(this.resizer){
this.resizeHandle=new _3.layout.ResizeHandle({targetId:this.editor,activeResize:true},this.statusBar.handle);
this.resizeHandle.startup();
}else{
_1.style(this.statusBar.handle.parentNode,"display","none");
}
var _9=null;
if(_8.footer.lastChild){
_9="after";
}
_1.place(this.statusBar.domNode,_8.footer.lastChild||_8.footer,_9);
this.statusBar.startup();
this.editor.statusBar=this;
this._msgListener=_1.subscribe(this.editor.id+"_statusBar",_1.hitch(this,this._setValueAttr));
},_getValueAttr:function(){
return this.statusBar.get("value");
},_setValueAttr:function(_a){
this.statusBar.set("value",_a);
},set:function(_b,_c){
if(_b){
var _d="_set"+_b.charAt(0).toUpperCase()+_b.substring(1,_b.length)+"Attr";
if(_1.isFunction(this[_d])){
this[_d](_c);
}else{
this[_b]=_c;
}
}
},get:function(_e){
if(_e){
var _f="_get"+_e.charAt(0).toUpperCase()+_e.substring(1,_e.length)+"Attr";
var f=this[_f];
if(_1.isFunction(f)){
return this[_f]();
}else{
return this[_e];
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
var _10=o.args.name.toLowerCase();
if(_10==="statusbar"){
var _11=("resizer" in o.args)?o.args.resizer:true;
o.plugin=new _3.editor.plugins.StatusBar({resizer:_11});
}
});
return _3.editor.plugins.StatusBar;
});

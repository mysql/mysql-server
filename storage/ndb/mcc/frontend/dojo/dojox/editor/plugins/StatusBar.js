//>>built
define("dojox/editor/plugins/StatusBar",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/layout/ResizeHandle"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.editor.plugins.StatusBar");
var _7=_1.declare("dojox.editor.plugins._StatusBar",[_4,_5],{templateString:"<div class=\"dojoxEditorStatusBar\">"+"<table><tbody><tr>"+"<td class=\"dojoxEditorStatusBarText\" tabindex=\"-1\" aria-role=\"presentation\" aria-live=\"aggressive\"><span dojoAttachPoint=\"barContent\">&nbsp;</span></td>"+"<td><span dojoAttachPoint=\"handle\"></span></td>"+"</tr></tbody><table>"+"</div>",_getValueAttr:function(){
return this.barContent.innerHTML;
},_setValueAttr:function(_8){
if(_8){
_8=_1.trim(_8);
if(!_8){
_8="&nbsp;";
}
}else{
_8="&nbsp;";
}
this.barContent.innerHTML=_8;
}});
var _9=_1.declare("dojox.editor.plugins.StatusBar",_6,{statusBar:null,resizer:true,setEditor:function(_a){
this.editor=_a;
this.statusBar=new _7();
if(this.resizer){
this.resizeHandle=new _3.layout.ResizeHandle({targetId:this.editor,activeResize:true},this.statusBar.handle);
this.resizeHandle.startup();
}else{
_1.style(this.statusBar.handle.parentNode,"display","none");
}
var _b=null;
if(_a.footer.lastChild){
_b="after";
}
_1.place(this.statusBar.domNode,_a.footer.lastChild||_a.footer,_b);
this.statusBar.startup();
this.editor.statusBar=this;
this._msgListener=_1.subscribe(this.editor.id+"_statusBar",_1.hitch(this,this._setValueAttr));
},_getValueAttr:function(){
return this.statusBar.get("value");
},_setValueAttr:function(_c){
this.statusBar.set("value",_c);
},set:function(_d,_e){
if(_d){
var _f="_set"+_d.charAt(0).toUpperCase()+_d.substring(1,_d.length)+"Attr";
if(_1.isFunction(this[_f])){
this[_f](_e);
}else{
this[_d]=_e;
}
}
},get:function(_10){
if(_10){
var _11="_get"+_10.charAt(0).toUpperCase()+_10.substring(1,_10.length)+"Attr";
var f=this[_11];
if(_1.isFunction(f)){
return this[_11]();
}else{
return this[_10];
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
_9._StatusBar=_7;
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _12=o.args.name.toLowerCase();
if(_12==="statusbar"){
var _13=("resizer" in o.args)?o.args.resizer:true;
o.plugin=new _9({resizer:_13});
}
});
return _9;
});

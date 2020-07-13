//>>built
require({cache:{"url:dojox/layout/resources/FloatingPane.html":"<div class=\"dojoxFloatingPane\" id=\"${id}\">\n\t<div tabindex=\"0\" role=\"button\" class=\"dojoxFloatingPaneTitle\" dojoAttachPoint=\"focusNode\">\n\t\t<span dojoAttachPoint=\"closeNode\" dojoAttachEvent=\"onclick: close\" class=\"dojoxFloatingCloseIcon\"></span>\n\t\t<span dojoAttachPoint=\"maxNode\" dojoAttachEvent=\"onclick: maximize\" class=\"dojoxFloatingMaximizeIcon\">&thinsp;</span>\n\t\t<span dojoAttachPoint=\"restoreNode\" dojoAttachEvent=\"onclick: _restore\" class=\"dojoxFloatingRestoreIcon\">&thinsp;</span>\t\n\t\t<span dojoAttachPoint=\"dockNode\" dojoAttachEvent=\"onclick: minimize\" class=\"dojoxFloatingMinimizeIcon\">&thinsp;</span>\n\t\t<span dojoAttachPoint=\"titleNode\" class=\"dijitInline dijitTitleNode\"></span>\n\t</div>\n\t<div dojoAttachPoint=\"canvas\" class=\"dojoxFloatingPaneCanvas\">\n\t\t<div dojoAttachPoint=\"containerNode\" role=\"region\" tabindex=\"-1\" class=\"${contentClass}\">\n\t\t</div>\n\t\t<span dojoAttachPoint=\"resizeHandle\" class=\"dojoxFloatingResizeHandle\"></span>\n\t</div>\n</div>\n"}});
define("dojox/layout/FloatingPane",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/window","dojo/_base/declare","dojo/_base/fx","dojo/_base/connect","dojo/_base/array","dojo/_base/sniff","dojo/window","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-construct","dojo/touch","dijit/_TemplatedMixin","dijit/_Widget","dijit/BackgroundIframe","dijit/registry","dojo/dnd/Moveable","./ContentPane","./ResizeHandle","dojo/text!./resources/FloatingPane.html","./Dock"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17){
_1.experimental("dojox.layout.FloatingPane");
return _4("dojox.layout.FloatingPane",[_14,_f],{closable:true,dockable:true,resizable:false,maxable:false,resizeAxis:"xy",title:"",dockTo:"",duration:400,contentClass:"dojoxFloatingPaneContent",_showAnim:null,_hideAnim:null,_dockNode:null,_restoreState:{},_allFPs:[],_startZ:100,templateString:_16,attributeMap:_2.delegate(_10.prototype.attributeMap,{title:{type:"innerHTML",node:"titleNode"}}),postCreate:function(){
this.inherited(arguments);
new _13(this.domNode,{handle:this.focusNode});
if(!this.dockable){
this.dockNode.style.display="none";
}
if(!this.closable){
this.closeNode.style.display="none";
}
if(!this.maxable){
this.maxNode.style.display="none";
this.restoreNode.style.display="none";
}
if(!this.resizable){
this.resizeHandle.style.display="none";
}else{
this.domNode.style.width=_c.getMarginBox(this.domNode).w+"px";
}
this._allFPs.push(this);
this.domNode.style.position="absolute";
this.bgIframe=new _11(this.domNode);
this._naturalState=_c.position(this.domNode);
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
if(this.resizable){
if(_8("ie")){
this.canvas.style.overflow="auto";
}else{
this.containerNode.style.overflow="auto";
}
this._resizeHandle=new _15({targetId:this.id,resizeAxis:this.resizeAxis},this.resizeHandle);
}
if(this.dockable){
var _18=this.dockTo;
if(this.dockTo){
this.dockTo=_12.byId(this.dockTo);
}else{
this.dockTo=_12.byId("dojoxGlobalFloatingDock");
}
if(!this.dockTo){
var _19,_1a;
if(_18){
_19=_18;
_1a=_a.byId(_18);
}else{
_1a=_d.create("div",null,_3.body());
_b.add(_1a,"dojoxFloatingDockDefault");
_19="dojoxGlobalFloatingDock";
}
this.dockTo=new _17({id:_19,autoPosition:"south"},_1a);
this.dockTo.startup();
}
if((this.domNode.style.display=="none")||(this.domNode.style.visibility=="hidden")){
this.minimize();
}
}
this.connect(this.focusNode,_e.press,"bringToTop");
this.connect(this.domNode,_e.press,"bringToTop");
this.resize(_c.position(this.domNode));
this._started=true;
},setTitle:function(_1b){
_1.deprecated("pane.setTitle","Use pane.set('title', someTitle)","2.0");
this.set("title",_1b);
},close:function(){
if(!this.closable){
return;
}
_6.unsubscribe(this._listener);
this.hide(_2.hitch(this,function(){
this.destroyRecursive();
}));
},hide:function(_1c){
_5.fadeOut({node:this.domNode,duration:this.duration,onEnd:_2.hitch(this,function(){
this.domNode.style.display="none";
this.domNode.style.visibility="hidden";
if(this.dockTo&&this.dockable){
this.dockTo._positionDock(null);
}
if(_1c){
_1c();
}
})}).play();
},show:function(_1d){
var _1e=_5.fadeIn({node:this.domNode,duration:this.duration,beforeBegin:_2.hitch(this,function(){
this.domNode.style.display="";
this.domNode.style.visibility="visible";
if(this.dockTo&&this.dockable){
this.dockTo._positionDock(null);
}
if(typeof _1d=="function"){
_1d();
}
this._isDocked=false;
if(this._dockNode){
this._dockNode.destroy();
this._dockNode=null;
}
})}).play();
var _1f=_c.getContentBox(this.domNode);
this.resize(_2.mixin(_c.position(this.domNode),{w:_1f.w,h:_1f.h}));
this._onShow();
},minimize:function(){
if(!this._isDocked){
this.hide(_2.hitch(this,"_dock"));
}
},maximize:function(){
if(this._maximized){
return;
}
this._naturalState=_c.position(this.domNode);
if(this._isDocked){
this.show();
setTimeout(_2.hitch(this,"maximize"),this.duration);
}
_b.add(this.focusNode,"floatingPaneMaximized");
this.resize(_9.getBox());
this._maximized=true;
},_restore:function(){
if(this._maximized){
this.resize(this._naturalState);
_b.remove(this.focusNode,"floatingPaneMaximized");
this._maximized=false;
}
},_dock:function(){
if(!this._isDocked&&this.dockable){
this._dockNode=this.dockTo.addNode(this);
this._isDocked=true;
}
},resize:function(dim){
dim=dim||this._naturalState;
this._naturalState=dim;
var dns=this.domNode.style;
if("t" in dim){
dns.top=dim.t+"px";
}else{
if("y" in dim){
dns.top=dim.y+"px";
}
}
if("l" in dim){
dns.left=dim.l+"px";
}else{
if("x" in dim){
dns.left=dim.x+"px";
}
}
dns.width=dim.w+"px";
dns.height=dim.h+"px";
var _20={l:0,t:0,w:dim.w,h:(dim.h-this.focusNode.offsetHeight)};
_c.setMarginBox(this.canvas,_20);
this._checkIfSingleChild();
if(this._singleChild&&this._singleChild.resize){
this._singleChild.resize(_20);
}
},bringToTop:function(){
var _21=_7.filter(this._allFPs,function(i){
return i!==this;
},this);
_21.sort(function(a,b){
return a.domNode.style.zIndex-b.domNode.style.zIndex;
});
_21.push(this);
_7.forEach(_21,function(w,x){
w.domNode.style.zIndex=this._startZ+(x*2);
_b.remove(w.domNode,"dojoxFloatingPaneFg");
},this);
_b.add(this.domNode,"dojoxFloatingPaneFg");
},destroy:function(){
this._allFPs.splice(_7.indexOf(this._allFPs,this),1);
if(this._resizeHandle){
this._resizeHandle.destroy();
}
this.inherited(arguments);
}});
});

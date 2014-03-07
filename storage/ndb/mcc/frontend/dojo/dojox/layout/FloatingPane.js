//>>built
require({cache:{"url:dojox/layout/resources/FloatingPane.html":"<div class=\"dojoxFloatingPane\" id=\"${id}\">\n\t<div tabindex=\"0\" role=\"button\" class=\"dojoxFloatingPaneTitle\" dojoAttachPoint=\"focusNode\">\n\t\t<span dojoAttachPoint=\"closeNode\" dojoAttachEvent=\"onclick: close\" class=\"dojoxFloatingCloseIcon\"></span>\n\t\t<span dojoAttachPoint=\"maxNode\" dojoAttachEvent=\"onclick: maximize\" class=\"dojoxFloatingMaximizeIcon\">&thinsp;</span>\n\t\t<span dojoAttachPoint=\"restoreNode\" dojoAttachEvent=\"onclick: _restore\" class=\"dojoxFloatingRestoreIcon\">&thinsp;</span>\t\n\t\t<span dojoAttachPoint=\"dockNode\" dojoAttachEvent=\"onclick: minimize\" class=\"dojoxFloatingMinimizeIcon\">&thinsp;</span>\n\t\t<span dojoAttachPoint=\"titleNode\" class=\"dijitInline dijitTitleNode\"></span>\n\t</div>\n\t<div dojoAttachPoint=\"canvas\" class=\"dojoxFloatingPaneCanvas\">\n\t\t<div dojoAttachPoint=\"containerNode\" role=\"region\" tabindex=\"-1\" class=\"${contentClass}\">\n\t\t</div>\n\t\t<span dojoAttachPoint=\"resizeHandle\" class=\"dojoxFloatingResizeHandle\"></span>\n\t</div>\n</div>\n"}});
define("dojox/layout/FloatingPane",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/window","dojo/_base/declare","dojo/_base/fx","dojo/_base/connect","dojo/_base/array","dojo/_base/sniff","dojo/window","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-construct","dijit/_TemplatedMixin","dijit/_Widget","dijit/BackgroundIframe","dojo/dnd/Moveable","./ContentPane","./ResizeHandle","dojo/text!./resources/FloatingPane.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14){
_1.experimental("dojox.layout.FloatingPane");
var _15=_4("dojox.layout.FloatingPane",[_12,_e],{closable:true,dockable:true,resizable:false,maxable:false,resizeAxis:"xy",title:"",dockTo:"",duration:400,contentClass:"dojoxFloatingPaneContent",_showAnim:null,_hideAnim:null,_dockNode:null,_restoreState:{},_allFPs:[],_startZ:100,templateString:_14,attributeMap:_2.delegate(_f.prototype.attributeMap,{title:{type:"innerHTML",node:"titleNode"}}),postCreate:function(){
this.inherited(arguments);
new _11(this.domNode,{handle:this.focusNode});
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
this.bgIframe=new _10(this.domNode);
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
this._resizeHandle=new _13({targetId:this.id,resizeAxis:this.resizeAxis},this.resizeHandle);
}
if(this.dockable){
var _16=this.dockTo;
if(this.dockTo){
this.dockTo=dijit.byId(this.dockTo);
}else{
this.dockTo=dijit.byId("dojoxGlobalFloatingDock");
}
if(!this.dockTo){
var _17,_18;
if(_16){
_17=_16;
_18=_a.byId(_16);
}else{
_18=_d.create("div",null,_3.body());
_b.add(_18,"dojoxFloatingDockDefault");
_17="dojoxGlobalFloatingDock";
}
this.dockTo=new _19({id:_17,autoPosition:"south"},_18);
this.dockTo.startup();
}
if((this.domNode.style.display=="none")||(this.domNode.style.visibility=="hidden")){
this.minimize();
}
}
this.connect(this.focusNode,"onmousedown","bringToTop");
this.connect(this.domNode,"onmousedown","bringToTop");
this.resize(_c.position(this.domNode));
this._started=true;
},setTitle:function(_1a){
_1.deprecated("pane.setTitle","Use pane.set('title', someTitle)","2.0");
this.set("title",_1a);
},close:function(){
if(!this.closable){
return;
}
_6.unsubscribe(this._listener);
this.hide(_2.hitch(this,function(){
this.destroyRecursive();
}));
},hide:function(_1b){
_5.fadeOut({node:this.domNode,duration:this.duration,onEnd:_2.hitch(this,function(){
this.domNode.style.display="none";
this.domNode.style.visibility="hidden";
if(this.dockTo&&this.dockable){
this.dockTo._positionDock(null);
}
if(_1b){
_1b();
}
})}).play();
},show:function(_1c){
var _1d=_5.fadeIn({node:this.domNode,duration:this.duration,beforeBegin:_2.hitch(this,function(){
this.domNode.style.display="";
this.domNode.style.visibility="visible";
if(this.dockTo&&this.dockable){
this.dockTo._positionDock(null);
}
if(typeof _1c=="function"){
_1c();
}
this._isDocked=false;
if(this._dockNode){
this._dockNode.destroy();
this._dockNode=null;
}
})}).play();
this.resize(_c.position(this.domNode));
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
this._currentState=dim;
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
var _1e={l:0,t:0,w:dim.w,h:(dim.h-this.focusNode.offsetHeight)};
_c.setMarginBox(this.canvas,_1e);
this._checkIfSingleChild();
if(this._singleChild&&this._singleChild.resize){
this._singleChild.resize(_1e);
}
},bringToTop:function(){
var _1f=_7.filter(this._allFPs,function(i){
return i!==this;
},this);
_1f.sort(function(a,b){
return a.domNode.style.zIndex-b.domNode.style.zIndex;
});
_1f.push(this);
_7.forEach(_1f,function(w,x){
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
var _19=_4("dojox.layout.Dock",[_f,_e],{templateString:"<div class=\"dojoxDock\"><ul dojoAttachPoint=\"containerNode\" class=\"dojoxDockList\"></ul></div>",_docked:[],_inPositioning:false,autoPosition:false,addNode:function(_20){
var div=_d.create("li",null,this.containerNode),_21=new _22({title:_20.title,paneRef:_20},div);
_21.startup();
return _21;
},startup:function(){
if(this.id=="dojoxGlobalFloatingDock"||this.isFixedDock){
this.connect(window,"onresize","_positionDock");
this.connect(window,"onscroll","_positionDock");
if(_8("ie")){
this.connect(this.domNode,"onresize","_positionDock");
}
}
this._positionDock(null);
this.inherited(arguments);
},_positionDock:function(e){
if(!this._inPositioning){
if(this.autoPosition=="south"){
setTimeout(_2.hitch(this,function(){
this._inPositiononing=true;
var _23=_9.getBox();
var s=this.domNode.style;
s.left=_23.l+"px";
s.width=(_23.w-2)+"px";
s.top=(_23.h+_23.t)-this.domNode.offsetHeight+"px";
this._inPositioning=false;
}),125);
}
}
}});
var _22=_4("dojox.layout._DockNode",[_f,_e],{title:"",paneRef:null,templateString:"<li dojoAttachEvent=\"onclick: restore\" class=\"dojoxDockNode\">"+"<span dojoAttachPoint=\"restoreNode\" class=\"dojoxDockRestoreButton\" dojoAttachEvent=\"onclick: restore\"></span>"+"<span class=\"dojoxDockTitleNode\" dojoAttachPoint=\"titleNode\">${title}</span>"+"</li>",restore:function(){
this.paneRef.show();
this.paneRef.bringToTop();
this.destroy();
}});
return _15;
});

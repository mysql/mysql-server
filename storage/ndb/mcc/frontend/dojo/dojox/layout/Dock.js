//>>built
define("dojox/layout/Dock",["dojo/_base/lang","dojo/_base/window","dojo/_base/declare","dojo/_base/fx","dojo/on","dojo/_base/array","dojo/_base/sniff","dojo/window","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-construct","dijit/_TemplatedMixin","dijit/_WidgetBase"],function(_1,_2,_3,fx,on,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_3("dojox.layout.Dock",[_c,_b],{templateString:"<div class=\"dojoxDock\"><ul data-dojo-attach-point=\"containerNode\" class=\"dojoxDockList\"></ul></div>",_docked:[],_inPositioning:false,autoPosition:false,addNode:function(_e){
var _f=_a.create("li",null,this.containerNode),_10=new _11({title:_e.title,paneRef:_e},_f);
_10.startup();
return _10;
},startup:function(){
if(this.id=="dojoxGlobalFloatingDock"||this.isFixedDock){
this.own(on(window,"resize",_1.hitch(this,"_positionDock")),on(window,"scroll",_1.hitch(this,"_positionDock")));
if(_5("ie")){
this.own(on(this.domNode,"resize",_1.hitch(this,"_positionDock")));
}
}
this._positionDock(null);
this.inherited(arguments);
},_positionDock:function(e){
if(!this._inPositioning){
if(this.autoPosition=="south"){
this.defer(function(){
this._inPositiononing=true;
var _12=_6.getBox();
var s=this.domNode.style;
s.left=_12.l+"px";
s.width=(_12.w-2)+"px";
s.top=(_12.h+_12.t)-this.domNode.offsetHeight+"px";
this._inPositioning=false;
},125);
}
}
}});
var _11=_3("dojox.layout._DockNode",[_c,_b],{title:"",paneRef:null,templateString:"<li data-dojo-attach-event=\"onclick: restore\" class=\"dojoxDockNode\">"+"<span data-dojo-attach-point=\"restoreNode\" class=\"dojoxDockRestoreButton\" data-dojo-attach-event=\"onclick: restore\"></span>"+"<span class=\"dojoxDockTitleNode\" data-dojo-attach-point=\"titleNode\">${title}</span>"+"</li>",restore:function(){
this.paneRef.show();
this.paneRef.bringToTop();
this.destroy();
}});
return _d;
});

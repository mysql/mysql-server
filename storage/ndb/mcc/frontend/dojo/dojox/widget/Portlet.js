//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/TitlePane,dojo/fx"],function(_1,_2,_3){
_2.experimental("dojox.widget.Portlet");
_2.provide("dojox.widget.Portlet");
_2.require("dijit.TitlePane");
_2.require("dojo.fx");
_2.declare("dojox.widget.Portlet",[_1.TitlePane,_1._Container],{resizeChildren:true,closable:true,_parents:null,_size:null,dragRestriction:false,buildRendering:function(){
this.inherited(arguments);
_2.style(this.domNode,"visibility","hidden");
},postCreate:function(){
this.inherited(arguments);
_2.addClass(this.domNode,"dojoxPortlet");
_2.removeClass(this.arrowNode,"dijitArrowNode");
_2.addClass(this.arrowNode,"dojoxPortletIcon dojoxArrowDown");
_2.addClass(this.titleBarNode,"dojoxPortletTitle");
_2.addClass(this.hideNode,"dojoxPortletContentOuter");
_2.addClass(this.domNode,"dojoxPortlet-"+(!this.dragRestriction?"movable":"nonmovable"));
var _4=this;
if(this.resizeChildren){
this.subscribe("/dnd/drop",function(){
_4._updateSize();
});
this.subscribe("/Portlet/sizechange",function(_5){
_4.onSizeChange(_5);
});
this.connect(window,"onresize",function(){
_4._updateSize();
});
var _6=_2.hitch(this,function(id,_7){
var _8=_1.byId(id);
if(_8.selectChild){
var s=this.subscribe(id+"-selectChild",function(_9){
var n=_4.domNode.parentNode;
while(n){
if(n==_9.domNode){
_4.unsubscribe(s);
_4._updateSize();
break;
}
n=n.parentNode;
}
});
var _a=_1.byId(_7);
if(_8&&_a){
_4._parents.push({parent:_8,child:_a});
}
}
});
var _b;
this._parents=[];
for(var p=this.domNode.parentNode;p!=null;p=p.parentNode){
var id=p.getAttribute?p.getAttribute("widgetId"):null;
if(id){
_6(id,_b);
_b=id;
}
}
}
this.connect(this.titleBarNode,"onmousedown",function(_c){
if(_2.hasClass(_c.target,"dojoxPortletIcon")){
_2.stopEvent(_c);
return false;
}
return true;
});
this.connect(this._wipeOut,"onEnd",function(){
_4._publish();
});
this.connect(this._wipeIn,"onEnd",function(){
_4._publish();
});
if(this.closable){
this.closeIcon=this._createIcon("dojoxCloseNode","dojoxCloseNodeHover",_2.hitch(this,"onClose"));
_2.style(this.closeIcon,"display","");
}
},startup:function(){
if(this._started){
return;
}
var _d=this.getChildren();
this._placeSettingsWidgets();
_2.forEach(_d,function(_e){
try{
if(!_e.started&&!_e._started){
_e.startup();
}
}
catch(e){
}
});
this.inherited(arguments);
_2.style(this.domNode,"visibility","visible");
},_placeSettingsWidgets:function(){
_2.forEach(this.getChildren(),_2.hitch(this,function(_f){
if(_f.portletIconClass&&_f.toggle&&!_f.attr("portlet")){
this._createIcon(_f.portletIconClass,_f.portletIconHoverClass,_2.hitch(_f,"toggle"));
_2.place(_f.domNode,this.containerNode,"before");
_f.attr("portlet",this);
this._settingsWidget=_f;
}
}));
},_createIcon:function(_10,_11,fn){
var _12=_2.create("div",{"class":"dojoxPortletIcon "+_10,"waiRole":"presentation"});
_2.place(_12,this.arrowNode,"before");
this.connect(_12,"onclick",fn);
if(_11){
this.connect(_12,"onmouseover",function(){
_2.addClass(_12,_11);
});
this.connect(_12,"onmouseout",function(){
_2.removeClass(_12,_11);
});
}
return _12;
},onClose:function(evt){
_2.style(this.domNode,"display","none");
},onSizeChange:function(_13){
if(_13==this){
return;
}
this._updateSize();
},_updateSize:function(){
if(!this.open||!this._started||!this.resizeChildren){
return;
}
if(this._timer){
clearTimeout(this._timer);
}
this._timer=setTimeout(_2.hitch(this,function(){
var _14={w:_2.style(this.domNode,"width"),h:_2.style(this.domNode,"height")};
for(var i=0;i<this._parents.length;i++){
var p=this._parents[i];
var sel=p.parent.selectedChildWidget;
if(sel&&sel!=p.child){
return;
}
}
if(this._size){
if(this._size.w==_14.w&&this._size.h==_14.h){
return;
}
}
this._size=_14;
var fns=["resize","layout"];
this._timer=null;
var _15=this.getChildren();
_2.forEach(_15,function(_16){
for(var i=0;i<fns.length;i++){
if(_2.isFunction(_16[fns[i]])){
try{
_16[fns[i]]();
}
catch(e){
}
break;
}
}
});
this.onUpdateSize();
}),100);
},onUpdateSize:function(){
},_publish:function(){
_2.publish("/Portlet/sizechange",[this]);
},_onTitleClick:function(evt){
if(evt.target==this.arrowNode){
this.inherited(arguments);
}
},addChild:function(_17){
this._size=null;
this.inherited(arguments);
if(this._started){
this._placeSettingsWidgets();
this._updateSize();
}
if(this._started&&!_17.started&&!_17._started){
_17.startup();
}
},destroyDescendants:function(_18){
this.inherited(arguments);
if(this._settingsWidget){
this._settingsWidget.destroyRecursive(_18);
}
},destroy:function(){
if(this._timer){
clearTimeout(this._timer);
}
this.inherited(arguments);
},_setCss:function(){
this.inherited(arguments);
_2.style(this.arrowNode,"display",this.toggleable?"":"none");
}});
_2.declare("dojox.widget.PortletSettings",[_1._Container,_1.layout.ContentPane],{portletIconClass:"dojoxPortletSettingsIcon",portletIconHoverClass:"dojoxPortletSettingsIconHover",postCreate:function(){
_2.style(this.domNode,"display","none");
_2.addClass(this.domNode,"dojoxPortletSettingsContainer");
_2.removeClass(this.domNode,"dijitContentPane");
},_setPortletAttr:function(_19){
this.portlet=_19;
},toggle:function(){
var n=this.domNode;
if(_2.style(n,"display")=="none"){
_2.style(n,{"display":"block","height":"1px","width":"auto"});
_2.fx.wipeIn({node:n}).play();
}else{
_2.fx.wipeOut({node:n,onEnd:_2.hitch(this,function(){
_2.style(n,{"display":"none","height":"","width":""});
})}).play();
}
}});
_2.declare("dojox.widget.PortletDialogSettings",_3.widget.PortletSettings,{dimensions:null,constructor:function(_1a,_1b){
this.dimensions=_1a.dimensions||[300,100];
},toggle:function(){
if(!this.dialog){
_2["require"]("dijit.Dialog");
this.dialog=new _1.Dialog({title:this.title});
_2.body().appendChild(this.dialog.domNode);
this.dialog.containerNode.appendChild(this.domNode);
_2.style(this.dialog.domNode,{"width":this.dimensions[0]+"px","height":this.dimensions[1]+"px"});
_2.style(this.domNode,"display","");
}
if(this.dialog.open){
this.dialog.hide();
}else{
this.dialog.show(this.domNode);
}
}});
});

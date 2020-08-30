//>>built
define("dojox/widget/Portlet",["dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/event","dojo/_base/connect","dojo/dom-style","dojo/dom-class","dojo/dom-construct","dojo/fx","dijit/registry","dijit/TitlePane","dijit/_Container","./PortletSettings","./PortletDialogSettings"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,fx,_a,_b,_c,_d,_e){
_2.experimental("dojox.widget.Portlet");
return _1("dojox.widget.Portlet",[_b,_c],{resizeChildren:true,closable:true,_parents:null,_size:null,dragRestriction:false,buildRendering:function(){
this.inherited(arguments);
_7.set(this.domNode,"visibility","hidden");
},postCreate:function(){
this.inherited(arguments);
_8.add(this.domNode,"dojoxPortlet");
_8.remove(this.arrowNode,"dijitArrowNode");
_8.add(this.arrowNode,"dojoxPortletIcon dojoxArrowDown");
_8.add(this.titleBarNode,"dojoxPortletTitle");
_8.add(this.hideNode,"dojoxPortletContentOuter");
_8.add(this.domNode,"dojoxPortlet-"+(!this.dragRestriction?"movable":"nonmovable"));
var _f=this;
if(this.resizeChildren){
this.subscribe("/dnd/drop",function(){
_f._updateSize();
});
this.subscribe("/Portlet/sizechange",function(_10){
_f.onSizeChange(_10);
});
this.connect(window,"onresize",function(){
_f._updateSize();
});
var _11=_3.hitch(this,function(id,_12){
var _13=_a.byId(id);
if(_13.selectChild){
var s=this.subscribe(id+"-selectChild",function(_14){
var n=_f.domNode.parentNode;
while(n){
if(n==_14.domNode){
_f.unsubscribe(s);
_f._updateSize();
break;
}
n=n.parentNode;
}
});
var _15=_a.byId(_12);
if(_13&&_15){
_f._parents.push({parent:_13,child:_15});
}
}
});
var _16;
this._parents=[];
for(var p=this.domNode.parentNode;p!=null;p=p.parentNode){
var id=p.getAttribute?p.getAttribute("widgetId"):null;
if(id){
_11(id,_16);
_16=id;
}
}
}
this.connect(this.titleBarNode,"onmousedown",function(evt){
if(_8.contains(evt.target,"dojoxPortletIcon")){
_5.stop(evt);
return false;
}
return true;
});
this.connect(this._wipeOut,"onEnd",function(){
_f._publish();
});
this.connect(this._wipeIn,"onEnd",function(){
_f._publish();
});
if(this.closable){
this.closeIcon=this._createIcon("dojoxCloseNode","dojoxCloseNodeHover",_3.hitch(this,"onClose"));
_7.set(this.closeIcon,"display","");
}
},startup:function(){
if(this._started){
return;
}
var _17=this.getChildren();
this._placeSettingsWidgets();
_4.forEach(_17,function(_18){
try{
if(!_18.started&&!_18._started){
_18.startup();
}
}
catch(e){
}
});
this.inherited(arguments);
_7.set(this.domNode,"visibility","visible");
},_placeSettingsWidgets:function(){
_4.forEach(this.getChildren(),_3.hitch(this,function(_19){
if(_19.portletIconClass&&_19.toggle&&!_19.get("portlet")){
this._createIcon(_19.portletIconClass,_19.portletIconHoverClass,_3.hitch(_19,"toggle"));
_9.place(_19.domNode,this.containerNode,"before");
_19.set("portlet",this);
this._settingsWidget=_19;
}
}));
},_createIcon:function(_1a,_1b,fn){
var _1c=_9.create("div",{"class":"dojoxPortletIcon "+_1a,"waiRole":"presentation"});
_9.place(_1c,this.arrowNode,"before");
this.connect(_1c,"onclick",fn);
if(_1b){
this.connect(_1c,"onmouseover",function(){
_8.add(_1c,_1b);
});
this.connect(_1c,"onmouseout",function(){
_8.remove(_1c,_1b);
});
}
return _1c;
},onClose:function(evt){
_7.set(this.domNode,"display","none");
},onSizeChange:function(_1d){
if(_1d==this){
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
this._timer=setTimeout(_3.hitch(this,function(){
var _1e={w:_7.get(this.domNode,"width"),h:_7.get(this.domNode,"height")};
for(var i=0;i<this._parents.length;i++){
var p=this._parents[i];
var sel=p.parent.selectedChildWidget;
if(sel&&sel!=p.child){
return;
}
}
if(this._size){
if(this._size.w==_1e.w&&this._size.h==_1e.h){
return;
}
}
this._size=_1e;
var fns=["resize","layout"];
this._timer=null;
var _1f=this.getChildren();
_4.forEach(_1f,function(_20){
for(var i=0;i<fns.length;i++){
if(_3.isFunction(_20[fns[i]])){
try{
_20[fns[i]]();
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
_6.publish("/Portlet/sizechange",[this]);
},_onTitleClick:function(evt){
if(evt.target==this.arrowNode){
this.inherited(arguments);
}
},addChild:function(_21){
this._size=null;
this.inherited(arguments);
if(this._started){
this._placeSettingsWidgets();
this._updateSize();
}
if(this._started&&!_21.started&&!_21._started){
_21.startup();
}
},destroyDescendants:function(_22){
this.inherited(arguments);
if(this._settingsWidget){
this._settingsWidget.destroyRecursive(_22);
}
},destroy:function(){
if(this._timer){
clearTimeout(this._timer);
}
this.inherited(arguments);
},_setCss:function(){
this.inherited(arguments);
_7.set(this.arrowNode,"display",this.toggleable?"":"none");
}});
});

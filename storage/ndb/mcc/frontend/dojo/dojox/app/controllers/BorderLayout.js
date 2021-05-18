//>>built
define("dojox/app/controllers/BorderLayout",["dojo/_base/declare","dojo/dom-attr","dojo/dom-style","./LayoutBase","dijit/layout/BorderContainer","dijit/layout/StackContainer","dijit/layout/ContentPane","dijit/registry"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.app.controllers.BorderLayout",_4,{initLayout:function(_9){
this.app.log("in app/controllers/BorderLayout.initLayout event.view.name=[",_9.view.name,"] event.view.parent.name=[",_9.view.parent.name,"]");
var bc;
if(!this.borderLayoutCreated){
this.borderLayoutCreated=true;
bc=new _5({id:this.app.id+"-BC",style:"height:100%;width:100%;border:1px solid black"});
_9.view.parent.domNode.appendChild(bc.domNode);
bc.startup();
}else{
bc=_8.byId(this.app.id+"-BC");
}
this.app.log("in app/controllers/BorderLayout.initLayout event.view.constraint=",_9.view.constraint);
var _a=_9.view.constraint;
if(_9.view.parent.id==this.app.id){
var _b=_8.byId(_9.view.parent.id+"-"+_a);
if(_b){
var _c=_8.byId(_9.view.id+"-cp-"+_a);
if(!_c){
_c=new _7({id:_9.view.id+"-cp-"+_a});
_c.addChild(_9.view);
_b.addChild(_c);
bc.addChild(_b);
}else{
_c.domNode.appendChild(_9.view.domNode);
}
}else{
var _d=this.app.borderLayoutNoSplitter||false;
var _e=new _6({doLayout:true,splitter:!_d,region:_a,id:_9.view.parent.id+"-"+_a});
var _c=new _7({id:_9.view.id+"-cp-"+_a});
_c.addChild(_9.view);
_e.addChild(_c);
bc.addChild(_e);
}
}else{
_9.view.parent.domNode.appendChild(_9.view.domNode);
_2.set(_9.view.domNode,"data-app-constraint",_9.view.constraint);
}
this.inherited(arguments);
},hideView:function(_f){
var bc=_8.byId(this.app.id+"-BC");
var sc=_8.byId(_f.parent.id+"-"+_f.constraint);
if(bc&&sc){
sc.removedFromBc=true;
bc.removeChild(sc);
}
},showView:function(_10){
var sc=_8.byId(_10.parent.id+"-"+_10.constraint);
var cp=_8.byId(_10.id+"-cp-"+_10.constraint);
if(sc&&cp){
if(sc.removedFromBc){
sc.removedFromBc=false;
_8.byId(this.app.id+"-BC").addChild(sc);
_3.set(_10.domNode,"display","");
}
_3.set(cp.domNode,"display","");
sc.selectChild(cp);
sc.resize();
}
}});
});

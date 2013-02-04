//>>built
define("dijit/layout/StackContainer",["dojo/_base/array","dojo/cookie","dojo/_base/declare","dojo/dom-class","dojo/_base/kernel","dojo/_base/lang","dojo/ready","dojo/topic","../registry","../_WidgetBase","./_LayoutWidget","dojo/i18n!../nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
if(!_5.isAsync){
_7(0,function(){
var _c=["dijit/layout/StackController"];
require(_c);
});
}
_6.extend(_a,{selected:false,closable:false,iconClass:"dijitNoIcon",showTitle:true});
return _3("dijit.layout.StackContainer",_b,{doLayout:true,persist:false,baseClass:"dijitStackContainer",buildRendering:function(){
this.inherited(arguments);
_4.add(this.domNode,"dijitLayoutContainer");
this.containerNode.setAttribute("role","tabpanel");
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onkeypress",this._onKeyPress);
},startup:function(){
if(this._started){
return;
}
var _d=this.getChildren();
_1.forEach(_d,this._setupChild,this);
if(this.persist){
this.selectedChildWidget=_9.byId(_2(this.id+"_selectedChild"));
}else{
_1.some(_d,function(_e){
if(_e.selected){
this.selectedChildWidget=_e;
}
return _e.selected;
},this);
}
var _f=this.selectedChildWidget;
if(!_f&&_d[0]){
_f=this.selectedChildWidget=_d[0];
_f.selected=true;
}
_8.publish(this.id+"-startup",{children:_d,selected:_f});
this.inherited(arguments);
},resize:function(){
if(!this._hasBeenShown){
this._hasBeenShown=true;
var _10=this.selectedChildWidget;
if(_10){
this._showChild(_10);
}
}
this.inherited(arguments);
},_setupChild:function(_11){
this.inherited(arguments);
_4.replace(_11.domNode,"dijitHidden","dijitVisible");
_11.domNode.title="";
},addChild:function(_12,_13){
this.inherited(arguments);
if(this._started){
_8.publish(this.id+"-addChild",_12,_13);
this.layout();
if(!this.selectedChildWidget){
this.selectChild(_12);
}
}
},removeChild:function(_14){
this.inherited(arguments);
if(this._started){
_8.publish(this.id+"-removeChild",_14);
}
if(this._descendantsBeingDestroyed){
return;
}
if(this.selectedChildWidget===_14){
this.selectedChildWidget=undefined;
if(this._started){
var _15=this.getChildren();
if(_15.length){
this.selectChild(_15[0]);
}
}
}
if(this._started){
this.layout();
}
},selectChild:function(_16,_17){
_16=_9.byId(_16);
if(this.selectedChildWidget!=_16){
var d=this._transition(_16,this.selectedChildWidget,_17);
this._set("selectedChildWidget",_16);
_8.publish(this.id+"-selectChild",_16);
if(this.persist){
_2(this.id+"_selectedChild",this.selectedChildWidget.id);
}
}
return d;
},_transition:function(_18,_19){
if(_19){
this._hideChild(_19);
}
var d=this._showChild(_18);
if(_18.resize){
if(this.doLayout){
_18.resize(this._containerContentBox||this._contentBox);
}else{
_18.resize();
}
}
return d;
},_adjacent:function(_1a){
var _1b=this.getChildren();
var _1c=_1.indexOf(_1b,this.selectedChildWidget);
_1c+=_1a?1:_1b.length-1;
return _1b[_1c%_1b.length];
},forward:function(){
return this.selectChild(this._adjacent(true),true);
},back:function(){
return this.selectChild(this._adjacent(false),true);
},_onKeyPress:function(e){
_8.publish(this.id+"-containerKeyPress",{e:e,page:this});
},layout:function(){
var _1d=this.selectedChildWidget;
if(_1d&&_1d.resize){
if(this.doLayout){
_1d.resize(this._containerContentBox||this._contentBox);
}else{
_1d.resize();
}
}
},_showChild:function(_1e){
var _1f=this.getChildren();
_1e.isFirstChild=(_1e==_1f[0]);
_1e.isLastChild=(_1e==_1f[_1f.length-1]);
_1e._set("selected",true);
_4.replace(_1e.domNode,"dijitVisible","dijitHidden");
return (_1e._onShow&&_1e._onShow())||true;
},_hideChild:function(_20){
_20._set("selected",false);
_4.replace(_20.domNode,"dijitHidden","dijitVisible");
_20.onHide&&_20.onHide();
},closeChild:function(_21){
var _22=_21.onClose(this,_21);
if(_22){
this.removeChild(_21);
_21.destroyRecursive();
}
},destroyDescendants:function(_23){
this._descendantsBeingDestroyed=true;
this.selectedChildWidget=undefined;
_1.forEach(this.getChildren(),function(_24){
if(!_23){
this.removeChild(_24);
}
_24.destroyRecursive(_23);
},this);
this._descendantsBeingDestroyed=false;
}});
});

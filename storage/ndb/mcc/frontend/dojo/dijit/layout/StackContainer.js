//>>built
define("dijit/layout/StackContainer",["dojo/_base/array","dojo/cookie","dojo/_base/declare","dojo/dom-class","dojo/has","dojo/_base/lang","dojo/ready","dojo/topic","../registry","../_WidgetBase","./_LayoutWidget","dojo/i18n!../nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
if(_5("dijit-legacy-requires")){
_7(0,function(){
var _c=["dijit/layout/StackController"];
require(_c);
});
}
var _d=_3("dijit.layout.StackContainer",_b,{doLayout:true,persist:false,baseClass:"dijitStackContainer",buildRendering:function(){
this.inherited(arguments);
_4.add(this.domNode,"dijitLayoutContainer");
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onkeypress",this._onKeyPress);
},startup:function(){
if(this._started){
return;
}
var _e=this.getChildren();
_1.forEach(_e,this._setupChild,this);
if(this.persist){
this.selectedChildWidget=_9.byId(_2(this.id+"_selectedChild"));
}else{
_1.some(_e,function(_f){
if(_f.selected){
this.selectedChildWidget=_f;
}
return _f.selected;
},this);
}
var _10=this.selectedChildWidget;
if(!_10&&_e[0]){
_10=this.selectedChildWidget=_e[0];
_10.selected=true;
}
_8.publish(this.id+"-startup",{children:_e,selected:_10});
this.inherited(arguments);
},resize:function(){
if(!this._hasBeenShown){
this._hasBeenShown=true;
var _11=this.selectedChildWidget;
if(_11){
this._showChild(_11);
}
}
this.inherited(arguments);
},_setupChild:function(_12){
this.inherited(arguments);
_4.replace(_12.domNode,"dijitHidden","dijitVisible");
_12.domNode.title="";
},addChild:function(_13,_14){
this.inherited(arguments);
if(this._started){
_8.publish(this.id+"-addChild",_13,_14);
this.layout();
if(!this.selectedChildWidget){
this.selectChild(_13);
}
}
},removeChild:function(_15){
this.inherited(arguments);
if(this._started){
_8.publish(this.id+"-removeChild",_15);
}
if(this._descendantsBeingDestroyed){
return;
}
if(this.selectedChildWidget===_15){
this.selectedChildWidget=undefined;
if(this._started){
var _16=this.getChildren();
if(_16.length){
this.selectChild(_16[0]);
}
}
}
if(this._started){
this.layout();
}
},selectChild:function(_17,_18){
_17=_9.byId(_17);
if(this.selectedChildWidget!=_17){
var d=this._transition(_17,this.selectedChildWidget,_18);
this._set("selectedChildWidget",_17);
_8.publish(this.id+"-selectChild",_17);
if(this.persist){
_2(this.id+"_selectedChild",this.selectedChildWidget.id);
}
}
return d;
},_transition:function(_19,_1a){
if(_1a){
this._hideChild(_1a);
}
var d=this._showChild(_19);
if(_19.resize){
if(this.doLayout){
_19.resize(this._containerContentBox||this._contentBox);
}else{
_19.resize();
}
}
return d;
},_adjacent:function(_1b){
var _1c=this.getChildren();
var _1d=_1.indexOf(_1c,this.selectedChildWidget);
_1d+=_1b?1:_1c.length-1;
return _1c[_1d%_1c.length];
},forward:function(){
return this.selectChild(this._adjacent(true),true);
},back:function(){
return this.selectChild(this._adjacent(false),true);
},_onKeyPress:function(e){
_8.publish(this.id+"-containerKeyPress",{e:e,page:this});
},layout:function(){
var _1e=this.selectedChildWidget;
if(_1e&&_1e.resize){
if(this.doLayout){
_1e.resize(this._containerContentBox||this._contentBox);
}else{
_1e.resize();
}
}
},_showChild:function(_1f){
var _20=this.getChildren();
_1f.isFirstChild=(_1f==_20[0]);
_1f.isLastChild=(_1f==_20[_20.length-1]);
_1f._set("selected",true);
_4.replace(_1f.domNode,"dijitVisible","dijitHidden");
return (_1f._onShow&&_1f._onShow())||true;
},_hideChild:function(_21){
_21._set("selected",false);
_4.replace(_21.domNode,"dijitHidden","dijitVisible");
_21.onHide&&_21.onHide();
},closeChild:function(_22){
var _23=_22.onClose(this,_22);
if(_23){
this.removeChild(_22);
_22.destroyRecursive();
}
},destroyDescendants:function(_24){
this._descendantsBeingDestroyed=true;
this.selectedChildWidget=undefined;
_1.forEach(this.getChildren(),function(_25){
if(!_24){
this.removeChild(_25);
}
_25.destroyRecursive(_24);
},this);
this._descendantsBeingDestroyed=false;
}});
_d.ChildWidgetProperties={selected:false,disabled:false,closable:false,iconClass:"dijitNoIcon",showTitle:true};
_6.extend(_a,_d.ChildWidgetProperties);
return _d;
});

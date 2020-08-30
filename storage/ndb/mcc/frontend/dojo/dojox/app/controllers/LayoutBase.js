//>>built
define("dojox/app/controllers/LayoutBase",["dojo/_base/lang","dojo/_base/declare","dojo/sniff","dojo/_base/window","dojo/_base/config","dojo/dom-attr","dojo/topic","dojo/dom-style","../utils/constraints","../Controller"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.app.controllers.LayoutBase",_a,{constructor:function(_b,_c){
this.events={"app-initLayout":this.initLayout,"app-layoutView":this.layoutView,"app-resize":this.onResize};
if(_5.mblHideAddressBar){
_7.subscribe("/dojox/mobile/afterResizeAll",_1.hitch(this,this.onResize));
}else{
this.bind(_4.global,_3("ios")?"orientationchange":"resize",_1.hitch(this,this.onResize));
}
},onResize:function(){
this._doResize(this.app);
for(var _d in this.app.selectedChildren){
if(this.app.selectedChildren[_d]){
this._doResize(this.app.selectedChildren[_d]);
}
}
},initLayout:function(_e){
_6.set(_e.view.domNode,"id",_e.view.id);
if(_e.callback){
_e.callback();
}
},_doLayout:function(_f){
if(!_f){
console.warn("layout empty view.");
}
},_doResize:function(_10){
this.app.log("in LayoutBase _doResize called for view.id="+_10.id+" view=",_10);
this._doLayout(_10);
},layoutView:function(_11){
var _12=_11.parent||this.app;
var _13=_11.view;
if(!_13){
return;
}
this.app.log("in LayoutBase layoutView called for event.view.id="+_11.view.id);
var _14=_9.getSelectedChild(_12,_13.constraint);
if(_11.removeView){
_13.viewShowing=false;
this.hideView(_13);
if(_13==_14){
_9.setSelectedChild(_12,_13.constraint,null);
}
}else{
if(_13!==_14){
if(_14){
_14.viewShowing=false;
if(_11.transition=="none"||_11.currentLastSubChildMatch!==_14){
this.hideView(_14);
}
}
_13.viewShowing=true;
this.showView(_13);
_9.setSelectedChild(_12,_13.constraint,_13);
}else{
_13.viewShowing=true;
}
}
},hideView:function(_15){
this.app.log("logTransitions:","LayoutBase"+" setting domStyle display none for view.id=["+_15.id+"], visibility=["+_15.domNode.style.visibility+"]");
_8.set(_15.domNode,"display","none");
},showView:function(_16){
if(_16.domNode){
this.app.log("logTransitions:","LayoutBase"+" setting domStyle display to display for view.id=["+_16.id+"], visibility=["+_16.domNode.style.visibility+"]");
_8.set(_16.domNode,"display","");
}
}});
});

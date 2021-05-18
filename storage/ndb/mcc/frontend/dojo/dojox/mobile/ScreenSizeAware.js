//>>built
define("dojox/mobile/ScreenSizeAware",["dojo/_base/kernel","dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom","dijit/registry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_1.experimental("dojox.mobile.ScreenSizeAware");
var _a=_5("dojox.mobile.ScreenSizeAware",null,{splitterId:"",leftPaneId:"",rightPaneId:"",leftViewId:"",leftListId:"",constructor:function(_b){
if(_b){
_6.mixin(this,_b);
}
_4.subscribe("/dojox/mobile/screenSize/tablet",this,function(_c){
this.transformUI("tablet");
});
_4.subscribe("/dojox/mobile/screenSize/phone",this,function(_d){
this.transformUI("phone");
});
},init:function(){
if(this._initialized){
return;
}
this._initialized=true;
this.splitter=this.splitterId?_9.byId(this.splitterId):_2.filter(_9.findWidgets(_7.body()),function(c){
return c.declaredClass.indexOf("Splitter")!==-1;
})[0];
if(!this.splitter){
console.error("Splitter not found.");
return;
}
this.leftPane=this.leftPaneId?_9.byId(this.leftPaneId):this.splitter.getChildren()[0];
if(!this.leftPane){
console.error("Left pane not found.");
return;
}
this.rightPane=this.rightPaneId?_9.byId(this.rightPaneId):this.splitter.getChildren()[1];
if(!this.rightPane){
console.error("Right pane not found.");
return;
}
this.leftView=this.leftViewId?_9.byId(this.leftViewId):_2.filter(_9.findWidgets(this.leftPane.containerNode),function(c){
return c.declaredClass.indexOf("View")!==-1;
})[0];
if(!this.leftView){
console.error("Left view not found.");
return;
}
this.leftList=this.leftListId?_9.byId(this.leftListId):_2.filter(_9.findWidgets(this.leftView.containerNode),function(c){
return c.declaredClass.indexOf("List")!==-1||c.declaredClass.indexOf("IconContainer")!==-1;
})[0];
if(!this.leftList){
console.error("Left list not found.");
return;
}
},isPhone:function(){
return this._currentMode==="phone";
},getShowingView:function(){
var _e=_2.filter(this.rightPane.getChildren(),function(c){
return c.declaredClass.indexOf("View")!==-1;
})[0];
if(!_e){
return null;
}
return _e.getShowingView()||_2.filter(this.rightPane.getChildren(),function(c){
return c.selected;
})[0]||_e;
},updateStateful:function(){
this.leftList.set("stateful",!this.isPhone());
},getDestinationId:function(_f){
return _f.moveTo;
},updateBackButton:function(){
_2.forEach(this.leftList.getChildren(),function(_10){
var id=this.getDestinationId(_10);
var _11=_9.byId(id);
if(_11){
var _12=_2.filter(_11.getChildren(),function(c){
return c.declaredClass.indexOf("Heading")!==-1;
})[0];
if(_12.backButton){
_12.backButton.domNode.style.display=this.isPhone()?"":"none";
}
if(_12.backBtnNode){
_12.backBtnNode.style.display=this.isPhone()?"":"none";
}
}
},this);
},updateTransition:function(){
var _13=this.isPhone()?"slide":"none";
_2.forEach(this.leftList.getChildren(),function(_14){
_14.set("transition",_13);
});
},moveList:function(){
var to=this.isPhone()?this.rightPane:this.leftPane;
to.containerNode.appendChild(this.leftView.domNode);
},showLeftView:function(){
this.leftPane.domNode.style.display=this.isPhone()?"none":"";
this.leftView.show();
},showRightView:function(){
if(this.isPhone()){
return;
}
var _15=this.getShowingView();
if(_15){
_15.show();
}else{
this.leftItemSelected();
}
},updateSelectedItem:function(){
var id;
var _16=this.getShowingView();
if(_16&&!this.isPhone()){
id=_16.id;
}
if(id){
var _17=_2.filter(this.leftList.getChildren(),function(_18){
return this.getDestinationId(_18)===id;
},this);
if(_17&&_17.length>0){
_17[0].set("selected",true);
}
}else{
this.leftList.deselectAll&&this.leftList.deselectAll();
}
},leftItemSelected:function(){
},transformUI:function(_19){
this.init();
if(_19===this._currentMode){
return;
}
this._currentMode=_19;
this.updateStateful();
this.updateBackButton();
this.updateTransition();
this.moveList();
this.showLeftView();
this.showRightView();
this.updateSelectedItem();
}});
_a._instance=null;
_a.getInstance=function(){
if(!_a._instance){
_a._instance=new _a();
}
return _a._instance;
};
return _a;
});

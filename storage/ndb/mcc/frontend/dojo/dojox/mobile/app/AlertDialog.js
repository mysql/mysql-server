//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_WidgetBase"],function(_1,_2,_3){
_2.provide("dojox.mobile.app.AlertDialog");
_2.experimental("dojox.mobile.app.AlertDialog");
_2.require("dijit._WidgetBase");
_2.declare("dojox.mobile.app.AlertDialog",_1._WidgetBase,{title:"",text:"",controller:null,buttons:null,defaultButtonLabel:"OK",onChoose:null,constructor:function(){
this.onClick=_2.hitch(this,this.onClick);
this._handleSelect=_2.hitch(this,this._handleSelect);
},buildRendering:function(){
this.domNode=_2.create("div",{"class":"alertDialog"});
var _4=_2.create("div",{"class":"alertDialogBody"},this.domNode);
_2.create("div",{"class":"alertTitle",innerHTML:this.title||""},_4);
_2.create("div",{"class":"alertText",innerHTML:this.text||""},_4);
var _5=_2.create("div",{"class":"alertBtns"},_4);
if(!this.buttons||this.buttons.length==0){
this.buttons=[{label:this.defaultButtonLabel,value:"ok","class":"affirmative"}];
}
var _6=this;
_2.forEach(this.buttons,function(_7){
var _8=new _3.mobile.Button({btnClass:_7["class"]||"",label:_7.label});
_8._dialogValue=_7.value;
_2.place(_8.domNode,_5);
_6.connect(_8,"onClick",_6._handleSelect);
});
var _9=this.controller.getWindowSize();
this.mask=_2.create("div",{"class":"dialogUnderlayWrapper",innerHTML:"<div class=\"dialogUnderlay\"></div>",style:{width:_9.w+"px",height:_9.h+"px"}},this.controller.assistant.domNode);
this.connect(this.mask,"onclick",function(){
_6.onChoose&&_6.onChoose();
_6.hide();
});
},postCreate:function(){
this.subscribe("/dojox/mobile/app/goback",this._handleSelect);
},_handleSelect:function(_a){
var _b;
if(_a&&_a.target){
_b=_a.target;
while(!_1.byNode(_b)){
_b-_b.parentNode;
}
}
if(this.onChoose){
this.onChoose(_b?_1.byNode(_b)._dialogValue:undefined);
}
this.hide();
},show:function(){
this._doTransition(1);
},hide:function(){
this._doTransition(-1);
},_doTransition:function(_c){
var _d;
var h=_2.marginBox(this.domNode.firstChild).h;
var _e=this.controller.getWindowSize().h;
var _f=_e-h;
var low=_e;
var _10=_2.fx.slideTo({node:this.domNode,duration:400,top:{start:_c<0?_f:low,end:_c<0?low:_f}});
var _11=_2[_c<0?"fadeOut":"fadeIn"]({node:this.mask,duration:400});
var _d=_2.fx.combine([_10,_11]);
var _12=this;
_2.connect(_d,"onEnd",this,function(){
if(_c<0){
_12.domNode.style.display="none";
_2.destroy(_12.domNode);
_2.destroy(_12.mask);
}
});
_d.play();
},destroy:function(){
this.inherited(arguments);
_2.destroy(this.mask);
},onClick:function(){
}});
});

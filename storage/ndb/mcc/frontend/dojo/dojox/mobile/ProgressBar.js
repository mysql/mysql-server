//>>built
define("dojox/mobile/ProgressBar",["dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dijit/_WidgetBase"],function(_1,_2,_3,_4){
return _1("dojox.mobile.ProgressBar",_4,{value:"0",maximum:100,label:"",baseClass:"mblProgressBar",buildRendering:function(){
this.inherited(arguments);
this.progressNode=_3.create("div",{className:"mblProgressBarProgress"},this.domNode);
this.msgNode=_3.create("div",{className:"mblProgressBarMsg"},this.domNode);
},_setValueAttr:function(_5){
_5+="";
this._set("value",_5);
var _6=Math.min(100,(_5.indexOf("%")!=-1?parseFloat(_5):this.maximum?100*_5/this.maximum:0));
this.progressNode.style.width=_6+"%";
_2.toggle(this.progressNode,"mblProgressBarNotStarted",!_6);
_2.toggle(this.progressNode,"mblProgressBarComplete",_6==100);
this.onChange(_5,this.maximum,_6);
},_setLabelAttr:function(_7){
this.msgNode.innerHTML=_7;
},onChange:function(){
}});
});

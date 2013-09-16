//>>built
define("dojox/mobile/Button",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dijit/_WidgetBase","dijit/form/_ButtonMixin","dijit/form/_FormWidgetMixin"],function(_1,_2,_3,_4,_5,_6,_7){
return _2("dojox.mobile.Button",[_5,_7,_6],{baseClass:"mblButton",_setTypeAttr:null,duration:1000,_onClick:function(e){
var _8=this.inherited(arguments);
if(_8&&this.duration>=0){
var _9=this.focusNode||this.domNode;
var _a=(this.baseClass+" "+this["class"]).split(" ");
_a=_1.map(_a,function(c){
return c+"Selected";
});
_3.add(_9,_a);
setTimeout(function(){
_3.remove(_9,_a);
},this.duration);
}
return _8;
},isFocusable:function(){
return false;
},buildRendering:function(){
if(!this.srcNodeRef){
this.srcNodeRef=_4.create("button",{"type":this.type});
}else{
if(this._cv){
var n=this.srcNodeRef.firstChild;
if(n&&n.nodeType===3){
n.nodeValue=this._cv(n.nodeValue);
}
}
}
this.inherited(arguments);
this.focusNode=this.domNode;
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onclick","_onClick");
},_setLabelAttr:function(_b){
this.inherited(arguments,[this._cv?this._cv(_b):_b]);
}});
});

//>>built
define("dojox/mobile/Button",["dojo/_base/array","dojo/_base/declare","dojo/_base/window","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/touch","dojo/on","./common","dijit/_WidgetBase","dijit/form/_ButtonMixin","dijit/form/_FormWidgetMixin","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/Button"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b,_c,_d){
var _e=_2(_c("dojo-bidi")?"dojox.mobile.NonBidiButton":"dojox.mobile.Button",[_9,_b,_a],{baseClass:"mblButton",_setTypeAttr:null,isFocusable:function(){
return false;
},buildRendering:function(){
if(!this.srcNodeRef){
this.srcNodeRef=_6.create("button",{"type":this.type});
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
this.domNode.dojoClick="useTarget";
var _f=this;
this.on(_7.press,function(e){
e.preventDefault();
if(_f.domNode.disabled){
return;
}
_f._press(true);
var _10=false;
_f._moveh=on(_3.doc,_7.move,function(e){
if(!_10){
e.preventDefault();
_10=true;
}
_f._press(_4.isDescendant(e.target,_f.domNode));
});
_f._endh=on(_3.doc,_7.release,function(e){
_f._press(false);
_f._moveh.remove();
_f._endh.remove();
});
});
_8.setSelectable(this.focusNode,false);
this.connect(this.domNode,"onclick","_onClick");
},_press:function(_11){
if(_11!=this._pressed){
this._pressed=_11;
var _12=this.focusNode||this.domNode;
var _13=(this.baseClass+" "+this["class"]).split(" ");
_13=_1.map(_13,function(c){
return c+"Selected";
});
_5.toggle(_12,_13,_11);
}
},_setLabelAttr:function(_14){
this.inherited(arguments,[this._cv?this._cv(_14):_14]);
}});
return _c("dojo-bidi")?_2("dojox.mobile.Button",[_e,_d]):_e;
});

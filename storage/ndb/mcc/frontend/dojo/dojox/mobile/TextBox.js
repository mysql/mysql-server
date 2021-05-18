//>>built
define("dojox/mobile/TextBox",["dojo/_base/declare","dojo/dom-construct","dijit/_WidgetBase","dijit/form/_FormValueMixin","dijit/form/_TextBoxMixin","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/TextBox"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_1(_6("dojo-bidi")?"dojox.mobile.NonBidiTextBox":"dojox.mobile.TextBox",[_3,_4,_5],{baseClass:"mblTextBox",_setTypeAttr:null,_setPlaceHolderAttr:function(_9){
_9=this._cv?this._cv(_9):_9;
this._set("placeHolder",_9);
this.textbox.setAttribute("placeholder",_9);
},buildRendering:function(){
if(!this.srcNodeRef){
this.srcNodeRef=_2.create("input",{"type":this.type});
}
this.inherited(arguments);
this.textbox=this.focusNode=this.domNode;
},postCreate:function(){
this.inherited(arguments);
this.connect(this.textbox,"onmouseup",function(){
this._mouseIsDown=false;
});
this.connect(this.textbox,"onmousedown",function(){
this._mouseIsDown=true;
});
this.connect(this.textbox,"onfocus",function(e){
this._onFocus(this._mouseIsDown?"mouse":e);
this._mouseIsDown=false;
});
this.connect(this.textbox,"onblur","_onBlur");
}});
return _6("dojo-bidi")?_1("dojox.mobile.TextBox",[_8,_7]):_8;
});

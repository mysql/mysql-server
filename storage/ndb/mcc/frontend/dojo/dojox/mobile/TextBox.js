//>>built
define("dojox/mobile/TextBox",["dojo/_base/declare","dojo/dom-construct","dijit/_WidgetBase","dijit/form/_FormValueMixin","dijit/form/_TextBoxMixin"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.TextBox",[_3,_4,_5],{baseClass:"mblTextBox",_setTypeAttr:null,_setPlaceHolderAttr:"textbox",buildRendering:function(){
if(!this.srcNodeRef){
this.srcNodeRef=_2.create("input",{"type":this.type});
}
this.inherited(arguments);
this.textbox=this.focusNode=this.domNode;
},postCreate:function(){
this.inherited(arguments);
this.connect(this.textbox,"onfocus","_onFocus");
this.connect(this.textbox,"onblur","_onBlur");
}});
});

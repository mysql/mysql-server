//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/layout/LayoutContainer,dijit/_Templated"],function(_1,_2,_3){
_2.provide("dojox.xmpp.widget.ChatSession");
_2.require("dijit.layout.LayoutContainer");
_2.require("dijit._Templated");
_2.declare("dojox.xmpp.widget.ChatSession",[_1.layout.LayoutContainer,_1._Templated],{templateString:_2.cache("dojox.xmpp.widget","templates/ChatSession.html","<div>\n<div dojoAttachPoint=\"messages\" dojoType=\"dijit.layout.ContentPane\" layoutAlign=\"client\" style=\"overflow:auto\">\n</div>\n<div dojoType=\"dijit.layout.ContentPane\" layoutAlign=\"bottom\" style=\"border-top: 2px solid #333333; height: 35px;\"><input dojoAttachPoint=\"chatInput\" dojoAttachEvent=\"onkeypress: onKeyPress\" style=\"width: 100%;height: 35px;\" /></div>\n</div>"),enableSubWidgets:true,widgetsInTemplate:true,widgetType:"ChatSession",chatWith:null,instance:null,postCreate:function(){
},displayMessage:function(_4,_5){
if(_4){
var _6=_4.from?this.chatWith:"me";
this.messages.domNode.innerHTML+="<b>"+_6+":</b> "+_4.body+"<br/>";
this.goToLastMessage();
}
},goToLastMessage:function(){
this.messages.domNode.scrollTop=this.messages.domNode.scrollHeight;
},onKeyPress:function(e){
var _7=e.keyCode||e.charCode;
if((_7==_2.keys.ENTER)&&(this.chatInput.value!="")){
this.instance.sendMessage({body:this.chatInput.value});
this.displayMessage({body:this.chatInput.value},"out");
this.chatInput.value="";
}
}});
});

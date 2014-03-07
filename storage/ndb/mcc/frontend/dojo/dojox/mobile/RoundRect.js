//>>built
define("dojox/mobile/RoundRect",["dojo/_base/array","dojo/_base/declare","dojo/_base/window","dijit/_Contained","dijit/_Container","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6){
return _2("dojox.mobile.RoundRect",[_6,_5,_4],{shadow:false,buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_3.doc.createElement("DIV");
this.domNode.className=this.shadow?"mblRoundRect mblShadow":"mblRoundRect";
},resize:function(){
_1.forEach(this.getChildren(),function(_7){
if(_7.resize){
_7.resize();
}
});
}});
});

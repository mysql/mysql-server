//>>built
define("dojox/mobile/RoundRectCategory",["dojo/_base/declare","dojo/_base/window","dijit/_Contained","dijit/_WidgetBase"],function(_1,_2,_3,_4){
return _1("dojox.mobile.RoundRectCategory",[_4,_3],{label:"",buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_2.doc.createElement("H2");
this.domNode.className="mblRoundRectCategory";
if(!this.label){
this.label=this.domNode.innerHTML;
}
},_setLabelAttr:function(_5){
this.label=_5;
this.domNode.innerHTML=this._cv?this._cv(_5):_5;
}});
});

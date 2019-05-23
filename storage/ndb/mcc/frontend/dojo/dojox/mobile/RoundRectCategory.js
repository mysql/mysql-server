//>>built
define("dojox/mobile/RoundRectCategory",["dojo/_base/declare","dojo/_base/window","dojo/dom-construct","dijit/_Contained","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.RoundRectCategory",[_5,_4],{label:"",tag:"h2",baseClass:"mblRoundRectCategory",buildRendering:function(){
var _6=this.domNode=this.containerNode=this.srcNodeRef||_3.create(this.tag);
this.inherited(arguments);
if(!this.label&&_6.childNodes.length===1&&_6.firstChild.nodeType===3){
this.label=_6.firstChild.nodeValue;
}
},_setLabelAttr:function(_7){
this.label=_7;
this.domNode.innerHTML=this._cv?this._cv(_7):_7;
}});
});

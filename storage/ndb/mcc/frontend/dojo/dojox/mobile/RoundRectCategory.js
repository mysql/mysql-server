//>>built
define("dojox/mobile/RoundRectCategory",["dojo/_base/declare","dojo/_base/window","dojo/dom-construct","dijit/_Contained","dijit/_WidgetBase","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/RoundRectCategory"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_1(_6("dojo-bidi")?"dojox.mobile.NonBidiRoundRectCategory":"dojox.mobile.RoundRectCategory",[_5,_4],{label:"",tag:"h2",baseClass:"mblRoundRectCategory",buildRendering:function(){
var _9=this.domNode=this.containerNode=this.srcNodeRef||_3.create(this.tag);
this.inherited(arguments);
if(!this.label&&_9.childNodes.length===1&&_9.firstChild.nodeType===3){
this.label=_9.firstChild.nodeValue;
}
},_setLabelAttr:function(_a){
this.label=_a;
this.domNode.innerHTML=this._cv?this._cv(_a):_a;
}});
return _6("dojo-bidi")?_1("dojox.mobile.RoundRectCategory",[_8,_7]):_8;
});

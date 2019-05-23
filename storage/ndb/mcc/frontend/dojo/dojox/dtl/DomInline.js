//>>built
define("dojox/dtl/DomInline",["dojo/_base/lang","./dom","./_base","dijit/_WidgetBase"],function(_1,_2,dd,_3){
dd.DomInline=_1.extend(function(_4,_5){
this.create(_4,_5);
},_3.prototype,{context:null,render:function(_6){
this.context=_6||this.context;
this.postMixInProperties();
var _7=this.template.render(this.context).getRootNode();
if(_7!=this.containerNode){
this.containerNode.parentNode.replaceChild(_7,this.containerNode);
this.containerNode=_7;
}
},declaredClass:"dojox.dtl.Inline",buildRendering:function(){
var _8=this.domNode=document.createElement("div");
this.containerNode=_8.appendChild(document.createElement("div"));
var _9=this.srcNodeRef;
if(_9.parentNode){
_9.parentNode.replaceChild(_8,_9);
}
this.template=new dojox.dtl.DomTemplate(_1.trim(_9.text),true);
this.render();
},postMixInProperties:function(){
this.context=(this.context.get===dojox.dtl._Context.prototype.get)?this.context:new dojox.dtl.Context(this.context);
}});
return dojox.dtl.DomInline;
});

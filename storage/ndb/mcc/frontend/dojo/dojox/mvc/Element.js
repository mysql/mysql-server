//>>built
define("dojox/mvc/Element",["dojo/_base/declare","dijit/_WidgetBase"],function(_1,_2){
return _1("dojox.mvc.Element",_2,{_setInnerTextAttr:{node:"domNode",type:"innerText"},_setInnerHTMLAttr:{node:"domNode",type:"innerHTML"},buildRendering:function(){
this.inherited(arguments);
if(/select|input|textarea/i.test(this.domNode.tagName)){
var _3=this,_4=this.focusNode=this.domNode;
this.on("change",function(e){
var _5=/^checkbox$/i.test(_4.getAttribute("type"))?"checked":"value";
_3._set(_5,_3.get(_5));
});
}
},_getCheckedAttr:function(){
return this.domNode.checked;
},_getValueAttr:function(){
return this.domNode.value;
}});
});

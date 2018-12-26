//>>built
define("dojox/mobile/TextArea",["dojo/_base/declare","dojo/dom-construct","./TextBox"],function(_1,_2,_3){
return _1("dojox.mobile.TextArea",_3,{baseClass:"mblTextArea",postMixInProperties:function(){
if(!this.value&&this.srcNodeRef){
this.value=this.srcNodeRef.value;
}
this.inherited(arguments);
},buildRendering:function(){
if(!this.srcNodeRef){
this.srcNodeRef=_2.create("textarea",{});
}
this.inherited(arguments);
}});
});

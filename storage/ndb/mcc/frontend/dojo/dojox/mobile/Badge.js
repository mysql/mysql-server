//>>built
define("dojox/mobile/Badge",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","./iconUtils"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.Badge",null,{value:"0",className:"mblDomButtonRedBadge",fontSize:16,constructor:function(_6,_7){
if(_6){
_2.mixin(this,_6);
}
this.domNode=_7?_7:_4.create("div");
_3.add(this.domNode,"mblBadge");
if(this.domNode.className.indexOf("mblDomButton")===-1){
_3.add(this.domNode,this.className);
}
if(this.fontSize!==16){
this.domNode.style.fontSize=this.fontSize+"px";
}
_5.createDomButton(this.domNode);
this.setValue(this.value);
},getValue:function(){
return this.domNode.firstChild.innerHTML||"";
},setValue:function(_8){
this.domNode.firstChild.innerHTML=_8;
}});
});

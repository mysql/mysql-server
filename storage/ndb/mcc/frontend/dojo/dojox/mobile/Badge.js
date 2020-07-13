//>>built
define("dojox/mobile/Badge",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","./iconUtils","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/Badge"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_1(_6("dojo-bidi")?"dojox.mobile.NonBidiBadge":"dojox.mobile.Badge",null,{value:"0",className:"mblDomButtonRedBadge",fontSize:16,constructor:function(_9,_a){
if(_9){
_2.mixin(this,_9);
}
this.domNode=_a?_a:_4.create("div");
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
},setValue:function(_b){
this.domNode.firstChild.innerHTML=_b;
}});
return _6("dojo-bidi")?_1("dojox.mobile.Badge",[_8,_7]):_8;
});

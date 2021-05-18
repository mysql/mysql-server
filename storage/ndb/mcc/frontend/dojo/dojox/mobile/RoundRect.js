//>>built
define("dojox/mobile/RoundRect",["dojo/_base/declare","dojo/dom-class","./Container"],function(_1,_2,_3){
return _1("dojox.mobile.RoundRect",_3,{shadow:false,baseClass:"mblRoundRect",buildRendering:function(){
this.inherited(arguments);
if(this.shadow){
_2.add(this.domNode,"mblShadow");
}
}});
});

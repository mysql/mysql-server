//>>built
define("dojox/mobile/FormLayout",["dojo/_base/declare","dojo/dom-class","./Container","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/FormLayout"],function(_1,_2,_3,_4,_5){
var _6=_1(_4("dojo-bidi")?"dojox.mobile.NonBidiFormLayout":"dojox.mobile.FormLayout",_3,{columns:"auto",rightAlign:false,baseClass:"mblFormLayout",buildRendering:function(){
this.inherited(arguments);
if(this.columns=="auto"){
_2.add(this.domNode,"mblFormLayoutAuto");
}else{
if(this.columns=="single"){
_2.add(this.domNode,"mblFormLayoutSingleCol");
}else{
if(this.columns=="two"){
_2.add(this.domNode,"mblFormLayoutTwoCol");
}
}
}
if(this.rightAlign){
_2.add(this.domNode,"mblFormLayoutRightAlign");
}
}});
return _4("dojo-bidi")?_1("dojox.mobile.FormLayout",[_6,_5]):_6;
});

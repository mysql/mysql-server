//>>built
define("dojox/mobile/Icon",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","./iconUtils"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.Icon",null,{icon:"",iconPos:"",alt:"",tag:"div",constructor:function(_6,_7){
if(_6){
_2.mixin(this,_6);
}
this.domNode=_7||_4.create(this.tag);
_5.createIcon(this.icon,this.iconPos,null,this.alt,this.domNode);
}});
});

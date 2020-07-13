//>>built
define("dojox/mobile/Icon",["dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","./iconUtils","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/Icon"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_1(_6("dojo-bidi")?"dojox.mobile.NonBidiIcon":"dojox.mobile.Icon",null,{icon:"",iconPos:"",alt:"",tag:"div",constructor:function(_9,_a){
if(_9){
_2.mixin(this,_9);
}
this.domNode=_a||_4.create(this.tag);
_5.createIcon(this.icon,this.iconPos,null,this.alt,this.domNode);
this._setCustomTransform();
},_setCustomTransform:function(){
}});
return _6("dojo-bidi")?_1("dojox.mobile.Icon",[_8,_7]):_8;
});

//>>built
define("dojox/dtl/render/dom",["dojo/_base/lang","dojo/dom","../Context","../dom","../_base"],function(_1,_2,_3,_4,dd){
var _5=_1.getObject("render.dom",true,dd);
_5.Render=function(_6,_7){
this._tpl=_7;
this.domNode=_2.byId(_6);
};
_1.extend(_5.Render,{setAttachPoint:function(_8){
this.domNode=_8;
},render:function(_9,_a,_b){
if(!this.domNode){
throw new Error("You cannot use the Render object without specifying where you want to render it");
}
this._tpl=_a=_a||this._tpl;
_b=_b||_a.getBuffer();
_9=_9||new _3();
var _c=_a.render(_9,_b).getParent();
if(!_c){
throw new Error("Rendered template does not have a root node");
}
if(this.domNode!==_c){
if(this.domNode.parentNode){
this.domNode.parentNode.replaceChild(_c,this.domNode);
}
this.domNode=_c;
}
}});
return _5;
});

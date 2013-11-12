//>>built
define("dojox/dtl/render/dom",["dojo/_base/lang","dojo/dom","../Context","../dom","../_base"],function(_1,_2,_3,_4,dd){
_1.getObject("dojox.dtl.render.dom",true);
dd.render.dom.Render=function(_5,_6){
this._tpl=_6;
this.domNode=_2.byId(_5);
};
_1.extend(dd.render.dom.Render,{setAttachPoint:function(_7){
this.domNode=_7;
},render:function(_8,_9,_a){
if(!this.domNode){
throw new Error("You cannot use the Render object without specifying where you want to render it");
}
this._tpl=_9=_9||this._tpl;
_a=_a||_9.getBuffer();
_8=_8||new _3();
var _b=_9.render(_8,_a).getParent();
if(!_b){
throw new Error("Rendered template does not have a root node");
}
if(this.domNode!==_b){
this.domNode.parentNode.replaceChild(_b,this.domNode);
this.domNode=_b;
}
}});
return dojox.dtl.render.dom;
});

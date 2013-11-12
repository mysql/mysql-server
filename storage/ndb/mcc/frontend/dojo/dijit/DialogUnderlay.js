//>>built
define("dijit/DialogUnderlay",["dojo/_base/declare","dojo/dom-attr","dojo/_base/window","dojo/window","./_Widget","./_TemplatedMixin","./BackgroundIframe"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dijit.DialogUnderlay",[_5,_6],{templateString:"<div class='dijitDialogUnderlayWrapper'><div class='dijitDialogUnderlay' data-dojo-attach-point='node'></div></div>",dialogId:"","class":"",_setDialogIdAttr:function(id){
_2.set(this.node,"id",id+"_underlay");
this._set("dialogId",id);
},_setClassAttr:function(_8){
this.node.className="dijitDialogUnderlay "+_8;
this._set("class",_8);
},postCreate:function(){
_3.body().appendChild(this.domNode);
},layout:function(){
var is=this.node.style,os=this.domNode.style;
os.display="none";
var _9=_4.getBox();
os.top=_9.t+"px";
os.left=_9.l+"px";
is.width=_9.w+"px";
is.height=_9.h+"px";
os.display="block";
},show:function(){
this.domNode.style.display="block";
this.layout();
this.bgIframe=new _7(this.domNode);
},hide:function(){
this.bgIframe.destroy();
delete this.bgIframe;
this.domNode.style.display="none";
}});
});

//>>built
define("dijit/DialogUnderlay",["dojo/_base/declare","dojo/dom-attr","dojo/window","./_Widget","./_TemplatedMixin","./BackgroundIframe"],function(_1,_2,_3,_4,_5,_6){
return _1("dijit.DialogUnderlay",[_4,_5],{templateString:"<div class='dijitDialogUnderlayWrapper'><div class='dijitDialogUnderlay' data-dojo-attach-point='node'></div></div>",dialogId:"","class":"",_setDialogIdAttr:function(id){
_2.set(this.node,"id",id+"_underlay");
this._set("dialogId",id);
},_setClassAttr:function(_7){
this.node.className="dijitDialogUnderlay "+_7;
this._set("class",_7);
},postCreate:function(){
this.ownerDocumentBody.appendChild(this.domNode);
},layout:function(){
var is=this.node.style,os=this.domNode.style;
os.display="none";
var _8=_3.getBox(this.ownerDocument);
os.top=_8.t+"px";
os.left=_8.l+"px";
is.width=_8.w+"px";
is.height=_8.h+"px";
os.display="block";
},show:function(){
this.domNode.style.display="block";
this.layout();
this.bgIframe=new _6(this.domNode);
},hide:function(){
this.bgIframe.destroy();
delete this.bgIframe;
this.domNode.style.display="none";
}});
});

//>>built
define("dojox/mobile/bidi/Switch",["dojo/_base/declare","./common","dojo/dom-class"],function(_1,_2,_3){
return _1(null,{postCreate:function(){
this.inherited(arguments);
if(!this.textDir&&this.getParent()&&this.getParent().get("textDir")){
this.set("textDir",this.getParent().get("textDir"));
}
},buildRendering:function(){
this.inherited(arguments);
if(!this.isLeftToRight()){
_3.add(this.left,"mblSwitchBgLeftRtl");
_3.add(this.left.firstChild,"mblSwitchTextLeftRtl");
_3.add(this.right,"mblSwitchBgRightRtl");
_3.add(this.right.firstChild,"mblSwitchTextRightRtl");
}
},_newState:function(_4){
if(this.isLeftToRight()){
return this.inherited(arguments);
}
return (this.inner.offsetLeft<-(this._width/2))?"on":"off";
},_setLeftLabelAttr:function(_5){
this.inherited(arguments);
this.left.firstChild.innerHTML=_2.enforceTextDirWithUcc(this.left.firstChild.innerHTML,this.textDir);
},_setRightLabelAttr:function(_6){
this.inherited(arguments);
this.right.firstChild.innerHTML=_2.enforceTextDirWithUcc(this.right.firstChild.innerHTML,this.textDir);
},_setTextDirAttr:function(_7){
if(_7&&(!this._created||this.textDir!==_7)){
this.textDir=_7;
this.left.firstChild.innerHTML=_2.removeUCCFromText(this.left.firstChild.innerHTML);
this.left.firstChild.innerHTML=_2.enforceTextDirWithUcc(this.left.firstChild.innerHTML,this.textDir);
this.right.firstChild.innerHTML=_2.removeUCCFromText(this.right.firstChild.innerHTML);
this.right.firstChild.innerHTML=_2.enforceTextDirWithUcc(this.right.firstChild.innerHTML,this.textDir);
}
}});
});

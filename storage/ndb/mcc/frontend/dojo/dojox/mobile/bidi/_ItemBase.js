//>>built
define("dojox/mobile/bidi/_ItemBase",["dojo/_base/declare","./common"],function(_1,_2){
return _1(null,{_setLabelAttr:function(_3){
this._set("label",_3);
this.labelNode.innerHTML=this._cv?this._cv(_3):_3;
if(!this.textDir){
var p=this.getParent();
this.textDir=p&&p.get("textDir")?p.get("textDir"):"";
}
this.labelNode.innerHTML=_2.enforceTextDirWithUcc(this.labelNode.innerHTML,this.textDir);
},_setTextDirAttr:function(_4){
if(!this._created||this.textDir!==_4){
this._set("textDir",_4);
this.labelNode.innerHTML=_2.enforceTextDirWithUcc(_2.removeUCCFromText(this.labelNode.innerHTML),this.textDir);
if(this.badgeObj&&this.badgeObj.setTextDir){
this.badgeObj.setTextDir(_4);
}
}
},getTransOpts:function(){
var _5=this.inherited(arguments);
if(!this.isLeftToRight()){
_5.transitionDir=_5.transitionDir*-1;
}
return _5;
}});
});

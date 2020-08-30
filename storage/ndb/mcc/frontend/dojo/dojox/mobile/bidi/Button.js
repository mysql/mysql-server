//>>built
define("dojox/mobile/bidi/Button",["dojo/_base/declare","./common"],function(_1,_2){
return _1(null,{_setLabelAttr:function(_3){
this.inherited(arguments,[this._cv?this._cv(_3):_3]);
this.focusNode.innerHTML=_2.enforceTextDirWithUcc(this.focusNode.innerHTML,this.textDir);
},_setTextDirAttr:function(_4){
if(!this._created||this.textDir!==_4){
this._set("textDir",_4);
this.focusNode.innerHTML=_2.enforceTextDirWithUcc(_2.removeUCCFromText(this.focusNode.innerHTML),this.textDir);
}
}});
});

//>>built
define("dojox/mobile/bidi/ValuePickerSlot",["dojo/_base/declare","./common"],function(_1,_2){
return _1(null,{postCreate:function(){
if(!this.textDir&&this.getParent()&&this.getParent().get("textDir")){
this.textDir=this.getParent().get("textDir");
}
},_getValueAttr:function(){
return _2.removeUCCFromText(this.inputNode.value);
},_setValueAttr:function(_3){
this.inherited(arguments);
this._applyTextDirToValueNode();
},_setTextDirAttr:function(_4){
if(_4&&this.textDir!==_4){
this.textDir=_4;
this._applyTextDirToValueNode();
}
},_applyTextDirToValueNode:function(){
this.inputNode.value=_2.removeUCCFromText(this.inputNode.value);
this.inputNode.value=_2.enforceTextDirWithUcc(this.inputNode.value,this.textDir);
}});
});

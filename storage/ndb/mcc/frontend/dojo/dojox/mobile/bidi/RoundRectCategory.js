//>>built
define("dojox/mobile/bidi/RoundRectCategory",["dojo/_base/declare","./common"],function(_1,_2){
return _1(null,{_setLabelAttr:function(_3){
if(this.textDir){
_3=_2.enforceTextDirWithUcc(_3,this.textDir);
}
this.inherited(arguments);
},_setTextDirAttr:function(_4){
if(_4&&this.textDir!==_4){
this.textDir=_4;
this.label=_2.removeUCCFromText(this.label);
this.set("label",this.label);
}
}});
});

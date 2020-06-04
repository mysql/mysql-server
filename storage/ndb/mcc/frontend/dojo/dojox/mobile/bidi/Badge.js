//>>built
define("dojox/mobile/bidi/Badge",["dojo/_base/declare","./common"],function(_1,_2){
return _1(null,{textDir:"",setValue:function(_3){
this.domNode.firstChild.innerHTML=_2.enforceTextDirWithUcc(_3,this.textDir);
},setTextDir:function(_4){
if(this.textDir!==_4){
this.textDir=_4;
this.domNode.firstChild.innerHTML=_2.enforceTextDirWithUcc(_2.removeUCCFromText(this.domNode.firstChild.innerHTML),this.textDir);
}
}});
});

//>>built
define("dojox/mobile/bidi/CarouselItem",["dojo/_base/declare","./common"],function(_1,_2){
return _1(null,{_setHeaderTextAttr:function(_3){
this._set("headerText",_3);
this.headerTextNode.innerHTML=this._cv?this._cv(_3):_3;
var p=this.getParent()?this.getParent().getParent():null;
this.textDir=this.textDir?this.textDir:p?p.get("textDir"):"";
if(this.textDir){
this.headerTextNode.innerHTML=_2.enforceTextDirWithUcc(this.headerTextNode.innerHTML,this.textDir);
}
},_setFooterTextAttr:function(_4){
this._set("footerText",_4);
this.footerTextNode.innerHTML=this._cv?this._cv(_4):_4;
var p=this.getParent()?this.getParent().getParent():null;
this.textDir=this.textDir?this.textDir:p?p.get("textDir"):"";
if(this.textDir){
this.footerTextNode.innerHTML=_BidiSupport.enforceTextDirWithUcc(this.footerTextNode.innerHTML,this.textDir);
}
}});
});

//>>built
define("dojox/mobile/bidi/IconMenu",["dojo/_base/declare","./common"],function(_1,_2){
return _1(null,{_setTextDirAttr:function(_3){
if(!this._created||this.textDir!==_3){
this._set("textDir",_3);
_2.setTextDirForButtons(this);
}
}});
});

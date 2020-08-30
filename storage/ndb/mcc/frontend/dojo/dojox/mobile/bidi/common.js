//>>built
define("dojox/mobile/bidi/common",["dojo/_base/array","dijit/_BidiSupport"],function(_1,_2){
var _3={};
_3.enforceTextDirWithUcc=function(_4,_5){
if(_5){
_5=(_5==="auto")?_2.prototype._checkContextual(_4):_5;
return ((_5==="rtl")?_3.MARK.RLE:_3.MARK.LRE)+_4+_3.MARK.PDF;
}
return _4;
};
_3.removeUCCFromText=function(_6){
if(!_6){
return _6;
}
return _6.replace(/\u202A|\u202B|\u202C/g,"");
};
_3.setTextDirForButtons=function(_7){
var _8=_7.getChildren();
if(_8&&_7.textDir){
_1.forEach(_8,function(ch){
ch.set("textDir",_7.textDir);
},_7);
}
};
_3.MARK={LRE:"‪",RLE:"‫",PDF:"‬"};
return _3;
});

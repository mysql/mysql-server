//>>built
define("dijit/_BidiSupport",["./_WidgetBase"],function(_1){
_1.extend({getTextDir:function(_2){
return this.textDir=="auto"?this._checkContextual(_2):this.textDir;
},_checkContextual:function(_3){
var _4=/[A-Za-z\u05d0-\u065f\u066a-\u06ef\u06fa-\u07ff\ufb1d-\ufdff\ufe70-\ufefc]/.exec(_3);
return _4?(_4[0]<="z"?"ltr":"rtl"):this.dir?this.dir:this.isLeftToRight()?"ltr":"rtl";
},applyTextDir:function(_5,_6){
var _7=this.textDir=="auto"?this._checkContextual(_6):this.textDir;
if(_5.dir!=_7){
_5.dir=_7;
}
},enforceTextDirWithUcc:function(_8,_9){
if(this.textDir){
_8.originalText=_9;
var _a=this.textDir=="auto"?this._checkContextual(_9):this.textDir;
return (_a=="ltr"?_b.LRE:_b.RLE)+_9+_b.PDF;
}
return _9;
},restoreOriginalText:function(_c){
if(_c.originalText){
_c.text=_c.originalText;
delete _c.originalText;
}
return _c;
}});
var _b={LRM:"‎",LRE:"‪",PDF:"‬",RLM:"‏",RLE:"‫"};
return _1;
});

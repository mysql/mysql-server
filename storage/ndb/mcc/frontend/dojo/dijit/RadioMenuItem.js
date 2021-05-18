//>>built
define("dijit/RadioMenuItem",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","dojo/query!css2","./CheckedMenuItem","./registry"],function(_1,_2,_3,_4,_5,_6){
return _2("dijit.RadioButtonMenuItem",_5,{baseClass:"dijitMenuItem dijitRadioMenuItem",role:"menuitemradio",checkedChar:"*",group:"",_setGroupAttr:"domNode",_setCheckedAttr:function(_7){
this.inherited(arguments);
if(!this._created){
return;
}
if(_7&&this.group){
_1.forEach(this._getRelatedWidgets(),function(_8){
if(_8!=this&&_8.checked){
_8.set("checked",false);
}
},this);
}
},_onClick:function(_9){
if(!this.disabled&&!this.checked){
this.set("checked",true);
this.onChange(true);
}
this.onClick(_9);
},_getRelatedWidgets:function(){
var _a=[];
_4("[group="+this.group+"][role="+this.role+"]").forEach(function(_b){
var _c=_6.getEnclosingWidget(_b);
if(_c){
_a.push(_c);
}
});
return _a;
}});
});

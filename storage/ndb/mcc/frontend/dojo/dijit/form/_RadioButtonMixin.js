//>>built
define("dijit/form/_RadioButtonMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/_base/event","dojo/_base/lang","dojo/query","dojo/_base/window","../registry"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dijit.form._RadioButtonMixin",null,{type:"radio",_getRelatedWidgets:function(){
var _9=[];
_6("input[type=radio]",this.focusNode.form||_7.doc).forEach(_5.hitch(this,function(_a){
if(_a.name==this.name&&_a.form==this.focusNode.form){
var _b=_8.getEnclosingWidget(_a);
if(_b){
_9.push(_b);
}
}
}));
return _9;
},_setCheckedAttr:function(_c){
this.inherited(arguments);
if(!this._created){
return;
}
if(_c){
_1.forEach(this._getRelatedWidgets(),_5.hitch(this,function(_d){
if(_d!=this&&_d.checked){
_d.set("checked",false);
}
}));
}
},_onClick:function(e){
if(this.checked||this.disabled){
_4.stop(e);
return false;
}
if(this.readOnly){
_4.stop(e);
_1.forEach(this._getRelatedWidgets(),_5.hitch(this,function(_e){
_3.set(this.focusNode||this.domNode,"checked",_e.checked);
}));
return false;
}
return this.inherited(arguments);
}});
});

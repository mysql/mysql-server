//>>built
define("dijit/form/_RadioButtonMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/_base/event","dojo/_base/lang","dojo/query","../registry"],function(_1,_2,_3,_4,_5,_6,_7){
return _2("dijit.form._RadioButtonMixin",null,{type:"radio",_getRelatedWidgets:function(){
var _8=[];
_6("input[type=radio]",this.focusNode.form||this.ownerDocument).forEach(_5.hitch(this,function(_9){
if(_9.name==this.name&&_9.form==this.focusNode.form){
var _a=_7.getEnclosingWidget(_9);
if(_a){
_8.push(_a);
}
}
}));
return _8;
},_setCheckedAttr:function(_b){
this.inherited(arguments);
if(!this._created){
return;
}
if(_b){
_1.forEach(this._getRelatedWidgets(),_5.hitch(this,function(_c){
if(_c!=this&&_c.checked){
_c.set("checked",false);
}
}));
}
},_getSubmitValue:function(_d){
return _d==null?"on":_d;
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

//>>built
define("dijit/form/MultiSelect",["dojo/_base/array","dojo/_base/declare","dojo/dom-geometry","dojo/sniff","dojo/query","./_FormValueWidget","dojo/NodeList-dom"],function(_1,_2,_3,_4,_5,_6){
var _7=_2("dijit.form.MultiSelect"+(_4("dojo-bidi")?"_NoBidi":""),_6,{size:7,baseClass:"dijitMultiSelect",templateString:"<select multiple='multiple' ${!nameAttrSetting} data-dojo-attach-point='containerNode,focusNode' data-dojo-attach-event='onchange: _onChange'></select>",addSelected:function(_8){
_8.getSelected().forEach(function(n){
this.containerNode.appendChild(n);
this.domNode.scrollTop=this.domNode.offsetHeight;
var _9=_8.domNode.scrollTop;
_8.domNode.scrollTop=0;
_8.domNode.scrollTop=_9;
},this);
this._set("value",this.get("value"));
},getSelected:function(){
return _5("option",this.containerNode).filter(function(n){
return n.selected;
});
},_getValueAttr:function(){
return _1.map(this.getSelected(),function(n){
return n.value;
});
},multiple:true,_setMultipleAttr:function(_a){
},_setValueAttr:function(_b){
if(_4("android")){
_5("option",this.containerNode).orphan().forEach(function(n){
var _c=n.ownerDocument.createElement("option");
_c.value=n.value;
_c.selected=(_1.indexOf(_b,n.value)!=-1);
_c.text=n.text;
_c.originalText=n.originalText;
this.containerNode.appendChild(_c);
},this);
}else{
_5("option",this.containerNode).forEach(function(n){
n.selected=(_1.indexOf(_b,n.value)!=-1);
});
}
this.inherited(arguments);
},invertSelection:function(_d){
var _e=[];
_5("option",this.containerNode).forEach(function(n){
if(!n.selected){
_e.push(n.value);
}
});
this._setValueAttr(_e,!(_d===false||_d==null));
},_onChange:function(){
this._handleOnChange(this.get("value"),true);
},resize:function(_f){
if(_f){
_3.setMarginBox(this.domNode,_f);
}
},postCreate:function(){
this._set("value",this.get("value"));
this.inherited(arguments);
}});
if(_4("dojo-bidi")){
_7=_2("dijit.form.MultiSelect",_7,{addSelected:function(_10){
_10.getSelected().forEach(function(n){
n.text=this.enforceTextDirWithUcc(this.restoreOriginalText(n),n.text);
},this);
this.inherited(arguments);
},_setTextDirAttr:function(_11){
if((this.textDir!=_11||!this._created)&&this.enforceTextDirWithUcc){
this._set("textDir",_11);
_5("option",this.containerNode).forEach(function(_12){
if(!this._created&&_12.value===_12.text){
_12.value=_12.text;
}
_12.text=this.enforceTextDirWithUcc(_12,_12.originalText||_12.text);
},this);
}
}});
}
return _7;
});

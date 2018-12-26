//>>built
define("dijit/form/MultiSelect",["dojo/_base/array","dojo/_base/declare","dojo/dom-geometry","dojo/query","./_FormValueWidget"],function(_1,_2,_3,_4,_5){
return _2("dijit.form.MultiSelect",_5,{size:7,templateString:"<select multiple='true' ${!nameAttrSetting} data-dojo-attach-point='containerNode,focusNode' data-dojo-attach-event='onchange: _onChange'></select>",addSelected:function(_6){
_6.getSelected().forEach(function(n){
this.containerNode.appendChild(n);
this.domNode.scrollTop=this.domNode.offsetHeight;
var _7=_6.domNode.scrollTop;
_6.domNode.scrollTop=0;
_6.domNode.scrollTop=_7;
},this);
this._set("value",this.get("value"));
},getSelected:function(){
return _4("option",this.containerNode).filter(function(n){
return n.selected;
});
},_getValueAttr:function(){
return _1.map(this.getSelected(),function(n){
return n.value;
});
},multiple:true,_setValueAttr:function(_8,_9){
_4("option",this.containerNode).forEach(function(n){
n.selected=(_1.indexOf(_8,n.value)!=-1);
});
this.inherited(arguments);
},invertSelection:function(_a){
var _b=[];
_4("option",this.containerNode).forEach(function(n){
if(!n.selected){
_b.push(n.value);
}
});
this._setValueAttr(_b,!(_a===false||_a==null));
},_onChange:function(){
this._handleOnChange(this.get("value"),true);
},resize:function(_c){
if(_c){
_3.setMarginBox(this.domNode,_c);
}
},postCreate:function(){
this._set("value",this.get("value"));
this.inherited(arguments);
}});
});

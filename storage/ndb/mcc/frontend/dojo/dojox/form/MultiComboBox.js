//>>built
define("dojox/form/MultiComboBox",["dojo/_base/kernel","dijit/form/ValidationTextBox","dijit/form/ComboBoxMixin","dojo/_base/declare"],function(_1,_2,_3,_4){
_1.experimental("dojox.form.MultiComboBox");
return _4("dojox.form.MultiComboBox",[_2,_3],{delimiter:",",_previousMatches:false,_setValueAttr:function(_5){
if(this.delimiter&&_5.length!=0){
_5=_5+this.delimiter+" ";
arguments[0]=this._addPreviousMatches(_5);
}
this.inherited(arguments);
},_addPreviousMatches:function(_6){
if(this._previousMatches){
if(!_6.match(new RegExp("^"+this._previousMatches))){
_6=this._previousMatches+_6;
}
_6=this._cleanupDelimiters(_6);
}
return _6;
},_cleanupDelimiters:function(_7){
if(this.delimiter){
_7=_7.replace(new RegExp("  +")," ");
_7=_7.replace(new RegExp("^ *"+this.delimiter+"* *"),"");
_7=_7.replace(new RegExp(this.delimiter+" *"+this.delimiter),this.delimiter);
}
return _7;
},_autoCompleteText:function(_8){
arguments[0]=this._addPreviousMatches(_8);
this.inherited(arguments);
},_startSearch:function(_9){
_9=this._cleanupDelimiters(_9);
var re=new RegExp("^.*"+this.delimiter+" *");
if((this._previousMatches=_9.match(re))){
arguments[0]=_9.replace(re,"");
}
this.inherited(arguments);
}});
});

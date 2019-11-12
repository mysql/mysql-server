//>>built
define("dojox/form/DateTextBox",["dojo/_base/kernel","dojo/dom-style","dojox/widget/Calendar","dijit/form/_DateTimeTextBox","dojo/_base/declare"],function(_1,_2,_3,_4,_5){
_1.experimental("dojox/form/DateTextBox");
return _5("dojox.form.DateTextBox",_4,{popupClass:_3,_selector:"date",openDropDown:function(){
this.inherited(arguments);
_2.set(this.dropDown.domNode.parentNode,"position","absolute");
}});
});

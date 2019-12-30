//>>built
define("dijit/form/TimeTextBox",["dojo/_base/declare","dojo/keys","dojo/_base/lang","../_TimePicker","./_DateTimeTextBox"],function(_1,_2,_3,_4,_5){
return _1("dijit.form.TimeTextBox",_5,{baseClass:"dijitTextBox dijitComboBox dijitTimeTextBox",popupClass:_4,_selector:"time",value:new Date(""),_onInput:function(){
this.inherited(arguments);
var _6=this.get("displayedValue");
this.filterString=(_6&&!this.parse(_6,this.constraints))?_6.toLowerCase():"";
if(this._opened){
this.closeDropDown();
}
this.openDropDown();
}});
});

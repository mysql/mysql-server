//>>built
define("dijit/form/TimeTextBox",["dojo/_base/declare","dojo/keys","dojo/query","dojo/_base/lang","../_TimePicker","./_DateTimeTextBox"],function(_1,_2,_3,_4,_5,_6){
var _7=_1("dijit.form.TimeTextBox",_6,{baseClass:"dijitTextBox dijitComboBox dijitTimeTextBox",popupClass:_5,_selector:"time",value:new Date(""),maxHeight:-1,openDropDown:function(_8){
this.inherited(arguments);
var _9=_3(".dijitTimePickerItemSelected",this.dropDown.domNode),_a=this.dropDown.domNode.parentNode;
if(_9[0]){
_a.scrollTop=_9[0].offsetTop-(_a.clientHeight-_9[0].clientHeight)/2;
}else{
_a.scrollTop=(_a.scrollHeight-_a.clientHeight)/2;
}
this.dropDown.on("input",_4.hitch(this,function(){
this.set("value",this.dropDown.get("value"),false);
}));
},_onInput:function(){
this.inherited(arguments);
var _b=this.get("displayedValue");
this.filterString=(_b&&!this.parse(_b,this.constraints))?_b.toLowerCase():"";
if(this._opened){
this.closeDropDown();
}
this.openDropDown();
}});
return _7;
});

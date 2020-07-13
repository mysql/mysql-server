//>>built
define("dojox/mobile/ValuePickerDatePicker",["dojo/_base/declare","dojo/dom-class","dojo/dom-attr","./_DatePickerMixin","./ValuePicker","./ValuePickerSlot"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.mobile.ValuePickerDatePicker",[_5,_4],{readOnly:false,yearPlusBtnLabel:"",yearPlusBtnLabelRef:"",yearMinusBtnLabel:"",yearMinusBtnLabelRef:"",monthPlusBtnLabel:"",monthPlusBtnLabelRef:"",monthMinusBtnLabel:"",monthMinusBtnLabelRef:"",dayPlusBtnLabel:"",dayPlusBtnLabelRef:"",dayMinusBtnLabel:"",dayMinusBtnLabelRef:"",slotClasses:[_6,_6,_6],slotProps:[{labelFrom:1970,labelTo:2038,style:{width:"87px"}},{style:{width:"72px"}},{style:{width:"72px"}}],buildRendering:function(){
var p=this.slotProps;
p[0].readOnly=p[1].readOnly=p[2].readOnly=this.readOnly;
this._setBtnLabels(p);
this.initSlots();
this.inherited(arguments);
_2.add(this.domNode,"mblValuePickerDatePicker");
this._conn=[this.connect(this.slots[0],"_spinToValue","_onYearSet"),this.connect(this.slots[1],"_spinToValue","_onMonthSet"),this.connect(this.slots[2],"_spinToValue","_onDaySet")];
},disableValues:function(_7){
var _8=this.slots[2].items;
if(this._tail){
this.slots[2].items=_8=_8.concat(this._tail);
}
this._tail=_8.slice(_7);
_8.splice(_7);
},_setBtnLabels:function(_9){
_9[0].plusBtnLabel=this.yearPlusBtnLabel;
_9[0].plusBtnLabelRef=this.yearPlusBtnLabelRef;
_9[0].minusBtnLabel=this.yearMinusBtnLabel;
_9[0].minusBtnLabelRef=this.yearMinusBtnLabelRef;
_9[1].plusBtnLabel=this.monthPlusBtnLabel;
_9[1].plusBtnLabelRef=this.monthPlusBtnLabelRef;
_9[1].minusBtnLabel=this.monthMinusBtnLabel;
_9[1].minusBtnLabelRef=this.monthMinusBtnLabelRef;
_9[2].plusBtnLabel=this.dayPlusBtnLabel;
_9[2].plusBtnLabelRef=this.dayPlusBtnLabelRef;
_9[2].minusBtnLabel=this.dayMinusBtnLabel;
_9[2].minusBtnLabelRef=this.dayMinusBtnLabelRef;
}});
});

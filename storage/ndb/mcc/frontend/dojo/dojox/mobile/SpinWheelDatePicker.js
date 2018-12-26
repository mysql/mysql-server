//>>built
define("dojox/mobile/SpinWheelDatePicker",["dojo/_base/declare","dojo/dom-class","dojo/date","dojo/date/locale","./SpinWheel","./SpinWheelSlot"],function(_1,_2,_3,_4,_5,_6){
var _7=_1(_6,{buildRendering:function(){
this.labels=[];
if(this.labelFrom!==this.labelTo){
var _8=new Date(this.labelFrom,0,1);
var i,_9;
for(i=this.labelFrom,_9=0;i<=this.labelTo;i++,_9++){
_8.setFullYear(i);
this.labels.push(_4.format(_8,{datePattern:"yyyy",selector:"date"}));
}
}
this.inherited(arguments);
}});
var _a=_1(_6,{buildRendering:function(){
this.labels=[];
var _b=new Date(2000,0,1);
var _c;
for(var i=0;i<12;i++){
_b.setMonth(i);
_c=_4.format(_b,{datePattern:"MMM",selector:"date"});
this.labels.push(_c);
}
this.inherited(arguments);
}});
var _d=_1(_6,{});
return _1("dojox.mobile.SpinWheelDatePicker",_5,{slotClasses:[_7,_a,_d],slotProps:[{labelFrom:1970,labelTo:2038},{},{labelFrom:1,labelTo:31}],buildRendering:function(){
this.inherited(arguments);
_2.add(this.domNode,"mblSpinWheelDatePicker");
this.connect(this.slots[1],"onFlickAnimationEnd","onMonthSet");
this.connect(this.slots[2],"onFlickAnimationEnd","onDaySet");
},reset:function(){
var _e=this.slots;
var _f=new Date();
var _10=_4.format(_f,{datePattern:"MMM",selector:"date"});
this.setValue([_f.getFullYear(),_10,_f.getDate()]);
},onMonthSet:function(){
var _11=this.onDaySet();
var _12={28:[29,30,31],29:[30,31],30:[31],31:[]};
this.slots[2].disableValues(_12[_11]);
},onDaySet:function(){
var y=this.slots[0].getValue();
var m=this.slots[1].getValue();
var _13=_4.parse(y+"/"+m,{datePattern:"yyyy/MMM",selector:"date"});
var _14=_3.getDaysInMonth(_13);
var d=this.slots[2].getValue();
if(_14<d){
this.slots[2].setValue(_14);
}
return _14;
}});
});

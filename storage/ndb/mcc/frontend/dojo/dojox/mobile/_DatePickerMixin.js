//>>built
define("dojox/mobile/_DatePickerMixin",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/date","dojo/date/locale","dojo/date/stamp"],function(_1,_2,_3,_4,_5,_6){
var _7={format:function(d){
return _5.format(d,{datePattern:this.pattern,selector:"date",locale:this.picker.lang});
}};
var _8=_3.mixin({initLabels:function(){
this.labels=[];
if(this.labelFrom!==this.labelTo){
var d=new Date(this.labelFrom,0,1);
var i,_9;
for(i=this.labelFrom,_9=0;i<=this.labelTo;i++,_9++){
d.setFullYear(i);
this.labels.push(this.format(d));
}
}
}},_7);
var _a=_3.mixin({initLabels:function(){
this.labels=[];
var d=new Date(2000,0,16);
for(var i=0;i<12;i++){
d.setMonth(i);
this.labels.push(this.format(d));
}
}},_7);
var _b=_3.mixin({initLabels:function(){
this.labels=[];
var d=new Date(2000,0,1);
for(var i=1;i<=31;i++){
d.setDate(i);
this.labels.push(this.format(d));
}
}},_7);
return _2("dojox.mobile._DatePickerMixin",null,{yearPattern:"yyyy",monthPattern:"MMM",dayPattern:"d",initSlots:function(){
var c=this.slotClasses,p=this.slotProps;
c[0]=_2(c[0],_8);
c[1]=_2(c[1],_a);
c[2]=_2(c[2],_b);
p[2].picker=p[1].picker=p[0].picker=this;
p[0].pattern=this.yearPattern;
p[1].pattern=this.monthPattern;
p[2].pattern=this.dayPattern;
this.reorderSlots();
},reorderSlots:function(){
if(this.slotOrder.length){
return;
}
var a=_5._parseInfo({locale:this.lang}).bundle["dateFormat-short"].toLowerCase().split(/[^ymd]+/,3);
this.slotOrder=_1.map(a,function(_c){
return {y:0,m:1,d:2}[_c.charAt(0)];
});
},reset:function(){
var _d=new Date();
var v=_1.map(this.slots,function(w){
return w.format(_d);
});
this.set("colors",v);
this._disableEndDaysOfMonth();
if(this.value){
this.set("value",this.value);
this.value=null;
}else{
if(this.values){
this.set("values",this.values);
this.values=null;
}else{
this.set("values",v);
}
}
},_onYearSet:function(){
var _e=this.slots[0];
var _f=_e.get("value");
if(!(_e._previousValue&&_f==_e._previousValue)){
this._disableEndDaysOfMonth();
_e._previousValue=_f;
_e._set("value",_f);
this.onYearSet();
}
},onYearSet:function(){
},_onMonthSet:function(){
var _10=this.slots[1];
var _11=_10.get("value");
if(!(_10._previousValue&&_11==_10._previousValue)){
this._disableEndDaysOfMonth();
_10._previousValue=_11;
_10._set("value",_11);
this.onMonthSet();
}
},onMonthSet:function(){
},_onDaySet:function(){
var _12=this.slots[2];
var _13=_12.get("value");
if(!(_12._previousValue&&_13==_12._previousValue)){
if(!this._disableEndDaysOfMonth()){
_12._previousValue=_13;
_12._set("value",_13);
this.onDaySet();
}
}
},onDaySet:function(){
},_disableEndDaysOfMonth:function(){
var pat=this.slots[0].pattern+"/"+this.slots[1].pattern,v=this.get("values"),_14=_5.parse(v[0]+"/"+v[1],{datePattern:pat,selector:"date",locale:this.lang}),_15=_4.getDaysInMonth(_14);
var _16=false;
if(_15<v[2]){
_16=true;
this.slots[2]._spinToValue(_15,false);
}
this.disableValues(_15);
return _16;
},_getDateAttr:function(){
var v=this.get("values"),s=this.slots,pat=s[0].pattern+"/"+s[1].pattern+"/"+s[2].pattern;
return _5.parse(v[0]+"/"+v[1]+"/"+v[2],{datePattern:pat,selector:"date",locale:this.lang});
},_setValuesAttr:function(_17){
_1.forEach(this.getSlots(),function(w,i){
var v=_17[i];
if(typeof v=="number"){
var arr=[1970,1,1];
arr.splice(i,1,v-0);
v=w.format(new Date(arr[0],arr[1]-1,arr[2]));
}
w.set("value",v);
});
},_setValueAttr:function(_18){
var _19=_6.fromISOString(_18);
this.set("values",_1.map(this.slots,function(w){
return w.format(_19);
}));
},_getValueAttr:function(){
return _6.toISOString(this.get("date"),{selector:"date"});
}});
});

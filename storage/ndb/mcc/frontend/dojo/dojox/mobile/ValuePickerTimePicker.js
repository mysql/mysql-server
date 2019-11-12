//>>built
define("dojox/mobile/ValuePickerTimePicker",["dojo/_base/declare","dojo/dom-class","./_TimePickerMixin","./ToolBarButton","./ValuePicker","./ValuePickerSlot"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.mobile.ValuePickerTimePicker",[_5,_3],{readOnly:false,is24h:false,slotClasses:[_6,_6],slotProps:[{labelFrom:0,labelTo:23,style:{width:"72px"}},{labelFrom:0,labelTo:59,zeroPad:2,style:{width:"72px"}}],buildRendering:function(){
var p=this.slotProps;
p[0].readOnly=p[1].readOnly=this.readOnly;
this.inherited(arguments);
var _7=this.slots[0].items;
this._zero=_7.slice(0,1);
this._pm=_7.slice(13);
_2.add(this.domNode,"mblValuePickerTimePicker");
_2.add(this.slots[0].domNode,"mblValuePickerTimePickerHourSlot");
_2.add(this.slots[1].domNode,"mblValuePickerTimePickerMinuteSlot");
this.ampmButton=new _4();
this.addChild(this.ampmButton);
this._conn=[this.connect(this.ampmButton,"onClick","onBtnClick")];
this.set("is24h",this.is24h);
},to12h:function(a){
var h=a[0]-0;
var _8=h<12?"AM":"PM";
if(h==0){
h=12;
}else{
if(h>12){
h=h-12;
}
}
return [h+"",a[1],_8];
},to24h:function(a){
var h=a[0]-0;
if(a[2]=="AM"){
h=h==12?0:h;
}else{
h=h==12?h:h+12;
}
return [h+"",a[1]];
},onBtnClick:function(e){
var _9=this.ampmButton.get("label")=="AM"?"PM":"AM";
var v=this.get("values12");
v[2]=_9;
this.set("values12",v);
},_setIs24hAttr:function(_a){
var _b=this.slots[0].items;
if(_a&&_b.length!=24){
this.slots[0].items=this._zero.concat(_b).concat(this._pm);
}else{
if(!_a&&_b.length!=12){
_b.splice(0,1);
_b.splice(12);
}
}
var v=this.get("values");
this._set("is24h",_a);
this.ampmButton.domNode.style.display=_a?"none":"";
this.set("values",v);
},_getValuesAttr:function(){
var v=this.inherited(arguments);
return this.is24h?v:this.to24h([v[0],v[1],this.ampmButton.get("label")]);
},_setValuesAttr:function(_c){
if(this.is24h){
this.inherited(arguments);
}else{
_c=this.to12h(_c);
this.ampmButton.set("label",_c[2]);
this.inherited(arguments);
}
},_getValues12Attr:function(){
return this.to12h(this._getValuesAttr());
},_setValues12Attr:function(_d){
this.set("values",this.to24h(_d));
}});
});

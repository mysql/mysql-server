//>>built
define("dojox/mobile/SpinWheelTimePicker",["dojo/_base/declare","dojo/dom-class","./SpinWheel","./SpinWheelSlot"],function(_1,_2,_3,_4){
return _1("dojox.mobile.SpinWheelTimePicker",_3,{slotClasses:[_4,_4],slotProps:[{labelFrom:0,labelTo:23},{labels:["00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31","32","33","34","35","36","37","38","39","40","41","42","43","44","45","46","47","48","49","50","51","52","53","54","55","56","57","58","59"]}],buildRendering:function(){
this.inherited(arguments);
_2.add(this.domNode,"mblSpinWheelTimePicker");
},reset:function(){
var _5=this.slots;
var _6=new Date();
var _7=_6.getHours()+"";
_5[0].setValue(_7);
_5[0].setColor(_7);
var m=_6.getMinutes();
var _8=(m<10?"0":"")+m;
_5[1].setValue(_8);
_5[1].setColor(_8);
}});
});

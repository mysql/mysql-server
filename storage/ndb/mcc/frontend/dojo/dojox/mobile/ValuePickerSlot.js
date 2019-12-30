//>>built
define("dojox/mobile/ValuePickerSlot",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/touch","dijit/_WidgetBase","./iconUtils"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.mobile.ValuePickerSlot",_9,{items:[],labels:[],labelFrom:0,labelTo:0,zeroPad:0,value:"",step:1,readOnly:false,tabIndex:"0",baseClass:"mblValuePickerSlot",buildRendering:function(){
this.inherited(arguments);
this.initLabels();
if(this.labels.length>0){
this.items=[];
for(i=0;i<this.labels.length;i++){
this.items.push([i,this.labels[i]]);
}
}
this.plusBtnNode=_7.create("div",{className:"mblValuePickerSlotPlusButton mblValuePickerSlotButton",title:"+"},this.domNode);
this.plusIconNode=_7.create("div",{className:"mblValuePickerSlotIcon"},this.plusBtnNode);
_a.createIcon("mblDomButtonGrayPlus",null,this.plusIconNode);
this.inputAreaNode=_7.create("div",{className:"mblValuePickerSlotInputArea"},this.domNode);
this.inputNode=_7.create("input",{className:"mblValuePickerSlotInput",readonly:this.readOnly},this.inputAreaNode);
this.minusBtnNode=_7.create("div",{className:"mblValuePickerSlotMinusButton mblValuePickerSlotButton",title:"-"},this.domNode);
this.minusIconNode=_7.create("div",{className:"mblValuePickerSlotIcon"},this.minusBtnNode);
_a.createIcon("mblDomButtonGrayMinus",null,this.minusIconNode);
if(this.value===""&&this.items.length>0){
this.value=this.items[0][1];
}
this._initialValue=this.value;
},startup:function(){
if(this._started){
return;
}
this._handlers=[this.connect(this.plusBtnNode,_8.press,"_onTouchStart"),this.connect(this.minusBtnNode,_8.press,"_onTouchStart"),this.connect(this.plusBtnNode,"onkeydown","_onClick"),this.connect(this.minusBtnNode,"onkeydown","_onClick"),this.connect(this.inputNode,"onchange",_4.hitch(this,function(e){
this._onChange(e);
}))];
this.inherited(arguments);
},initLabels:function(){
if(this.labelFrom!==this.labelTo){
var a=this.labels=[],_b=this.zeroPad&&Array(this.zeroPad).join("0");
for(var i=this.labelFrom;i<=this.labelTo;i+=this.step){
a.push(this.zeroPad?(_b+i).slice(-this.zeroPad):i+"");
}
}
},spin:function(_c){
var _d=-1,v=this.get("value"),_e=this.items.length;
for(var i=0;i<_e;i++){
if(this.items[i][1]===v){
_d=i;
break;
}
}
if(v==-1){
return;
}
_d+=_c;
if(_d<0){
_d+=(Math.abs(Math.ceil(_d/_e))+1)*_e;
}
var _f=this.items[_d%_e];
this.set("value",_f[1]);
},setInitialValue:function(){
this.set("value",this._initialValue);
},_onClick:function(e){
if(e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
var _10=e.currentTarget;
if(_10===this.plusBtnNode||_10===this.minusBtnNode){
this._btn=_10;
}
this.spin(this._btn===this.plusBtnNode?1:-1);
},onClick:function(){
},_onChange:function(e){
if(this.onChange(e)===false){
return;
}
var v=this.get("value"),a=this.validate(v);
this.set("value",a.length?a[0][1]:this.value);
},onChange:function(){
},validate:function(_11){
return _1.filter(this.items,function(a){
return (a[1]+"").toLowerCase()==(_11+"").toLowerCase();
});
},_onTouchStart:function(e){
this._conn=[this.connect(_5.body(),_8.move,"_onTouchMove"),this.connect(_5.body(),_8.release,"_onTouchEnd")];
this.touchStartX=e.touches?e.touches[0].pageX:e.clientX;
this.touchStartY=e.touches?e.touches[0].pageY:e.clientY;
_6.add(e.currentTarget,"mblValuePickerSlotButtonSelected");
this._btn=e.currentTarget;
if(this._timer){
clearTimeout(this._timer);
this._timer=null;
}
if(this._interval){
clearInterval(this._interval);
this._interval=null;
}
this._timer=setTimeout(_4.hitch(this,function(){
this._interval=setInterval(_4.hitch(this,function(){
this.spin(this._btn===this.plusBtnNode?1:-1);
}),60);
this._timer=null;
}),1000);
_3.stop(e);
},_onTouchMove:function(e){
var x=e.touches?e.touches[0].pageX:e.clientX;
var y=e.touches?e.touches[0].pageY:e.clientY;
if(Math.abs(x-this.touchStartX)>=4||Math.abs(y-this.touchStartY)>=4){
if(this._timer){
clearTimeout(this._timer);
this._timer=null;
}
if(this._interval){
clearInterval(this._interval);
this._interval=null;
}
_1.forEach(this._conn,this.disconnect,this);
_6.remove(this._btn,"mblValuePickerSlotButtonSelected");
}
},_onTouchEnd:function(e){
if(this._timer){
clearTimeout(this._timer);
this._timer=null;
}
_1.forEach(this._conn,this.disconnect,this);
_6.remove(this._btn,"mblValuePickerSlotButtonSelected");
if(this._interval){
clearInterval(this._interval);
this._interval=null;
}else{
this._onClick(e);
}
},_getKeyAttr:function(){
var val=this.get("value");
var _12=_1.filter(this.items,function(_13){
return _13[1]===val;
})[0];
return _12?_12[0]:null;
},_getValueAttr:function(){
return this.inputNode.value;
},_setValueAttr:function(_14){
this.inputNode.value=_14;
this._set("value",_14);
var _15=this.getParent();
if(_15&&_15.onValueChanged){
_15.onValueChanged(this);
}
},_setTabIndexAttr:function(_16){
this.plusBtnNode.setAttribute("tabIndex",_16);
this.minusBtnNode.setAttribute("tabIndex",_16);
}});
});

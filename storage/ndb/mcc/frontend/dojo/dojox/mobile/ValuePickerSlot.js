//>>built
define("dojox/mobile/ValuePickerSlot",["dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-attr","dojo/touch","dijit/_WidgetBase","./iconUtils","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/ValuePickerSlot"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var _e=_2(_c("dojo-bidi")?"dojox.mobile.NonBidiValuePickerSlot":"dojox.mobile.ValuePickerSlot",_a,{items:[],labels:[],labelFrom:0,labelTo:0,zeroPad:0,value:"",step:1,readOnly:false,tabIndex:"0",plusBtnLabel:"",plusBtnLabelRef:"",minusBtnLabel:"",minusBtnLabelRef:"",baseClass:"mblValuePickerSlot",buildRendering:function(){
this.inherited(arguments);
this.initLabels();
if(this.labels.length>0){
this.items=[];
for(var i=0;i<this.labels.length;i++){
this.items.push([i,this.labels[i]]);
}
}
this.plusBtnNode=_7.create("div",{className:"mblValuePickerSlotPlusButton mblValuePickerSlotButton",title:"+"},this.domNode);
this.plusIconNode=_7.create("div",{className:"mblValuePickerSlotIcon"},this.plusBtnNode);
_b.createIcon("mblDomButtonGrayPlus",null,this.plusIconNode);
this.inputAreaNode=_7.create("div",{className:"mblValuePickerSlotInputArea"},this.domNode);
this.inputNode=_7.create("input",{className:"mblValuePickerSlotInput",readonly:this.readOnly},this.inputAreaNode);
this.minusBtnNode=_7.create("div",{className:"mblValuePickerSlotMinusButton mblValuePickerSlotButton",title:"-"},this.domNode);
this.minusIconNode=_7.create("div",{className:"mblValuePickerSlotIcon"},this.minusBtnNode);
_b.createIcon("mblDomButtonGrayMinus",null,this.minusIconNode);
_8.set(this.plusBtnNode,"role","button");
this._setPlusBtnLabelAttr(this.plusBtnLabel);
this._setPlusBtnLabelRefAttr(this.plusBtnLabelRef);
_8.set(this.inputNode,"role","textbox");
var _f=require("dijit/registry");
var _10=_f.getUniqueId("dojo_mobile__mblValuePickerSlotInput");
_8.set(this.inputNode,"id",_10);
_8.set(this.plusBtnNode,"aria-controls",_10);
_8.set(this.minusBtnNode,"role","button");
_8.set(this.minusBtnNode,"aria-controls",_10);
this._setMinusBtnLabelAttr(this.minusBtnLabel);
this._setMinusBtnLabelRefAttr(this.minusBtnLabelRef);
if(this.value===""&&this.items.length>0){
this.value=this.items[0][1];
}
this._initialValue=this.value;
},startup:function(){
if(this._started){
return;
}
this._handlers=[this.connect(this.plusBtnNode,_9.press,"_onTouchStart"),this.connect(this.minusBtnNode,_9.press,"_onTouchStart"),this.connect(this.plusBtnNode,"onkeydown","_onClick"),this.connect(this.minusBtnNode,"onkeydown","_onClick"),this.connect(this.inputNode,"onchange",_4.hitch(this,function(e){
this._onChange(e);
}))];
this.inherited(arguments);
this._set(this.plusBtnLabel);
},initLabels:function(){
if(this.labelFrom!==this.labelTo){
var a=this.labels=[],_11=this.zeroPad&&Array(this.zeroPad).join("0");
for(var i=this.labelFrom;i<=this.labelTo;i+=this.step){
a.push(this.zeroPad?(_11+i).slice(-this.zeroPad):i+"");
}
}
},spin:function(_12){
var pos=-1,v=this.get("value"),len=this.items.length;
for(var i=0;i<len;i++){
if(this.items[i][1]===v){
pos=i;
break;
}
}
if(v==-1){
return;
}
pos+=_12;
if(pos<0){
pos+=(Math.abs(Math.ceil(pos/len))+1)*len;
}
var _13=this.items[pos%len];
this.set("value",_13[1]);
},setInitialValue:function(){
this.set("value",this._initialValue);
},_onClick:function(e){
if(e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
if(this.onClick(e)===false){
return;
}
var _14=e.currentTarget;
if(_14===this.plusBtnNode||_14===this.minusBtnNode){
this._btn=_14;
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
},validate:function(_15){
return _1.filter(this.items,function(a){
return (a[1]+"").toLowerCase()==(_15+"").toLowerCase();
});
},_onTouchStart:function(e){
this._conn=[this.connect(_5.body(),_9.move,"_onTouchMove"),this.connect(_5.body(),_9.release,"_onTouchEnd")];
this.touchStartX=e.touches?e.touches[0].pageX:e.clientX;
this.touchStartY=e.touches?e.touches[0].pageY:e.clientY;
_6.add(e.currentTarget,"mblValuePickerSlotButtonSelected");
this._btn=e.currentTarget;
if(this._timer){
this._timer.remove();
this._timer=null;
}
if(this._interval){
clearInterval(this._interval);
this._interval=null;
}
this._timer=this.defer(function(){
this._interval=setInterval(_4.hitch(this,function(){
this.spin(this._btn===this.plusBtnNode?1:-1);
}),60);
this._timer=null;
},1000);
_3.stop(e);
},_onTouchMove:function(e){
var x=e.touches?e.touches[0].pageX:e.clientX;
var y=e.touches?e.touches[0].pageY:e.clientY;
if(Math.abs(x-this.touchStartX)>=4||Math.abs(y-this.touchStartY)>=4){
if(this._timer){
this._timer.remove();
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
this._timer.remove();
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
var _16=_1.filter(this.items,function(_17){
return _17[1]===val;
})[0];
return _16?_16[0]:null;
},_getValueAttr:function(){
return this.inputNode.value;
},_setValueAttr:function(_18){
this._spinToValue(_18,true);
},_spinToValue:function(_19,_1a){
if(this.get("value")==_19){
return;
}
this.inputNode.value=_19;
if(_1a){
this._set("value",_19);
}
var _1b=this.getParent();
if(_1b&&_1b.onValueChanged){
_1b.onValueChanged(this);
}
},_setTabIndexAttr:function(_1c){
this.plusBtnNode.setAttribute("tabIndex",_1c);
this.minusBtnNode.setAttribute("tabIndex",_1c);
},_setAria:function(_1d,_1e,_1f){
if(_1f){
_8.set(_1d,_1e,_1f);
}else{
_8.remove(_1d,_1e);
}
},_setPlusBtnLabelAttr:function(_20){
this._setAria(this.plusBtnNode,"aria-label",_20);
},_setPlusBtnLabelRefAttr:function(_21){
this._setAria(this.plusBtnNode,"aria-labelledby",_21);
},_setMinusBtnLabelAttr:function(_22){
this._setAria(this.minusBtnNode,"aria-label",_22);
},_setMinusBtnLabelRefAttr:function(_23){
this._setAria(this.minusBtnNode,"aria-labelledby",_23);
}});
return _c("dojo-bidi")?_2("dojox.mobile.ValuePickerSlot",[_e,_d]):_e;
});

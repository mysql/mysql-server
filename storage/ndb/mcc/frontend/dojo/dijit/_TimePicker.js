//>>built
define("dijit/_TimePicker",["dojo/_base/array","dojo/date","dojo/date/locale","dojo/date/stamp","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/query","dojo/mouse","dojo/on","./_WidgetBase","./form/_ListMouseMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,on,_e,_f){
var _10=_5("dijit._TimePicker",[_e,_f],{baseClass:"dijitTimePicker",pickerMin:"T00:00:00",pickerMax:"T23:59:59",clickableIncrement:"T00:15:00",visibleIncrement:"T01:00:00",value:new Date(),_visibleIncrement:2,_clickableIncrement:1,_totalIncrements:10,constraints:{},serialize:_4.toISOString,buildRendering:function(){
this.inherited(arguments);
this.containerNode=this.domNode;
this.timeMenu=this.domNode;
},setValue:function(_11){
_8.deprecated("dijit._TimePicker:setValue() is deprecated.  Use set('value', ...) instead.","","2.0");
this.set("value",_11);
},_setValueAttr:function(_12){
this._set("value",_12);
this._showText();
},_setFilterStringAttr:function(val){
this._set("filterString",val);
this._showText();
},isDisabledDate:function(){
return false;
},_getFilteredNodes:function(_13,_14,_15,_16){
var _17=this.ownerDocument.createDocumentFragment();
for(var i=0;i<this._maxIncrement;i++){
var n=this._createOption(i);
if(n){
_17.appendChild(n);
}
}
return _17;
},_showText:function(){
var _18=_4.fromISOString;
this.domNode.innerHTML="";
this._clickableIncrementDate=_18(this.clickableIncrement);
this._visibleIncrementDate=_18(this.visibleIncrement);
var _19=function(_1a){
return _1a.getHours()*60*60+_1a.getMinutes()*60+_1a.getSeconds();
},_1b=_19(this._clickableIncrementDate),_1c=_19(this._visibleIncrementDate),_1d=(this.value||this.currentFocus).getTime();
this._refDate=_18(this.pickerMin);
this._refDate.setFullYear(1970,0,1);
this._clickableIncrement=1;
this._visibleIncrement=_1c/_1b;
var _1e=_18(this.pickerMax);
_1e.setFullYear(1970,0,1);
var _1f=(_1e.getTime()-this._refDate.getTime())*0.001;
this._maxIncrement=Math.ceil((_1f+1)/_1b);
var _20=this._getFilteredNodes();
if(!_20.firstChild&&this.filterString){
this.filterString="";
this._showText();
}else{
this.domNode.appendChild(_20);
}
},constructor:function(){
this.constraints={};
},postMixInProperties:function(){
this.inherited(arguments);
this._setConstraintsAttr(this.constraints);
},_setConstraintsAttr:function(_21){
for(var key in {clickableIncrement:1,visibleIncrement:1,pickerMin:1,pickerMax:1}){
if(key in _21){
this[key]=_21[key];
}
}
if(!_21.locale){
_21.locale=this.lang;
}
},_createOption:function(_22){
var _23=new Date(this._refDate);
var _24=this._clickableIncrementDate;
_23.setHours(_23.getHours()+_24.getHours()*_22,_23.getMinutes()+_24.getMinutes()*_22,_23.getSeconds()+_24.getSeconds()*_22);
if(this.constraints.selector=="time"){
_23.setFullYear(1970,0,1);
}
var _25=_3.format(_23,this.constraints);
if(this.filterString&&_25.toLowerCase().indexOf(this.filterString)!==0){
return null;
}
var div=this.ownerDocument.createElement("div");
div.className=this.baseClass+"Item";
div.date=_23;
div.idx=_22;
_7.create("div",{"class":this.baseClass+"ItemInner",innerHTML:_25},div);
var _26=_22%this._visibleIncrement<1&&_22%this._visibleIncrement>-1,_27=!_26&&!(_22%this._clickableIncrement);
if(_26){
div.className+=" "+this.baseClass+"Marker";
}else{
if(_27){
div.className+=" "+this.baseClass+"Tick";
}
}
if(this.isDisabledDate(_23)){
div.className+=" "+this.baseClass+"ItemDisabled";
}
if(this.value&&!_2.compare(this.value,_23,this.constraints.selector)){
div.selected=true;
div.className+=" "+this.baseClass+"ItemSelected";
this._selectedDiv=div;
if(_26){
div.className+=" "+this.baseClass+"MarkerSelected";
}else{
if(_27){
div.className+=" "+this.baseClass+"TickSelected";
}
}
this._highlightOption(div,true);
}
return div;
},onOpen:function(){
this.inherited(arguments);
this.set("selected",this._selectedDiv);
},_onOptionSelected:function(tgt,_28){
var _29=tgt.target.date||tgt.target.parentNode.date;
if(!_29||this.isDisabledDate(_29)){
return;
}
this._set("value",_29);
this.emit("input");
if(_28){
this._highlighted_option=null;
this.set("value",_29);
this.onChange(_29);
}
},onChange:function(){
},_highlightOption:function(_2a,_2b){
if(!_2a){
return;
}
if(_2b){
if(this._highlighted_option){
this._highlightOption(this._highlighted_option,false);
}
this._highlighted_option=_2a;
}else{
if(this._highlighted_option!==_2a){
return;
}else{
this._highlighted_option=null;
}
}
_6.toggle(_2a,this.baseClass+"ItemHover",_2b);
if(_6.contains(_2a,this.baseClass+"Marker")){
_6.toggle(_2a,this.baseClass+"MarkerHover",_2b);
}else{
_6.toggle(_2a,this.baseClass+"TickHover",_2b);
}
},handleKey:function(e){
if(e.keyCode==_9.DOWN_ARROW){
this.selectNextNode();
this._onOptionSelected({target:this._highlighted_option},false);
e.stopPropagation();
e.preventDefault();
return false;
}else{
if(e.keyCode==_9.UP_ARROW){
this.selectPreviousNode();
this._onOptionSelected({target:this._highlighted_option},false);
e.stopPropagation();
e.preventDefault();
return false;
}else{
if(e.keyCode==_9.ENTER||e.keyCode===_9.TAB){
if(!this._keyboardSelected&&e.keyCode===_9.TAB){
return true;
}
if(this._highlighted_option){
this._onOptionSelected({target:this._highlighted_option},true);
}
return e.keyCode===_9.TAB;
}
}
}
return undefined;
},onHover:function(_2c){
this._highlightOption(_2c,true);
},onUnhover:function(_2d){
this._highlightOption(_2d,false);
},onSelect:function(_2e){
this._highlightOption(_2e,true);
},onDeselect:function(_2f){
this._highlightOption(_2f,false);
},onClick:function(_30){
this._onOptionSelected({target:_30},true);
}});
return _10;
});

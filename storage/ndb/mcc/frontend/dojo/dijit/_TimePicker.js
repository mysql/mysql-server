//>>built
require({cache:{"url:dijit/templates/TimePicker.html":"<div id=\"widget_${id}\" class=\"dijitMenu\"\n    ><div data-dojo-attach-point=\"upArrow\" class=\"dijitButtonNode dijitUpArrowButton\" data-dojo-attach-event=\"onmouseenter:_buttonMouse,onmouseleave:_buttonMouse\"\n\t\t><div class=\"dijitReset dijitInline dijitArrowButtonInner\" role=\"presentation\">&#160;</div\n\t\t><div class=\"dijitArrowButtonChar\">&#9650;</div></div\n    ><div data-dojo-attach-point=\"timeMenu,focusNode\" data-dojo-attach-event=\"onclick:_onOptionSelected,onmouseover,onmouseout\"></div\n    ><div data-dojo-attach-point=\"downArrow\" class=\"dijitButtonNode dijitDownArrowButton\" data-dojo-attach-event=\"onmouseenter:_buttonMouse,onmouseleave:_buttonMouse\"\n\t\t><div class=\"dijitReset dijitInline dijitArrowButtonInner\" role=\"presentation\">&#160;</div\n\t\t><div class=\"dijitArrowButtonChar\">&#9660;</div></div\n></div>\n"}});
define("dijit/_TimePicker",["dojo/_base/array","dojo/date","dojo/date/locale","dojo/date/stamp","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/_base/event","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/sniff","dojo/query","dojo/mouse","./typematic","./_Widget","./_TemplatedMixin","./form/_FormValueWidget","dojo/text!./templates/TimePicker.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13){
var _14=_5("dijit._TimePicker",[_10,_11],{templateString:_13,baseClass:"dijitTimePicker",clickableIncrement:"T00:15:00",visibleIncrement:"T01:00:00",visibleRange:"T05:00:00",value:new Date(),_visibleIncrement:2,_clickableIncrement:1,_totalIncrements:10,constraints:{},serialize:_4.toISOString,setValue:function(_15){
_9.deprecated("dijit._TimePicker:setValue() is deprecated.  Use set('value', ...) instead.","","2.0");
this.set("value",_15);
},_setValueAttr:function(_16){
this._set("value",_16);
this._showText();
},_setFilterStringAttr:function(val){
this._set("filterString",val);
this._showText();
},isDisabledDate:function(){
return false;
},_getFilteredNodes:function(_17,_18,_19,_1a){
var _1b=[],_1c=_1a?_1a.date:this._refDate,n,i=_17,max=this._maxIncrement+Math.abs(i),chk=_19?-1:1,dec=_19?1:0,inc=1-dec;
do{
i-=dec;
n=this._createOption(i);
if(n){
if((_19&&n.date>_1c)||(!_19&&n.date<_1c)){
break;
}
_1b[_19?"unshift":"push"](n);
_1c=n.date;
}
i+=inc;
}while(_1b.length<_18&&(i*chk)<max);
return _1b;
},_showText:function(){
var _1d=_4.fromISOString;
this.timeMenu.innerHTML="";
this._clickableIncrementDate=_1d(this.clickableIncrement);
this._visibleIncrementDate=_1d(this.visibleIncrement);
this._visibleRangeDate=_1d(this.visibleRange);
var _1e=function(_1f){
return _1f.getHours()*60*60+_1f.getMinutes()*60+_1f.getSeconds();
},_20=_1e(this._clickableIncrementDate),_21=_1e(this._visibleIncrementDate),_22=_1e(this._visibleRangeDate),_23=(this.value||this.currentFocus).getTime();
this._refDate=new Date(_23-_23%(_20*1000));
this._refDate.setFullYear(1970,0,1);
this._clickableIncrement=1;
this._totalIncrements=_22/_20;
this._visibleIncrement=_21/_20;
this._maxIncrement=(60*60*24)/_20;
var _24=Math.min(this._totalIncrements,10),_25=this._getFilteredNodes(0,(_24>>1)+1,false),_26=[],_27=_24-_25.length,_28=this._getFilteredNodes(0,_27,true,_25[0]);
if(_28.length<_27&&_25.length>0){
_26=this._getFilteredNodes(_25[_25.length-1].idx+1,_27-_28.length,false,_25[_25.length-1]);
}
_1.forEach(_28.concat(_25,_26),function(n){
this.timeMenu.appendChild(n);
},this);
if(!_28.length&&!_25.length&&!_26.length&&this.filterString){
this.filterString="";
this._showText();
}
},constructor:function(){
this.constraints={};
},postMixInProperties:function(){
this.inherited(arguments);
this._setConstraintsAttr(this.constraints);
},_setConstraintsAttr:function(_29){
for(var key in {clickableIncrement:1,visibleIncrement:1,visibleRange:1}){
if(key in _29){
this[key]=_29[key];
}
}
if(!_29.locale){
_29.locale=this.lang;
}
},postCreate:function(){
this.connect(this.timeMenu,_e.wheel,"_mouseWheeled");
this.own(_f.addMouseListener(this.upArrow,this,"_onArrowUp",33,250),_f.addMouseListener(this.downArrow,this,"_onArrowDown",33,250));
this.inherited(arguments);
},_buttonMouse:function(e){
_6.toggle(e.currentTarget,e.currentTarget==this.upArrow?"dijitUpArrowHover":"dijitDownArrowHover",e.type=="mouseenter"||e.type=="mouseover");
},_createOption:function(_2a){
var _2b=new Date(this._refDate);
var _2c=this._clickableIncrementDate;
_2b.setTime(_2b.getTime()+_2c.getHours()*_2a*3600000+_2c.getMinutes()*_2a*60000+_2c.getSeconds()*_2a*1000);
if(this.constraints.selector=="time"){
_2b.setFullYear(1970,0,1);
}
var _2d=_3.format(_2b,this.constraints);
if(this.filterString&&_2d.toLowerCase().indexOf(this.filterString)!==0){
return null;
}
var div=this.ownerDocument.createElement("div");
div.className=this.baseClass+"Item";
div.date=_2b;
div.idx=_2a;
_7.create("div",{"class":this.baseClass+"ItemInner",innerHTML:_2d},div);
if(_2a%this._visibleIncrement<1&&_2a%this._visibleIncrement>-1){
_6.add(div,this.baseClass+"Marker");
}else{
if(!(_2a%this._clickableIncrement)){
_6.add(div,this.baseClass+"Tick");
}
}
if(this.isDisabledDate(_2b)){
_6.add(div,this.baseClass+"ItemDisabled");
}
if(this.value&&!_2.compare(this.value,_2b,this.constraints.selector)){
div.selected=true;
_6.add(div,this.baseClass+"ItemSelected");
if(_6.contains(div,this.baseClass+"Marker")){
_6.add(div,this.baseClass+"MarkerSelected");
}else{
_6.add(div,this.baseClass+"TickSelected");
}
this._highlightOption(div,true);
}
return div;
},_onOptionSelected:function(tgt){
var _2e=tgt.target.date||tgt.target.parentNode.date;
if(!_2e||this.isDisabledDate(_2e)){
return;
}
this._highlighted_option=null;
this.set("value",_2e);
this.onChange(_2e);
},onChange:function(){
},_highlightOption:function(_2f,_30){
if(!_2f){
return;
}
if(_30){
if(this._highlighted_option){
this._highlightOption(this._highlighted_option,false);
}
this._highlighted_option=_2f;
}else{
if(this._highlighted_option!==_2f){
return;
}else{
this._highlighted_option=null;
}
}
_6.toggle(_2f,this.baseClass+"ItemHover",_30);
if(_6.contains(_2f,this.baseClass+"Marker")){
_6.toggle(_2f,this.baseClass+"MarkerHover",_30);
}else{
_6.toggle(_2f,this.baseClass+"TickHover",_30);
}
},onmouseover:function(e){
this._keyboardSelected=null;
var tgr=(e.target.parentNode===this.timeMenu)?e.target:e.target.parentNode;
if(!_6.contains(tgr,this.baseClass+"Item")){
return;
}
this._highlightOption(tgr,true);
},onmouseout:function(e){
this._keyboardSelected=null;
var tgr=(e.target.parentNode===this.timeMenu)?e.target:e.target.parentNode;
this._highlightOption(tgr,false);
},_mouseWheeled:function(e){
this._keyboardSelected=null;
_8.stop(e);
this[(e.wheelDelta>0?"_onArrowUp":"_onArrowDown")]();
},_onArrowUp:function(_31){
if(_31===-1){
_6.remove(this.upArrow,"dijitUpArrowActive");
return;
}else{
if(_31===0){
_6.add(this.upArrow,"dijitUpArrowActive");
}
}
if(!this.timeMenu.childNodes.length){
return;
}
var _32=this.timeMenu.childNodes[0].idx;
var _33=this._getFilteredNodes(_32,1,true,this.timeMenu.childNodes[0]);
if(_33.length){
this.timeMenu.removeChild(this.timeMenu.childNodes[this.timeMenu.childNodes.length-1]);
this.timeMenu.insertBefore(_33[0],this.timeMenu.childNodes[0]);
}
},_onArrowDown:function(_34){
if(_34===-1){
_6.remove(this.downArrow,"dijitDownArrowActive");
return;
}else{
if(_34===0){
_6.add(this.downArrow,"dijitDownArrowActive");
}
}
if(!this.timeMenu.childNodes.length){
return;
}
var _35=this.timeMenu.childNodes[this.timeMenu.childNodes.length-1].idx+1;
var _36=this._getFilteredNodes(_35,1,false,this.timeMenu.childNodes[this.timeMenu.childNodes.length-1]);
if(_36.length){
this.timeMenu.removeChild(this.timeMenu.childNodes[0]);
this.timeMenu.appendChild(_36[0]);
}
},handleKey:function(e){
if(e.keyCode==_a.DOWN_ARROW||e.keyCode==_a.UP_ARROW){
_8.stop(e);
if(this._highlighted_option&&!this._highlighted_option.parentNode){
this._highlighted_option=null;
}
var _37=this.timeMenu,tgt=this._highlighted_option||_d("."+this.baseClass+"ItemSelected",_37)[0];
if(!tgt){
tgt=_37.childNodes[0];
}else{
if(_37.childNodes.length){
if(e.keyCode==_a.DOWN_ARROW&&!tgt.nextSibling){
this._onArrowDown();
}else{
if(e.keyCode==_a.UP_ARROW&&!tgt.previousSibling){
this._onArrowUp();
}
}
if(e.keyCode==_a.DOWN_ARROW){
tgt=tgt.nextSibling;
}else{
tgt=tgt.previousSibling;
}
}
}
this._highlightOption(tgt,true);
this._keyboardSelected=tgt;
return false;
}else{
if(e.keyCode==_a.ENTER||e.keyCode===_a.TAB){
if(!this._keyboardSelected&&e.keyCode===_a.TAB){
return true;
}
if(this._highlighted_option){
this._onOptionSelected({target:this._highlighted_option});
}
return e.keyCode===_a.TAB;
}
}
return undefined;
}});
return _14;
});

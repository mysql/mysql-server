//>>built
require({cache:{"url:dojox/widget/Calendar/Calendar.html":"<div class=\"dojoxCalendar\">\n    <div tabindex=\"0\" class=\"dojoxCalendarContainer\" style=\"visibility: visible;\" dojoAttachPoint=\"container\">\n\t\t<div style=\"display:none\">\n\t\t\t<div dojoAttachPoint=\"previousYearLabelNode\"></div>\n\t\t\t<div dojoAttachPoint=\"nextYearLabelNode\"></div>\n\t\t\t<div dojoAttachPoint=\"monthLabelSpacer\"></div>\n\t\t</div>\n        <div class=\"dojoxCalendarHeader\">\n            <div>\n                <div class=\"dojoxCalendarDecrease\" dojoAttachPoint=\"decrementMonth\"></div>\n            </div>\n            <div class=\"\">\n                <div class=\"dojoxCalendarIncrease\" dojoAttachPoint=\"incrementMonth\"></div>\n            </div>\n            <div class=\"dojoxCalendarTitle\" dojoAttachPoint=\"header\" dojoAttachEvent=\"onclick: onHeaderClick\">\n            </div>\n        </div>\n        <div class=\"dojoxCalendarBody\" dojoAttachPoint=\"containerNode\"></div>\n        <div class=\"\">\n            <div class=\"dojoxCalendarFooter\" dojoAttachPoint=\"footer\">                        \n            </div>\n        </div>\n    </div>\n</div>\n"}});
define("dojox/widget/_CalendarBase",["dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_Container","dijit/_WidgetsInTemplateMixin","dijit/typematic","dojo/_base/declare","dojo/date","dojo/date/stamp","dojo/date/locale","dojo/dom-style","dojo/dom-class","dojo/_base/fx","dojo/on","dojo/_base/array","dojo/_base/lang","dojo/text!./Calendar/Calendar.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,fx,on,_c,_d,_e){
return _6("dojox.widget._CalendarBase",[_1,_2,_3,_4],{templateString:_e,_views:null,useFx:true,value:new Date(),constraints:null,footerFormat:"medium",constructor:function(){
this._views=[];
this.value=new Date();
},_setConstraintsAttr:function(_f){
var c=this.constraints=_f;
if(c){
if(typeof c.min=="string"){
c.min=_8.fromISOString(c.min);
}
if(typeof c.max=="string"){
c.max=_8.fromISOString(c.max);
}
}
},postMixInProperties:function(){
this.inherited(arguments);
this.value=this.parseInitialValue(this.value);
},parseInitialValue:function(_10){
if(!_10||_10===-1){
return new Date();
}else{
if(_10.getFullYear){
return _10;
}else{
if(!isNaN(_10)){
if(typeof this.value=="string"){
_10=parseInt(_10);
}
_10=this._makeDate(_10);
}
}
}
return _10;
},_makeDate:function(_11){
return _11;
},postCreate:function(){
this.displayMonth=new Date(this.get("value"));
if(this._isInvalidDate(this.displayMonth)){
this.displayMonth=new Date();
}
var _12={parent:this,_getValueAttr:_d.hitch(this,function(){
return new Date(this._internalValue||this.value);
}),_getDisplayMonthAttr:_d.hitch(this,function(){
return new Date(this.displayMonth);
}),_getConstraintsAttr:_d.hitch(this,function(){
return this.constraints;
}),getLang:_d.hitch(this,function(){
return this.lang;
}),isDisabledDate:_d.hitch(this,this.isDisabledDate),getClassForDate:_d.hitch(this,this.getClassForDate),addFx:this.useFx?_d.hitch(this,this.addFx):function(){
}};
_c.forEach(this._views,function(_13){
var _14=new _13(_12).placeAt(this);
var _15=_14.getHeader();
if(_15){
this.header.appendChild(_15);
_a.set(_15,"display","none");
}
_a.set(_14.domNode,"visibility","hidden");
_14.on("valueSelected",_d.hitch(this,"_onDateSelected"));
_14.set("value",this.get("value"));
},this);
if(this._views.length<2){
_a.set(this.header,"cursor","auto");
}
this.inherited(arguments);
this._children=this.getChildren();
this._currentChild=0;
var _16=new Date();
this.footer.innerHTML="Today: "+_9.format(_16,{formatLength:this.footerFormat,selector:"date",locale:this.lang});
on(this.footer,"click",_d.hitch(this,"goToToday"));
var _17=this._children[0];
_a.set(_17.domNode,"top","0px");
_a.set(_17.domNode,"visibility","visible");
var _18=_17.getHeader();
if(_18){
_a.set(_17.getHeader(),"display","");
}
_b.toggle(this.container,"no-header",!_17.useHeader);
_17.onDisplay();
var _19=this;
var _1a=function(_1b,_1c,adj){
_5.addMouseListener(_19[_1b],_19,function(_1d){
if(_1d>=0){
_19._adjustDisplay(_1c,adj);
}
},0.8,500);
};
_1a("incrementMonth","month",1);
_1a("decrementMonth","month",-1);
this._updateTitleStyle();
},addFx:function(_1e,_1f){
},_isInvalidDate:function(_20){
return !_20||isNaN(_20)||typeof _20!="object"||_20.toString()==this._invalidDate;
},_setValueAttr:function(_21){
if(!_21){
_21=new Date();
}
if(!_21["getFullYear"]){
_21=_8.fromISOString(_21+"");
}
if(this._isInvalidDate(_21)){
return false;
}
if(!this.value||_7.compare(_21,this.value)){
_21=new Date(_21);
this.displayMonth=new Date(_21);
this._internalValue=_21;
if(!this.isDisabledDate(_21,this.lang)&&this._currentChild===0){
this.value=_21;
this.onChange(_21);
}
if(this._children&&this._children.length>0){
this._children[this._currentChild].set("value",this.value);
}
return true;
}
this.onExecute();
return false;
},isDisabledDate:function(_22,_23){
var c=this.constraints;
var _24=_7.compare;
return c&&(c.min&&(_24(c.min,_22,"date")>0)||(c.max&&_24(c.max,_22,"date")<0));
},onValueSelected:function(_25){
},_onDateSelected:function(_26,_27,_28){
this.displayMonth=_26;
this.set("value",_26);
if(!this._transitionVert(-1)){
if(!_27&&_27!==0){
_27=this.get("value");
}
this.onValueSelected(_27);
}
},onChange:function(_29){
},onHeaderClick:function(e){
this._transitionVert(1);
},goToToday:function(){
this.set("value",new Date());
this.onValueSelected(this.get("value"));
},_transitionVert:function(_2a){
var _2b=this._children[this._currentChild];
var _2c=this._children[this._currentChild+_2a];
if(!_2c){
return false;
}
_a.set(_2c.domNode,"visibility","visible");
var _2d=_a.get(this.containerNode,"height");
_2c.set("value",this.displayMonth);
if(_2b.header){
_a.set(_2b.header,"display","none");
}
if(_2c.header){
_a.set(_2c.header,"display","");
}
_a.set(_2c.domNode,"top",(_2d*-1)+"px");
_a.set(_2c.domNode,"visibility","visible");
this._currentChild+=_2a;
var _2e=_2d*_2a;
var _2f=0;
_a.set(_2c.domNode,"top",(_2e*-1)+"px");
var _30=fx.animateProperty({node:_2b.domNode,properties:{top:_2e},onEnd:function(){
_a.set(_2b.domNode,"visibility","hidden");
}});
var _31=fx.animateProperty({node:_2c.domNode,properties:{top:_2f},onEnd:function(){
_2c.onDisplay();
}});
_b.toggle(this.container,"no-header",!_2c.useHeader);
_30.play();
_31.play();
_2b.onBeforeUnDisplay();
_2c.onBeforeDisplay();
this._updateTitleStyle();
return true;
},_updateTitleStyle:function(){
_b.toggle(this.header,"navToPanel",this._currentChild<this._children.length-1);
},_slideTable:function(_32,_33,_34){
var _35=_32.domNode;
var _36=_35.cloneNode(true);
var _37=_a.get(_35,"width");
_35.parentNode.appendChild(_36);
_a.set(_35,"left",(_37*_33)+"px");
_34();
var _38=fx.animateProperty({node:_36,properties:{left:_37*_33*-1},duration:500,onEnd:function(){
_36.parentNode.removeChild(_36);
}});
var _39=fx.animateProperty({node:_35,properties:{left:0},duration:500});
_38.play();
_39.play();
},_addView:function(_3a){
this._views.push(_3a);
},getClassForDate:function(_3b,_3c){
},_adjustDisplay:function(_3d,_3e,_3f){
var _40=this._children[this._currentChild];
var _41=this.displayMonth=_40.adjustDate(this.displayMonth,_3e);
this._slideTable(_40,_3e,function(){
_40.set("value",_41);
});
},onExecute:function(){
}});
});

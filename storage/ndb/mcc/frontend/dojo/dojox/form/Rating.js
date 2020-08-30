//>>built
define("dojox/form/Rating",["dojo/_base/declare","dojo/_base/lang","dojo/dom-attr","dojo/dom-class","dojo/mouse","dojo/on","dojo/string","dojo/query","dijit/form/_FormWidget"],function(_1,_2,_3,_4,_5,on,_6,_7,_8){
return _1("dojox.form.Rating",_8,{templateString:null,numStars:3,value:0,buildRendering:function(_9){
var _a="rating-"+Math.random().toString(36).substring(2);
var _b="<label class=\"dojoxRatingStar dijitInline ${hidden}\">"+"<span class=\"dojoxRatingLabel\">${value} stars</span>"+"<input type=\"radio\" name=\""+_a+"\" value=\"${value}\" dojoAttachPoint=\"focusNode\" class=\"dojoxRatingInput\">"+"</label>";
var _c="<div dojoAttachPoint=\"domNode\" class=\"dojoxRating dijitInline\">"+"<div data-dojo-attach-point=\"list\">"+_6.substitute(_b,{value:0,hidden:"dojoxRatingHidden"})+"${stars}"+"</div></div>";
var _d="";
for(var i=0;i<this.numStars;i++){
_d+=_6.substitute(_b,{value:i+1,hidden:""});
}
this.templateString=_6.substitute(_c,{stars:_d});
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this._renderStars(this.value);
this.own(on(this.list,on.selector(".dojoxRatingStar","mouseover"),_2.hitch(this,"_onMouse")),on(this.list,on.selector(".dojoxRatingStar","click"),_2.hitch(this,"_onClick")),on(this.list,on.selector(".dojoxRatingInput","change"),_2.hitch(this,"onStarChange")),on(this.list,_5.leave,_2.hitch(this,function(){
this._renderStars(this.value);
})));
},_onMouse:function(_e){
var _f=+_3.get(_e.target.querySelector("input"),"value");
this._renderStars(_f,true);
this.onMouseOver(_e,_f);
},_onClick:function(evt){
if(evt.target.tagName==="LABEL"){
var _10=+_3.get(evt.target.querySelector("input"),"value");
evt.target.value=_10;
this.onStarClick(evt,_10);
if(_10==this.value){
evt.preventDefault();
this.onStarChange(evt);
}
}
},_renderStars:function(_11,_12){
_7(".dojoxRatingStar",this.domNode).forEach(function(_13,i){
if(i>_11){
_4.remove(_13,"dojoxRatingStarHover");
_4.remove(_13,"dojoxRatingStarChecked");
}else{
_4.remove(_13,"dojoxRatingStar"+(_12?"Checked":"Hover"));
_4.add(_13,"dojoxRatingStar"+(_12?"Hover":"Checked"));
}
});
},onStarChange:function(evt){
var _14=+_3.get(evt.target,"value");
this.setAttribute("value",_14==this.value?0:_14);
this._renderStars(this.value);
this.onChange(this.value);
},onStarClick:function(evt,_15){
},onMouseOver:function(){
},setAttribute:function(key,_16){
this.set(key,_16);
},_setValueAttr:function(val){
this._set("value",val);
this._renderStars(val);
var _17=_7("input[type=radio]",this.domNode)[val];
if(_17){
_17.checked=true;
}
this.onChange(val);
}});
});

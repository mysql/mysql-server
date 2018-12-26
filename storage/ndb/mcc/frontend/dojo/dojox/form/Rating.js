//>>built
define("dojox/form/Rating",["dojo/_base/declare","dojo/_base/lang","dojo/dom-attr","dojo/dom-class","dojo/string","dojo/query","dijit/form/_FormWidget"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.form.Rating",_7,{templateString:null,numStars:3,value:0,constructor:function(_8){
_2.mixin(this,_8);
var _9="<div dojoAttachPoint=\"domNode\" class=\"dojoxRating dijitInline\">"+"<input type=\"hidden\" value=\"0\" dojoAttachPoint=\"focusNode\" /><ul>${stars}</ul>"+"</div>";
var _a="<li class=\"dojoxRatingStar dijitInline\" dojoAttachEvent=\"onclick:onStarClick,onmouseover:_onMouse,onmouseout:_onMouse\" value=\"${value}\"></li>";
var _b="";
for(var i=0;i<this.numStars;i++){
_b+=_5.substitute(_a,{value:i+1});
}
this.templateString=_5.substitute(_9,{stars:_b});
},postCreate:function(){
this.inherited(arguments);
this._renderStars(this.value);
},_onMouse:function(_c){
if(this.hovering){
var _d=+_3.get(_c.target,"value");
this.onMouseOver(_c,_d);
this._renderStars(_d,true);
}else{
this._renderStars(this.value);
}
},_renderStars:function(_e,_f){
_6(".dojoxRatingStar",this.domNode).forEach(function(_10,i){
if(i+1>_e){
_4.remove(_10,"dojoxRatingStarHover");
_4.remove(_10,"dojoxRatingStarChecked");
}else{
_4.remove(_10,"dojoxRatingStar"+(_f?"Checked":"Hover"));
_4.add(_10,"dojoxRatingStar"+(_f?"Hover":"Checked"));
}
});
},onStarClick:function(evt){
var _11=+_3.get(evt.target,"value");
this.setAttribute("value",_11==this.value?0:_11);
this._renderStars(this.value);
this.onChange(this.value);
},onMouseOver:function(){
},setAttribute:function(key,_12){
this.set(key,_12);
if(key=="value"){
this._renderStars(this.value);
this.onChange(this.value);
}
}});
});

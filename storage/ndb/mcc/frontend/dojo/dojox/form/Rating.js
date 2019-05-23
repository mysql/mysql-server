//>>built
define("dojox/form/Rating",["dojo/_base/declare","dojo/_base/lang","dojo/dom-attr","dojo/dom-class","dojo/mouse","dojo/on","dojo/string","dojo/query","dijit/form/_FormWidget"],function(_1,_2,_3,_4,_5,on,_6,_7,_8){
return _1("dojox.form.Rating",_8,{templateString:null,numStars:3,value:0,buildRendering:function(_9){
var _a="<div dojoAttachPoint=\"domNode\" class=\"dojoxRating dijitInline\">"+"<input type=\"hidden\" value=\"0\" dojoAttachPoint=\"focusNode\" /><ul data-dojo-attach-point=\"list\">${stars}</ul>"+"</div>";
var _b="<li class=\"dojoxRatingStar dijitInline\" value=\"${value}\"></li>";
var _c="";
for(var i=0;i<this.numStars;i++){
_c+=_6.substitute(_b,{value:i+1});
}
this.templateString=_6.substitute(_a,{stars:_c});
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this._renderStars(this.value);
this.own(on(this.list,on.selector(".dojoxRatingStar","mouseover"),_2.hitch(this,"_onMouse")),on(this.list,on.selector(".dojoxRatingStar","click"),_2.hitch(this,"onStarClick")),on(this.list,_5.leave,_2.hitch(this,function(){
this._renderStars(this.value);
})));
},_onMouse:function(_d){
var _e=+_3.get(_d.target,"value");
this._renderStars(_e,true);
this.onMouseOver(_d,_e);
},_renderStars:function(_f,_10){
_7(".dojoxRatingStar",this.domNode).forEach(function(_11,i){
if(i+1>_f){
_4.remove(_11,"dojoxRatingStarHover");
_4.remove(_11,"dojoxRatingStarChecked");
}else{
_4.remove(_11,"dojoxRatingStar"+(_10?"Checked":"Hover"));
_4.add(_11,"dojoxRatingStar"+(_10?"Hover":"Checked"));
}
});
},onStarClick:function(evt){
var _12=+_3.get(evt.target,"value");
this.setAttribute("value",_12==this.value?0:_12);
this._renderStars(this.value);
this.onChange(this.value);
},onMouseOver:function(){
},setAttribute:function(key,_13){
this.set(key,_13);
},_setValueAttr:function(val){
this.focusNode.value=val;
this._set("value",val);
this._renderStars(val);
this.onChange(val);
}});
});

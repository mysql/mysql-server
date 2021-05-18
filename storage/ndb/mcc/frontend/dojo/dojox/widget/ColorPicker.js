//>>built
require({cache:{"url:dojox/widget/ColorPicker/ColorPicker.html":"<table class=\"dojoxColorPicker\" dojoAttachEvent=\"onkeypress: _handleKey\" cellpadding=\"0\" cellspacing=\"0\" role=\"presentation\">\n\t<tr>\n\t\t<td valign=\"top\" class=\"dojoxColorPickerRightPad\">\n\t\t\t<div class=\"dojoxColorPickerBox\">\n\t\t\t\t<!-- Forcing ABS in style attr due to dojo DND issue with not picking it up form the class. -->\n\t\t\t\t<img title=\"${saturationPickerTitle}\" alt=\"${saturationPickerTitle}\" class=\"dojoxColorPickerPoint\" src=\"${_pickerPointer}\" tabIndex=\"0\" dojoAttachPoint=\"cursorNode\" style=\"position: absolute; top: 0px; left: 0px;\">\n\t\t\t\t<img role=\"presentation\" alt=\"\" dojoAttachPoint=\"colorUnderlay\" dojoAttachEvent=\"onclick: _setPoint, onmousedown: _stopDrag\" class=\"dojoxColorPickerUnderlay\" src=\"${_underlay}\" ondragstart=\"return false\">\n\t\t\t</div>\n\t\t</td>\n\t\t<td valign=\"top\" class=\"dojoxColorPickerRightPad\">\n\t\t\t<div class=\"dojoxHuePicker\">\n\t\t\t\t<!-- Forcing ABS in style attr due to dojo DND issue with not picking it up form the class. -->\n\t\t\t\t<img dojoAttachPoint=\"hueCursorNode\" tabIndex=\"0\" class=\"dojoxHuePickerPoint\" title=\"${huePickerTitle}\" alt=\"${huePickerTitle}\" src=\"${_huePickerPointer}\" style=\"position: absolute; top: 0px; left: 0px;\">\n\t\t\t\t<div class=\"dojoxHuePickerUnderlay\" dojoAttachPoint=\"hueNode\">\n\t\t\t\t    <img role=\"presentation\" alt=\"\" dojoAttachEvent=\"onclick: _setHuePoint, onmousedown: _stopDrag\" src=\"${_hueUnderlay}\">\n\t\t\t\t</div>\n\t\t\t</div>\n\t\t</td>\n\t\t<td valign=\"top\">\n\t\t\t<table cellpadding=\"0\" cellspacing=\"0\" role=\"presentation\">\n\t\t\t\t<tr>\n\t\t\t\t\t<td valign=\"top\" class=\"dojoxColorPickerPreviewContainer\">\n\t\t\t\t\t\t<table cellpadding=\"0\" cellspacing=\"0\" role=\"presentation\">\n\t\t\t\t\t\t\t<tr>\n\t\t\t\t\t\t\t\t<td valign=\"top\" class=\"dojoxColorPickerRightPad\">\n\t\t\t\t\t\t\t\t\t<div dojoAttachPoint=\"previewNode\" class=\"dojoxColorPickerPreview\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td valign=\"top\">\n\t\t\t\t\t\t\t\t\t<div dojoAttachPoint=\"safePreviewNode\" class=\"dojoxColorPickerWebSafePreview\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t</tr>\n\t\t\t\t\t\t</table>\n\t\t\t\t\t</td>\n\t\t\t\t</tr>\n\t\t\t\t<tr>\n\t\t\t\t\t<td valign=\"bottom\">\n\t\t\t\t\t\t<table class=\"dojoxColorPickerOptional\" cellpadding=\"0\" cellspacing=\"0\" role=\"presentation\">\n\t\t\t\t\t\t\t<tr>\n\t\t\t\t\t\t\t\t<td>\n\t\t\t\t\t\t\t\t\t<div class=\"dijitInline dojoxColorPickerRgb\" dojoAttachPoint=\"rgbNode\">\n\t\t\t\t\t\t\t\t\t\t<table cellpadding=\"1\" cellspacing=\"1\" role=\"presentation\">\n\t\t\t\t\t\t\t\t\t\t<tr><td><label for=\"${_uId}_r\">${redLabel}</label></td><td><input id=\"${_uId}_r\" dojoAttachPoint=\"Rval\" size=\"1\" dojoAttachEvent=\"onchange: _colorInputChange\"></td></tr>\n\t\t\t\t\t\t\t\t\t\t<tr><td><label for=\"${_uId}_g\">${greenLabel}</label></td><td><input id=\"${_uId}_g\" dojoAttachPoint=\"Gval\" size=\"1\" dojoAttachEvent=\"onchange: _colorInputChange\"></td></tr>\n\t\t\t\t\t\t\t\t\t\t<tr><td><label for=\"${_uId}_b\">${blueLabel}</label></td><td><input id=\"${_uId}_b\" dojoAttachPoint=\"Bval\" size=\"1\" dojoAttachEvent=\"onchange: _colorInputChange\"></td></tr>\n\t\t\t\t\t\t\t\t\t\t</table>\n\t\t\t\t\t\t\t\t\t</div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td>\n\t\t\t\t\t\t\t\t\t<div class=\"dijitInline dojoxColorPickerHsv\" dojoAttachPoint=\"hsvNode\">\n\t\t\t\t\t\t\t\t\t\t<table cellpadding=\"1\" cellspacing=\"1\" role=\"presentation\">\n\t\t\t\t\t\t\t\t\t\t<tr><td><label for=\"${_uId}_h\">${hueLabel}</label></td><td><input id=\"${_uId}_h\" dojoAttachPoint=\"Hval\"size=\"1\" dojoAttachEvent=\"onchange: _colorInputChange\"> ${degLabel}</td></tr>\n\t\t\t\t\t\t\t\t\t\t<tr><td><label for=\"${_uId}_s\">${saturationLabel}</label></td><td><input id=\"${_uId}_s\" dojoAttachPoint=\"Sval\" size=\"1\" dojoAttachEvent=\"onchange: _colorInputChange\"> ${percentSign}</td></tr>\n\t\t\t\t\t\t\t\t\t\t<tr><td><label for=\"${_uId}_v\">${valueLabel}</label></td><td><input id=\"${_uId}_v\" dojoAttachPoint=\"Vval\" size=\"1\" dojoAttachEvent=\"onchange: _colorInputChange\"> ${percentSign}</td></tr>\n\t\t\t\t\t\t\t\t\t\t</table>\n\t\t\t\t\t\t\t\t\t</div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t</tr>\n\t\t\t\t\t\t\t<tr>\n\t\t\t\t\t\t\t\t<td colspan=\"2\">\n\t\t\t\t\t\t\t\t\t<div class=\"dojoxColorPickerHex\" dojoAttachPoint=\"hexNode\" aria-live=\"polite\">\t\n\t\t\t\t\t\t\t\t\t\t<label for=\"${_uId}_hex\">&nbsp;${hexLabel}&nbsp;</label><input id=\"${_uId}_hex\" dojoAttachPoint=\"hexCode, focusNode, valueNode\" size=\"6\" class=\"dojoxColorPickerHexCode\" dojoAttachEvent=\"onchange: _colorInputChange\">\n\t\t\t\t\t\t\t\t\t</div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t</tr>\n\t\t\t\t\t\t</table>\n\t\t\t\t\t</td>\n\t\t\t\t</tr>\n\t\t\t</table>\n\t\t</td>\n\t</tr>\n</table>\n\n"}});
define("dojox/widget/ColorPicker",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/_base/html","dojo/_base/connect","dojo/_base/sniff","dojo/_base/window","dojo/_base/event","dojo/dom","dojo/dom-class","dojo/keys","dojo/fx","dojo/dnd/move","dijit/registry","dijit/_base/focus","dijit/form/_FormWidget","dijit/typematic","dojox/color","dojo/i18n","dojo/i18n!./nls/ColorPicker","dojo/i18n!dojo/cldr/nls/number","dojo/text!./ColorPicker/ColorPicker.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,fx,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16){
_1.experimental("dojox.widget.ColorPicker");
var _17=function(hex){
return hex;
};
return _2("dojox.widget.ColorPicker",_10,{showRgb:true,showHsv:true,showHex:true,webSafe:true,animatePoint:true,slideDuration:250,liveUpdate:false,PICKER_HUE_H:150,PICKER_SAT_VAL_H:150,PICKER_SAT_VAL_W:150,PICKER_HUE_SELECTOR_H:8,PICKER_SAT_SELECTOR_H:10,PICKER_SAT_SELECTOR_W:10,value:"#ffffff",_underlay:require.toUrl("dojox/widget/ColorPicker/images/underlay.png"),_hueUnderlay:require.toUrl("dojox/widget/ColorPicker/images/hue.png"),_pickerPointer:require.toUrl("dojox/widget/ColorPicker/images/pickerPointer.png"),_huePickerPointer:require.toUrl("dojox/widget/ColorPicker/images/hueHandle.png"),_huePickerPointerAlly:require.toUrl("dojox/widget/ColorPicker/images/hueHandleA11y.png"),templateString:_16,postMixInProperties:function(){
if(_b.contains(_8.body(),"dijit_a11y")){
this._huePickerPointer=this._huePickerPointerAlly;
}
this._uId=_e.getUniqueId(this.id);
_3.mixin(this,_13.getLocalization("dojox.widget","ColorPicker"));
_3.mixin(this,_13.getLocalization("dojo.cldr","number"));
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
if(_7("ie")<7){
this.colorUnderlay.style.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+this._underlay+"', sizingMethod='scale')";
this.colorUnderlay.src=this._blankGif.toString();
}
if(!this.showRgb){
this.rgbNode.style.visibility="hidden";
}
if(!this.showHsv){
this.hsvNode.style.visibility="hidden";
}
if(!this.showHex){
this.hexNode.style.visibility="hidden";
}
if(!this.webSafe){
this.safePreviewNode.style.visibility="hidden";
}
},startup:function(){
if(this._started){
return;
}
this._started=true;
this.set("value",this.value);
this._mover=new _d.boxConstrainedMoveable(this.cursorNode,{box:{t:-(this.PICKER_SAT_SELECTOR_H/2),l:-(this.PICKER_SAT_SELECTOR_W/2),w:this.PICKER_SAT_VAL_W,h:this.PICKER_SAT_VAL_H}});
this._hueMover=new _d.boxConstrainedMoveable(this.hueCursorNode,{box:{t:-(this.PICKER_HUE_SELECTOR_H/2),l:0,w:0,h:this.PICKER_HUE_H}});
this._subs=[];
this._subs.push(_6.subscribe("/dnd/move/stop",_3.hitch(this,"_clearTimer")));
this._subs.push(_6.subscribe("/dnd/move/start",_3.hitch(this,"_setTimer")));
this._keyListeners=[];
this._connects.push(_11.addKeyListener(this.hueCursorNode,{charOrCode:_c.UP_ARROW,shiftKey:false,metaKey:false,ctrlKey:false,altKey:false},this,_3.hitch(this,this._updateHueCursorNode),25,25));
this._connects.push(_11.addKeyListener(this.hueCursorNode,{charOrCode:_c.DOWN_ARROW,shiftKey:false,metaKey:false,ctrlKey:false,altKey:false},this,_3.hitch(this,this._updateHueCursorNode),25,25));
this._connects.push(_11.addKeyListener(this.cursorNode,{charOrCode:_c.UP_ARROW,shiftKey:false,metaKey:false,ctrlKey:false,altKey:false},this,_3.hitch(this,this._updateCursorNode),25,25));
this._connects.push(_11.addKeyListener(this.cursorNode,{charOrCode:_c.DOWN_ARROW,shiftKey:false,metaKey:false,ctrlKey:false,altKey:false},this,_3.hitch(this,this._updateCursorNode),25,25));
this._connects.push(_11.addKeyListener(this.cursorNode,{charOrCode:_c.LEFT_ARROW,shiftKey:false,metaKey:false,ctrlKey:false,altKey:false},this,_3.hitch(this,this._updateCursorNode),25,25));
this._connects.push(_11.addKeyListener(this.cursorNode,{charOrCode:_c.RIGHT_ARROW,shiftKey:false,metaKey:false,ctrlKey:false,altKey:false},this,_3.hitch(this,this._updateCursorNode),25,25));
},_setValueAttr:function(_18,_19){
if(!this._started){
return;
}
this.setColor(_18,_19);
},setColor:function(col,_1a){
col=_12.fromString(col);
this._updatePickerLocations(col);
this._updateColorInputs(col);
this._updateValue(col,_1a);
},_setTimer:function(_1b){
if(_1b.node!=this.cursorNode&&_1b.node!=this.hueCursorNode){
return;
}
_f.focus(_1b.node);
_a.setSelectable(this.domNode,false);
this._timer=setInterval(_3.hitch(this,"_updateColor"),45);
},_clearTimer:function(_1c){
if(!this._timer){
return;
}
clearInterval(this._timer);
this._timer=null;
this.onChange(this.value);
_a.setSelectable(this.domNode,true);
},_setHue:function(h){
_5.style(this.colorUnderlay,"backgroundColor",_12.fromHsv(h,100,100).toHex());
},_updateHueCursorNode:function(_1d,_1e,e){
if(_1d!==-1){
var y=_5.style(this.hueCursorNode,"top");
var _1f=this.PICKER_HUE_SELECTOR_H/2;
y+=_1f;
var _20=false;
if(e.charOrCode==_c.UP_ARROW){
if(y>0){
y-=1;
_20=true;
}
}else{
if(e.charOrCode==_c.DOWN_ARROW){
if(y<this.PICKER_HUE_H){
y+=1;
_20=true;
}
}
}
y-=_1f;
if(_20){
_5.style(this.hueCursorNode,"top",y+"px");
}
}else{
this._updateColor(true);
}
},_updateCursorNode:function(_21,_22,e){
var _23=this.PICKER_SAT_SELECTOR_H/2;
var _24=this.PICKER_SAT_SELECTOR_W/2;
if(_21!==-1){
var y=_5.style(this.cursorNode,"top");
var x=_5.style(this.cursorNode,"left");
y+=_23;
x+=_24;
var _25=false;
if(e.charOrCode==_c.UP_ARROW){
if(y>0){
y-=1;
_25=true;
}
}else{
if(e.charOrCode==_c.DOWN_ARROW){
if(y<this.PICKER_SAT_VAL_H){
y+=1;
_25=true;
}
}else{
if(e.charOrCode==_c.LEFT_ARROW){
if(x>0){
x-=1;
_25=true;
}
}else{
if(e.charOrCode==_c.RIGHT_ARROW){
if(x<this.PICKER_SAT_VAL_W){
x+=1;
_25=true;
}
}
}
}
}
if(_25){
y-=_23;
x-=_24;
_5.style(this.cursorNode,"top",y+"px");
_5.style(this.cursorNode,"left",x+"px");
}
}else{
this._updateColor(true);
}
},_updateColor:function(_26){
var _27=this.PICKER_HUE_SELECTOR_H/2,_28=this.PICKER_SAT_SELECTOR_H/2,_29=this.PICKER_SAT_SELECTOR_W/2;
var _2a=_5.style(this.hueCursorNode,"top")+_27,_2b=_5.style(this.cursorNode,"top")+_28,_2c=_5.style(this.cursorNode,"left")+_29,h=Math.round(360-(_2a/this.PICKER_HUE_H*360)),col=_12.fromHsv(h,_2c/this.PICKER_SAT_VAL_W*100,100-(_2b/this.PICKER_SAT_VAL_H*100));
this._updateColorInputs(col);
this._updateValue(col,_26);
if(h!=this._hue){
this._setHue(h);
}
},_colorInputChange:function(e){
var col,_2d=false;
switch(e.target){
case this.hexCode:
col=_12.fromString(e.target.value);
_2d=true;
break;
case this.Rval:
case this.Gval:
case this.Bval:
col=_12.fromArray([this.Rval.value,this.Gval.value,this.Bval.value]);
_2d=true;
break;
case this.Hval:
case this.Sval:
case this.Vval:
col=_12.fromHsv(this.Hval.value,this.Sval.value,this.Vval.value);
_2d=true;
break;
}
if(_2d){
this._updatePickerLocations(col);
this._updateColorInputs(col);
this._updateValue(col,true);
}
},_updateValue:function(col,_2e){
var hex=col.toHex();
this.value=this.valueNode.value=hex;
if(_2e&&(!this._timer||this.liveUpdate)){
this.onChange(hex);
}
},_updatePickerLocations:function(col){
var _2f=this.PICKER_HUE_SELECTOR_H/2,_30=this.PICKER_SAT_SELECTOR_H/2,_31=this.PICKER_SAT_SELECTOR_W/2;
var hsv=col.toHsv(),_32=Math.round(this.PICKER_HUE_H-hsv.h/360*this.PICKER_HUE_H)-_2f,_33=Math.round(hsv.s/100*this.PICKER_SAT_VAL_W)-_31,_34=Math.round(this.PICKER_SAT_VAL_H-hsv.v/100*this.PICKER_SAT_VAL_H)-_30;
if(this.animatePoint){
fx.slideTo({node:this.hueCursorNode,duration:this.slideDuration,top:_32,left:0}).play();
fx.slideTo({node:this.cursorNode,duration:this.slideDuration,top:_34,left:_33}).play();
}else{
_5.style(this.hueCursorNode,"top",_32+"px");
_5.style(this.cursorNode,{left:_33+"px",top:_34+"px"});
}
if(hsv.h!=this._hue){
this._setHue(hsv.h);
}
},_updateColorInputs:function(col){
var hex=col.toHex();
if(this.showRgb){
this.Rval.value=col.r;
this.Gval.value=col.g;
this.Bval.value=col.b;
}
if(this.showHsv){
var hsv=col.toHsv();
this.Hval.value=Math.round((hsv.h));
this.Sval.value=Math.round(hsv.s);
this.Vval.value=Math.round(hsv.v);
}
if(this.showHex){
this.hexCode.value=hex;
}
this.previewNode.style.backgroundColor=hex;
if(this.webSafe){
this.safePreviewNode.style.backgroundColor=_17(hex);
}
},_setHuePoint:function(evt){
var _35=this.PICKER_HUE_SELECTOR_H/2;
var _36=evt.layerY||(evt.y-evt.target.getBoundingClientRect().top);
_36-=_35;
if(this.animatePoint){
fx.slideTo({node:this.hueCursorNode,duration:this.slideDuration,top:_36,left:0,onEnd:_3.hitch(this,function(){
this._updateColor(true);
_f.focus(this.hueCursorNode);
})}).play();
}else{
_5.style(this.hueCursorNode,"top",_36+"px");
this._updateColor(true);
}
},_setPoint:function(evt){
var _37=this.PICKER_SAT_SELECTOR_H/2;
var _38=this.PICKER_SAT_SELECTOR_W/2;
var _39=evt.layerY||(evt.y-evt.target.getBoundingClientRect().top);
_39-=_37;
var _3a=evt.layerX||(evt.x-evt.target.getBoundingClientRect().left);
_3a-=_38;
if(evt){
_f.focus(evt.target);
}
if(this.animatePoint){
fx.slideTo({node:this.cursorNode,duration:this.slideDuration,top:_39,left:_3a,onEnd:_3.hitch(this,function(){
this._updateColor(true);
_f.focus(this.cursorNode);
})}).play();
}else{
_5.style(this.cursorNode,{left:_3a+"px",top:_39+"px"});
this._updateColor(true);
}
},_handleKey:function(e){
},focus:function(){
if(!this.focused){
_f.focus(this.focusNode);
}
},_stopDrag:function(e){
_9.stop(e);
},destroy:function(){
this.inherited(arguments);
_4.forEach(this._subs,function(sub){
_6.unsubscribe(sub);
});
delete this._subs;
}});
});

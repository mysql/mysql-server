//>>built
define("dojox/dgauges/TextIndicator",["dojo/_base/lang","dojo/_base/declare","dojo/_base/sniff","dojo/_base/array","dojo/on","dojox/gfx","./IndicatorBase"],function(_1,_2,_3,_4,on,_5,_6){
return _2("dojox.dgauges.TextIndicator",_6,{font:null,x:0,y:0,align:"middle",color:"black",indicator:null,labelFunc:null,constructor:function(){
this.addInvalidatingProperties(["indicator"]);
var _7=["x","y","font","align","color","labelFunc"];
_4.forEach(_7,_1.hitch(this,function(_8){
this.watch(_8,this._resetText);
}));
this.watch("indicator",_1.hitch(this,this._indicatorChanged));
},postscript:function(_9){
this.inherited(arguments);
if(_9&&_9.indicator){
this._indicatorChanged("indicator",null,_9.indicator);
}
},_resetText:function(){
this._textCreated=false;
this.invalidateRendering();
},_valueWatcher:null,_indicatorChanged:function(_a,_b,_c){
if(this._valueWatcher){
this._valueWatcher.unwatch();
}
this._valueWatcher=_c.watch("value",_1.hitch(this,this.refreshRendering));
},_getFont:function(){
var _d=this.font;
if(!_d&&this._gauge){
_d=this._gauge.font;
}
if(!_d){
_d=_5.defaultFont;
}
return _d;
},_textCreated:false,_textInstance:null,_createText:function(_e,_f,_10,_11,x,y,_12){
var _13=_e.createText({x:x,y:y,text:_11,align:_12}).setFont(_f).setFill(_10);
return _13;
},refreshRendering:function(){
if(this._gfxGroup==null){
return;
}
var _14;
if(this.indicator){
_14=this.indicator.value;
}else{
_14=this.value;
}
if(this.labelFunc){
_14=this.labelFunc(_14);
}
var _15=_3("iphone");
if(!this._textCreated||(_15!=undefined&&_15<5)){
this._gfxGroup.clear();
var _16=this._getFont();
this._textInstance=this._createText(this._gfxGroup,_16,_16.color?_16.color:this.color,"",this.x,this.y,this.align);
this._textCreated=true;
}
this._textInstance.setShape({text:_14});
return this._textInstance;
}});
});

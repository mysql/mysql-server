//>>built
define("dojox/dgauges/ScaleBase",["dojo/_base/lang","dojo/_base/declare","dojox/gfx","dojo/_base/array","dojox/widget/_Invalidating","dojo/_base/sniff"],function(_1,_2,_3,_4,_5,_6){
return _2("dojox.dgauges.ScaleBase",_5,{scaler:null,font:null,labelPosition:null,labelGap:1,tickStroke:null,_gauge:null,_gfxGroup:null,_bgGroup:null,_fgGroup:null,_indicators:null,_indicatorsIndex:null,_indicatorsRenderers:null,constructor:function(){
this._indicators=[];
this._indicatorsIndex={};
this._indicatorsRenderers={};
this._gauge=null;
this._gfxGroup=null;
this.tickStroke={color:"black",width:_6("ie")<=8?1:0.5};
this.addInvalidatingProperties(["scaler","font","labelGap","labelPosition","tickShapeFunc","tickLabelFunc","tickStroke"]);
this.watch("scaler",_1.hitch(this,this._watchScaler));
},postscript:function(_7){
this.inherited(arguments);
if(_7&&_7.scaler){
this._watchScaler("scaler",null,_7.scaler);
}
},_watchers:null,_watchScaler:function(_8,_9,_a){
_4.forEach(this._watchers,_1.hitch(this,function(_b){
_b.unwatch();
}));
var _c=_a.watchedProperties;
this._watchers=[];
_4.forEach(_c,_1.hitch(this,function(_d){
this._watchers.push(_a.watch(_d,_1.hitch(this,this.invalidateRendering)));
}));
},_getFont:function(){
var _e=this.font;
if(!_e){
_e=this._gauge.font;
}
if(!_e){
_e=_3.defaultFont;
}
return _e;
},positionForValue:function(_f){
return 0;
},valueForPosition:function(_10){
},tickLabelFunc:function(_11){
if(_11.isMinor){
return null;
}else{
return String(_11.value);
}
},tickShapeFunc:function(_12,_13,_14){
return _12.createLine({x1:0,y1:0,x2:_14.isMinor?6:10,y2:0}).setStroke(this.tickStroke);
},getIndicatorRenderer:function(_15){
return this._indicatorsRenderers[_15];
},removeIndicator:function(_16){
var _17=this._indicatorsIndex[_16];
if(_17){
_17._gfxGroup.removeShape();
var idx=this._indicators.indexOf(_17);
this._indicators.splice(idx,1);
_17._disconnectListeners();
delete this._indicatorsIndex[_16];
delete this._indicatorsRenderers[_16];
}
if(this._gauge){
this._gauge._resetMainIndicator();
}
this.invalidateRendering();
return _17;
},getIndicator:function(_18){
return this._indicatorsIndex[_18];
},addIndicator:function(_19,_1a,_1b){
if(this._indicatorsIndex[_19]&&this._indicatorsIndex[_19]!=_1a){
this.removeIndicator(_19);
}
this._indicators.push(_1a);
this._indicatorsIndex[_19]=_1a;
if(!this._ticksGroup){
this._createSubGroups();
}
var _1c=_1b?this._bgGroup:this._fgGroup;
_1a._gfxGroup=_1c.createGroup();
_1a.scale=this;
return this.invalidateRendering();
},_createSubGroups:function(){
if(!this._gfxGroup||this._ticksGroup){
return;
}
this._bgGroup=this._gfxGroup.createGroup();
this._ticksGroup=this._gfxGroup.createGroup();
this._fgGroup=this._gfxGroup.createGroup();
},refreshRendering:function(){
if(!this._ticksGroup){
this._createSubGroups();
}
}});
});

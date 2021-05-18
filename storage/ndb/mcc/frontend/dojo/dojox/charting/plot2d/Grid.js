//>>built
define("dojox/charting/plot2d/Grid",["dojo/_base/lang","dojo/_base/declare","dojo/_base/array","dojo/sniff","./CartesianBase","./common","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,dc,du,fx){
var _6=function(a,b){
return a.value-b.value;
};
return _2("dojox.charting.plot2d.Grid",_5,{defaultParams:{hMajorLines:true,hMinorLines:false,vMajorLines:true,vMinorLines:false,hStripes:false,vStripes:false,animate:null,enableCache:false,renderOnAxis:true},optionalParams:{majorHLine:{},minorHLine:{},majorVLine:{},minorVLine:{},hFill:{},vFill:{},hAlternateFill:{},vAlternateFill:{}},constructor:function(_7,_8){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
this.animate=this.opt.animate;
if(this.opt.enableCache){
this._lineFreePool=[];
this._lineUsePool=[];
this._rectFreePool=[];
this._rectUsePool=[];
}
},addSeries:function(_9){
return this;
},getSeriesStats:function(){
return _1.delegate(dc.defaultStats);
},cleanGroup:function(){
this.inherited(arguments);
if(this.opt.enableCache){
this._lineFreePool=this._lineFreePool.concat(this._lineUsePool);
this._lineUsePool=[];
this._rectFreePool=this._rectFreePool.concat(this._rectUsePool);
this._rectUsePool=[];
}
},createLine:function(_a,_b){
var _c;
if(this.opt.enableCache&&this._lineFreePool.length>0){
_c=this._lineFreePool.pop();
_c.setShape(_b);
_a.add(_c);
}else{
_c=_a.createLine(_b);
}
if(this.opt.enableCache){
this._lineUsePool.push(_c);
}
return _c;
},createRect:function(_d,_e){
var _f;
if(this.opt.enableCache&&this._rectFreePool.length>0){
_f=this._rectFreePool.pop();
_f.setShape(_e);
_d.add(_f);
}else{
_f=_d.createRect(_e);
}
if(this.opt.enableCache){
this._rectUsePool.push(_f);
}
return _f;
},render:function(dim,_10){
if(this.zoom){
return this.performZoom(dim,_10);
}
this.dirty=this.isDirty();
if(!this.dirty){
return this;
}
this.cleanGroup();
var s=this.getGroup(),ta=this.chart.theme,_11,_12;
if((_4("ios")&&_4("ios")<6)||_4("android")||(_4("safari")&&!_4("ios"))){
var w=Math.max(0,dim.width-_10.l-_10.r),h=Math.max(0,dim.height-_10.t-_10.b);
s.createRect({x:_10.l,y:_10.t,width:w,height:h});
}
if(this._vAxis){
_12=this._vAxis.getTicks();
var _13=this._vAxis.getScaler();
if(_12!=null&&_13!=null){
var vt=_13.scaler.getTransformerFromModel(_13);
if(this.opt.hStripes){
this._renderHRect(_12,ta.grid,dim,_10,_13,vt);
}
if(this.opt.hMinorLines){
_11=this.opt.minorHLine||(ta.grid&&ta.grid.minorLine)||ta.axis.minorTick;
this._renderHLines(_12.minor,_11,dim,_10,_13,vt);
}
if(this.opt.hMajorLines){
_11=this.opt.majorHLine||(ta.grid&&ta.grid.majorLine)||ta.axis.majorTick;
this._renderHLines(_12.major,_11,dim,_10,_13,vt);
}
}
}
if(this._hAxis){
_12=this._hAxis.getTicks();
var _14=this._hAxis.getScaler();
if(_12!=null&&_14!=null){
var ht=_14.scaler.getTransformerFromModel(_14);
if(this.opt.vStripes){
this._renderVRect(_12,ta.grid,dim,_10,_14,ht);
}
if(_12&&this.opt.vMinorLines){
_11=this.opt.minorVLine||(ta.grid&&ta.grid.minorLine)||ta.axis.minorTick;
this._renderVLines(_12.minor,_11,dim,_10,_14,ht);
}
if(_12&&this.opt.vMajorLines){
_11=this.opt.majorVLine||(ta.grid&&ta.grid.majorLine)||ta.axis.majorTick;
this._renderVLines(_12.major,_11,dim,_10,_14,ht);
}
}
}
this.dirty=false;
return this;
},_renderHLines:function(_15,_16,dim,_17,_18,vt){
var s=this.getGroup();
_3.forEach(_15,function(_19){
if(!this.opt.renderOnAxis&&_19.value==(this._vAxis.opt.leftBottom?_18.bounds.from:_18.bounds.to)){
return;
}
var y=dim.height-_17.b-vt(_19.value);
var _1a=this.createLine(s,{x1:_17.l,y1:y,x2:dim.width-_17.r,y2:y}).setStroke(_16);
if(this.animate){
this._animateGrid(_1a,"h",_17.l,_17.r+_17.l-dim.width);
}
},this);
},_renderVLines:function(_1b,_1c,dim,_1d,_1e,ht){
var s=this.getGroup();
_3.forEach(_1b,function(_1f){
if(!this.opt.renderOnAxis&&_1f.value==(this._hAxis.opt.leftBottom?_1e.bounds.from:_1e.bounds.to)){
return;
}
var x=_1d.l+ht(_1f.value);
var _20=this.createLine(s,{x1:x,y1:_1d.t,x2:x,y2:dim.height-_1d.b}).setStroke(_1c);
if(this.animate){
this._animateGrid(_20,"v",dim.height-_1d.b,dim.height-_1d.b-_1d.t);
}
},this);
},_renderHRect:function(_21,_22,dim,_23,_24,vt){
var _25,_26,y,y2,_27;
var _28=_21.major.concat(_21.minor);
_28.sort(_6);
if(_28[0].value>_24.bounds.from){
_28.splice(0,0,{value:_24.bounds.from});
}
if(_28[_28.length-1].value<_24.bounds.to){
_28.push({value:_24.bounds.to});
}
var s=this.getGroup();
for(var j=0;j<_28.length-1;j++){
_26=_28[j];
y=dim.height-_23.b-vt(_26.value);
y2=dim.height-_23.b-vt(_28[j+1].value);
_25=(j%2==0)?(this.opt.hAlternateFill||(_22&&_22.alternateFill)):(this.opt.hFill||(_22&&_22.fill));
if(_25){
_27=this.createRect(s,{x:_23.l,y:y,width:dim.width-_23.r,height:y-y2}).setFill(_25);
if(this.animate){
this._animateGrid(_27,"h",_23.l,_23.r+_23.l-dim.width);
}
}
}
},_renderVRect:function(_29,_2a,dim,_2b,_2c,ht){
var _2d,_2e,x,x2,_2f;
var _30=_29.major.concat(_29.minor);
_30.sort(_6);
if(_30[0].value>_2c.bounds.from){
_30.splice(0,0,{value:_2c.bounds.from});
}
if(_30[_30.length-1].value<_2c.bounds.to){
_30.push({value:_2c.bounds.to});
}
var s=this.getGroup();
for(var j=0;j<_30.length-1;j++){
_2e=_30[j];
x=_2b.l+ht(_2e.value);
x2=_2b.l+ht(_30[j+1].value);
_2d=(j%2==0)?(this.opt.vAlternateFill||(_2a&&_2a.alternateFill)):(this.opt.vFill||(_2a&&_2a.fill));
if(_2d){
_2f=this.createRect(s,{x:x,y:_2b.t,width:x2-x,height:dim.width-_2b.r}).setFill(_2d);
if(this.animate){
this._animateGrid(_2f,"v",dim.height-_2b.b,dim.height-_2b.b-_2b.t);
}
}
}
},_animateGrid:function(_31,_32,_33,_34){
var _35=_32=="h"?[_33,0]:[0,_33];
var _36=_32=="h"?[1/_34,1]:[1,1/_34];
fx.animateTransform(_1.delegate({shape:_31,duration:1200,transform:[{name:"translate",start:_35,end:[0,0]},{name:"scale",start:_36,end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});

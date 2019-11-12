//>>built
define("dojox/charting/plot2d/CartesianBase",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","./Base","../scaler/primitive","dojox/gfx/fx"],function(_1,_2,_3,_4,_5,fx){
return _2("dojox.charting.plot2d.CartesianBase",_4,{constructor:function(_6,_7){
this.axes=["hAxis","vAxis"];
this.zoom=null,this.zoomQueue=[];
this.lastWindow={vscale:1,hscale:1,xoffset:0,yoffset:0};
},clear:function(){
this.inherited(arguments);
this._hAxis=null;
this._vAxis=null;
return this;
},cleanGroup:function(_8){
this.inherited(arguments,[_8||this.chart.plotGroup]);
},setAxis:function(_9){
if(_9){
this[_9.vertical?"_vAxis":"_hAxis"]=_9;
}
return this;
},toPage:function(_a){
var ah=this._hAxis,av=this._vAxis,sh=ah.getScaler(),sv=av.getScaler(),th=sh.scaler.getTransformerFromModel(sh),tv=sv.scaler.getTransformerFromModel(sv),c=this.chart.getCoords(),o=this.chart.offsets,_b=this.chart.dim;
var t=function(_c){
var r={};
r.x=th(_c[ah.name])+c.x+o.l;
r.y=c.y+_b.height-o.b-tv(_c[av.name]);
return r;
};
return _a?t(_a):t;
},toData:function(_d){
var ah=this._hAxis,av=this._vAxis,sh=ah.getScaler(),sv=av.getScaler(),th=sh.scaler.getTransformerFromPlot(sh),tv=sv.scaler.getTransformerFromPlot(sv),c=this.chart.getCoords(),o=this.chart.offsets,_e=this.chart.dim;
var t=function(_f){
var r={};
r[ah.name]=th(_f.x-c.x-o.l);
r[av.name]=tv(c.y+_e.height-_f.y-o.b);
return r;
};
return _d?t(_d):t;
},isDirty:function(){
return this.dirty||this._hAxis&&this._hAxis.dirty||this._vAxis&&this._vAxis.dirty;
},performZoom:function(dim,_10){
var vs=this._vAxis.scale||1,hs=this._hAxis.scale||1,_11=dim.height-_10.b,_12=this._hScaler.bounds,_13=(_12.from-_12.lower)*_12.scale,_14=this._vScaler.bounds,_15=(_14.from-_14.lower)*_14.scale,_16=vs/this.lastWindow.vscale,_17=hs/this.lastWindow.hscale,_18=(this.lastWindow.xoffset-_13)/((this.lastWindow.hscale==1)?hs:this.lastWindow.hscale),_19=(_15-this.lastWindow.yoffset)/((this.lastWindow.vscale==1)?vs:this.lastWindow.vscale),_1a=this.group,_1b=fx.animateTransform(_1.delegate({shape:_1a,duration:1200,transform:[{name:"translate",start:[0,0],end:[_10.l*(1-_17),_11*(1-_16)]},{name:"scale",start:[1,1],end:[_17,_16]},{name:"original"},{name:"translate",start:[0,0],end:[_18,_19]}]},this.zoom));
_1.mixin(this.lastWindow,{vscale:vs,hscale:hs,xoffset:_13,yoffset:_15});
this.zoomQueue.push(_1b);
_3.connect(_1b,"onEnd",this,function(){
this.zoom=null;
this.zoomQueue.shift();
if(this.zoomQueue.length>0){
this.zoomQueue[0].play();
}
});
if(this.zoomQueue.length==1){
this.zoomQueue[0].play();
}
return this;
},initializeScalers:function(dim,_1c){
if(this._hAxis){
if(!this._hAxis.initialized()){
this._hAxis.calculate(_1c.hmin,_1c.hmax,dim.width);
}
this._hScaler=this._hAxis.getScaler();
}else{
this._hScaler=_5.buildScaler(_1c.hmin,_1c.hmax,dim.width);
}
if(this._vAxis){
if(!this._vAxis.initialized()){
this._vAxis.calculate(_1c.vmin,_1c.vmax,dim.height);
}
this._vScaler=this._vAxis.getScaler();
}else{
this._vScaler=_5.buildScaler(_1c.vmin,_1c.vmax,dim.height);
}
return this;
}});
});

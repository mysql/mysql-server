//>>built
define("dojox/charting/plot2d/Bars",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./Base","./common","dojox/gfx/fx","dojox/lang/utils","dojox/lang/functional","dojox/lang/functional/reversed"],function(_1,_2,_3,_4,_5,dc,fx,du,df,_6){
var _7=_6.lambda("item.purgeGroup()");
return _4("dojox.charting.plot2d.Bars",_5,{defaultParams:{hAxis:"x",vAxis:"y",gap:0,animate:null,enableCache:false},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_8,_9){
this.opt=_2.clone(this.defaultParams);
du.updateWithObject(this.opt,_9);
du.updateWithPattern(this.opt,_9,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},getSeriesStats:function(){
var _a=dc.collectSimpleStats(this.series),t;
_a.hmin-=0.5;
_a.hmax+=0.5;
t=_a.hmin,_a.hmin=_a.vmin,_a.vmin=t;
t=_a.hmax,_a.hmax=_a.vmax,_a.vmax=t;
return _a;
},createRect:function(_b,_c,_d){
var _e;
if(this.opt.enableCache&&_b._rectFreePool.length>0){
_e=_b._rectFreePool.pop();
_e.setShape(_d);
_c.add(_e);
}else{
_e=_c.createRect(_d);
}
if(this.opt.enableCache){
_b._rectUsePool.push(_e);
}
return _e;
},render:function(_f,_10){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_f,_10);
}
this.dirty=this.isDirty();
this.resetEvents();
if(this.dirty){
_3.forEach(this.series,_7);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_11){
_11.cleanGroup(s);
});
}
var t=this.chart.theme,f,gap,_12,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_13=Math.max(0,this._hScaler.bounds.lower),_14=ht(_13),_15=this.events();
f=dc.calculateBarSize(this._vScaler.bounds.scale,this.opt);
gap=f.gap;
_12=f.size;
for(var i=this.series.length-1;i>=0;--i){
var run=this.series[i];
if(!this.dirty&&!run.dirty){
t.skip();
this._reconnectEvents(run.name);
continue;
}
run.cleanGroup();
if(this.opt.enableCache){
run._rectFreePool=(run._rectFreePool?run._rectFreePool:[]).concat(run._rectUsePool?run._rectUsePool:[]);
run._rectUsePool=[];
}
var _16=t.next("bar",[this.opt,run]),s=run.group,_17=new Array(run.data.length);
for(var j=0;j<run.data.length;++j){
var _18=run.data[j];
if(_18!==null){
var v=typeof _18=="number"?_18:_18.y,hv=ht(v),_19=hv-_14,w=Math.abs(_19),_1a=typeof _18!="number"?t.addMixin(_16,"bar",_18,true):t.post(_16,"bar");
if(w>=0&&_12>=1){
var _1b={x:_10.l+(v<_13?hv:_14),y:_f.height-_10.b-vt(j+1.5)+gap,width:w,height:_12};
var _1c=this._plotFill(_1a.series.fill,_f,_10);
_1c=this._shapeFill(_1c,_1b);
var _1d=this.createRect(run,s,_1b).setFill(_1c).setStroke(_1a.series.stroke);
run.dyn.fill=_1d.getFill();
run.dyn.stroke=_1d.getStroke();
if(_15){
var o={element:"bar",index:j,run:run,shape:_1d,x:v,y:j+1.5};
this._connectEvents(o);
_17[j]=o;
}
if(this.animate){
this._animateBar(_1d,_10.l+_14,-w);
}
}
}
}
this._eventSeries[run.name]=_17;
run.dirty=false;
}
this.dirty=false;
return this;
},_animateBar:function(_1e,_1f,_20){
fx.animateTransform(_2.delegate({shape:_1e,duration:1200,transform:[{name:"translate",start:[_1f-(_1f/_20),0],end:[0,0]},{name:"scale",start:[1/_20,1],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});

//>>built
define("dojox/charting/plot2d/Columns",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./Base","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils","dojox/gfx/fx"],function(_1,_2,_3,_4,dc,df,_5,du,fx){
var _6=_5.lambda("item.purgeGroup()");
return _3("dojox.charting.plot2d.Columns",_4,{defaultParams:{hAxis:"x",vAxis:"y",gap:0,animate:null,enableCache:false},optionalParams:{minBarSize:1,maxBarSize:1,stroke:{},outline:{},shadow:{},fill:{},font:"",fontColor:""},constructor:function(_7,_8){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_8);
du.updateWithPattern(this.opt,_8,this.optionalParams);
this.series=[];
this.hAxis=this.opt.hAxis;
this.vAxis=this.opt.vAxis;
this.animate=this.opt.animate;
},getSeriesStats:function(){
var _9=dc.collectSimpleStats(this.series);
_9.hmin-=0.5;
_9.hmax+=0.5;
return _9;
},createRect:function(_a,_b,_c){
var _d;
if(this.opt.enableCache&&_a._rectFreePool.length>0){
_d=_a._rectFreePool.pop();
_d.setShape(_c);
_b.add(_d);
}else{
_d=_b.createRect(_c);
}
if(this.opt.enableCache){
_a._rectUsePool.push(_d);
}
return _d;
},render:function(_e,_f){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_e,_f);
}
var t=this.getSeriesStats();
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_2.forEach(this.series,_6);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_10){
_10.cleanGroup(s);
});
}
var t=this.chart.theme,f,gap,_11,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_12=Math.max(0,this._vScaler.bounds.lower),_13=vt(_12),min=Math.max(0,Math.floor(this._hScaler.bounds.from-1)),max=Math.ceil(this._hScaler.bounds.to),_14=this.events();
f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt);
gap=f.gap;
_11=f.size;
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
var _15=t.next("column",[this.opt,run]),s=run.group,_16=new Array(run.data.length);
var l=Math.min(run.data.length,max);
for(var j=min;j<l;++j){
var _17=run.data[j];
if(_17!==null){
var v=typeof _17=="number"?_17:_17.y,vv=vt(v),_18=vv-_13,h=Math.abs(_18),_19=typeof _17!="number"?t.addMixin(_15,"column",_17,true):t.post(_15,"column");
if(_11>=1&&h>=0){
var _1a={x:_f.l+ht(j+0.5)+gap,y:_e.height-_f.b-(v>_12?vv:_13),width:_11,height:h};
var _1b=this._plotFill(_19.series.fill,_e,_f);
_1b=this._shapeFill(_1b,_1a);
var _1c=this.createRect(run,s,_1a).setFill(_1b).setStroke(_19.series.stroke);
run.dyn.fill=_1c.getFill();
run.dyn.stroke=_1c.getStroke();
if(_14){
var o={element:"column",index:j,run:run,shape:_1c,x:j+0.5,y:v};
this._connectEvents(o);
_16[j]=o;
}
if(this.animate){
this._animateColumn(_1c,_e.height-_f.b-_13,h);
}
}
}
}
this._eventSeries[run.name]=_16;
run.dirty=false;
}
this.dirty=false;
return this;
},_animateColumn:function(_1d,_1e,_1f){
fx.animateTransform(_1.delegate({shape:_1d,duration:1200,transform:[{name:"translate",start:[0,_1e-(_1e/_1f)],end:[0,0]},{name:"scale",start:[1,1/_1f],end:[1,1]},{name:"original"}]},this.animate)).play();
}});
});

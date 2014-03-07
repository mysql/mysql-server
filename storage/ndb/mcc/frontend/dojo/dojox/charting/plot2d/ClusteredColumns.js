//>>built
define("dojox/charting/plot2d/ClusteredColumns",["dojo/_base/array","dojo/_base/declare","./Columns","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils"],function(_1,_2,_3,dc,df,_4,du){
var _5=_4.lambda("item.purgeGroup()");
return _2("dojox.charting.plot2d.ClusteredColumns",_3,{render:function(_6,_7){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_6,_7);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_1.forEach(this.series,_5);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_8){
_8.cleanGroup(s);
});
}
var t=this.chart.theme,f,_9,_a,_b,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_c=Math.max(0,this._vScaler.bounds.lower),_d=vt(_c),_e=this.events();
f=dc.calculateBarSize(this._hScaler.bounds.scale,this.opt,this.series.length);
_9=f.gap;
_a=_b=f.size;
for(var i=0;i<this.series.length;++i){
var _f=this.series[i],_10=_b*i;
if(!this.dirty&&!_f.dirty){
t.skip();
this._reconnectEvents(_f.name);
continue;
}
_f.cleanGroup();
var _11=t.next("column",[this.opt,_f]),s=_f.group,_12=new Array(_f.data.length);
for(var j=0;j<_f.data.length;++j){
var _13=_f.data[j];
if(_13!==null){
var v=typeof _13=="number"?_13:_13.y,vv=vt(v),_14=vv-_d,h=Math.abs(_14),_15=typeof _13!="number"?t.addMixin(_11,"column",_13,true):t.post(_11,"column");
if(_a>=1&&h>=0){
var _16={x:_7.l+ht(j+0.5)+_9+_10,y:_6.height-_7.b-(v>_c?vv:_d),width:_a,height:h};
var _17=this._plotFill(_15.series.fill,_6,_7);
_17=this._shapeFill(_17,_16);
var _18=s.createRect(_16).setFill(_17).setStroke(_15.series.stroke);
_f.dyn.fill=_18.getFill();
_f.dyn.stroke=_18.getStroke();
if(_e){
var o={element:"column",index:j,run:_f,shape:_18,x:j+0.5,y:v};
this._connectEvents(o);
_12[j]=o;
}
if(this.animate){
this._animateColumn(_18,_6.height-_7.b-_d,h);
}
}
}
}
this._eventSeries[_f.name]=_12;
_f.dirty=false;
}
this.dirty=false;
return this;
}});
});

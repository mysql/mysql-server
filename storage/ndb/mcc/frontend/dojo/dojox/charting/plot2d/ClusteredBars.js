//>>built
define("dojox/charting/plot2d/ClusteredBars",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./Bars","./common","dojox/lang/functional","dojox/lang/functional/reversed","dojox/lang/utils"],function(_1,_2,_3,_4,dc,df,_5,du){
var _6=_5.lambda("item.purgeGroup()");
return _3("dojox.charting.plot2d.ClusteredBars",_4,{render:function(_7,_8){
if(this.zoom&&!this.isDataDirty()){
return this.performZoom(_7,_8);
}
this.resetEvents();
this.dirty=this.isDirty();
if(this.dirty){
_2.forEach(this.series,_6);
this._eventSeries={};
this.cleanGroup();
var s=this.group;
df.forEachRev(this.series,function(_9){
_9.cleanGroup(s);
});
}
var t=this.chart.theme,f,_a,_b,_c,ht=this._hScaler.scaler.getTransformerFromModel(this._hScaler),vt=this._vScaler.scaler.getTransformerFromModel(this._vScaler),_d=Math.max(0,this._hScaler.bounds.lower),_e=ht(_d),_f=this.events();
f=dc.calculateBarSize(this._vScaler.bounds.scale,this.opt,this.series.length);
_a=f.gap;
_b=_c=f.size;
for(var i=this.series.length-1;i>=0;--i){
var run=this.series[i],_10=_c*(this.series.length-i-1);
if(!this.dirty&&!run.dirty){
t.skip();
this._reconnectEvents(run.name);
continue;
}
run.cleanGroup();
var _11=t.next("bar",[this.opt,run]),s=run.group,_12=new Array(run.data.length);
for(var j=0;j<run.data.length;++j){
var _13=run.data[j];
if(_13!==null){
var v=typeof _13=="number"?_13:_13.y,hv=ht(v),_14=hv-_e,w=Math.abs(_14),_15=typeof _13!="number"?t.addMixin(_11,"bar",_13,true):t.post(_11,"bar");
if(w>=0&&_b>=1){
var _16={x:_8.l+(v<_d?hv:_e),y:_7.height-_8.b-vt(j+1.5)+_a+_10,width:w,height:_b};
var _17=this._plotFill(_15.series.fill,_7,_8);
_17=this._shapeFill(_17,_16);
var _18=s.createRect(_16).setFill(_17).setStroke(_15.series.stroke);
run.dyn.fill=_18.getFill();
run.dyn.stroke=_18.getStroke();
if(_f){
var o={element:"bar",index:j,run:run,shape:_18,x:v,y:j+1.5};
this._connectEvents(o);
_12[j]=o;
}
if(this.animate){
this._animateBar(_18,_8.l+_e,-_14);
}
}
}
}
this._eventSeries[run.name]=_12;
run.dirty=false;
}
this.dirty=false;
return this;
}});
});

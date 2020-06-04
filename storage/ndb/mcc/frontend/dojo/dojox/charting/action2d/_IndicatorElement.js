//>>built
define("dojox/charting/action2d/_IndicatorElement",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","../plot2d/Indicator","dojo/has","../plot2d/common","../axis2d/common","dojox/gfx"],function(_1,_2,_3,_4,_5){
var _6=function(v,_7,_8){
var c2,c1=v?{x:_7[0],y:_8[0][0]}:{x:_8[0][0],y:_7[0]};
if(_7.length>1){
c2=v?{x:_7[1],y:_8[1][0]}:{x:_8[1][0],y:_7[1]};
}
return [c1,c2];
};
var _9=_3("dojox.charting.action2d._IndicatorElement",_4,{constructor:function(_a,_b){
if(!_b){
_b={};
}
this.inter=_b.inter;
},_updateVisibility:function(cp,_c,_d){
var _e=_d=="x"?this._hAxis:this._vAxis;
var _f=_e.getWindowScale();
this.chart.setAxisWindow(_e.name,_f,_e.getWindowOffset()+(cp[_d]-_c[_d])/_f);
this._noDirty=true;
this.chart.render();
this._noDirty=false;
this._initTrack();
},_trackMove:function(){
this._updateIndicator(this.pageCoord);
this._tracker=setTimeout(_1.hitch(this,this._trackMove),100);
},_initTrack:function(){
if(!this._tracker){
this._tracker=setTimeout(_1.hitch(this,this._trackMove),500);
}
},stopTrack:function(){
if(this._tracker){
clearTimeout(this._tracker);
this._tracker=null;
}
},render:function(){
if(!this.isDirty()){
return;
}
var _10=this.inter,_11=_10.plot,v=_10.opt.vertical;
this.opt.offset=_10.opt.offset||(v?{x:0,y:5}:{x:5,y:0});
if(_10.opt.labelFunc){
this.opt.labelFunc=function(_12,_13,_14,_15,_16){
var _17=_6(v,_13,_14);
return _10.opt.labelFunc(_17[0],_17[1],_15,_16);
};
}
if(_10.opt.fillFunc){
this.opt.fillFunc=function(_18,_19,_1a){
var _1b=_6(v,_19,_1a);
return _10.opt.fillFunc(_1b[0],_1b[1]);
};
}
this.opt=_1.delegate(_10.opt,this.opt);
if(!this.pageCoord){
this.opt.values=null;
this.inter.onChange({});
}else{
this.opt.values=[];
this.opt.labels=this.secondCoord?"trend":"markers";
}
this.hAxis=_11.hAxis;
this.vAxis=_11.vAxis;
this.inherited(arguments);
},_updateIndicator:function(){
var _1c=this._updateCoordinates(this.pageCoord,this.secondCoord);
if(_1c.length>1){
var v=this.opt.vertical;
this._data=[];
this.opt.values=[];
_2.forEach(_1c,function(_1d){
if(_1d){
this.opt.values.push(v?_1d.x:_1d.y);
this._data.push([v?_1d.y:_1d.x]);
}
},this);
}else{
this.inter.onChange({});
return;
}
this.inherited(arguments);
},_renderText:function(g,_1e,t,x,y,_1f,_20,_21){
if(this.inter.opt.labels){
this.inherited(arguments);
}
var _22=_6(this.opt.vertical,_20,_21);
this.inter.onChange({start:_22[0],end:_22[1],label:_1e});
},_updateCoordinates:function(cp1,cp2){
if(_5("dojo-bidi")){
this._checkXCoords(cp1,cp2);
}
var _23=this.inter,_24=_23.plot,v=_23.opt.vertical;
var _25=this.chart.getAxis(_24.hAxis),_26=this.chart.getAxis(_24.vAxis);
var hn=_25.name,vn=_26.name,hb=_25.getScaler().bounds,vb=_26.getScaler().bounds;
var _27=v?"x":"y",n=v?hn:vn,_28=v?hb:vb;
if(cp2){
var tmp;
if(v){
if(cp1.x>cp2.x){
tmp=cp2;
cp2=cp1;
cp1=tmp;
}
}else{
if(cp1.y>cp2.y){
tmp=cp2;
cp2=cp1;
cp1=tmp;
}
}
}
var cd1=_24.toData(cp1),cd2;
if(cp2){
cd2=_24.toData(cp2);
}
var o={};
o[hn]=hb.from;
o[vn]=vb.from;
var min=_24.toPage(o);
o[hn]=hb.to;
o[vn]=vb.to;
var max=_24.toPage(o);
if(cd1[n]<_28.from){
if(!cd2&&_23.opt.autoScroll&&!_23.opt.mouseOver){
this._updateVisibility(cp1,min,_27);
return [];
}else{
if(_23.opt.mouseOver){
return [];
}
cp1[_27]=min[_27];
}
cd1=_24.toData(cp1);
}else{
if(cd1[n]>_28.to){
if(!cd2&&_23.opt.autoScroll&&!_23.opt.mouseOver){
this._updateVisibility(cp1,max,_27);
return [];
}else{
if(_23.opt.mouseOver){
return [];
}
cp1[_27]=max[_27];
}
cd1=_24.toData(cp1);
}
}
var c1=this._snapData(cd1,_27,v),c2;
if(c1.y==null){
return [];
}
if(cp2){
if(cd2[n]<_28.from){
cp2[_27]=min[_27];
cd2=_24.toData(cp2);
}else{
if(cd2[n]>_28.to){
cp2[_27]=max[_27];
cd2=_24.toData(cp2);
}
}
c2=this._snapData(cd2,_27,v);
if(c2.y==null){
c2=null;
}
}
return [c1,c2];
},_snapData:function(cd,_29,v){
var _2a=this.chart.getSeries(this.inter.opt.series).data;
var i,r,l=_2a.length;
for(i=0;i<l;++i){
r=_2a[i];
if(r==null){
}else{
if(typeof r=="number"){
if(i+1>cd[_29]){
break;
}
}else{
if(r[_29]>cd[_29]){
break;
}
}
}
}
var x,y,px,py;
if(typeof r=="number"){
x=i+1;
y=r;
if(i>0){
px=i;
py=_2a[i-1];
}
}else{
x=r.x;
y=r.y;
if(i>0){
px=_2a[i-1].x;
py=_2a[i-1].y;
}
}
if(i>0){
var m=v?(x+px)/2:(y+py)/2;
if(cd[_29]<=m){
x=px;
y=py;
}
}
return {x:x,y:y};
},cleanGroup:function(_2b){
this.inherited(arguments);
this.group.moveToFront();
return this;
},isDirty:function(){
return !this._noDirty&&(this.dirty||this.inter.plot.isDirty());
}});
if(_5("dojo-bidi")){
_9.extend({_checkXCoords:function(cp1,cp2){
if(this.chart.isRightToLeft()&&this.isDirty()){
var _2c=this.chart.node.offsetLeft;
function _2d(_2e,cp){
var x=cp.x-_2c;
var _2f=(_2e.chart.offsets.l-_2e.chart.offsets.r);
var _30=_2e.chart.dim.width+_2f-x;
return _30+_2c;
};
if(cp1){
cp1.x=_2d(this,cp1);
}
if(cp2){
cp2.x=_2d(this,cp2);
}
}
}});
}
return _9;
});

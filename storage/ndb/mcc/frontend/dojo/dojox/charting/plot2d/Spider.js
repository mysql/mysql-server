//>>built
define("dojox/charting/plot2d/Spider",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/array","dojo/dom-geometry","dojo/_base/fx","dojo/fx","dojo/sniff","./Base","./_PlotEvents","./common","../axis2d/common","dojox/gfx","dojox/gfx/matrix","dojox/gfx/fx","dojox/lang/functional","dojox/lang/utils","dojo/fx/easing"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,dc,da,g,m,_b,df,du,_c){
var _d=0.2;
var _e=_2("dojox.charting.plot2d.Spider",[_9,_a],{defaultParams:{labels:true,ticks:false,fixed:true,precision:1,labelOffset:-10,labelStyle:"default",htmlLabels:true,startAngle:-90,divisions:3,axisColor:"",axisWidth:0,spiderColor:"",spiderWidth:0,seriesWidth:0,seriesFillAlpha:0.2,spiderOrigin:0.16,markerSize:3,spiderType:"polygon",animationType:_c.backOut,animate:null,axisTickFont:"",axisTickFontColor:"",axisFont:"",axisFontColor:""},optionalParams:{radius:0,font:"",fontColor:""},constructor:function(_f,_10){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_10);
du.updateWithPattern(this.opt,_10,this.optionalParams);
this.dyn=[];
this.datas={};
this.labelKey=[];
this.oldSeriePoints={};
this.animate=this.opt.animate===null?{}:this.opt.animate;
this.animations={};
},clear:function(){
this.inherited(arguments);
this.dyn=[];
this.axes=[];
this.datas={};
this.labelKey=[];
this.oldSeriePoints={};
this.animations={};
return this;
},setAxis:function(_11){
if(_11){
if(_11.opt.min!=undefined){
this.datas[_11.name].min=_11.opt.min;
}
if(_11.opt.max!=undefined){
this.datas[_11.name].max=_11.opt.max;
}
}
return this;
},addSeries:function(run){
this.series.push(run);
var key;
for(key in run.data){
var val=run.data[key],_12=this.datas[key];
if(_12){
_12.vlist.push(val);
_12.min=Math.min(_12.min,val);
_12.max=Math.max(_12.max,val);
}else{
var _13="__"+key;
this.axes.push(_13);
this[_13]=key;
this.datas[key]={min:val,max:val,vlist:[val]};
}
}
if(this.labelKey.length<=0){
for(key in run.data){
this.labelKey.push(key);
}
}
return this;
},getSeriesStats:function(){
return dc.collectSimpleStats(this.series,function(v){
return v===null;
});
},render:function(dim,_14){
if(!this.dirty){
return this;
}
this.dirty=false;
this.cleanGroup();
var s=this.group,t=this.chart.theme;
this.resetEvents();
if(!this.series||!this.series.length){
return this;
}
var o=this.opt,ta=t.axis,rx=(dim.width-_14.l-_14.r)/2,ry=(dim.height-_14.t-_14.b)/2,r=Math.min(rx,ry),_15=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font)||"normal normal normal 7pt Tahoma",_16=o.axisFont||(ta.tick&&ta.tick.titleFont)||"normal normal normal 11pt Tahoma",_17=o.axisTickFontColor||(ta.majorTick&&ta.majorTick.fontColor)||(ta.tick&&ta.tick.fontColor)||"silver",_18=o.axisFontColor||(ta.tick&&ta.tick.titleFontColor)||"black",_19=o.axisColor||(ta.tick&&ta.tick.axisColor)||"silver",_1a=o.spiderColor||(ta.tick&&ta.tick.spiderColor)||"silver",_1b=o.axisWidth||(ta.stroke&&ta.stroke.width)||2,_1c=o.spiderWidth||(ta.stroke&&ta.stroke.width)||2,_1d=o.seriesWidth||(ta.stroke&&ta.stroke.width)||2,_1e=g.normalizedLength(g.splitFontString(_16).size),_1f=m._degToRad(o.startAngle),_20=_1f,_21,_22,_23,_24,_25,_26,_27,_28,ro=o.spiderOrigin,dv=o.divisions>=3?o.divisions:3,ms=o.markerSize,spt=o.spiderType,at=o.animationType,_29=o.labelOffset<-10?o.labelOffset:-10,_2a=0.2,i,j,_2b,len,_2c,_2d,_2e,run,_2f,min,max,_30;
if(o.labels){
_21=_4.map(this.series,function(s){
return s.name;
},this);
_22=df.foldl1(df.map(_21,function(_31){
var _32=t.series.font;
return g._base._getTextBox(_31,{font:_32}).w;
},this),"Math.max(a, b)")/2;
r=Math.min(rx-2*_22,ry-_1e)+_29;
_23=r-_29;
}
if("radius" in o){
r=o.radius;
_23=r-_29;
}
r/=(1+_2a);
var _33={cx:_14.l+rx,cy:_14.t+ry,r:r};
for(var i=0;i<this.series.length;i++){
_2e=this.series[i];
if(!this.dirty&&!_2e.dirty){
t.skip();
continue;
}
_2e.cleanGroup();
run=_2e.data;
if(run!==null){
len=this._getObjectLength(run);
if(!_24||_24.length<=0){
_24=[],_25=[],_28=[];
this._buildPoints(_24,len,_33,r,_20,true,dim);
this._buildPoints(_25,len,_33,r*ro,_20,true,dim);
this._buildPoints(_28,len,_33,_23,_20,false,dim);
if(dv>2){
_26=[],_27=[];
for(j=0;j<dv-2;j++){
_26[j]=[];
this._buildPoints(_26[j],len,_33,r*(ro+(1-ro)*(j+1)/(dv-1)),_20,true,dim);
_27[j]=r*(ro+(1-ro)*(j+1)/(dv-1));
}
}
}
}
}
var _34=s.createGroup(),_35={color:_19,width:_1b},_36={color:_1a,width:_1c};
for(j=_24.length-1;j>=0;--j){
_2b=_24[j];
var st={x:_2b.x+(_2b.x-_33.cx)*_2a,y:_2b.y+(_2b.y-_33.cy)*_2a},nd={x:_2b.x+(_2b.x-_33.cx)*_2a/2,y:_2b.y+(_2b.y-_33.cy)*_2a/2};
_34.createLine({x1:_33.cx,y1:_33.cy,x2:st.x,y2:st.y}).setStroke(_35);
this._drawArrow(_34,st,nd,_35);
}
var _37=s.createGroup();
for(j=_28.length-1;j>=0;--j){
_2b=_28[j];
_2c=g._base._getTextBox(this.labelKey[j],{font:_16}).w||0;
_2d=this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx";
var _38=da.createText[_2d](this.chart,_37,(!_5.isBodyLtr()&&_2d=="html")?(_2b.x+_2c-dim.width):_2b.x,_2b.y,"middle",this.labelKey[j],_16,_18);
if(this.opt.htmlLabels){
this.htmlElements.push(_38);
}
}
var _39=s.createGroup();
if(spt=="polygon"){
_39.createPolyline(_24).setStroke(_36);
_39.createPolyline(_25).setStroke(_36);
if(_26.length>0){
for(j=_26.length-1;j>=0;--j){
_39.createPolyline(_26[j]).setStroke(_36);
}
}
}else{
_39.createCircle({cx:_33.cx,cy:_33.cy,r:r}).setStroke(_36);
_39.createCircle({cx:_33.cx,cy:_33.cy,r:r*ro}).setStroke(_36);
if(_27.length>0){
for(j=_27.length-1;j>=0;--j){
_39.createCircle({cx:_33.cx,cy:_33.cy,r:_27[j]}).setStroke(_36);
}
}
}
len=this._getObjectLength(this.datas);
var _3a=s.createGroup(),k=0;
for(var key in this.datas){
_2f=this.datas[key];
min=_2f.min;
max=_2f.max;
_30=max-min;
end=_20+2*Math.PI*k/len;
for(i=0;i<dv;i++){
var _3b=min+_30*i/(dv-1);
_2b=this._getCoordinate(_33,r*(ro+(1-ro)*i/(dv-1)),end,dim);
_3b=this._getLabel(_3b);
_2c=g._base._getTextBox(_3b,{font:_15}).w||0;
_2d=this.opt.htmlLabels&&g.renderer!="vml"?"html":"gfx";
if(this.opt.htmlLabels){
this.htmlElements.push(da.createText[_2d](this.chart,_3a,(!_5.isBodyLtr()&&_2d=="html")?(_2b.x+_2c-dim.width):_2b.x,_2b.y,"start",_3b,_15,_17));
}
}
k++;
}
this.chart.seriesShapes={};
for(i=this.series.length-1;i>=0;i--){
_2e=this.series[i];
run=_2e.data;
if(run!==null){
var _3c=t.next("spider",[o,_2e]),f=g.normalizeColor(_3c.series.fill),sk={color:_3c.series.fill,width:_1d};
f.a=o.seriesFillAlpha;
_2e.dyn={fill:f,stroke:sk};
if(_2e.hidden){
continue;
}
var _3d=[],_3e=[];
k=0;
for(key in run){
_2f=this.datas[key];
min=_2f.min;
max=_2f.max;
_30=max-min;
var _3f=run[key],end=_20+2*Math.PI*k/len;
_2b=this._getCoordinate(_33,r*(ro+(1-ro)*(_3f-min)/_30),end,dim);
_3d.push(_2b);
_3e.push({sname:_2e.name,key:key,data:_3f});
k++;
}
_3d[_3d.length]=_3d[0];
_3e[_3e.length]=_3e[0];
var _40=this._getBoundary(_3d),ts=_2e.group;
var _41=this.oldSeriePoints[_2e.name];
var cs=this._createSeriesEntry(ts,(_41||_25),_3d,f,sk,r,ro,ms,at);
this.chart.seriesShapes[_2e.name]=cs;
this.oldSeriePoints[_2e.name]=_3d;
var po={element:"spider_poly",index:i,id:"spider_poly_"+_2e.name,run:_2e,plot:this,shape:cs.poly,parent:ts,brect:_40,cx:_33.cx,cy:_33.cy,cr:r,f:f,s:s};
this._connectEvents(po);
var so={element:"spider_plot",index:i,id:"spider_plot_"+_2e.name,run:_2e,plot:this,shape:_2e.group};
this._connectEvents(so);
_4.forEach(cs.circles,function(c,i){
var co={element:"spider_circle",index:i,id:"spider_circle_"+_2e.name+i,run:_2e,plot:this,shape:c,parent:ts,tdata:_3e[i],cx:_3d[i].x,cy:_3d[i].y,f:f,s:s};
this._connectEvents(co);
},this);
}
}
return this;
},_createSeriesEntry:function(ts,_42,sps,f,sk,r,ro,ms,at){
var _43=this.animate?_42:sps;
var _44=ts.createPolyline(_43).setFill(f).setStroke(sk),_45=[];
for(var j=0;j<_43.length;j++){
var _46=_43[j],cr=ms;
var _47=ts.createCircle({cx:_46.x,cy:_46.y,r:cr}).setFill(f).setStroke(sk);
_45.push(_47);
}
if(this.animate){
var _48=_4.map(sps,function(np,j){
var sp=_42[j],_49=new _6.Animation(_1.delegate({duration:1000,easing:at,curve:[sp.y,np.y]},this.animate));
var spl=_44,sc=_45[j];
_3.connect(_49,"onAnimate",function(y){
var _4a=spl.getShape();
_4a.points[j].y=y;
spl.setShape(_4a);
var _4b=sc.getShape();
_4b.cy=y;
sc.setShape(_4b);
});
return _49;
},this);
var _4c=_4.map(sps,function(np,j){
var sp=_42[j],_4d=new _6.Animation(_1.delegate({duration:1000,easing:at,curve:[sp.x,np.x]},this.animate));
var spl=_44,sc=_45[j];
_3.connect(_4d,"onAnimate",function(x){
var _4e=spl.getShape();
_4e.points[j].x=x;
spl.setShape(_4e);
var _4f=sc.getShape();
_4f.cx=x;
sc.setShape(_4f);
});
return _4d;
},this);
var _50=_7.combine(_48.concat(_4c));
_50.play();
}
return {group:ts,poly:_44,circles:_45};
},plotEvent:function(o){
if(o.element=="spider_plot"){
if(o.type=="onmouseover"&&!_8("ie")){
o.shape.moveToFront();
}
}
},tooltipFunc:function(o){
if(o.element=="spider_circle"){
return o.tdata.sname+"<br/>"+o.tdata.key+"<br/>"+o.tdata.data;
}else{
return null;
}
},_getBoundary:function(_51){
var _52=_51[0].x,_53=_51[0].x,_54=_51[0].y,_55=_51[0].y;
for(var i=0;i<_51.length;i++){
var _56=_51[i];
_52=Math.max(_56.x,_52);
_54=Math.max(_56.y,_54);
_53=Math.min(_56.x,_53);
_55=Math.min(_56.y,_55);
}
return {x:_53,y:_55,width:_52-_53,height:_54-_55};
},_drawArrow:function(s,_57,end,_58){
var len=Math.sqrt(Math.pow(end.x-_57.x,2)+Math.pow(end.y-_57.y,2)),sin=(end.y-_57.y)/len,cos=(end.x-_57.x)/len,_59={x:end.x+(len/3)*(-sin),y:end.y+(len/3)*cos},_5a={x:end.x+(len/3)*sin,y:end.y+(len/3)*(-cos)};
s.createPolyline([_57,_59,_5a]).setFill(_58.color).setStroke(_58);
},_buildPoints:function(_5b,_5c,_5d,_5e,_5f,_60,dim){
for(var i=0;i<_5c;i++){
var end=_5f+2*Math.PI*i/_5c;
_5b.push(this._getCoordinate(_5d,_5e,end,dim));
}
if(_60){
_5b.push(this._getCoordinate(_5d,_5e,_5f+2*Math.PI,dim));
}
},_getCoordinate:function(_61,_62,_63,dim){
var x=_61.cx+_62*Math.cos(_63);
if(_8("dojo-bidi")&&this.chart.isRightToLeft()&&dim){
x=dim.width-x;
}
return {x:x,y:_61.cy+_62*Math.sin(_63)};
},_getObjectLength:function(obj){
var _64=0;
if(_1.isObject(obj)){
for(var key in obj){
_64++;
}
}
return _64;
},_getLabel:function(_65){
return dc.getLabel(_65,this.opt.fixed,this.opt.precision);
}});
return _e;
});

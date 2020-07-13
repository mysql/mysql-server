//>>built
define("dojox/charting/DataChart",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/_base/html","dojo/_base/connect","dojo/_base/array","./Chart2D","./themes/PlotKit/blue","dojo/dom"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_1.experimental("dojox.charting.DataChart");
var _a={vertical:true,min:0,max:10,majorTickStep:5,minorTickStep:1,natural:false,stroke:"black",majorTick:{stroke:"black",length:8},minorTick:{stroke:"gray",length:2},majorLabels:true};
var _b={natural:true,majorLabels:true,includeZero:false,majorTickStep:1,majorTick:{stroke:"black",length:8},fixUpper:"major",stroke:"black",htmlLabels:true,from:1};
var _c={markers:true,tension:2,gap:2};
return _3("dojox.charting.DataChart",_7,{scroll:true,comparative:false,query:"*",queryOptions:"",fieldName:"value",chartTheme:_8,displayRange:0,stretchToFit:true,minWidth:200,minHeight:100,showing:true,label:"name",constructor:function(_d,_e){
this.domNode=_9.byId(_d);
_2.mixin(this,_e);
this.xaxis=_2.mixin(_2.mixin({},_b),_e.xaxis);
if(this.xaxis.labelFunc=="seriesLabels"){
this.xaxis.labelFunc=_2.hitch(this,"seriesLabels");
}
this.yaxis=_2.mixin(_2.mixin({},_a),_e.yaxis);
if(this.yaxis.labelFunc=="seriesLabels"){
this.yaxis.labelFunc=_2.hitch(this,"seriesLabels");
}
this._events=[];
this.convertLabels(this.yaxis);
this.convertLabels(this.xaxis);
this.onSetItems={};
this.onSetInterval=0;
this.dataLength=0;
this.seriesData={};
this.seriesDataBk={};
this.firstRun=true;
this.dataOffset=0;
this.chartTheme.plotarea.stroke={color:"gray",width:3};
this.setTheme(this.chartTheme);
if(this.displayRange){
this.stretchToFit=false;
}
if(!this.stretchToFit){
this.xaxis.to=this.displayRange;
}
var _f=_e.type&&_e.type!="Pie"&&_e.type.prototype.declaredClass!="dojox.charting.plot2d.Pie";
if(_f){
this.addAxis("x",this.xaxis);
this.addAxis("y",this.yaxis);
}
_c.type=_e.type||"Markers";
this.addPlot("default",_2.mixin(_c,_e.chartPlot));
if(_f){
this.addPlot("grid",_2.mixin(_e.grid||{},{type:"Grid",hMinorLines:true}));
}
if(this.showing){
this.render();
}
if(_e.store){
this.setStore(_e.store,_e.query,_e.fieldName,_e.queryOptions);
}
},destroy:function(){
_6.forEach(this._events,_5.disconnect);
this.inherited(arguments);
},setStore:function(_10,_11,_12,_13){
this.firstRun=true;
this.store=_10||this.store;
this.query=_11||this.query;
this.fieldName=_12||this.fieldName;
this.label=this.store.getLabelAttributes();
this.queryOptions=_13||_13;
_6.forEach(this._events,_5.disconnect);
this._events=[_5.connect(this.store,"onSet",this,"onSet"),_5.connect(this.store,"onError",this,"onError")];
this.fetch();
},show:function(){
if(!this.showing){
_4.style(this.domNode,"display","");
this.showing=true;
this.render();
}
},hide:function(){
if(this.showing){
_4.style(this.domNode,"display","none");
this.showing=false;
}
},onSet:function(_14){
var nm=this.getProperty(_14,this.label);
if(nm in this.runs||this.comparative){
clearTimeout(this.onSetInterval);
if(!this.onSetItems[nm]){
this.onSetItems[nm]=_14;
}
this.onSetInterval=setTimeout(_2.hitch(this,function(){
clearTimeout(this.onSetInterval);
var _15=[];
for(var nm in this.onSetItems){
_15.push(this.onSetItems[nm]);
}
this.onData(_15);
this.onSetItems={};
}),200);
}
},onError:function(err){
console.error("DataChart Error:",err);
},onDataReceived:function(_16){
},getProperty:function(_17,_18){
if(_18==this.label){
return this.store.getLabel(_17);
}
if(_18=="id"){
return this.store.getIdentity(_17);
}
var _19=this.store.getValues(_17,_18);
if(_19.length<2){
_19=this.store.getValue(_17,_18);
}
return _19;
},onData:function(_1a){
if(!_1a||!_1a.length){
return;
}
if(this.items&&this.items.length!=_1a.length){
_6.forEach(_1a,function(m){
var id=this.getProperty(m,"id");
_6.forEach(this.items,function(m2,i){
if(this.getProperty(m2,"id")==id){
this.items[i]=m2;
}
},this);
},this);
_1a=this.items;
}
if(this.stretchToFit){
this.displayRange=_1a.length;
}
this.onDataReceived(_1a);
this.items=_1a;
if(this.comparative){
var nm="default";
this.seriesData[nm]=[];
this.seriesDataBk[nm]=[];
_6.forEach(_1a,function(m){
var _1b=this.getProperty(m,this.fieldName);
this.seriesData[nm].push(_1b);
},this);
}else{
_6.forEach(_1a,function(m,i){
var nm=this.store.getLabel(m);
if(!this.seriesData[nm]){
this.seriesData[nm]=[];
this.seriesDataBk[nm]=[];
}
var _1c=this.getProperty(m,this.fieldName);
if(_2.isArray(_1c)){
this.seriesData[nm]=_1c;
}else{
if(!this.scroll){
var ar=_6.map(new Array(i+1),function(){
return 0;
});
ar.push(Number(_1c));
this.seriesData[nm]=ar;
}else{
if(this.seriesDataBk[nm].length>this.seriesData[nm].length){
this.seriesData[nm]=this.seriesDataBk[nm];
}
this.seriesData[nm].push(Number(_1c));
}
this.seriesDataBk[nm].push(Number(_1c));
}
},this);
}
var _1d;
if(this.firstRun){
this.firstRun=false;
for(nm in this.seriesData){
this.addSeries(nm,this.seriesData[nm]);
_1d=this.seriesData[nm];
}
}else{
for(nm in this.seriesData){
_1d=this.seriesData[nm];
if(this.scroll&&_1d.length>this.displayRange){
this.dataOffset=_1d.length-this.displayRange-1;
_1d=_1d.slice(_1d.length-this.displayRange,_1d.length);
}
this.updateSeries(nm,_1d);
}
}
this.dataLength=_1d.length;
if(this.showing){
this.render();
}
},fetch:function(){
if(!this.store){
return;
}
this.store.fetch({query:this.query,queryOptions:this.queryOptions,start:this.start,count:this.count,sort:this.sort,onComplete:_2.hitch(this,function(_1e){
setTimeout(_2.hitch(this,function(){
this.onData(_1e);
}),0);
}),onError:_2.hitch(this,"onError")});
},convertLabels:function(_1f){
if(!_1f.labels||_2.isObject(_1f.labels[0])){
return null;
}
_1f.labels=_6.map(_1f.labels,function(ele,i){
return {value:i,text:ele};
});
return null;
},seriesLabels:function(val){
val--;
if(this.series.length<1||(!this.comparative&&val>this.series.length)){
return "-";
}
if(this.comparative){
return this.store.getLabel(this.items[val]);
}else{
for(var i=0;i<this.series.length;i++){
if(this.series[i].data[val]>0){
return this.series[i].name;
}
}
}
return "-";
},resizeChart:function(dim){
var w=Math.max(dim.w,this.minWidth);
var h=Math.max(dim.h,this.minHeight);
this.resize(w,h);
}});
});

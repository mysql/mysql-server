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
this.addAxis("x",this.xaxis);
this.addAxis("y",this.yaxis);
_c.type=_e.type||"Markers";
this.addPlot("default",_2.mixin(_c,_e.chartPlot));
this.addPlot("grid",_2.mixin(_e.grid||{},{type:"Grid",hMinorLines:true}));
if(this.showing){
this.render();
}
if(_e.store){
this.setStore(_e.store,_e.query,_e.fieldName,_e.queryOptions);
}
},destroy:function(){
_6.forEach(this._events,_5.disconnect);
this.inherited(arguments);
},setStore:function(_f,_10,_11,_12){
this.firstRun=true;
this.store=_f||this.store;
this.query=_10||this.query;
this.fieldName=_11||this.fieldName;
this.label=this.store.getLabelAttributes();
this.queryOptions=_12||_12;
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
},onSet:function(_13){
var nm=this.getProperty(_13,this.label);
if(nm in this.runs||this.comparative){
clearTimeout(this.onSetInterval);
if(!this.onSetItems[nm]){
this.onSetItems[nm]=_13;
}
this.onSetInterval=setTimeout(_2.hitch(this,function(){
clearTimeout(this.onSetInterval);
var _14=[];
for(var nm in this.onSetItems){
_14.push(this.onSetItems[nm]);
}
this.onData(_14);
this.onSetItems={};
}),200);
}
},onError:function(err){
console.error("DataChart Error:",err);
},onDataReceived:function(_15){
},getProperty:function(_16,_17){
if(_17==this.label){
return this.store.getLabel(_16);
}
if(_17=="id"){
return this.store.getIdentity(_16);
}
var _18=this.store.getValues(_16,_17);
if(_18.length<2){
_18=this.store.getValue(_16,_17);
}
return _18;
},onData:function(_19){
if(!_19||!_19.length){
return;
}
if(this.items&&this.items.length!=_19.length){
_6.forEach(_19,function(m){
var id=this.getProperty(m,"id");
_6.forEach(this.items,function(m2,i){
if(this.getProperty(m2,"id")==id){
this.items[i]=m2;
}
},this);
},this);
_19=this.items;
}
if(this.stretchToFit){
this.displayRange=_19.length;
}
this.onDataReceived(_19);
this.items=_19;
if(this.comparative){
var nm="default";
this.seriesData[nm]=[];
this.seriesDataBk[nm]=[];
_6.forEach(_19,function(m,i){
var _1a=this.getProperty(m,this.fieldName);
this.seriesData[nm].push(_1a);
},this);
}else{
_6.forEach(_19,function(m,i){
var nm=this.store.getLabel(m);
if(!this.seriesData[nm]){
this.seriesData[nm]=[];
this.seriesDataBk[nm]=[];
}
var _1b=this.getProperty(m,this.fieldName);
if(_2.isArray(_1b)){
this.seriesData[nm]=_1b;
}else{
if(!this.scroll){
var ar=_6.map(new Array(i+1),function(){
return 0;
});
ar.push(Number(_1b));
this.seriesData[nm]=ar;
}else{
if(this.seriesDataBk[nm].length>this.seriesData[nm].length){
this.seriesData[nm]=this.seriesDataBk[nm];
}
this.seriesData[nm].push(Number(_1b));
}
this.seriesDataBk[nm].push(Number(_1b));
}
},this);
}
var _1c;
if(this.firstRun){
this.firstRun=false;
for(nm in this.seriesData){
this.addSeries(nm,this.seriesData[nm]);
_1c=this.seriesData[nm];
}
}else{
for(nm in this.seriesData){
_1c=this.seriesData[nm];
if(this.scroll&&_1c.length>this.displayRange){
this.dataOffset=_1c.length-this.displayRange-1;
_1c=_1c.slice(_1c.length-this.displayRange,_1c.length);
}
this.updateSeries(nm,_1c);
}
}
this.dataLength=_1c.length;
if(this.showing){
this.render();
}
},fetch:function(){
if(!this.store){
return;
}
this.store.fetch({query:this.query,queryOptions:this.queryOptions,start:this.start,count:this.count,sort:this.sort,onComplete:_2.hitch(this,function(_1d){
setTimeout(_2.hitch(this,function(){
this.onData(_1d);
}),0);
}),onError:_2.hitch(this,"onError")});
},convertLabels:function(_1e){
if(!_1e.labels||_2.isObject(_1e.labels[0])){
return null;
}
_1e.labels=_6.map(_1e.labels,function(ele,i){
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

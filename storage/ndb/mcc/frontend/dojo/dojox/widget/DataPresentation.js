//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/grid/DataGrid,dojox/charting/Chart2D,dojox/charting/widget/Legend,dojox/charting/action2d/Tooltip,dojox/charting/action2d/Highlight,dojo/colors,dojo/data/ItemFileWriteStore"],function(_1,_2,_3){
_2.provide("dojox.widget.DataPresentation");
_2.experimental("dojox.widget.DataPresentation");
_2.require("dojox.grid.DataGrid");
_2.require("dojox.charting.Chart2D");
_2.require("dojox.charting.widget.Legend");
_2.require("dojox.charting.action2d.Tooltip");
_2.require("dojox.charting.action2d.Highlight");
_2.require("dojo.colors");
_2.require("dojo.data.ItemFileWriteStore");
(function(){
var _4=function(_5,_6,_7,_8){
var _9=[];
_9[0]={value:0,text:""};
var _a=_5.length;
if((_7!=="ClusteredBars")&&(_7!=="StackedBars")){
var _b=_8.offsetWidth;
var _c=(""+_5[0]).length*_5.length*7;
if(_6==1){
for(var z=1;z<500;++z){
if((_c/z)<_b){
break;
}
++_6;
}
}
}
for(var i=0;i<_a;i++){
_9.push({value:i+1,text:(!_6||i%_6)?"":_5[i]});
}
_9.push({value:_a+1,text:""});
return _9;
};
var _d=function(_e,_f){
var _10={vertical:false,labels:_f,min:0,max:_f.length-1,majorTickStep:1,minorTickStep:1};
if((_e==="ClusteredBars")||(_e==="StackedBars")){
_10.vertical=true;
}
if((_e==="Lines")||(_e==="Areas")||(_e==="StackedAreas")){
_10.min++;
_10.max--;
}
return _10;
};
var _11=function(_12,_13,_14,_15){
var _16={vertical:true,fixLower:"major",fixUpper:"major",natural:true};
if(_13==="secondary"){
_16.leftBottom=false;
}
if((_12==="ClusteredBars")||(_12==="StackedBars")){
_16.vertical=false;
}
if(_14==_15){
_16.min=_14-1;
_16.max=_15+1;
}
return _16;
};
var _17=function(_18,_19,_1a){
var _1b={type:_18,hAxis:"independent",vAxis:"dependent-"+_19,gap:4,lines:false,areas:false,markers:false};
if((_18==="ClusteredBars")||(_18==="StackedBars")){
_1b.hAxis=_1b.vAxis;
_1b.vAxis="independent";
}
if((_18==="Lines")||(_18==="Hybrid-Lines")||(_18==="Areas")||(_18==="StackedAreas")){
_1b.lines=true;
}
if((_18==="Areas")||(_18==="StackedAreas")){
_1b.areas=true;
}
if(_18==="Lines"){
_1b.markers=true;
}
if(_18==="Hybrid-Lines"){
_1b.shadows={dx:2,dy:2,dw:2};
_1b.type="Lines";
}
if(_18==="Hybrid-ClusteredColumns"){
_1b.type="ClusteredColumns";
}
if(_1a){
_1b.animate=_1a;
}
return _1b;
};
var _1c=function(_1d,_1e,_1f,_20,_21,_22,_23,_24,_25,_26,_27){
var _28=_1e;
if(!_28){
_1d.innerHTML="";
_28=new _3.charting.Chart2D(_1d);
}
if(_23){
_23._clone=function(){
var _29=new _3.charting.Theme({chart:this.chart,plotarea:this.plotarea,axis:this.axis,series:this.series,marker:this.marker,antiAlias:this.antiAlias,assignColors:this.assignColors,assignMarkers:this.assigneMarkers,colors:_2.delegate(this.colors)});
_29.markers=this.markers;
_29._buildMarkerArray();
return _29;
};
_28.setTheme(_23);
}
var _2a=_25.series_data[0].slice(0);
if(_20){
_2a.reverse();
}
var _2b=_4(_2a,_22,_1f,_1d);
var _2c={};
var _2d=null;
var _2e=null;
var _2f={};
for(var _30 in _28.runs){
_2f[_30]=true;
}
var _31=_25.series_name.length;
for(var i=0;i<_31;i++){
if(_25.series_chart[i]&&(_25.series_data[i].length>0)){
var _32=_1f;
var _33=_25.series_axis[i];
if(_32=="Hybrid"){
if(_25.series_charttype[i]=="line"){
_32="Hybrid-Lines";
}else{
_32="Hybrid-ClusteredColumns";
}
}
if(!_2c[_33]){
_2c[_33]={};
}
if(!_2c[_33][_32]){
var _34=_33+"-"+_32;
_28.addPlot(_34,_17(_32,_33,_21));
var _35={};
if(typeof _24=="string"){
_35.text=function(o){
var _36=[o.element,o.run.name,_2a[o.index],((_32==="ClusteredBars")||(_32==="StackedBars"))?o.x:o.y];
return _2.replace(_24,_36);
};
}else{
if(typeof _24=="function"){
_35.text=_24;
}
}
new _3.charting.action2d.Tooltip(_28,_34,_35);
if(_32!=="Lines"&&_32!=="Hybrid-Lines"){
new _3.charting.action2d.Highlight(_28,_34);
}
_2c[_33][_32]=true;
}
var _37=[];
var _38=_25.series_data[i].length;
for(var j=0;j<_38;j++){
var val=_25.series_data[i][j];
_37.push(val);
if(_2d===null||val>_2d){
_2d=val;
}
if(_2e===null||val<_2e){
_2e=val;
}
}
if(_20){
_37.reverse();
}
var _39={plot:_33+"-"+_32};
if(_25.series_linestyle[i]){
_39.stroke={style:_25.series_linestyle[i]};
}
_28.addSeries(_25.series_name[i],_37,_39);
delete _2f[_25.series_name[i]];
}
}
for(_30 in _2f){
_28.removeSeries(_30);
}
_28.addAxis("independent",_d(_1f,_2b));
_28.addAxis("dependent-primary",_11(_1f,"primary",_2e,_2d));
_28.addAxis("dependent-secondary",_11(_1f,"secondary",_2e,_2d));
return _28;
};
var _3a=function(_3b,_3c,_3d,_3e){
var _3f=_3c;
if(!_3f){
_3f=new _3.charting.widget.Legend({chart:_3d,horizontal:_3e},_3b);
}else{
_3f.refresh();
}
return _3f;
};
var _40=function(_41,_42,_43,_44,_45){
var _46=_42||new _3.grid.DataGrid({},_41);
_46.startup();
_46.setStore(_43,_44,_45);
var _47=[];
for(var ser=0;ser<_43.series_name.length;ser++){
if(_43.series_grid[ser]&&(_43.series_data[ser].length>0)){
_47.push({field:"data."+ser,name:_43.series_name[ser],width:"auto",formatter:_43.series_gridformatter[ser]});
}
}
_46.setStructure(_47);
return _46;
};
var _48=function(_49,_4a){
if(_4a.title){
_49.innerHTML=_4a.title;
}
};
var _4b=function(_4c,_4d){
if(_4d.footer){
_4c.innerHTML=_4d.footer;
}
};
var _4e=function(_4f,_50){
var _51=_4f;
if(_50){
var _52=_50.split(/[.\[\]]+/);
for(var _53=0,l=_52.length;_53<l;_53++){
if(_51){
_51=_51[_52[_53]];
}
}
}
return _51;
};
_2.declare("dojox.widget.DataPresentation",null,{type:"chart",chartType:"clusteredBars",reverse:false,animate:null,labelMod:1,legendHorizontal:true,constructor:function(_54,_55){
_2.mixin(this,_55);
this.domNode=_2.byId(_54);
this[this.type+"Node"]=this.domNode;
if(typeof this.theme=="string"){
this.theme=_2.getObject(this.theme);
}
this.chartNode=_2.byId(this.chartNode);
this.legendNode=_2.byId(this.legendNode);
this.gridNode=_2.byId(this.gridNode);
this.titleNode=_2.byId(this.titleNode);
this.footerNode=_2.byId(this.footerNode);
if(this.legendVertical){
this.legendHorizontal=!this.legendVertical;
}
if(this.url){
this.setURL(null,null,this.refreshInterval);
}else{
if(this.data){
this.setData(null,this.refreshInterval);
}else{
this.setStore();
}
}
},setURL:function(url,_56,_57){
if(_57){
this.cancelRefresh();
}
this.url=url||this.url;
this.urlContent=_56||this.urlContent;
this.refreshInterval=_57||this.refreshInterval;
var me=this;
_2.xhrGet({url:this.url,content:this.urlContent,handleAs:"json-comment-optional",load:function(_58,_59){
me.setData(_58);
},error:function(xhr,_5a){
if(me.urlError&&(typeof me.urlError=="function")){
me.urlError(xhr,_5a);
}
}});
if(_57&&(this.refreshInterval>0)){
this.refreshIntervalPending=setInterval(function(){
me.setURL();
},this.refreshInterval);
}
},setData:function(_5b,_5c){
if(_5c){
this.cancelRefresh();
}
this.data=_5b||this.data;
this.refreshInterval=_5c||this.refreshInterval;
var _5d=(typeof this.series=="function")?this.series(this.data):this.series;
var _5e=[],_5f=[],_60=[],_61=[],_62=[],_63=[],_64=[],_65=[],_66=[],_67=0;
for(var ser=0;ser<_5d.length;ser++){
_5e[ser]=_4e(this.data,_5d[ser].datapoints);
if(_5e[ser]&&(_5e[ser].length>_67)){
_67=_5e[ser].length;
}
_5f[ser]=[];
_60[ser]=_5d[ser].name||(_5d[ser].namefield?_4e(this.data,_5d[ser].namefield):null)||("series "+ser);
_61[ser]=(_5d[ser].chart!==false);
_62[ser]=_5d[ser].charttype||"bar";
_63[ser]=_5d[ser].linestyle;
_64[ser]=_5d[ser].axis||"primary";
_65[ser]=(_5d[ser].grid!==false);
_66[ser]=_5d[ser].gridformatter;
}
var _68,_69,_6a,_6b;
var _6c=[];
for(_68=0;_68<_67;_68++){
_69={index:_68};
for(ser=0;ser<_5d.length;ser++){
if(_5e[ser]&&(_5e[ser].length>_68)){
_6a=_4e(_5e[ser][_68],_5d[ser].field);
if(_61[ser]){
_6b=parseFloat(_6a);
if(!isNaN(_6b)){
_6a=_6b;
}
}
_69["data."+ser]=_6a;
_5f[ser].push(_6a);
}
}
_6c.push(_69);
}
if(_67<=0){
_6c.push({index:0});
}
var _6d=new _2.data.ItemFileWriteStore({data:{identifier:"index",items:_6c}});
if(this.data.title){
_6d.title=this.data.title;
}
if(this.data.footer){
_6d.footer=this.data.footer;
}
_6d.series_data=_5f;
_6d.series_name=_60;
_6d.series_chart=_61;
_6d.series_charttype=_62;
_6d.series_linestyle=_63;
_6d.series_axis=_64;
_6d.series_grid=_65;
_6d.series_gridformatter=_66;
this.setPreparedStore(_6d);
if(_5c&&(this.refreshInterval>0)){
var me=this;
this.refreshIntervalPending=setInterval(function(){
me.setData();
},this.refreshInterval);
}
},refresh:function(){
if(this.url){
this.setURL(this.url,this.urlContent,this.refreshInterval);
}else{
if(this.data){
this.setData(this.data,this.refreshInterval);
}
}
},cancelRefresh:function(){
if(this.refreshIntervalPending){
clearInterval(this.refreshIntervalPending);
this.refreshIntervalPending=undefined;
}
},setStore:function(_6e,_6f,_70){
this.setPreparedStore(_6e,_6f,_70);
},setPreparedStore:function(_71,_72,_73){
this.preparedstore=_71||this.store;
this.query=_72||this.query;
this.queryOptions=_73||this.queryOptions;
if(this.preparedstore){
if(this.chartNode){
this.chartWidget=_1c(this.chartNode,this.chartWidget,this.chartType,this.reverse,this.animate,this.labelMod,this.theme,this.tooltip,this.preparedstore,this.query,this,_73);
this.renderChartWidget();
}
if(this.legendNode){
this.legendWidget=_3a(this.legendNode,this.legendWidget,this.chartWidget,this.legendHorizontal);
}
if(this.gridNode){
this.gridWidget=_40(this.gridNode,this.gridWidget,this.preparedstore,this.query,this.queryOptions);
this.renderGridWidget();
}
if(this.titleNode){
_48(this.titleNode,this.preparedstore);
}
if(this.footerNode){
_4b(this.footerNode,this.preparedstore);
}
}
},renderChartWidget:function(){
if(this.chartWidget){
this.chartWidget.render();
}
},renderGridWidget:function(){
if(this.gridWidget){
this.gridWidget.render();
}
},getChartWidget:function(){
return this.chartWidget;
},getGridWidget:function(){
return this.gridWidget;
},destroy:function(){
this.cancelRefresh();
if(this.chartWidget){
this.chartWidget.destroy();
delete this.chartWidget;
}
if(this.legendWidget){
delete this.legendWidget;
}
if(this.gridWidget){
delete this.gridWidget;
}
if(this.chartNode){
this.chartNode.innerHTML="";
}
if(this.legendNode){
this.legendNode.innerHTML="";
}
if(this.gridNode){
this.gridNode.innerHTML="";
}
if(this.titleNode){
this.titleNode.innerHTML="";
}
if(this.footerNode){
this.footerNode.innerHTML="";
}
}});
})();
});

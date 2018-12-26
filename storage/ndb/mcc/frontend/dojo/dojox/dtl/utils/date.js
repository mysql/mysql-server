//>>built
define("dojox/dtl/utils/date",["dojo/_base/lang","dojox/date/php","../_base"],function(_1,_2,dd){
_1.getObject("dojox.dtl.utils.date",true);
dd.utils.date.DateFormat=_2.DateFormat;
_1.extend(dd.utils.date.DateFormat,_2.DateFormat.prototype,{f:function(){
return (!this.date.getMinutes())?this.g():this.g()+":"+this.i();
},N:function(){
return dojox.dtl.utils.date._months_ap[this.date.getMonth()];
},P:function(){
if(!this.date.getMinutes()&&!this.date.getHours()){
return "midnight";
}
if(!this.date.getMinutes()&&this.date.getHours()==12){
return "noon";
}
return this.f()+" "+this.a();
}});
_1.mixin(dojox.dtl.utils.date,{format:function(_3,_4){
var df=new dojox.dtl.utils.date.DateFormat(_4);
return df.format(_3);
},timesince:function(d,_5){
if(!(d instanceof Date)){
d=new Date(d.year,d.month,d.day);
}
if(!_5){
_5=new Date();
}
var _6=Math.abs(_5.getTime()-d.getTime());
for(var i=0,_7;_7=dojox.dtl.utils.date._chunks[i];i++){
var _8=Math.floor(_6/_7[0]);
if(_8){
break;
}
}
return _8+" "+_7[1](_8);
},_chunks:[[60*60*24*365*1000,function(n){
return (n==1)?"year":"years";
}],[60*60*24*30*1000,function(n){
return (n==1)?"month":"months";
}],[60*60*24*7*1000,function(n){
return (n==1)?"week":"weeks";
}],[60*60*24*1000,function(n){
return (n==1)?"day":"days";
}],[60*60*1000,function(n){
return (n==1)?"hour":"hours";
}],[60*1000,function(n){
return (n==1)?"minute":"minutes";
}]],_months_ap:["Jan.","Feb.","March","April","May","June","July","Aug.","Sept.","Oct.","Nov.","Dec."]});
return dojox.dtl.utils.date;
});

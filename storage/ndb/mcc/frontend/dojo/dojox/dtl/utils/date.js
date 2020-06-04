//>>built
define("dojox/dtl/utils/date",["dojo/_base/lang","dojox/date/php","../_base"],function(_1,_2,dd){
var _3=_1.getObject("utils.date",true,dd);
_3.DateFormat=_2.DateFormat;
_1.extend(_3.DateFormat,_2.DateFormat.prototype,{f:function(){
return (!this.date.getMinutes())?this.g():this.g()+":"+this.i();
},N:function(){
return _3._months_ap[this.date.getMonth()];
},P:function(){
if(!this.date.getMinutes()&&!this.date.getHours()){
return "midnight";
}
if(!this.date.getMinutes()&&this.date.getHours()==12){
return "noon";
}
return this.f()+" "+this.a();
}});
_1.mixin(dojox.dtl.utils.date,{format:function(_4,_5){
var df=new dojox.dtl.utils.date.DateFormat(_5);
return df.format(_4);
},timesince:function(d,_6){
if(!(d instanceof Date)){
d=new Date(d.year,d.month,d.day);
}
if(!_6){
_6=new Date();
}
var _7=Math.abs(_6.getTime()-d.getTime());
for(var i=0,_8;_8=dojox.dtl.utils.date._chunks[i];i++){
var _9=Math.floor(_7/_8[0]);
if(_9){
break;
}
}
return _9+" "+_8[1](_9);
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
return _3;
});

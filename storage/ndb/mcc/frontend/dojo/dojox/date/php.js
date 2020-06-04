//>>built
define("dojox/date/php",["dojo/_base/kernel","dojo/_base/lang","dojo/date","dojox/string/tokenize"],function(_1,_2,_3,_4){
var _5=_1.getObject("date.php",true,dojox);
_5.format=function(_6,_7){
var df=new _5.DateFormat(_7);
return df.format(_6);
};
_5.DateFormat=function(_8){
if(!this.regex){
var _9=[];
for(var _a in this.constructor.prototype){
if(_1.isString(_a)&&_a.length==1&&_1.isFunction(this[_a])){
_9.push(_a);
}
}
this.constructor.prototype.regex=new RegExp("(?:(\\\\.)|(["+_9.join("")+"]))","g");
}
var _b=[];
this.tokens=_4(_8,this.regex,function(_c,_d,i){
if(_d){
_b.push([i,_d]);
return _d;
}
if(_c){
return _c.charAt(1);
}
});
this.replacements=_b;
};
_1.extend(_5.DateFormat,{weekdays:["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"],weekdays_3:["Sun","Mon","Tue","Wed","Thu","Fri","Sat"],months:["January","February","March","April","May","June","July","August","September","October","November","December"],months_3:["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"],monthdays:[31,28,31,30,31,30,31,31,30,31,30,31],format:function(_e){
this.date=_e;
for(var i=0,_f;_f=this.replacements[i];i++){
this.tokens[_f[0]]=this[_f[1]]();
}
return this.tokens.join("");
},d:function(){
var j=this.j();
return (j.length==1)?"0"+j:j;
},D:function(){
return this.weekdays_3[this.date.getDay()];
},j:function(){
return this.date.getDate()+"";
},l:function(){
return this.weekdays[this.date.getDay()];
},N:function(){
var w=this.w();
return (!w)?7:w;
},S:function(){
switch(this.date.getDate()){
case 11:
case 12:
case 13:
return "th";
case 1:
case 21:
case 31:
return "st";
case 2:
case 22:
return "nd";
case 3:
case 23:
return "rd";
default:
return "th";
}
},w:function(){
return this.date.getDay()+"";
},z:function(){
var _10=this.date.getTime()-new Date(this.date.getFullYear(),0,1).getTime();
return Math.floor(_10/86400000)+"";
},W:function(){
var _11;
var _12=new Date(this.date.getFullYear(),0,1).getDay()+1;
var w=this.date.getDay()+1;
var z=parseInt(this.z());
if(z<=(8-_12)&&_12>4){
var _13=new Date(this.date.getFullYear()-1,this.date.getMonth(),this.date.getDate());
if(_12==5||(_12==6&&_3.isLeapYear(_13))){
_11=53;
}else{
_11=52;
}
}else{
var i;
if(Boolean(this.L())){
i=366;
}else{
i=365;
}
if((i-z)<(4-w)){
_11=1;
}else{
var j=z+(7-w)+(_12-1);
_11=Math.ceil(j/7);
if(_12>4){
--_11;
}
}
}
return _11;
},F:function(){
return this.months[this.date.getMonth()];
},m:function(){
var n=this.n();
return (n.length==1)?"0"+n:n;
},M:function(){
return this.months_3[this.date.getMonth()];
},n:function(){
return this.date.getMonth()+1+"";
},t:function(){
return (Boolean(this.L())&&this.date.getMonth()==1)?29:this.monthdays[this.getMonth()];
},L:function(){
return (_3.isLeapYear(this.date))?"1":"0";
},o:function(){
},Y:function(){
return this.date.getFullYear()+"";
},y:function(){
return this.Y().slice(-2);
},a:function(){
return this.date.getHours()>=12?"pm":"am";
},b:function(){
return this.a().toUpperCase();
},B:function(){
var off=this.date.getTimezoneOffset()+60;
var _14=(this.date.getHours()*3600)+(this.date.getMinutes()*60)+this.getSeconds()+(off*60);
var _15=Math.abs(Math.floor(_14/86.4)%1000)+"";
while(_15.length<2){
_15="0"+_15;
}
return _15;
},g:function(){
return (this.date.getHours()%12||12)+"";
},G:function(){
return this.date.getHours()+"";
},h:function(){
var g=this.g();
return (g.length==1)?"0"+g:g;
},H:function(){
var G=this.G();
return (G.length==1)?"0"+G:G;
},i:function(){
var _16=this.date.getMinutes()+"";
return (_16.length==1)?"0"+_16:_16;
},s:function(){
var _17=this.date.getSeconds()+"";
return (_17.length==1)?"0"+_17:_17;
},e:function(){
return _3.getTimezoneName(this.date);
},I:function(){
},O:function(){
var off=Math.abs(this.date.getTimezoneOffset());
var _18=Math.floor(off/60)+"";
var _19=(off%60)+"";
if(_18.length==1){
_18="0"+_18;
}
if(_19.length==1){
_18="0"+_19;
}
return ((this.date.getTimezoneOffset()<0)?"+":"-")+_18+_19;
},P:function(){
var O=this.O();
return O.substring(0,2)+":"+O.substring(2,4);
},T:function(){
return this.e().substring(0,3);
},Z:function(){
return this.date.getTimezoneOffset()*-60;
},c:function(){
return this.Y()+"-"+this.m()+"-"+this.d()+"T"+this.h()+":"+this.i()+":"+this.s()+this.P();
},r:function(){
return this.D()+", "+this.d()+" "+this.M()+" "+this.Y()+" "+this.H()+":"+this.i()+":"+this.s()+" "+this.O();
},U:function(){
return Math.floor(this.date.getTime()/1000);
}});
return _5;
});

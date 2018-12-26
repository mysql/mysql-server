//>>built
define("dojox/date/php",["dojo/_base/kernel","dojo/_base/lang","dojo/date","dojox/string/tokenize"],function(_1,_2,_3,_4){
_1.getObject("date.php",true,dojox);
dojox.date.php.format=function(_5,_6){
var df=new dojox.date.php.DateFormat(_6);
return df.format(_5);
};
dojox.date.php.DateFormat=function(_7){
if(!this.regex){
var _8=[];
for(var _9 in this.constructor.prototype){
if(_1.isString(_9)&&_9.length==1&&_1.isFunction(this[_9])){
_8.push(_9);
}
}
this.constructor.prototype.regex=new RegExp("(?:(\\\\.)|(["+_8.join("")+"]))","g");
}
var _a=[];
this.tokens=_4(_7,this.regex,function(_b,_c,i){
if(_c){
_a.push([i,_c]);
return _c;
}
if(_b){
return _b.charAt(1);
}
});
this.replacements=_a;
};
_1.extend(dojox.date.php.DateFormat,{weekdays:["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"],weekdays_3:["Sun","Mon","Tue","Wed","Thu","Fri","Sat"],months:["January","February","March","April","May","June","July","August","September","October","November","December"],months_3:["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"],monthdays:[31,28,31,30,31,30,31,31,30,31,30,31],format:function(_d){
this.date=_d;
for(var i=0,_e;_e=this.replacements[i];i++){
this.tokens[_e[0]]=this[_e[1]]();
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
var _f=this.date.getTime()-new Date(this.date.getFullYear(),0,1).getTime();
return Math.floor(_f/86400000)+"";
},W:function(){
var _10;
var _11=new Date(this.date.getFullYear(),0,1).getDay()+1;
var w=this.date.getDay()+1;
var z=parseInt(this.z());
if(z<=(8-_11)&&_11>4){
var _12=new Date(this.date.getFullYear()-1,this.date.getMonth(),this.date.getDate());
if(_11==5||(_11==6&&_3.isLeapYear(_12))){
_10=53;
}else{
_10=52;
}
}else{
var i;
if(Boolean(this.L())){
i=366;
}else{
i=365;
}
if((i-z)<(4-w)){
_10=1;
}else{
var j=z+(7-w)+(_11-1);
_10=Math.ceil(j/7);
if(_11>4){
--_10;
}
}
}
return _10;
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
var _13=(this.date.getHours()*3600)+(this.date.getMinutes()*60)+this.getSeconds()+(off*60);
var _14=Math.abs(Math.floor(_13/86.4)%1000)+"";
while(_14.length<2){
_14="0"+_14;
}
return _14;
},g:function(){
return (this.date.getHours()>12)?this.date.getHours()-12+"":this.date.getHours()+"";
},G:function(){
return this.date.getHours()+"";
},h:function(){
var g=this.g();
return (g.length==1)?"0"+g:g;
},H:function(){
var G=this.G();
return (G.length==1)?"0"+G:G;
},i:function(){
var _15=this.date.getMinutes()+"";
return (_15.length==1)?"0"+_15:_15;
},s:function(){
var _16=this.date.getSeconds()+"";
return (_16.length==1)?"0"+_16:_16;
},e:function(){
return _3.getTimezoneName(this.date);
},I:function(){
},O:function(){
var off=Math.abs(this.date.getTimezoneOffset());
var _17=Math.floor(off/60)+"";
var _18=(off%60)+"";
if(_17.length==1){
_17="0"+_17;
}
if(_18.length==1){
_17="0"+_18;
}
return ((this.date.getTimezoneOffset()<0)?"+":"-")+_17+_18;
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
return dojox.date.php;
});

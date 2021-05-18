//>>built
define("dojox/data/AndOrReadStore",["dojo/_base/declare","dojo/_base/lang","dojo/data/ItemFileReadStore","dojo/data/util/filter","dojo/_base/array","dojo/_base/json"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.data.AndOrReadStore",[_3],{_containsValue:function(_7,_8,_9,_a){
return _5.some(this.getValues(_7,_8),function(_b){
if(_2.isString(_a)){
return eval(_a);
}else{
if(_b!==null&&!_2.isObject(_b)&&_a){
if(_b.toString().match(_a)){
return true;
}
}else{
if(_9===_b){
return true;
}else{
return false;
}
}
}
});
},filter:function(_c,_d,_e){
var _f=[];
if(_c.query){
var _10=_6.fromJson(_6.toJson(_c.query));
if(typeof _10=="object"){
var _11=0;
var p;
for(p in _10){
_11++;
}
if(_11>1&&_10.complexQuery){
var cq=_10.complexQuery;
var _12=false;
for(p in _10){
if(p!=="complexQuery"){
if(!_12){
cq="( "+cq+" )";
_12=true;
}
var v=_c.query[p];
if(_2.isString(v)){
v="'"+v+"'";
}
cq+=" AND "+p+":"+v;
delete _10[p];
}
}
_10.complexQuery=cq;
}
}
var _13=_c.queryOptions?_c.queryOptions.ignoreCase:false;
if(typeof _10!="string"){
_10=_6.toJson(_10);
_10=_10.replace(/\\\\/g,"\\");
}
_10=_10.replace(/\\"/g,"\"");
var _14=_2.trim(_10.replace(/{|}/g,""));
var _15,i;
if(_14.match(/"? *complexQuery *"?:/)){
_14=_2.trim(_14.replace(/"?\s*complexQuery\s*"?:/,""));
var _16=["'","\""];
var _17,_18;
var _19=false;
for(i=0;i<_16.length;i++){
_17=_14.indexOf(_16[i]);
_15=_14.indexOf(_16[i],1);
_18=_14.indexOf(":",1);
if(_17===0&&_15!=-1&&_18<_15){
_19=true;
break;
}
}
if(_19){
_14=_14.replace(/^\"|^\'|\"$|\'$/g,"");
}
}
var _1a=_14;
var _1b=/^>=|^<=|^<|^>|^,|^NOT |^AND |^OR |^\(|^\)|^!|^&&|^\|\|/i;
var _1c="";
var op="";
var val="";
var pos=-1;
var err=false;
var key="";
var _1d="";
var tok="";
_15=-1;
for(i=0;i<_d.length;++i){
var _1e=true;
var _1f=_d[i];
if(_1f===null){
_1e=false;
}else{
_14=_1a;
_1c="";
while(_14.length>0&&!err){
op=_14.match(_1b);
while(op&&!err){
_14=_2.trim(_14.replace(op[0],""));
op=_2.trim(op[0]).toUpperCase();
op=op=="NOT"?"!":op=="AND"||op==","?"&&":op=="OR"?"||":op;
op=" "+op+" ";
_1c+=op;
op=_14.match(_1b);
}
if(_14.length>0){
var _20=/:|>=|<=|>|</g,_21=_14.match(_20),_1e=_21&&_21.shift(),_22;
pos=_14.indexOf(_1e);
if(pos==-1){
err=true;
break;
}else{
key=_2.trim(_14.substring(0,pos).replace(/\"|\'/g,""));
_14=_2.trim(_14.substring(pos+_1e.length));
tok=_14.match(/^\'|^\"/);
if(tok){
tok=tok[0];
pos=_14.indexOf(tok);
_15=_14.indexOf(tok,pos+1);
if(_15==-1){
err=true;
break;
}
_1d=_14.substring(pos+_1e.length,_15);
if(_15==_14.length-1){
_14="";
}else{
_14=_2.trim(_14.substring(_15+1));
}
if(_1e!=":"){
_22=this.getValue(_1f,key)+_1e+_1d;
}else{
_22=_4.patternToRegExp(_1d,_13);
}
_1c+=this._containsValue(_1f,key,_1d,_22);
}else{
tok=_14.match(/\s|\)|,/);
if(tok){
var _23=new Array(tok.length);
for(var j=0;j<tok.length;j++){
_23[j]=_14.indexOf(tok[j]);
}
pos=_23[0];
if(_23.length>1){
for(var j=1;j<_23.length;j++){
pos=Math.min(pos,_23[j]);
}
}
_1d=_2.trim(_14.substring(0,pos));
_14=_2.trim(_14.substring(pos));
}else{
_1d=_2.trim(_14);
_14="";
}
if(_1e!=":"){
_22=this.getValue(_1f,key)+_1e+_1d;
}else{
_22=_4.patternToRegExp(_1d,_13);
}
_1c+=this._containsValue(_1f,key,_1d,_22);
}
}
}
}
_1e=eval(_1c);
}
if(_1e){
_f.push(_1f);
}
}
if(err){
_f=[];
}
}else{
for(var i=0;i<_d.length;++i){
var _24=_d[i];
if(_24!==null){
_f.push(_24);
}
}
}
_e(_f,_c);
}});
});

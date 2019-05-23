//>>built
define("dojox/jsonPath/query",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.jsonPath.query");
_3.jsonPath.query=function(_4,_5,_6){
var re=_3.jsonPath._regularExpressions;
if(!_6){
_6={};
}
var _7=[];
function _8(i){
return _7[i];
};
var _9=_8.name;
var _a;
if(_6.resultType=="PATH"&&_6.evalType=="RESULT"){
throw Error("RESULT based evaluation not supported with PATH based results");
}
var P={resultType:_6.resultType||"VALUE",normalize:function(_b){
var _c=[];
_b=_b.replace(/'([^']|'')*'/g,function(t){
return _9+"("+(_7.push(eval(t))-1)+")";
});
var ll=-1;
while(ll!=_c.length){
ll=_c.length;
_b=_b.replace(/(\??\([^\(\)]*\))/g,function($0){
return "#"+(_c.push($0)-1);
});
}
_b=_b.replace(/[\['](#[0-9]+)[\]']/g,"[$1]").replace(/'?\.'?|\['?/g,";").replace(/;;;|;;/g,";..;").replace(/;$|'?\]|'$/g,"");
ll=-1;
while(ll!=_b){
ll=_b;
_b=_b.replace(/#([0-9]+)/g,function($0,$1){
return _c[$1];
});
}
return _b.split(";");
},asPaths:function(_d){
for(var j=0;j<_d.length;j++){
var p="$";
var x=_d[j];
for(var i=1,n=x.length;i<n;i++){
p+=/^[0-9*]+$/.test(x[i])?("["+x[i]+"]"):("['"+x[i]+"']");
}
_d[j]=p;
}
return _d;
},exec:function(_e,_f,rb){
var _10=["$"];
var _11=rb?_f:[_f];
var _12=[_10];
function add(v,p,def){
if(v&&v.hasOwnProperty(p)&&P.resultType!="VALUE"){
_12.push(_10.concat([p]));
}
if(def){
_11=v[p];
}else{
if(v&&v.hasOwnProperty(p)){
_11.push(v[p]);
}
}
};
function _13(v){
_11.push(v);
_12.push(_10);
P.walk(v,function(i){
if(typeof v[i]==="object"){
var _14=_10;
_10=_10.concat(i);
_13(v[i]);
_10=_14;
}
});
};
function _15(loc,val){
if(val instanceof Array){
var len=val.length,_16=0,end=len,_17=1;
loc.replace(/^(-?[0-9]*):(-?[0-9]*):?(-?[0-9]*)$/g,function($0,$1,$2,$3){
_16=parseInt($1||_16);
end=parseInt($2||end);
_17=parseInt($3||_17);
});
_16=(_16<0)?Math.max(0,_16+len):Math.min(len,_16);
end=(end<0)?Math.max(0,end+len):Math.min(len,end);
for(var i=_16;i<end;i+=_17){
add(val,i);
}
}
};
function _18(str){
var i=loc.match(/^_str\(([0-9]+)\)$/);
return i?_7[i[1]]:str;
};
function _19(val){
if(/^\(.*?\)$/.test(loc)){
add(val,P.eval(loc,val),rb);
}else{
if(loc==="*"){
P.walk(val,rb&&val instanceof Array?function(i){
P.walk(val[i],function(j){
add(val[i],j);
});
}:function(i){
add(val,i);
});
}else{
if(loc===".."){
_13(val);
}else{
if(/,/.test(loc)){
for(var s=loc.split(/'?,'?/),i=0,n=s.length;i<n;i++){
add(val,_18(s[i]));
}
}else{
if(/^\?\(.*?\)$/.test(loc)){
P.walk(val,function(i){
if(P.eval(loc.replace(/^\?\((.*?)\)$/,"$1"),val[i])){
add(val,i);
}
});
}else{
if(/^(-?[0-9]*):(-?[0-9]*):?([0-9]*)$/.test(loc)){
_15(loc,val);
}else{
loc=_18(loc);
if(rb&&val instanceof Array&&!/^[0-9*]+$/.test(loc)){
P.walk(val,function(i){
add(val[i],loc);
});
}else{
add(val,loc,rb);
}
}
}
}
}
}
}
};
while(_e.length){
var loc=_e.shift();
if((_f=_11)===null||_f===undefined){
return _f;
}
_11=[];
var _1a=_12;
_12=[];
if(rb){
_19(_f);
}else{
P.walk(_f,function(i){
_10=_1a[i]||_10;
_19(_f[i]);
});
}
}
if(P.resultType=="BOTH"){
_12=P.asPaths(_12);
var _1b=[];
for(var i=0;i<_12.length;i++){
_1b.push({path:_12[i],value:_11[i]});
}
return _1b;
}
return P.resultType=="PATH"?P.asPaths(_12):_11;
},walk:function(val,f){
if(val instanceof Array){
for(var i=0,n=val.length;i<n;i++){
if(i in val){
f(i);
}
}
}else{
if(typeof val==="object"){
for(var m in val){
if(val.hasOwnProperty(m)){
f(m);
}
}
}
}
},eval:function(x,v){
try{
return $&&v&&eval(x.replace(/@/g,"v"));
}
catch(e){
throw new SyntaxError("jsonPath: "+e.message+": "+x.replace(/@/g,"v").replace(/\^/g,"_a"));
}
}};
var $=_4;
if(_5&&_4){
return P.exec(P.normalize(_5).slice(1),_4,_6.evalType=="RESULT");
}
return false;
};
});

//>>built
define("dojox/lang/functional/lambda",["../..","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3){
var df=_2.getObject("lang.functional",true,_1);
var _4={};
var _5="ab".split(/a*/).length>1?String.prototype.split:function(_6){
var r=this.split.call(this,_6),m=_6.exec(this);
if(m&&m.index==0){
r.unshift("");
}
return r;
};
var _7=function(s){
var _8=[],_9=_5.call(s,/\s*->\s*/m);
if(_9.length>1){
while(_9.length){
s=_9.pop();
_8=_9.pop().split(/\s*,\s*|\s+/m);
if(_9.length){
_9.push("(function("+_8.join(", ")+"){ return ("+s+"); })");
}
}
}else{
if(s.match(/\b_\b/)){
_8=["_"];
}else{
var l=s.match(/^\s*(?:[+*\/%&|\^\.=<>]|!=)/m),r=s.match(/[+\-*\/%&|\^\.=<>!]\s*$/m);
if(l||r){
if(l){
_8.push("$1");
s="$1"+s;
}
if(r){
_8.push("$2");
s=s+"$2";
}
}else{
var _a=s.replace(/(?:\b[A-Z]|\.[a-zA-Z_$])[a-zA-Z_$\d]*|[a-zA-Z_$][a-zA-Z_$\d]*:|this|true|false|null|undefined|typeof|instanceof|in|delete|new|void|arguments|decodeURI|decodeURIComponent|encodeURI|encodeURIComponent|escape|eval|isFinite|isNaN|parseFloat|parseInt|unescape|dojo|dijit|dojox|window|document|'(?:[^'\\]|\\.)*'|"(?:[^"\\]|\\.)*"/g,"").match(/([a-z_$][a-z_$\d]*)/gi)||[],t={};
_3.forEach(_a,function(v){
if(!t.hasOwnProperty(v)){
_8.push(v);
t[v]=1;
}
});
}
}
}
return {args:_8,body:s};
};
var _b=function(a){
return a.length?function(){
var i=a.length-1,x=df.lambda(a[i]).apply(this,arguments);
for(--i;i>=0;--i){
x=df.lambda(a[i]).call(this,x);
}
return x;
}:function(x){
return x;
};
};
_2.mixin(df,{rawLambda:function(s){
return _7(s);
},buildLambda:function(s){
var l=_7(s);
return "function("+l.args.join(",")+"){return ("+l.body+");}";
},lambda:function(s){
if(typeof s=="function"){
return s;
}
if(s instanceof Array){
return _b(s);
}
if(_4.hasOwnProperty(s)){
return _4[s];
}
var l=_7(s);
return _4[s]=new Function(l.args,"return ("+l.body+");");
},clearLambdaCache:function(){
_4={};
}});
return df;
});

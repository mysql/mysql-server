//>>built
define("dojox/lang/functional/lambda",["../..","dojo/_base/kernel","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3,_4){
var df=_3.getObject("lang.functional",true,_1);
var _5={};
var _6="ab".split(/a*/).length>1?String.prototype.split:function(_7){
var r=this.split.call(this,_7),m=_7.exec(this);
if(m&&m.index==0){
r.unshift("");
}
return r;
};
var _8=function(s){
var _9=[],_a=_6.call(s,/\s*->\s*/m);
if(_a.length>1){
while(_a.length){
s=_a.pop();
_9=_a.pop().split(/\s*,\s*|\s+/m);
if(_a.length){
_a.push("(function("+_9+"){return ("+s+")})");
}
}
}else{
if(s.match(/\b_\b/)){
_9=["_"];
}else{
var l=s.match(/^\s*(?:[+*\/%&|\^\.=<>]|!=)/m),r=s.match(/[+\-*\/%&|\^\.=<>!]\s*$/m);
if(l||r){
if(l){
_9.push("$1");
s="$1"+s;
}
if(r){
_9.push("$2");
s=s+"$2";
}
}else{
var _b=s.replace(/(?:\b[A-Z]|\.[a-zA-Z_$])[a-zA-Z_$\d]*|[a-zA-Z_$][a-zA-Z_$\d]*:|this|true|false|null|undefined|typeof|instanceof|in|delete|new|void|arguments|decodeURI|decodeURIComponent|encodeURI|encodeURIComponent|escape|eval|isFinite|isNaN|parseFloat|parseInt|unescape|dojo|dijit|dojox|window|document|'(?:[^'\\]|\\.)*'|"(?:[^"\\]|\\.)*"/g,"").match(/([a-z_$][a-z_$\d]*)/gi)||[],t={};
_4.forEach(_b,function(v){
if(!(v in t)){
_9.push(v);
t[v]=1;
}
});
}
}
}
return {args:_9,body:s};
};
var _c=function(a){
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
_3.mixin(df,{rawLambda:function(s){
return _8(s);
},buildLambda:function(s){
s=_8(s);
return "function("+s.args.join(",")+"){return ("+s.body+");}";
},lambda:function(s){
if(typeof s=="function"){
return s;
}
if(s instanceof Array){
return _c(s);
}
if(s in _5){
return _5[s];
}
s=_8(s);
return _5[s]=new Function(s.args,"return ("+s.body+");");
},clearLambdaCache:function(){
_5={};
}});
return df;
});

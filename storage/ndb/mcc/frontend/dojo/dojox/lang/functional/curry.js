//>>built
define("dojox/lang/functional/curry",["dojo/_base/lang","./lambda"],function(_1,df){
var ap=Array.prototype;
var _2=function(_3){
return function(){
var _4=_3.args.concat(ap.slice.call(arguments,0));
if(arguments.length+_3.args.length<_3.arity){
return _2({func:_3.func,arity:_3.arity,args:_4});
}
return _3.func.apply(this,_4);
};
};
_1.mixin(df,{curry:function(f,_5){
f=df.lambda(f);
_5=typeof _5=="number"?_5:f.length;
return _2({func:f,arity:_5,args:[]});
},arg:{},partial:function(f){
var a=arguments,l=a.length,_6=new Array(l-1),p=[],i=1,t;
f=df.lambda(f);
for(;i<l;++i){
t=a[i];
_6[i-1]=t;
if(t===df.arg){
p.push(i-1);
}
}
return function(){
var t=ap.slice.call(_6,0),i=0,l=p.length;
for(;i<l;++i){
t[p[i]]=arguments[i];
}
return f.apply(this,t);
};
},mixer:function(f,_7){
f=df.lambda(f);
return function(){
var t=new Array(_7.length),i=0,l=_7.length;
for(;i<l;++i){
t[i]=arguments[_7[i]];
}
return f.apply(this,t);
};
},flip:function(f){
f=df.lambda(f);
return function(){
var a=arguments,l=a.length-1,t=new Array(l+1),i=0;
for(;i<=l;++i){
t[l-i]=a[i];
}
return f.apply(this,t);
};
}});
});

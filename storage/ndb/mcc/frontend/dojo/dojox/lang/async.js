//>>built
define("dojox/lang/async",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.async");
(function(){
var d=_1,_4=d.Deferred,_5=d.forEach,_6=d.some,_7=_3.lang.async,_8=Array.prototype.slice,_9=Object.prototype.toString;
_7.seq=function(x){
var fs=_9.call(x)=="[object Array]"?x:arguments;
return function(_a){
var x=new _4();
_5(fs,function(f){
x.addCallback(f);
});
x.callback(_a);
return x;
};
};
_7.par=function(x){
var fs=_9.call(x)=="[object Array]"?x:arguments;
return function(_b){
var _c=new Array(fs.length),_d=function(){
_5(_c,function(v){
if(v instanceof _4&&v.fired<0){
v.cancel();
}
});
},x=new _4(_d),_e=fs.length;
_5(fs,function(f,i){
var x;
try{
x=f(_b);
}
catch(e){
x=e;
}
_c[i]=x;
});
var _f=_6(_c,function(v){
if(v instanceof Error){
_d();
x.errback(v);
return true;
}
return false;
});
if(!_f){
_5(_c,function(v,i){
if(v instanceof _4){
v.addCallbacks(function(v){
_c[i]=v;
if(!--_e){
x.callback(_c);
}
},function(v){
_d();
x.errback(v);
});
}else{
--_e;
}
});
}
if(!_e){
x.callback(_c);
}
return x;
};
};
_7.any=function(x){
var fs=_9.call(x)=="[object Array]"?x:arguments;
return function(_10){
var _11=new Array(fs.length),_12=true;
cancel=function(_13){
_5(_11,function(v,i){
if(i!=_13&&v instanceof _4&&v.fired<0){
v.cancel();
}
});
},x=new _4(cancel);
_5(fs,function(f,i){
var x;
try{
x=f(_10);
}
catch(e){
x=e;
}
_11[i]=x;
});
var _14=_6(_11,function(v,i){
if(!(v instanceof _4)){
cancel(i);
x.callback(v);
return true;
}
return false;
});
if(!_14){
_5(_11,function(v,i){
v.addBoth(function(v){
if(_12){
_12=false;
cancel(i);
x.callback(v);
}
});
});
}
return x;
};
};
_7.select=function(_15,x){
var fs=_9.call(x)=="[object Array]"?x:_8.call(arguments,1);
return function(_16){
return new _4().addCallback(_15).addCallback(function(v){
if(typeof v=="number"&&v>=0&&v<fs.length){
return fs[v](_16);
}else{
return new Error("async.select: out of range");
}
}).callback(_16);
};
};
_7.ifThen=function(_17,_18,_19){
return function(_1a){
return new _4().addCallback(_17).addCallback(function(v){
return (v?_18:_19)(_1a);
}).callback(_1a);
};
};
_7.loop=function(_1b,_1c){
return function(_1d){
var x,y=new _4(function(){
x.cancel();
});
function _1e(v){
y.errback(v);
};
function _1f(v){
if(v){
x.addCallback(_1c).addCallback(_20);
}else{
y.callback(v);
}
return v;
};
function _20(_21){
x=new _4().addCallback(_1b).addCallback(_1f).addErrback(_1e);
x.callback(_21);
};
_20(_1d);
return y;
};
};
})();
});

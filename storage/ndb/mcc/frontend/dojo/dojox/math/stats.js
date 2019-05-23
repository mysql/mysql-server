//>>built
define("dojox/math/stats",["dojo","../main"],function(_1,_2){
_1.getObject("math.stats",true,_2);
var st=_2.math.stats;
_1.mixin(st,{sd:function(a){
return Math.sqrt(st.variance(a));
},variance:function(a){
var _3=0,_4=0;
_1.forEach(a,function(_5){
_3+=_5;
_4+=Math.pow(_5,2);
});
return (_4/a.length)-Math.pow(_3/a.length,2);
},bestFit:function(a,_6,_7){
_6=_6||"x",_7=_7||"y";
if(a[0]!==undefined&&typeof (a[0])=="number"){
a=_1.map(a,function(_8,_9){
return {x:_9,y:_8};
});
}
var sx=0,sy=0,_a=0,_b=0,_c=0,_d=0,_e=0,n=a.length,t;
for(var i=0;i<n;i++){
sx+=a[i][_6];
sy+=a[i][_7];
_a+=Math.pow(a[i][_6],2);
_b+=Math.pow(a[i][_7],2);
_c+=a[i][_6]*a[i][_7];
}
for(i=0;i<n;i++){
t=a[i][_6]-sx/n;
_d+=t*t;
_e+=t*a[i][_7];
}
var _f=_e/(_d||1);
var d=Math.sqrt((_a-Math.pow(sx,2)/n)*(_b-Math.pow(sy,2)/n));
if(d===0){
throw new Error("dojox.math.stats.bestFit: the denominator for Pearson's R is 0.");
}
var r=(_c-(sx*sy/n))/d;
var r2=Math.pow(r,2);
if(_f<0){
r=-r;
}
return {slope:_f,intercept:(sy-sx*_f)/(n||1),r:r,r2:r2};
},forecast:function(a,x,_10,_11){
var fit=st.bestFit(a,_10,_11);
return (fit.slope*x)+fit.intercept;
},mean:function(a){
var t=0;
_1.forEach(a,function(v){
t+=v;
});
return t/Math.max(a.length,1);
},min:function(a){
return Math.min.apply(null,a);
},max:function(a){
return Math.max.apply(null,a);
},median:function(a){
var t=a.slice(0).sort(function(a,b){
return a-b;
});
return (t[Math.floor(a.length/2)]+t[Math.ceil(a.length/2)])/2;
},mode:function(a){
var o={},r=0,m=Number.MIN_VALUE;
_1.forEach(a,function(v){
(o[v]!==undefined)?o[v]++:o[v]=1;
});
for(var p in o){
if(m<o[p]){
m=o[p],r=p;
}
}
return r;
},sum:function(a){
var sum=0;
_1.forEach(a,function(n){
sum+=n;
});
return sum;
},approxLin:function(a,pos){
var p=pos*(a.length-1),t=Math.ceil(p),f=t-1;
if(f<0){
return a[0];
}
if(t>=a.length){
return a[a.length-1];
}
return a[f]*(t-p)+a[t]*(p-f);
},summary:function(a,_12){
if(!_12){
a=a.slice(0);
a.sort(function(a,b){
return a-b;
});
}
var l=st.approxLin,_13={min:a[0],p25:l(a,0.25),med:l(a,0.5),p75:l(a,0.75),max:a[a.length-1],p10:l(a,0.1),p90:l(a,0.9)};
return _13;
}});
return _2.math.stats;
});

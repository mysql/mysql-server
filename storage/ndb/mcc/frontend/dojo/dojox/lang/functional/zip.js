//>>built
define("dojox/lang/functional/zip",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.zip");
(function(){
var df=_3.lang.functional;
_1.mixin(df,{zip:function(){
var n=arguments[0].length,m=arguments.length,i=1,t=new Array(n),j,p;
for(;i<m;n=Math.min(n,arguments[i++].length)){
}
for(i=0;i<n;++i){
p=new Array(m);
for(j=0;j<m;p[j]=arguments[j][i],++j){
}
t[i]=p;
}
return t;
},unzip:function(a){
return df.zip.apply(null,a);
}});
})();
});

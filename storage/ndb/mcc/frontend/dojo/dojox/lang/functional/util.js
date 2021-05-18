//>>built
define("dojox/lang/functional/util",["dojo","dijit","dojox","dojo/require!dojox/lang/functional/lambda"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.util");
_1.require("dojox.lang.functional.lambda");
(function(){
var df=_3.lang.functional;
_1.mixin(df,{inlineLambda:function(_4,_5,_6){
var s=df.rawLambda(_4);
if(_6){
df.forEach(s.args,_6);
}
var ap=typeof _5=="string",n=ap?s.args.length:Math.min(s.args.length,_5.length),a=new Array(4*n+4),i,j=1;
for(i=0;i<n;++i){
a[j++]=s.args[i];
a[j++]="=";
a[j++]=ap?_5+"["+i+"]":_5[i];
a[j++]=",";
}
a[0]="(";
a[j++]="(";
a[j++]=s.body;
a[j]="))";
return a.join("");
}});
})();
});

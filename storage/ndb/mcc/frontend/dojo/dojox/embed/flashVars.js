//>>built
define("dojox/embed/flashVars",["dojo"],function(_1){
_1.getObject("dojox.embed",true);
_1.deprecated("dojox.embed.flashVars","Will be removed in 2.0","2.0");
dojox.embed.flashVars={serialize:function(n,o){
var _2=function(_3){
if(typeof _3=="string"){
_3=_3.replace(/;/g,"_sc_");
_3=_3.replace(/\./g,"_pr_");
_3=_3.replace(/\:/g,"_cl_");
}
return _3;
};
var df=dojox.embed.flashVars.serialize;
var _4="";
if(_1.isArray(o)){
for(var i=0;i<o.length;i++){
_4+=df(n+"."+i,_2(o[i]))+";";
}
return _4.replace(/;{2,}/g,";");
}else{
if(_1.isObject(o)){
for(var nm in o){
_4+=df(n+"."+nm,_2(o[nm]))+";";
}
return _4.replace(/;{2,}/g,";");
}
}
return n+":"+o;
}};
return dojox.embed.flashVars;
});

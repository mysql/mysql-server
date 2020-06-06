//>>built
define("dojox/embed/flashVars",["dojo"],function(_1){
_1.deprecated("dojox.embed.flashVars","Will be removed in 2.0","2.0");
var _2={serialize:function(n,o){
var _3=function(_4){
if(typeof _4=="string"){
_4=_4.replace(/;/g,"_sc_");
_4=_4.replace(/\./g,"_pr_");
_4=_4.replace(/\:/g,"_cl_");
}
return _4;
};
var df=dojox.embed.flashVars.serialize;
var _5="";
if(_1.isArray(o)){
for(var i=0;i<o.length;i++){
_5+=df(n+"."+i,_3(o[i]))+";";
}
return _5.replace(/;{2,}/g,";");
}else{
if(_1.isObject(o)){
for(var nm in o){
_5+=df(n+"."+nm,_3(o[nm]))+";";
}
return _5.replace(/;{2,}/g,";");
}
}
return n+":"+o;
}};
_1.setObject("dojox.embed.flashVars",_2);
return _2;
});

//>>built
define("dojox/rpc/ProxiedPath",["dojo","dojox","dojox/rpc/Service"],function(_1,_2){
_2.rpc.envelopeRegistry.register("PROXIED-PATH",function(_3){
return _3=="PROXIED-PATH";
},{serialize:function(_4,_5,_6){
var i;
var _7=_2.rpc.getTarget(_4,_5);
if(_1.isArray(_6)){
for(i=0;i<_6.length;i++){
_7+="/"+(_6[i]==null?"":_6[i]);
}
}else{
for(i in _6){
_7+="/"+i+"/"+_6[i];
}
}
return {data:"",target:(_5.proxyUrl||_4.proxyUrl)+"?url="+encodeURIComponent(_7)};
},deserialize:function(_8){
return _8;
}});
});

//>>built
define("dojox/math/random/Simple",["dojo"],function(_1){
return _1.declare("dojox.math.random.Simple",null,{destroy:function(){
},nextBytes:function(_2){
for(var i=0,l=_2.length;i<l;++i){
_2[i]=Math.floor(256*Math.random());
}
}});
});

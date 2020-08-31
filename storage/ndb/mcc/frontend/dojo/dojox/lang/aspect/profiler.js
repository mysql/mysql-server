//>>built
define("dojox/lang/aspect/profiler",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.aspect.profiler");
(function(){
var _4=_3.lang.aspect,_5=0;
var _6=function(_7){
this.args=_7?[_7]:[];
this.inCall=0;
};
_1.extend(_6,{before:function(){
if(!(this.inCall++)){
console.profile.apply(console,this.args);
}
},after:function(){
if(!--this.inCall){
}
}});
_4.profiler=function(_8){
return new _6(_8);
};
})();
});

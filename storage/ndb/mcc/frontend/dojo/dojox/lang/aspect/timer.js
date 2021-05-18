//>>built
define("dojox/lang/aspect/timer",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.aspect.timer");
(function(){
var _4=_3.lang.aspect,_5=0;
var _6=function(_7){
this.name=_7||("DojoAopTimer #"+ ++_5);
this.inCall=0;
};
_1.extend(_6,{before:function(){
if(!(this.inCall++)){
}
},after:function(){
if(!--this.inCall){
}
}});
_4.timer=function(_8){
return new _6(_8);
};
})();
});

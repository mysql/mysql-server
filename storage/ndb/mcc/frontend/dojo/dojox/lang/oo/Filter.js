//>>built
define("dojox/lang/oo/Filter",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.oo.Filter");
(function(){
var oo=_3.lang.oo,F=oo.Filter=function(_4,_5){
this.bag=_4;
this.filter=typeof _5=="object"?function(){
return _5.exec.apply(_5,arguments);
}:_5;
},_6=function(_7){
this.map=_7;
};
_6.prototype.exec=function(_8){
return this.map.hasOwnProperty(_8)?this.map[_8]:_8;
};
oo.filter=function(_9,_a){
return new F(_9,new _6(_a));
};
})();
});

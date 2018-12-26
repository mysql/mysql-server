//>>built
define("dojox/NodeList/delegate",["dojo/_base/lang","dojo/query","dojo/_base/NodeList","dojo/NodeList-traverse"],function(_1,_2,_3){
_1.extend(_3,{delegate:function(_4,_5,fn){
return this.connect(_5,function(_6){
var _7=_2(_6.target).closest(_4,this);
if(_7.length){
fn.call(_7[0],_6);
}
});
}});
return _3;
});

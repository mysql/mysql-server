//>>built
define("dojox/dtl/ext-dojo/NodeList",["dojo/_base/lang","dojo/query","../_base"],function(_1,_2,dd){
var nl=_1.getObject("dojox.dtl.ext-dojo.NodeList",true);
var _3=_2.NodeList;
_1.extend(_3,{dtl:function(_4,_5){
var d=dd,_6=this;
var _7=function(_8,_9){
var _a=_8.render(new d._Context(_9));
_6.forEach(function(_b){
_b.innerHTML=_a;
});
};
d.text._resolveTemplateArg(_4).addCallback(function(_c){
_4=new d.Template(_c);
d.text._resolveContextArg(_5).addCallback(function(_d){
_7(_4,_d);
});
});
return this;
}});
return _3;
});

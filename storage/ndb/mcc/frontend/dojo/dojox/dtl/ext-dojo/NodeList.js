//>>built
define("dojox/dtl/ext-dojo/NodeList",["dojo/_base/lang","dojo/_base/NodeList","../_base"],function(_1,_2,dd){
var nl=_1.getObject("dojox.dtl.ext-dojo.NodeList",true);
_1.extend(_2,{dtl:function(_3,_4){
var d=dd,_5=this;
var _6=function(_7,_8){
var _9=_7.render(new d._Context(_8));
_5.forEach(function(_a){
_a.innerHTML=_9;
});
};
d.text._resolveTemplateArg(_3).addCallback(function(_b){
_3=new d.Template(_b);
d.text._resolveContextArg(_4).addCallback(function(_c){
_6(_3,_c);
});
});
return this;
}});
return nl;
});

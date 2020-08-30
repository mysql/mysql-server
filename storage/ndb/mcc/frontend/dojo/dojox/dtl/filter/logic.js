//>>built
define("dojox/dtl/filter/logic",["dojo/_base/lang","../_base"],function(_1,dd){
var _2=_1.getObject("filter.logic",true,dd);
_1.mixin(_2,{default_:function(_3,_4){
return _3||_4||"";
},default_if_none:function(_5,_6){
return (_5===null)?_6||"":_5||"";
},divisibleby:function(_7,_8){
return (parseInt(_7,10)%parseInt(_8,10))===0;
},_yesno:/\s*,\s*/g,yesno:function(_9,_a){
if(!_a){
_a="yes,no,maybe";
}
var _b=_a.split(dojox.dtl.filter.logic._yesno);
if(_b.length<2){
return _9;
}
if(_9){
return _b[0];
}
if((!_9&&_9!==null)||_b.length<3){
return _b[1];
}
return _b[2];
}});
return _2;
});

//>>built
define("dojox/dtl/filter/logic",["dojo/_base/lang","../_base"],function(_1,dd){
_1.getObject("dojox.dtl.filter.logic",true);
_1.mixin(dd.filter.logic,{default_:function(_2,_3){
return _2||_3||"";
},default_if_none:function(_4,_5){
return (_4===null)?_5||"":_4||"";
},divisibleby:function(_6,_7){
return (parseInt(_6,10)%parseInt(_7,10))===0;
},_yesno:/\s*,\s*/g,yesno:function(_8,_9){
if(!_9){
_9="yes,no,maybe";
}
var _a=_9.split(dojox.dtl.filter.logic._yesno);
if(_a.length<2){
return _8;
}
if(_8){
return _a[0];
}
if((!_8&&_8!==null)||_a.length<3){
return _a[1];
}
return _a[2];
}});
return dojox.dtl.filter.logic;
});

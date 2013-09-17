//>>built
define("dojox/grid/enhanced/plugins/filter/FilterBuilder",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","./_FilterExpr"],function(_1,_2,_3,_4){
var _5=function(_6){
return _3.partial(function(_7,_8){
return new _4[_7](_8);
},_6);
},_9=function(_a){
return _3.partial(function(_b,_c){
return new _4.LogicNOT(new _4[_b](_c));
},_a);
};
return _1("dojox.grid.enhanced.plugins.filter.FilterBuilder",null,{buildExpression:function(_d){
if("op" in _d){
return this.supportedOps[_d.op.toLowerCase()](_2.map(_d.data,this.buildExpression,this));
}else{
var _e=_3.mixin(this.defaultArgs[_d.datatype],_d.args||{});
return new this.supportedTypes[_d.datatype](_d.data,_d.isColumn,_e);
}
},supportedOps:{"equalto":_5("EqualTo"),"lessthan":_5("LessThan"),"lessthanorequalto":_5("LessThanOrEqualTo"),"largerthan":_5("LargerThan"),"largerthanorequalto":_5("LargerThanOrEqualTo"),"contains":_5("Contains"),"startswith":_5("StartsWith"),"endswith":_5("EndsWith"),"notequalto":_9("EqualTo"),"notcontains":_9("Contains"),"notstartswith":_9("StartsWith"),"notendswith":_9("EndsWith"),"isempty":_5("IsEmpty"),"range":function(_f){
return new _4.LogicALL(new _4.LargerThanOrEqualTo(_f.slice(0,2)),new _4.LessThanOrEqualTo(_f[0],_f[2]));
},"logicany":_5("LogicANY"),"logicall":_5("LogicALL")},supportedTypes:{"number":_4.NumberExpr,"string":_4.StringExpr,"boolean":_4.BooleanExpr,"date":_4.DateExpr,"time":_4.TimeExpr},defaultArgs:{"boolean":{"falseValue":"false","convert":function(_10,_11){
var _12=_11.falseValue;
var _13=_11.trueValue;
if(_3.isString(_10)){
if(_13&&_10.toLowerCase()==_13){
return true;
}
if(_12&&_10.toLowerCase()==_12){
return false;
}
}
return !!_10;
}}}});
});

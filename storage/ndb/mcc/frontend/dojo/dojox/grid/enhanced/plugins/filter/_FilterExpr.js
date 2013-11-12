//>>built
define("dojox/grid/enhanced/plugins/filter/_FilterExpr",["dojo/_base/declare","dojo/_base/lang","dojo/date","./_DataExprs"],function(_1,_2,_3,_4){
var _5=_1("dojox.grid.enhanced.plugins.filter.LogicAND",_4._BiOpExpr,{_name:"and",_calculate:function(_6,_7,_8,_9){
var _a=_6.applyRow(_8,_9).getValue()&&_7.applyRow(_8,_9).getValue();
return new _4.BooleanExpr(_a);
}});
var _b=_1("dojox.grid.enhanced.plugins.filter.LogicOR",_4._BiOpExpr,{_name:"or",_calculate:function(_c,_d,_e,_f){
var res=_c.applyRow(_e,_f).getValue()||_d.applyRow(_e,_f).getValue();
return new _4.BooleanExpr(res);
}});
var _10=_1("dojox.grid.enhanced.plugins.filter.LogicXOR",_4._BiOpExpr,{_name:"xor",_calculate:function(_11,_12,_13,_14){
var _15=_11.applyRow(_13,_14).getValue();
var _16=_12.applyRow(_13,_14).getValue();
return new _4.BooleanExpr((!!_15)!=(!!_16));
}});
var _17=_1("dojox.grid.enhanced.plugins.filter.LogicNOT",_4._UniOpExpr,{_name:"not",_calculate:function(_18,_19,_1a){
return new _4.BooleanExpr(!_18.applyRow(_19,_1a).getValue());
}});
var _1b=_1("dojox.grid.enhanced.plugins.filter.LogicALL",_4._OperatorExpr,{_name:"all",applyRow:function(_1c,_1d){
for(var i=0,res=true;res&&(this._operands[i] instanceof _4._ConditionExpr);++i){
res=this._operands[i].applyRow(_1c,_1d).getValue();
}
return new _4.BooleanExpr(res);
}});
var _1e=_1("dojox.grid.enhanced.plugins.filter.LogicANY",_4._OperatorExpr,{_name:"any",applyRow:function(_1f,_20){
for(var i=0,res=false;!res&&(this._operands[i] instanceof _4._ConditionExpr);++i){
res=this._operands[i].applyRow(_1f,_20).getValue();
}
return new _4.BooleanExpr(res);
}});
function _21(_22,_23,row,_24){
_22=_22.applyRow(row,_24);
_23=_23.applyRow(row,_24);
var _25=_22.getValue();
var _26=_23.getValue();
if(_22 instanceof _4.TimeExpr){
return _3.compare(_25,_26,"time");
}else{
if(_22 instanceof _4.DateExpr){
return _3.compare(_25,_26,"date");
}else{
if(_22 instanceof _4.StringExpr){
_25=_25.toLowerCase();
_26=String(_26).toLowerCase();
}
return _25==_26?0:(_25<_26?-1:1);
}
}
};
var _27=_1("dojox.grid.enhanced.plugins.filter.EqualTo",_4._BiOpExpr,{_name:"equal",_calculate:function(_28,_29,_2a,_2b){
var res=_21(_28,_29,_2a,_2b);
return new _4.BooleanExpr(res===0);
}});
var _2c=_1("dojox.grid.enhanced.plugins.filter.LessThan",_4._BiOpExpr,{_name:"less",_calculate:function(_2d,_2e,_2f,_30){
var res=_21(_2d,_2e,_2f,_30);
return new _4.BooleanExpr(res<0);
}});
var _31=_1("dojox.grid.enhanced.plugins.filter.LessThanOrEqualTo",_4._BiOpExpr,{_name:"lessEqual",_calculate:function(_32,_33,_34,_35){
var res=_21(_32,_33,_34,_35);
return new _4.BooleanExpr(res<=0);
}});
var _36=_1("dojox.grid.enhanced.plugins.filter.LargerThan",_4._BiOpExpr,{_name:"larger",_calculate:function(_37,_38,_39,_3a){
var res=_21(_37,_38,_39,_3a);
return new _4.BooleanExpr(res>0);
}});
var _3b=_1("dojox.grid.enhanced.plugins.filter.LargerThanOrEqualTo",_4._BiOpExpr,{_name:"largerEqual",_calculate:function(_3c,_3d,_3e,_3f){
var res=_21(_3c,_3d,_3e,_3f);
return new _4.BooleanExpr(res>=0);
}});
var _40=_1("dojox.grid.enhanced.plugins.filter.Contains",_4._BiOpExpr,{_name:"contains",_calculate:function(_41,_42,_43,_44){
var _45=String(_41.applyRow(_43,_44).getValue()).toLowerCase();
var _46=String(_42.applyRow(_43,_44).getValue()).toLowerCase();
return new _4.BooleanExpr(_45.indexOf(_46)>=0);
}});
var _47=_1("dojox.grid.enhanced.plugins.filter.StartsWith",_4._BiOpExpr,{_name:"startsWith",_calculate:function(_48,_49,_4a,_4b){
var _4c=String(_48.applyRow(_4a,_4b).getValue()).toLowerCase();
var _4d=String(_49.applyRow(_4a,_4b).getValue()).toLowerCase();
return new _4.BooleanExpr(_4c.substring(0,_4d.length)==_4d);
}});
var _4e=_1("dojox.grid.enhanced.plugins.filter.EndsWith",_4._BiOpExpr,{_name:"endsWith",_calculate:function(_4f,_50,_51,_52){
var _53=String(_4f.applyRow(_51,_52).getValue()).toLowerCase();
var _54=String(_50.applyRow(_51,_52).getValue()).toLowerCase();
return new _4.BooleanExpr(_53.substring(_53.length-_54.length)==_54);
}});
var _55=_1("dojox.grid.enhanced.plugins.filter.Matches",_4._BiOpExpr,{_name:"matches",_calculate:function(_56,_57,_58,_59){
var _5a=String(_56.applyRow(_58,_59).getValue());
var _5b=new RegExp(_57.applyRow(_58,_59).getValue());
return new _4.BooleanExpr(_5a.search(_5b)>=0);
}});
var _5c=_1("dojox.grid.enhanced.plugins.filter.IsEmpty",_4._UniOpExpr,{_name:"isEmpty",_calculate:function(_5d,_5e,_5f){
var res=_5d.applyRow(_5e,_5f).getValue();
return new _4.BooleanExpr(res===""||res==null);
}});
return _2.mixin({LogicAND:_5,LogicOR:_b,LogicXOR:_10,LogicNOT:_17,LogicALL:_1b,LogicANY:_1e,EqualTo:_27,LessThan:_2c,LessThanOrEqualTo:_31,LargerThan:_36,LargerThanOrEqualTo:_3b,Contains:_40,StartsWith:_47,EndsWith:_4e,Matches:_55,IsEmpty:_5c},_4);
});

//>>built
define("dojox/grid/enhanced/plugins/filter/_DataExprs",["dojo/_base/declare","dojo/_base/lang","dojo/date/locale","./_ConditionExpr"],function(_1,_2,_3,_4){
var _5=_1("dojox.grid.enhanced.plugins.filter.BooleanExpr",_4._DataExpr,{_name:"bool",_convertData:function(_6){
return !!_6;
}});
var _7=_1("dojox.grid.enhanced.plugins.filter.StringExpr",_4._DataExpr,{_name:"string",_convertData:function(_8){
return String(_8);
}});
var _9=_1("dojox.grid.enhanced.plugins.filter.NumberExpr",_4._DataExpr,{_name:"number",_convertDataToExpr:function(_a){
return parseFloat(_a);
}});
var _b=_1("dojox.grid.enhanced.plugins.filter.DateExpr",_4._DataExpr,{_name:"date",_convertData:function(_c){
if(_c instanceof Date){
return _c;
}else{
if(typeof _c=="number"){
return new Date(_c);
}else{
var _d=_3.parse(String(_c),_2.mixin({selector:this._name},this._convertArgs));
if(!_d){
throw new Error("Datetime parse failed: "+_c);
}
return _d;
}
}
},toObject:function(){
if(this._value instanceof Date){
var _e=this._value;
this._value=this._value.valueOf();
var _f=this.inherited(arguments);
this._value=_e;
return _f;
}else{
return this.inherited(arguments);
}
}});
var _10=_1("dojox.grid.enhanced.plugins.filter.TimeExpr",_b,{_name:"time"});
return _2.mixin({BooleanExpr:_5,StringExpr:_7,NumberExpr:_9,DateExpr:_b,TimeExpr:_10},_4);
});

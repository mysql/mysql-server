//>>built
define("dojox/grid/enhanced/plugins/filter/_ConditionExpr",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array"],function(_1,_2,_3){
var _4=_1("dojox.grid.enhanced.plugins.filter._ConditionExpr",null,{_name:"expr",applyRow:function(_5,_6){
throw new Error("_ConditionExpr.applyRow: unimplemented interface");
},toObject:function(){
return {};
},getName:function(){
return this._name;
}});
var _7=_1("dojox.grid.enhanced.plugins.filter._DataExpr",_4,{_name:"data",constructor:function(_8,_9,_a){
this._convertArgs=_a||{};
if(_2.isFunction(this._convertArgs.convert)){
this._convertData=_2.hitch(this._convertArgs.scope,this._convertArgs.convert);
}
if(_9){
this._colArg=_8;
}else{
this._value=this._convertData(_8,this._convertArgs);
}
},getValue:function(){
return this._value;
},applyRow:function(_b,_c){
return typeof this._colArg=="undefined"?this:new (_2.getObject(this.declaredClass))(this._convertData(_c(_b,this._colArg),this._convertArgs));
},_convertData:function(_d){
return _d;
},toObject:function(){
return {op:this.getName(),data:this._colArg===undefined?this._value:this._colArg,isCol:this._colArg!==undefined};
}});
var _e=_1("dojox.grid.enhanced.plugins.filter._OperatorExpr",_4,{_name:"operator",constructor:function(){
if(_2.isArray(arguments[0])){
this._operands=arguments[0];
}else{
this._operands=[];
for(var i=0;i<arguments.length;++i){
this._operands.push(arguments[i]);
}
}
},toObject:function(){
return {op:this.getName(),data:_3.map(this._operands,function(_f){
return _f.toObject();
})};
}});
var _10=_1("dojox.grid.enhanced.plugins.filter._UniOpExpr",_e,{_name:"uniOperator",applyRow:function(_11,_12){
if(!(this._operands[0] instanceof _4)){
throw new Error("_UniOpExpr: operand is not expression.");
}
return this._calculate(this._operands[0],_11,_12);
},_calculate:function(_13,_14,_15){
throw new Error("_UniOpExpr._calculate: unimplemented interface");
}});
var _16=_1("dojox.grid.enhanced.plugins.filter._BiOpExpr",_e,{_name:"biOperator",applyRow:function(_17,_18){
if(!(this._operands[0] instanceof _4)){
throw new Error("_BiOpExpr: left operand is not expression.");
}else{
if(!(this._operands[1] instanceof _4)){
throw new Error("_BiOpExpr: right operand is not expression.");
}
}
return this._calculate(this._operands[0],this._operands[1],_17,_18);
},_calculate:function(_19,_1a,_1b,_1c){
throw new Error("_BiOpExpr._calculate: unimplemented interface");
}});
return {_ConditionExpr:_4,_DataExpr:_7,_OperatorExpr:_e,_UniOpExpr:_10,_BiOpExpr:_16};
});

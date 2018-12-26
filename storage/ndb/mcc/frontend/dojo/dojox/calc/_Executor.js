//>>built
define("dojox/calc/_Executor",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/lang","dojo/number","dijit/_base/manager","dijit/_WidgetBase","dijit/_TemplatedMixin","dojox/math/_base"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.calc");
var _9,_a;
var _b=(1<<30)-35;
var _c=_2("dojox.calc._Executor",[_6,_7],{templateString:"<iframe src=\""+require.toUrl("dojox/calc/_ExecutorIframe.html")+"\" style=\"display:none;\" onload=\"if(arguments[0] && arguments[0].Function)"+_5._scopeName+".byNode(this)._onLoad(arguments[0])\"></iframe>",_onLoad:function(_d){
_9=_d;
_d.outerPrompt=window.prompt;
_d.dojox={math:{}};
for(var f in _8){
_d.dojox.math[f]=_3.hitch(_8,f);
}
if("toFrac" in _a){
_d.toFracCall=_3.hitch(_a,"toFrac");
this.Function("toFrac","x","return toFracCall(x)");
}
_d.isJavaScriptLanguage=_4.format(1.5,{pattern:"#.#"})=="1.5";
_d.Ans=0;
_d.pi=Math.PI;
_d.eps=Math.E;
_d.powCall=_3.hitch(_a,"pow");
this.normalizedFunction("sqrt","x","return Math.sqrt(x)");
this.normalizedFunction("sin","x","return Math.sin(x)");
this.normalizedFunction("cos","x","return Math.cos(x)");
this.normalizedFunction("tan","x","return Math.tan(x)");
this.normalizedFunction("asin","x","return Math.asin(x)");
this.normalizedFunction("acos","x","return Math.acos(x)");
this.normalizedFunction("atan","x","return Math.atan(x)");
this.normalizedFunction("atan2","y, x","return Math.atan2(y, x)");
this.normalizedFunction("Round","x","return Math.round(x)");
this.normalizedFunction("Int","x","return Math.floor(x)");
this.normalizedFunction("Ceil","x","return Math.ceil(x)");
this.normalizedFunction("ln","x","return Math.log(x)");
this.normalizedFunction("log","x","return Math.log(x)/Math.log(10)");
this.normalizedFunction("pow","x, y","return powCall(x,y)");
this.normalizedFunction("permutations","n, r","return dojox.math.permutations(n, r);");
this.normalizedFunction("P","n, r","return dojox.math.permutations(n, r);");
this.normalizedFunction("combinations","n, r","return dojox.math.combinations(n, r);");
this.normalizedFunction("C","n, r","return dojox.math.combinations(n, r)");
this.normalizedFunction("toRadix","number, baseOut","if(!baseOut){ baseOut = 10; } if(typeof number == 'string'){ number = parseFloat(number); }return number.toString(baseOut);");
this.normalizedFunction("toBin","number","return toRadix(number, 2)");
this.normalizedFunction("toOct","number","return toRadix(number, 8)");
this.normalizedFunction("toHex","number","return toRadix(number, 16)");
this.onLoad();
},onLoad:function(){
},Function:function(_e,_f,_10){
return _3.hitch(_9,_9.Function.apply(_9,arguments));
},normalizedFunction:function(_11,_12,_13){
return _3.hitch(_9,_9.normalizedFunction.apply(_9,arguments));
},deleteFunction:function(_14){
_9[_14]=undefined;
delete _9[_14];
},eval:function(_15){
return _9.eval.apply(_9,arguments);
},destroy:function(){
this.inherited(arguments);
_9=null;
}});
return _a={pow:function(_16,_17){
function _18(n){
return Math.floor(n)==n;
};
if(_16>=0||_18(_17)){
return Math.pow(_16,_17);
}else{
var inv=1/_17;
return (_18(inv)&&(inv&1))?-Math.pow(-_16,_17):NaN;
}
},approx:function(r){
if(typeof r=="number"){
return Math.round(r*_b)/_b;
}
return r;
},_Executor:_c};
});

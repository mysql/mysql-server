//>>built
define("dijit/form/CurrencyTextBox",["dojo/currency","dojo/_base/declare","dojo/_base/lang","./NumberTextBox"],function(_1,_2,_3,_4){
return _2("dijit.form.CurrencyTextBox",_4,{currency:"",baseClass:"dijitTextBox dijitCurrencyTextBox",pattern:function(_5){
return "("+(this.focused?this.inherited(arguments,[_3.mixin({},_5,this.editOptions)])+"|":"")+_1.regexp(_5)+")";
},_formatter:_1.format,_parser:_1.parse,parse:function(_6,_7){
var v=this.inherited(arguments);
if(isNaN(v)&&/\d+/.test(_6)){
v=_3.hitch(_3.mixin({},this,{_parser:_4.prototype._parser}),"inherited")(arguments);
}
return v;
},_setConstraintsAttr:function(_8){
if(!_8.currency&&this.currency){
_8.currency=this.currency;
}
this.inherited(arguments,[_1._mixInDefaults(_3.mixin(_8,{exponent:false}))]);
}});
});

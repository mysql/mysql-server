//>>built
define("dijit/form/NumberSpinner",["dojo/_base/declare","dojo/_base/event","dojo/keys","./_Spinner","./NumberTextBox"],function(_1,_2,_3,_4,_5){
return _1("dijit.form.NumberSpinner",[_4,_5.Mixin],{adjust:function(_6,_7){
var tc=this.constraints,v=isNaN(_6),_8=!isNaN(tc.max),_9=!isNaN(tc.min);
if(v&&_7!=0){
_6=(_7>0)?_9?tc.min:_8?tc.max:0:_8?this.constraints.max:_9?tc.min:0;
}
var _a=_6+_7;
if(v||isNaN(_a)){
return _6;
}
if(_8&&(_a>tc.max)){
_a=tc.max;
}
if(_9&&(_a<tc.min)){
_a=tc.min;
}
return _a;
},_onKeyPress:function(e){
if((e.charOrCode==_3.HOME||e.charOrCode==_3.END)&&!(e.ctrlKey||e.altKey||e.metaKey)&&typeof this.get("value")!="undefined"){
var _b=this.constraints[(e.charOrCode==_3.HOME?"min":"max")];
if(typeof _b=="number"){
this._setValueAttr(_b,false);
}
_2.stop(e);
}
}});
});

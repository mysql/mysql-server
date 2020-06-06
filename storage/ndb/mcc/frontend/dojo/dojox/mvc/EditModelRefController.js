//>>built
define("dojox/mvc/EditModelRefController",["dojo/_base/declare","dojo/_base/lang","./getPlainValue","./getStateful","./ModelRefController"],function(_1,_2,_3,_4,_5){
function _6(_7,_8,_9){
if(_8!==_9){
_7.set(_7._refOriginalModelProp,_7.holdModelUntilCommit?_9:_7.cloneModel(_9));
_7.set(_7._refEditModelProp,_7.holdModelUntilCommit?_7.cloneModel(_9):_9);
}
};
return _1("dojox.mvc.EditModelRefController",_5,{getStatefulOptions:null,getPlainValueOptions:null,holdModelUntilCommit:false,originalModel:null,sourceModel:null,_refOriginalModelProp:"originalModel",_refSourceModelProp:"sourceModel",_refEditModelProp:"model",postscript:function(_a,_b){
for(var s in {getStatefulOptions:1,getPlainValueOptions:1,holdModelUntilCommit:1}){
var _c=(_a||{})[s];
if(typeof _c!="undefined"){
this[s]=_c;
}
}
this.inherited(arguments);
},set:function(_d,_e){
if(_d==this._refSourceModelProp){
_6(this,this[this._refSourceModelProp],_e);
}
this.inherited(arguments);
},cloneModel:function(_f){
var _10=_2.isFunction((_f||{}).set)&&_2.isFunction((_f||{}).watch)?_3(_f,this.getPlainValueOptions):_f;
return _4(_10,this.getStatefulOptions);
},commit:function(){
this.set(this.holdModelUntilCommit?this._refSourceModelProp:this._refOriginalModelProp,this.cloneModel(this.get(this._refEditModelProp)));
},reset:function(){
this.set(this.holdModelUntilCommit?this._refEditModelProp:this._refSourceModelProp,this.cloneModel(this.get(this._refOriginalModelProp)));
},hasControllerProperty:function(_11){
return this.inherited(arguments)||_11==this._refOriginalModelProp||_11==this._refSourceModelProp;
}});
});

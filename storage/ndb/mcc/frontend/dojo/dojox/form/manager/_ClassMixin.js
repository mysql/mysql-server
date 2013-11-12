//>>built
define("dojox/form/manager/_ClassMixin",["dojo/_base/lang","dojo/_base/kernel","dojo/dom-class","./_Mixin","dojo/_base/declare"],function(_1,_2,_3,_4,_5){
var fm=_1.getObject("dojox.form.manager",true),aa=fm.actionAdapter,ia=fm.inspectorAdapter;
return _5("dojox.form.manager._ClassMixin",null,{gatherClassState:function(_6,_7){
var _8=this.inspect(ia(function(_9,_a){
return _3.contains(_a,_6);
}),_7);
return _8;
},addClass:function(_b,_c){
this.inspect(aa(function(_d,_e){
_3.add(_e,_b);
}),_c);
return this;
},removeClass:function(_f,_10){
this.inspect(aa(function(_11,_12){
_3.remove(_12,_f);
}),_10);
return this;
}});
});

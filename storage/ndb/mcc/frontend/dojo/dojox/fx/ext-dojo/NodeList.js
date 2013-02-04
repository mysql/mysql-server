//>>built
define("dojox/fx/ext-dojo/NodeList",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/fx","dojox/fx","dojo/NodeList-fx"],function(_1,_2,_3,_4,_5){
_1.experimental("dojox.fx.ext-dojo.NodeList");
_2.extend(_5,{sizeTo:function(_6){
return this._anim(_4,"sizeTo",_6);
},slideBy:function(_7){
return this._anim(_4,"slideBy",_7);
},highlight:function(_8){
return this._anim(_4,"highlight",_8);
},fadeTo:function(_9){
return this._anim(_3,"_fade",_9);
},wipeTo:function(_a){
return this._anim(_4,"wipeTo",_a);
}});
return _5;
});

//>>built
define("dojox/mvc/at",["dojo/_base/kernel","dojo/_base/lang","./sync","./_atBindingExtension"],function(_1,_2,_3){
_1.experimental("dojox.mvc");
var at=function(_4,_5){
return {atsignature:"dojox.mvc.at",target:_4,targetProp:_5,bindDirection:_3.both,direction:function(_6){
this.bindDirection=_6;
return this;
},transform:function(_7){
this.converter=_7;
return this;
}};
};
at.from=_3.from;
at.to=_3.to;
at.both=_3.both;
return _2.setObject("dojox.mvc.at",at);
});

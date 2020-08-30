//>>built
define("dojox/mobile/_PickerChooser",["dojo/_base/lang","dojo/_base/window"],function(_1,_2){
return {load:function(id,_3,_4){
var dm=_2.global._no_dojo_dm||_1.getObject("dojox.mobile",true);
_3([(dm.currentTheme==="android"||dm.currentTheme==="holodark"?"./ValuePicker":"./SpinWheel")+id],_4);
}};
});

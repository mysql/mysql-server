//>>built
define("dijit/_editor/plugins/TextColor",["require","dojo/colors","dojo/_base/declare","dojo/_base/lang","../_Plugin","../../form/DropDownButton"],function(_1,_2,_3,_4,_5,_6){
var _7=_3("dijit._editor.plugins.TextColor",_5,{buttonClass:_6,colorPicker:"dijit/ColorPalette",useDefaultCommand:false,_initButton:function(){
this.command=this.name;
this.inherited(arguments);
var _8=this;
this.button.loadDropDown=function(_9){
function _a(_b){
_8.button.dropDown=new _b({dir:_8.editor.dir,ownerDocument:_8.editor.ownerDocument,value:_8.value,onChange:function(_c){
_8.editor.execCommand(_8.command,_c);
},onExecute:function(){
_8.editor.execCommand(_8.command,this.get("value"));
}});
_9();
};
if(typeof _8.colorPicker=="string"){
_1([_8.colorPicker],_a);
}else{
_a(_8.colorPicker);
}
};
},updateState:function(){
var _d=this.editor;
var _e=this.command;
if(!_d||!_d.isLoaded||!_e.length){
return;
}
if(this.button){
var _f=this.get("disabled");
this.button.set("disabled",_f);
if(_f){
return;
}
var _10;
try{
_10=_d.queryCommandValue(_e)||"";
}
catch(e){
_10="";
}
}
if(_10==""){
_10="#000000";
}
if(_10=="transparent"){
_10="#ffffff";
}
if(typeof _10=="string"){
if(_10.indexOf("rgb")>-1){
_10=_2.fromRgb(_10).toHex();
}
}else{
_10=((_10&255)<<16)|(_10&65280)|((_10&16711680)>>>16);
_10=_10.toString(16);
_10="#000000".slice(0,7-_10.length)+_10;
}
this.value=_10;
var _11=this.button.dropDown;
if(_11&&_11.get&&_10!==_11.get("value")){
_11.set("value",_10,false);
}
}});
_5.registry["foreColor"]=function(_12){
return new _7(_12);
};
_5.registry["hiliteColor"]=function(_13){
return new _7(_13);
};
return _7;
});

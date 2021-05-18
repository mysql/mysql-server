//>>built
define("dojox/form/TimeSpinner",["dojo/_base/lang","dojo/_base/event","dijit/form/_Spinner","dojo/keys","dojo/date","dojo/date/locale","dojo/date/stamp","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _8("dojox.form.TimeSpinner",_3,{required:false,adjust:function(_9,_a){
return _5.add(_9,"minute",_a);
},isValid:function(){
return true;
},smallDelta:5,largeDelta:30,timeoutChangeRate:0.5,parse:function(_b,_c){
return _6.parse(_b,{selector:"time",formatLength:"short"});
},format:function(_d,_e){
if(_1.isString(_d)){
return _d;
}
return _6.format(_d,{selector:"time",formatLength:"short"});
},serialize:_7.toISOString,value:"12:00 AM",_onKeyDown:function(e){
if((e.keyCode==_4.HOME||e.keyCode==_4.END)&&!(e.ctrlKey||e.altKey||e.metaKey)&&typeof this.get("value")!="undefined"){
var _f=this.constraints[(e.keyCode==_4.HOME?"min":"max")];
if(_f){
this._setValueAttr(_f,true);
}
_2.stop(e);
}
}});
});

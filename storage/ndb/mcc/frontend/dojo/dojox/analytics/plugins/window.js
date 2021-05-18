//>>built
define("dojox/analytics/plugins/window",["dojo/_base/lang","../_base","dojo/ready","dojo/_base/config","dojo/aspect"],function(_1,_2,_3,_4,_5){
return (_2.plugins.window=new (function(){
this.addData=_1.hitch(_2,"addData","window");
this.windowConnects=_4["windowConnects"]||["open","onerror"];
for(var i=0;i<this.windowConnects.length;i++){
_5.after(window,this.windowConnects[i],_1.hitch(this,"addData",this.windowConnects[i]),true);
}
_3(_1.hitch(this,function(){
var _6={};
for(var i in window){
if(typeof window[i]=="object"||typeof window[i]=="function"){
switch(i){
case "location":
case "console":
_6[i]=window[i];
break;
default:
break;
}
}else{
_6[i]=window[i];
}
}
this.addData(_6);
}));
})());
});

//>>built
define("dojox/analytics/plugins/idle",["dojo/_base/lang","../_base","dojo/_base/config","dojo/ready","dojo/aspect","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6){
return (_2.plugins.idle=new (function(){
this.addData=_1.hitch(_2,"addData","idle");
this.idleTime=_3["idleTime"]||60000;
this.idle=true;
this.setIdle=function(){
this.addData("isIdle");
this.idle=true;
};
_4(_1.hitch(this,function(){
var _7=["onmousemove","onkeydown","onclick","onscroll"];
for(var i=0;i<_7.length;i++){
_5.after(_6.doc,_7[i],_1.hitch(this,function(e){
if(this.idle){
this.idle=false;
this.addData("isActive");
this.idleTimer=setTimeout(_1.hitch(this,"setIdle"),this.idleTime);
}else{
clearTimeout(this.idleTimer);
this.idleTimer=setTimeout(_1.hitch(this,"setIdle"),this.idleTime);
}
}),true);
}
}));
})());
});

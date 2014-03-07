//>>built
define("dojox/analytics/plugins/dojo",["dojo/_base/lang","../_base","dojo/_base/config","dojo/ready"],function(_1,_2,_3,_4){
var _5=_1.getObject("dojox.analytics.plugins",true);
return (_5.dojo=new (function(){
this.addData=_1.hitch(_2,"addData","dojo");
_4(_1.hitch(this,function(){
var _6={};
for(var i in dojo){
if((i=="version")||((!(typeof dojo[i]=="object"||typeof dojo[i]=="function"))&&(i[0]!="_"))){
_6[i]=dojo[i];
}
}
if(_3){
_6.djConfig=_3;
}
this.addData(_6);
}));
})());
});

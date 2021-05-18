//>>built
define("dojox/mobile/dh/UrlDataSource",["dojo/_base/declare","dojo/_base/lang","dojo/_base/xhr"],function(_1,_2,_3){
return _1("dojox.mobile.dh.UrlDataSource",null,{text:"",_url:"",constructor:function(_4){
this._url=_4;
},getData:function(){
var _5=_3.get({url:this._url,handleAs:"text"});
_5.addCallback(_2.hitch(this,function(_6,_7){
this.text=_6;
}));
_5.addErrback(function(_8){
});
return _5;
}});
});

//>>built
define("dojox/app/module/lifecycle",["dojo/_base/declare","dojo/_base/connect"],function(_1,_2){
return _1(null,{lifecycle:{UNKNOWN:0,STARTING:1,STARTED:2,STOPPING:3,STOPPED:4},_status:0,getStatus:function(){
return this._status;
},setStatus:function(_3){
this._status=_3;
_2.publish("/app/status",[_3]);
}});
});

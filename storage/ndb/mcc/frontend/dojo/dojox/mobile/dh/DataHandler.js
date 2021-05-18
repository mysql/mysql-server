//>>built
define("dojox/mobile/dh/DataHandler",["dojo/_base/declare","dojo/_base/lang","dojo/_base/Deferred","./ContentTypeMap"],function(_1,_2,_3,_4){
return _1("dojox.mobile.dh.DataHandler",null,{ds:null,target:null,refNode:null,constructor:function(ds,_5,_6){
this.ds=ds;
this.target=_5;
this.refNode=_6;
},processData:function(_7,_8){
var ch=_4.getHandlerClass(_7);
require([ch],_2.hitch(this,function(_9){
_3.when(this.ds.getData(),_2.hitch(this,function(){
_3.when(new _9().parse(this.ds.text,this.target,this.refNode),function(id){
_8(id);
});
}));
}));
}});
});

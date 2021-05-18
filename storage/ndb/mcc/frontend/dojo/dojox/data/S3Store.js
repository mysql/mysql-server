//>>built
define("dojox/data/S3Store",["dojo/_base/declare","dojox/data/JsonRestStore","dojox/rpc/ProxiedPath"],function(_1,_2,_3){
return _1("dojox.data.S3Store",_2,{_processResults:function(_4){
var _5=_4.getElementsByTagName("Key");
var _6=[];
var _7=this;
for(var i=0;i<_5.length;i++){
var _8=_5[i];
var _9={_loadObject:(function(_a,_b){
return function(_c){
delete this._loadObject;
_7.service(_a).addCallback(_c);
};
})(_8.firstChild.nodeValue,_9)};
_6.push(_9);
}
return {totalCount:_6.length,items:_6};
}});
});

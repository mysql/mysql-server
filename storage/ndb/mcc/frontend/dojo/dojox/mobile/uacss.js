//>>built
define("dojox/mobile/uacss",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/window","./sniff"],function(_1,_2,_3,_4){
var _5=_3.doc.documentElement;
_5.className=_2.trim(_5.className+" "+[_4("bb")?"dj_bb":"",_4("android")?"dj_android":"",_4("ios")?"dj_ios":"",_4("ios")>=6?"dj_ios6":"",_4("ios")?"dj_iphone":"",_4("ipod")?"dj_ipod":"",_4("ipad")?"dj_ipad":"",_4("ie")?"dj_ie":""].join(" ").replace(/ +/g," "));
return _1;
});

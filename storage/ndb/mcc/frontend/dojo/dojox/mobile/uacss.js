//>>built
define("dojox/mobile/uacss",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/window","dojox/mobile/sniff"],function(_1,_2,_3,_4){
_3.doc.documentElement.className+=_2.trim([_4("bb")?"dj_bb":"",_4("android")?"dj_android":"",_4("iphone")?"dj_iphone":"",_4("ipod")?"dj_ipod":"",_4("ipad")?"dj_ipad":""].join(" ").replace(/ +/g," "));
return _1;
});

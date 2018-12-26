//>>built
define("dojox/validate/web",["./_base","./regexp"],function(_1,_2){
_1.isIpAddress=function(_3,_4){
var re=new RegExp("^"+_2.ipAddress(_4)+"$","i");
return re.test(_3);
};
_1.isUrl=function(_5,_6){
var re=new RegExp("^"+_2.url(_6)+"$","i");
return re.test(_5);
};
_1.isEmailAddress=function(_7,_8){
var re=new RegExp("^"+_2.emailAddress(_8)+"$","i");
return re.test(_7);
};
_1.isEmailAddressList=function(_9,_a){
var re=new RegExp("^"+_2.emailAddressList(_a)+"$","i");
return re.test(_9);
};
_1.getEmailAddressList=function(_b,_c){
if(!_c){
_c={};
}
if(!_c.listSeparator){
_c.listSeparator="\\s;,";
}
if(_1.isEmailAddressList(_b,_c)){
return _b.split(new RegExp("\\s*["+_c.listSeparator+"]\\s*"));
}
return [];
};
return _1;
});

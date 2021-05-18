//>>built
define("dojox/validate/us",["dojo/_base/lang","./_base","./regexp"],function(_1,_2,_3){
var us=_1.getObject("us",true,_2);
us.isState=function(_4,_5){
var re=new RegExp("^"+_3.us.state(_5)+"$","i");
return re.test(_4);
};
us.isPhoneNumber=function(_6){
var _7={format:["###-###-####","(###) ###-####","(###) ### ####","###.###.####","###/###-####","### ### ####","###-###-#### x#???","(###) ###-#### x#???","(###) ### #### x#???","###.###.#### x#???","###/###-#### x#???","### ### #### x#???","##########"]};
return _2.isNumberFormat(_6,_7);
};
us.isSocialSecurityNumber=function(_8){
var _9={format:["###-##-####","### ## ####","#########"]};
return _2.isNumberFormat(_8,_9);
};
us.isZipCode=function(_a){
var _b={format:["#####-####","##### ####","#########","#####"]};
return _2.isNumberFormat(_a,_b);
};
return us;
});

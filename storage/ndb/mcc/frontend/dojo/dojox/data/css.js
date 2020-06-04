//>>built
define("dojox/data/css",["dojo/_base/lang","dojo/_base/array"],function(_1,_2){
var _3=_1.getObject("dojox.data.css",true);
_3.rules={};
_3.rules.forEach=function(fn,_4,_5){
if(_5){
var _6=function(_7){
_2.forEach(_7[_7.cssRules?"cssRules":"rules"],function(_8){
if(!_8.type||_8.type!==3){
var _9="";
if(_7&&_7.href){
_9=_7.href;
}
fn.call(_4?_4:this,_8,_7,_9);
}
});
};
_2.forEach(_5,_6);
}
};
_3.findStyleSheets=function(_a){
var _b=[];
var _c=function(_d){
var s=_3.findStyleSheet(_d);
if(s){
_2.forEach(s,function(_e){
if(_2.indexOf(_b,_e)===-1){
_b.push(_e);
}
});
}
};
_2.forEach(_a,_c);
return _b;
};
_3.findStyleSheet=function(_f){
var _10=[];
if(_f.charAt(0)==="."){
_f=_f.substring(1);
}
var _11=function(_12){
if(_12.href&&_12.href.match(_f)){
_10.push(_12);
return true;
}
if(_12.imports){
return _2.some(_12.imports,function(_13){
return _11(_13);
});
}
return _2.some(_12[_12.cssRules?"cssRules":"rules"],function(_14){
if(_14.type&&_14.type===3&&_11(_14.styleSheet)){
return true;
}
return false;
});
};
_2.some(document.styleSheets,_11);
return _10;
};
_3.determineContext=function(_15){
var ret=[];
if(_15&&_15.length>0){
_15=_3.findStyleSheets(_15);
}else{
_15=document.styleSheets;
}
var _16=function(_17){
ret.push(_17);
if(_17.imports){
_2.forEach(_17.imports,function(_18){
_16(_18);
});
}
_2.forEach(_17[_17.cssRules?"cssRules":"rules"],function(_19){
if(_19.type&&_19.type===3){
_16(_19.styleSheet);
}
});
};
_2.forEach(_15,_16);
return ret;
};
return _3;
});

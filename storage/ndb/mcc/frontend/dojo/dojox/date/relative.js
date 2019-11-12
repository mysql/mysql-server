//>>built
define("dojox/date/relative",["..","dojo/_base/lang","dojo/date/locale","dojo/i18n"],function(_1,_2,_3,_4){
var _5=_2.getObject("date.relative",true,_1);
var _6=1000*60*60*24,_7=6*_6,_8=dojo.delegate,_9=_3._getGregorianBundle,_a=_3.format;
function _b(_c){
_c=new Date(_c);
_c.setHours(0,0,0,0);
return _c;
};
_5.format=function(_d,_e){
_e=_e||{};
var _f=_b(_e.relativeDate||new Date()),_10=_f.getTime()-_b(_d).getTime(),_11={locale:_e.locale};
if(_10===0){
return _a(_d,_8(_11,{selector:"time"}));
}else{
if(_10<=_7&&_10>0&&_e.weekCheck!==false){
return _a(_d,_8(_11,{selector:"date",datePattern:"EEE"}))+" "+_a(_d,_8(_11,{selector:"time",formatLength:"short"}));
}else{
if(_d.getFullYear()==_f.getFullYear()){
var _12=_9(_4.normalizeLocale(_e.locale));
return _a(_d,_8(_11,{selector:"date",datePattern:_12["dateFormatItem-MMMd"]}));
}else{
return _a(_d,_8(_11,{selector:"date",formatLength:"medium",locale:_e.locale}));
}
}
}
};
return _5;
});

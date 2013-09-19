//>>built
define("dojox/date/relative",["dojo/_base/kernel","dojo/_base/lang","dojo/date/locale","dojo/i18n"],function(_1,_2,_3,_4){
_1.getObject("date.relative",true,dojox);
var _5=1000*60*60*24,_6=6*_5,_7=_1.delegate,_8=_3._getGregorianBundle,_9=_3.format;
function _a(_b){
_b=new Date(_b);
_b.setHours(0,0,0,0);
return _b;
};
dojox.date.relative.format=function(_c,_d){
_d=_d||{};
var _e=_a(_d.relativeDate||new Date()),_f=_e.getTime()-_a(_c).getTime(),_10={locale:_d.locale};
if(_f===0){
return _9(_c,_7(_10,{selector:"time"}));
}else{
if(_f<=_6&&_f>0&&_d.weekCheck!==false){
return _9(_c,_7(_10,{selector:"date",datePattern:"EEE"}))+" "+_9(_c,_7(_10,{selector:"time",formatLength:"short"}));
}else{
if(_c.getFullYear()==_e.getFullYear()){
var _11=_8(_4.normalizeLocale(_d.locale));
return _9(_c,_7(_10,{selector:"date",datePattern:_11["dateFormatItem-MMMd"]}));
}else{
return _9(_c,_7(_10,{selector:"date",formatLength:"medium",locale:_d.locale}));
}
}
}
};
});

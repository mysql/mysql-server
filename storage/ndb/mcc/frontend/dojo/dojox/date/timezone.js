//>>built
define("dojox/date/timezone",["dojo/_base/array","dojo/_base/config","dojo/_base/declare","dojo/_base/kernel","dojo/_base/lang","dojo/date","dojo/date/locale","dojo/request","dojo/request/handlers"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_4.experimental("dojox.date.timezone");
var _a=["africa","antarctica","asia","australasia","backward","etcetera","europe","northamerica","pacificnew","southamerica"];
var _b=1835,_c=2038;
var _d={},_e={},_f={},_10={};
var _11=_2.timezoneLoadingScheme||"preloadAll";
var _12=_2.defaultZoneFile||((_11=="preloadAll")?_a:"northamerica");
_9.register("olson_zoneinfo",function(_13){
var _14=_13.text,s="",_15=_14.split("\n"),arr=[],_16="",_17=null,_18=null,ret={zones:{},rules:{}};
for(var i=0;i<_15.length;i++){
var l=_15[i];
if(l.match(/^\s/)){
l="Zone "+_17+l;
}
l=l.split("#")[0];
if(l.length>3){
arr=l.split(/\s+/);
_16=arr.shift();
switch(_16){
case "Zone":
_17=arr.shift();
if(arr[0]){
if(!ret.zones[_17]){
ret.zones[_17]=[];
}
ret.zones[_17].push(arr);
}
break;
case "Rule":
_18=arr.shift();
if(!ret.rules[_18]){
ret.rules[_18]=[];
}
ret.rules[_18].push(arr);
break;
case "Link":
if(ret.zones[arr[1]]){
throw new Error("Error with Link "+arr[1]);
}
ret.zones[arr[1]]=arr[0];
break;
case "Leap":
break;
default:
break;
}
}
}
return ret;
});
function _19(_1a){
_1a=_1a||{};
_e=_5.mixin(_e,_1a.zones||{});
_10=_5.mixin(_10,_1a.rules||{});
};
function _1b(_1c){
_d[_1c]=true;
_8.get(require.toUrl((_2.timezoneFileBasePath||"dojox/date/zoneinfo")+"/"+_1c),{handleAs:"olson_zoneinfo",sync:true}).then(_19,function(e){
console.error("Error loading zone file:",e);
throw e;
});
};
var _1d={"jan":0,"feb":1,"mar":2,"apr":3,"may":4,"jun":5,"jul":6,"aug":7,"sep":8,"oct":9,"nov":10,"dec":11},_1e={"sun":0,"mon":1,"tue":2,"wed":3,"thu":4,"fri":5,"sat":6},_1f={"EST":"northamerica","MST":"northamerica","HST":"northamerica","EST5EDT":"northamerica","CST6CDT":"northamerica","MST7MDT":"northamerica","PST8PDT":"northamerica","America":"northamerica","Pacific":"australasia","Atlantic":"europe","Africa":"africa","Indian":"africa","Antarctica":"antarctica","Asia":"asia","Australia":"australasia","Europe":"europe","WET":"europe","CET":"europe","MET":"europe","EET":"europe"},_20={"Pacific/Honolulu":"northamerica","Atlantic/Bermuda":"northamerica","Atlantic/Cape_Verde":"africa","Atlantic/St_Helena":"africa","Indian/Kerguelen":"antarctica","Indian/Chagos":"asia","Indian/Maldives":"asia","Indian/Christmas":"australasia","Indian/Cocos":"australasia","America/Danmarkshavn":"europe","America/Scoresbysund":"europe","America/Godthab":"europe","America/Thule":"europe","Asia/Yekaterinburg":"europe","Asia/Omsk":"europe","Asia/Novosibirsk":"europe","Asia/Krasnoyarsk":"europe","Asia/Irkutsk":"europe","Asia/Yakutsk":"europe","Asia/Vladivostok":"europe","Asia/Sakhalin":"europe","Asia/Magadan":"europe","Asia/Kamchatka":"europe","Asia/Anadyr":"europe","Africa/Ceuta":"europe","America/Argentina/Buenos_Aires":"southamerica","America/Argentina/Cordoba":"southamerica","America/Argentina/Tucuman":"southamerica","America/Argentina/La_Rioja":"southamerica","America/Argentina/San_Juan":"southamerica","America/Argentina/Jujuy":"southamerica","America/Argentina/Catamarca":"southamerica","America/Argentina/Mendoza":"southamerica","America/Argentina/Rio_Gallegos":"southamerica","America/Argentina/Ushuaia":"southamerica","America/Aruba":"southamerica","America/La_Paz":"southamerica","America/Noronha":"southamerica","America/Belem":"southamerica","America/Fortaleza":"southamerica","America/Recife":"southamerica","America/Araguaina":"southamerica","America/Maceio":"southamerica","America/Bahia":"southamerica","America/Sao_Paulo":"southamerica","America/Campo_Grande":"southamerica","America/Cuiaba":"southamerica","America/Porto_Velho":"southamerica","America/Boa_Vista":"southamerica","America/Manaus":"southamerica","America/Eirunepe":"southamerica","America/Rio_Branco":"southamerica","America/Santiago":"southamerica","Pacific/Easter":"southamerica","America/Bogota":"southamerica","America/Curacao":"southamerica","America/Guayaquil":"southamerica","Pacific/Galapagos":"southamerica","Atlantic/Stanley":"southamerica","America/Cayenne":"southamerica","America/Guyana":"southamerica","America/Asuncion":"southamerica","America/Lima":"southamerica","Atlantic/South_Georgia":"southamerica","America/Paramaribo":"southamerica","America/Port_of_Spain":"southamerica","America/Montevideo":"southamerica","America/Caracas":"southamerica"},_21={"US":"S","Chatham":"S","NZ":"S","NT_YK":"S","Edm":"S","Salv":"S","Canada":"S","StJohns":"S","TC":"S","Guat":"S","Mexico":"S","Haiti":"S","Barb":"S","Belize":"S","CR":"S","Moncton":"S","Swift":"S","Hond":"S","Thule":"S","NZAQ":"S","Zion":"S","ROK":"S","PRC":"S","Taiwan":"S","Ghana":"GMT","SL":"WAT","Chicago":"S","Detroit":"S","Vanc":"S","Denver":"S","Halifax":"S","Cuba":"S","Indianapolis":"S","Starke":"S","Marengo":"S","Pike":"S","Perry":"S","Vincennes":"S","Pulaski":"S","Louisville":"S","CA":"S","Nic":"S","Menominee":"S","Mont":"S","Bahamas":"S","NYC":"S","Regina":"S","Resolute":"ES","DR":"S","Toronto":"S","Winn":"S"};
function _22(t){
throw new Error("Timezone \""+t+"\" is either incorrect, or not loaded in the timezone registry.");
};
function _23(tz){
var ret=_20[tz];
if(!ret){
var reg=tz.split("/")[0];
ret=_1f[reg];
if(!ret){
var _24=_e[tz];
if(typeof _24=="string"){
return _23(_24);
}else{
if(!_d.backward){
_1b("backward");
return _23(tz);
}else{
_22(tz);
}
}
}
}
return ret;
};
function _25(str){
var pat=/(\d+)(?::0*(\d*))?(?::0*(\d*))?([su])?$/;
var hms=str.match(pat);
if(!hms){
return null;
}
hms[1]=parseInt(hms[1],10);
hms[2]=hms[2]?parseInt(hms[2],10):0;
hms[3]=hms[3]?parseInt(hms[3],10):0;
return hms;
};
function _26(y,m,d,h,mn,s,off){
return Date.UTC(y,m,d,h,mn,s)+((off||0)*60*1000);
};
function _27(m){
return _1d[m.substr(0,3).toLowerCase()];
};
function _28(str){
var off=_25(str);
if(off===null){
return 0;
}
var adj=str.indexOf("-")===0?-1:1;
off=adj*(((off[1]*60+off[2])*60+off[3])*1000);
return -off/60/1000;
};
function _29(_2a,_2b,off){
var _2c=_27(_2a[3]),day=_2a[4],_2d=_25(_2a[5]);
if(_2d[4]=="u"){
off=0;
}
var d,_2e,_2f;
if(isNaN(day)){
if(day.substr(0,4)=="last"){
day=_1e[day.substr(4,3).toLowerCase()];
d=new Date(_26(_2b,_2c+1,1,_2d[1]-24,_2d[2],_2d[3],off));
_2e=_6.add(d,"minute",-off).getUTCDay();
_2f=(day>_2e)?(day-_2e-7):(day-_2e);
if(_2f!==0){
d=_6.add(d,"hour",_2f*24);
}
return d;
}else{
day=_1e[day.substr(0,3).toLowerCase()];
if(day!="undefined"){
if(_2a[4].substr(3,2)==">="){
d=new Date(_26(_2b,_2c,parseInt(_2a[4].substr(5),10),_2d[1],_2d[2],_2d[3],off));
_2e=_6.add(d,"minute",-off).getUTCDay();
_2f=(day<_2e)?(day-_2e+7):(day-_2e);
if(_2f!==0){
d=_6.add(d,"hour",_2f*24);
}
return d;
}else{
if(day.substr(3,2)=="<="){
d=new Date(_26(_2b,_2c,parseInt(_2a[4].substr(5),10),_2d[1],_2d[2],_2d[3],off));
_2e=_6.add(d,"minute",-off).getUTCDay();
_2f=(day>_2e)?(day-_2e-7):(day-_2e);
if(_2f!==0){
d=_6.add(d,"hour",_2f*24);
}
return d;
}
}
}
}
}else{
d=new Date(_26(_2b,_2c,parseInt(day,10),_2d[1],_2d[2],_2d[3],off));
return d;
}
return null;
};
function _30(_31,_32){
var _33=[];
_1.forEach(_10[_31[1]]||[],function(r){
for(var i=0;i<2;i++){
switch(r[i]){
case "min":
r[i]=_b;
break;
case "max":
r[i]=_c;
break;
case "only":
break;
default:
r[i]=parseInt(r[i],10);
if(isNaN(r[i])){
throw new Error("Invalid year found on rule");
}
break;
}
}
if(typeof r[6]=="string"){
r[6]=_28(r[6]);
}
if((r[0]<=_32&&r[1]>=_32)||(r[0]==_32&&r[1]=="only")){
_33.push({r:r,d:_29(r,_32,_31[0])});
}
});
return _33;
};
function _34(tz,_35){
var zr=_f[tz]=[];
for(var i=0;i<_35.length;i++){
var z=_35[i];
var r=zr[i]=[];
var _36=null;
var _37=null;
var _38=[];
if(typeof z[0]=="string"){
z[0]=_28(z[0]);
}
if(i===0){
r[0]=Date.UTC(_b,0,1,0,0,0,0);
}else{
r[0]=zr[i-1][1];
_36=_35[i-1];
_37=zr[i-1];
_38=_37[2];
}
var _39=new Date(r[0]).getUTCFullYear();
var _3a=z[3]?parseInt(z[3],10):_c;
var rlz=[];
var j;
for(j=_39;j<=_3a;j++){
rlz=rlz.concat(_30(z,j));
}
rlz.sort(function(a,b){
return _6.compare(a.d,b.d);
});
var rl;
for(j=0,rl;(rl=rlz[j]);j++){
var _3b=j>0?rlz[j-1]:null;
if(rl.r[5].indexOf("u")<0&&rl.r[5].indexOf("s")<0){
if(j===0&&i>0){
if(_38.length){
rl.d=_6.add(rl.d,"minute",_38[_38.length-1].r[6]);
}else{
if(_6.compare(new Date(_37[1]),rl.d,"date")===0){
rl.d=new Date(_37[1]);
}else{
rl.d=_6.add(rl.d,"minute",_28(_36[1]));
}
}
}else{
if(j>0){
rl.d=_6.add(rl.d,"minute",_3b.r[6]);
}
}
}
}
r[2]=rlz;
if(!z[3]){
r[1]=Date.UTC(_c,11,31,23,59,59,999);
}else{
var _3c=parseInt(z[3],10),_3d=_27(z[4]||"Jan"),day=parseInt(z[5]||"1",10),_3e=_25(z[6]||"0");
var _3f=r[1]=_26(_3c,_3d,day,_3e[1],_3e[2],_3e[3],((_3e[4]=="u")?0:z[0]));
if(isNaN(_3f)){
_3f=r[1]=_29([0,0,0,z[4],z[5],z[6]||"0"],_3c,((_3e[4]=="u")?0:z[0])).getTime();
}
var _40=_1.filter(rlz,function(rl,idx){
var o=idx>0?rlz[idx-1].r[6]*60*1000:0;
return (rl.d.getTime()<_3f+o);
});
if(_3e[4]!="u"&&_3e[4]!="s"){
if(_40.length){
r[1]+=_40[_40.length-1].r[6]*60*1000;
}else{
r[1]+=_28(z[1])*60*1000;
}
}
}
}
};
function _41(dt,tz){
var t=tz;
var _42=_e[t];
while(typeof _42=="string"){
t=_42;
_42=_e[t];
}
if(!_42){
if(!_d.backward){
var _43=_1b("backward",true);
return _41(dt,tz);
}
_22(t);
}
if(!_f[tz]){
_34(tz,_42);
}
var _44=_f[tz];
var tm=dt.getTime();
for(var i=0,r;(r=_44[i]);i++){
if(tm>=r[0]&&tm<r[1]){
return {zone:_42[i],range:_44[i],idx:i};
}
}
throw new Error("No Zone found for \""+tz+"\" on "+dt);
};
function _45(dt,_46){
var _47=-1;
var _48=_46.range[2]||[];
var tsp=dt.getTime();
var zr=_46.range;
for(var i=0,r;(r=_48[i]);i++){
if(tsp>=r.d.getTime()){
_47=i;
}
}
if(_47>=0){
return _48[_47].r;
}
return null;
};
function _49(tz,_4a,_4b){
var res;
var _4c=_4a.zone;
var _4d=_4c[2];
if(_4d.indexOf("%s")>-1){
var _4e;
if(_4b){
_4e=_4b[7];
if(_4e=="-"){
_4e="";
}
}else{
if(_4c[1] in _21){
_4e=_21[_4c[1]];
}else{
if(_4a.idx>0){
var pz=_e[tz][_4a.idx-1];
var pb=pz[2];
if(pb.indexOf("%s")<0){
if(_4d.replace("%s","S")==pb){
_4e="S";
}else{
_4e="";
}
}else{
_4e="";
}
}else{
_4e="";
}
}
}
res=_4d.replace("%s",_4e);
}else{
if(_4d.indexOf("/")>-1){
var bs=_4d.split("/");
if(_4b){
res=bs[_4b[6]===0?0:1];
}else{
res=bs[0];
}
}else{
res=_4d;
}
}
return res;
};
_5.setObject("dojox.date.timezone",{getTzInfo:function(dt,tz){
if(_11=="lazyLoad"){
var _4f=_23(tz);
if(!_4f){
throw new Error("Not a valid timezone ID.");
}else{
if(!_d[_4f]){
_1b(_4f);
}
}
}
var _50=_41(dt,tz);
var off=_50.zone[0];
var _51=_45(dt,_50);
if(_51){
off+=_51[6];
}else{
if(_10[_50.zone[1]]&&_50.idx>0){
off+=_28(_e[tz][_50.idx-1][1]);
}else{
off+=_28(_50.zone[1]);
}
}
var _52=_49(tz,_50,_51);
return {tzOffset:off,tzAbbr:_52};
},loadZoneData:function(_53){
_19(_53);
},getAllZones:function(){
var arr=[];
for(var z in _e){
arr.push(z);
}
arr.sort();
return arr;
}});
if(typeof _12=="string"&&_12){
_12=[_12];
}
if(_12 instanceof Array){
_1.forEach(_12,_1b);
}
var _54=_7.format,_55=_7._getZone;
_7.format=function(_56,_57){
_57=_57||{};
if(_57.timezone&&!_57._tzInfo){
_57._tzInfo=dojox.date.timezone.getTzInfo(_56,_57.timezone);
}
if(_57._tzInfo){
var _58=_56.getTimezoneOffset()-_57._tzInfo.tzOffset;
_56=new Date(_56.getTime()+(_58*60*1000));
}
return _54.call(this,_56,_57);
};
_7._getZone=function(_59,_5a,_5b){
if(_5b._tzInfo){
return _5a?_5b._tzInfo.tzAbbr:_5b._tzInfo.tzOffset;
}
return _55.call(this,_59,_5a,_5b);
};
return dojox.date.timezone;
});

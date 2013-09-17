/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/selector/acme",["../_base/kernel","../has","../dom","../_base/sniff","../_base/array","../_base/lang","../_base/window"],function(_1,_2,_3){
var _4=_1.trim;
var _5=_1.forEach;
var _6=function(){
return _1.doc;
};
var _7=((_1.isWebKit||_1.isMozilla)&&((_6().compatMode)=="BackCompat"));
var _8=">~+";
var _9=false;
var _a=function(){
return true;
};
var _b=function(_c){
if(_8.indexOf(_c.slice(-1))>=0){
_c+=" * ";
}else{
_c+=" ";
}
var ts=function(s,e){
return _4(_c.slice(s,e));
};
var _d=[];
var _e=-1,_f=-1,_10=-1,_11=-1,_12=-1,_13=-1,_14=-1,lc="",cc="",_15;
var x=0,ql=_c.length,_16=null,_17=null;
var _18=function(){
if(_14>=0){
var tv=(_14==x)?null:ts(_14,x);
_16[(_8.indexOf(tv)<0)?"tag":"oper"]=tv;
_14=-1;
}
};
var _19=function(){
if(_13>=0){
_16.id=ts(_13,x).replace(/\\/g,"");
_13=-1;
}
};
var _1a=function(){
if(_12>=0){
_16.classes.push(ts(_12+1,x).replace(/\\/g,""));
_12=-1;
}
};
var _1b=function(){
_19();
_18();
_1a();
};
var _1c=function(){
_1b();
if(_11>=0){
_16.pseudos.push({name:ts(_11+1,x)});
}
_16.loops=(_16.pseudos.length||_16.attrs.length||_16.classes.length);
_16.oquery=_16.query=ts(_15,x);
_16.otag=_16.tag=(_16["oper"])?null:(_16.tag||"*");
if(_16.tag){
_16.tag=_16.tag.toUpperCase();
}
if(_d.length&&(_d[_d.length-1].oper)){
_16.infixOper=_d.pop();
_16.query=_16.infixOper.query+" "+_16.query;
}
_d.push(_16);
_16=null;
};
for(;lc=cc,cc=_c.charAt(x),x<ql;x++){
if(lc=="\\"){
continue;
}
if(!_16){
_15=x;
_16={query:null,pseudos:[],attrs:[],classes:[],tag:null,oper:null,id:null,getTag:function(){
return (_9)?this.otag:this.tag;
}};
_14=x;
}
if(_e>=0){
if(cc=="]"){
if(!_17.attr){
_17.attr=ts(_e+1,x);
}else{
_17.matchFor=ts((_10||_e+1),x);
}
var cmf=_17.matchFor;
if(cmf){
if((cmf.charAt(0)=="\"")||(cmf.charAt(0)=="'")){
_17.matchFor=cmf.slice(1,-1);
}
}
_16.attrs.push(_17);
_17=null;
_e=_10=-1;
}else{
if(cc=="="){
var _1d=("|~^$*".indexOf(lc)>=0)?lc:"";
_17.type=_1d+cc;
_17.attr=ts(_e+1,x-_1d.length);
_10=x+1;
}
}
}else{
if(_f>=0){
if(cc==")"){
if(_11>=0){
_17.value=ts(_f+1,x);
}
_11=_f=-1;
}
}else{
if(cc=="#"){
_1b();
_13=x+1;
}else{
if(cc=="."){
_1b();
_12=x;
}else{
if(cc==":"){
_1b();
_11=x;
}else{
if(cc=="["){
_1b();
_e=x;
_17={};
}else{
if(cc=="("){
if(_11>=0){
_17={name:ts(_11+1,x),value:null};
_16.pseudos.push(_17);
}
_f=x;
}else{
if((cc==" ")&&(lc!=cc)){
_1c();
}
}
}
}
}
}
}
}
}
return _d;
};
var _1e=function(_1f,_20){
if(!_1f){
return _20;
}
if(!_20){
return _1f;
}
return function(){
return _1f.apply(window,arguments)&&_20.apply(window,arguments);
};
};
var _21=function(i,arr){
var r=arr||[];
if(i){
r.push(i);
}
return r;
};
var _22=function(n){
return (1==n.nodeType);
};
var _23="";
var _24=function(_25,_26){
if(!_25){
return _23;
}
if(_26=="class"){
return _25.className||_23;
}
if(_26=="for"){
return _25.htmlFor||_23;
}
if(_26=="style"){
return _25.style.cssText||_23;
}
return (_9?_25.getAttribute(_26):_25.getAttribute(_26,2))||_23;
};
var _27={"*=":function(_28,_29){
return function(_2a){
return (_24(_2a,_28).indexOf(_29)>=0);
};
},"^=":function(_2b,_2c){
return function(_2d){
return (_24(_2d,_2b).indexOf(_2c)==0);
};
},"$=":function(_2e,_2f){
return function(_30){
var ea=" "+_24(_30,_2e);
return (ea.lastIndexOf(_2f)==(ea.length-_2f.length));
};
},"~=":function(_31,_32){
var _33=" "+_32+" ";
return function(_34){
var ea=" "+_24(_34,_31)+" ";
return (ea.indexOf(_33)>=0);
};
},"|=":function(_35,_36){
var _37=_36+"-";
return function(_38){
var ea=_24(_38,_35);
return ((ea==_36)||(ea.indexOf(_37)==0));
};
},"=":function(_39,_3a){
return function(_3b){
return (_24(_3b,_39)==_3a);
};
}};
var _3c=(typeof _6().firstChild.nextElementSibling=="undefined");
var _3d=!_3c?"nextElementSibling":"nextSibling";
var _3e=!_3c?"previousElementSibling":"previousSibling";
var _3f=(_3c?_22:_a);
var _40=function(_41){
while(_41=_41[_3e]){
if(_3f(_41)){
return false;
}
}
return true;
};
var _42=function(_43){
while(_43=_43[_3d]){
if(_3f(_43)){
return false;
}
}
return true;
};
var _44=function(_45){
var _46=_45.parentNode;
var i=0,_47=_46.children||_46.childNodes,ci=(_45["_i"]||-1),cl=(_46["_l"]||-1);
if(!_47){
return -1;
}
var l=_47.length;
if(cl==l&&ci>=0&&cl>=0){
return ci;
}
_46["_l"]=l;
ci=-1;
for(var te=_46["firstElementChild"]||_46["firstChild"];te;te=te[_3d]){
if(_3f(te)){
te["_i"]=++i;
if(_45===te){
ci=i;
}
}
}
return ci;
};
var _48=function(_49){
return !((_44(_49))%2);
};
var _4a=function(_4b){
return ((_44(_4b))%2);
};
var _4c={"checked":function(_4d,_4e){
return function(_4f){
return !!("checked" in _4f?_4f.checked:_4f.selected);
};
},"first-child":function(){
return _40;
},"last-child":function(){
return _42;
},"only-child":function(_50,_51){
return function(_52){
return _40(_52)&&_42(_52);
};
},"empty":function(_53,_54){
return function(_55){
var cn=_55.childNodes;
var cnl=_55.childNodes.length;
for(var x=cnl-1;x>=0;x--){
var nt=cn[x].nodeType;
if((nt===1)||(nt==3)){
return false;
}
}
return true;
};
},"contains":function(_56,_57){
var cz=_57.charAt(0);
if(cz=="\""||cz=="'"){
_57=_57.slice(1,-1);
}
return function(_58){
return (_58.innerHTML.indexOf(_57)>=0);
};
},"not":function(_59,_5a){
var p=_b(_5a)[0];
var _5b={el:1};
if(p.tag!="*"){
_5b.tag=1;
}
if(!p.classes.length){
_5b.classes=1;
}
var ntf=_5c(p,_5b);
return function(_5d){
return (!ntf(_5d));
};
},"nth-child":function(_5e,_5f){
var pi=parseInt;
if(_5f=="odd"){
return _4a;
}else{
if(_5f=="even"){
return _48;
}
}
if(_5f.indexOf("n")!=-1){
var _60=_5f.split("n",2);
var _61=_60[0]?((_60[0]=="-")?-1:pi(_60[0])):1;
var idx=_60[1]?pi(_60[1]):0;
var lb=0,ub=-1;
if(_61>0){
if(idx<0){
idx=(idx%_61)&&(_61+(idx%_61));
}else{
if(idx>0){
if(idx>=_61){
lb=idx-idx%_61;
}
idx=idx%_61;
}
}
}else{
if(_61<0){
_61*=-1;
if(idx>0){
ub=idx;
idx=idx%_61;
}
}
}
if(_61>0){
return function(_62){
var i=_44(_62);
return (i>=lb)&&(ub<0||i<=ub)&&((i%_61)==idx);
};
}else{
_5f=idx;
}
}
var _63=pi(_5f);
return function(_64){
return (_44(_64)==_63);
};
}};
var _65=(_1.isIE&&(_1.isIE<9||_1.isQuirks))?function(_66){
var clc=_66.toLowerCase();
if(clc=="class"){
_66="className";
}
return function(_67){
return (_9?_67.getAttribute(_66):_67[_66]||_67[clc]);
};
}:function(_68){
return function(_69){
return (_69&&_69.getAttribute&&_69.hasAttribute(_68));
};
};
var _5c=function(_6a,_6b){
if(!_6a){
return _a;
}
_6b=_6b||{};
var ff=null;
if(!("el" in _6b)){
ff=_1e(ff,_22);
}
if(!("tag" in _6b)){
if(_6a.tag!="*"){
ff=_1e(ff,function(_6c){
return (_6c&&(_6c.tagName==_6a.getTag()));
});
}
}
if(!("classes" in _6b)){
_5(_6a.classes,function(_6d,idx,arr){
var re=new RegExp("(?:^|\\s)"+_6d+"(?:\\s|$)");
ff=_1e(ff,function(_6e){
return re.test(_6e.className);
});
ff.count=idx;
});
}
if(!("pseudos" in _6b)){
_5(_6a.pseudos,function(_6f){
var pn=_6f.name;
if(_4c[pn]){
ff=_1e(ff,_4c[pn](pn,_6f.value));
}
});
}
if(!("attrs" in _6b)){
_5(_6a.attrs,function(_70){
var _71;
var a=_70.attr;
if(_70.type&&_27[_70.type]){
_71=_27[_70.type](a,_70.matchFor);
}else{
if(a.length){
_71=_65(a);
}
}
if(_71){
ff=_1e(ff,_71);
}
});
}
if(!("id" in _6b)){
if(_6a.id){
ff=_1e(ff,function(_72){
return (!!_72&&(_72.id==_6a.id));
});
}
}
if(!ff){
if(!("default" in _6b)){
ff=_a;
}
}
return ff;
};
var _73=function(_74){
return function(_75,ret,bag){
while(_75=_75[_3d]){
if(_3c&&(!_22(_75))){
continue;
}
if((!bag||_76(_75,bag))&&_74(_75)){
ret.push(_75);
}
break;
}
return ret;
};
};
var _77=function(_78){
return function(_79,ret,bag){
var te=_79[_3d];
while(te){
if(_3f(te)){
if(bag&&!_76(te,bag)){
break;
}
if(_78(te)){
ret.push(te);
}
}
te=te[_3d];
}
return ret;
};
};
var _7a=function(_7b){
_7b=_7b||_a;
return function(_7c,ret,bag){
var te,x=0,_7d=_7c.children||_7c.childNodes;
while(te=_7d[x++]){
if(_3f(te)&&(!bag||_76(te,bag))&&(_7b(te,x))){
ret.push(te);
}
}
return ret;
};
};
var _7e=function(_7f,_80){
var pn=_7f.parentNode;
while(pn){
if(pn==_80){
break;
}
pn=pn.parentNode;
}
return !!pn;
};
var _81={};
var _82=function(_83){
var _84=_81[_83.query];
if(_84){
return _84;
}
var io=_83.infixOper;
var _85=(io?io.oper:"");
var _86=_5c(_83,{el:1});
var qt=_83.tag;
var _87=("*"==qt);
var ecs=_6()["getElementsByClassName"];
if(!_85){
if(_83.id){
_86=(!_83.loops&&_87)?_a:_5c(_83,{el:1,id:1});
_84=function(_88,arr){
var te=_3.byId(_83.id,(_88.ownerDocument||_88));
if(!te||!_86(te)){
return;
}
if(9==_88.nodeType){
return _21(te,arr);
}else{
if(_7e(te,_88)){
return _21(te,arr);
}
}
};
}else{
if(ecs&&/\{\s*\[native code\]\s*\}/.test(String(ecs))&&_83.classes.length&&!_7){
_86=_5c(_83,{el:1,classes:1,id:1});
var _89=_83.classes.join(" ");
_84=function(_8a,arr,bag){
var ret=_21(0,arr),te,x=0;
var _8b=_8a.getElementsByClassName(_89);
while((te=_8b[x++])){
if(_86(te,_8a)&&_76(te,bag)){
ret.push(te);
}
}
return ret;
};
}else{
if(!_87&&!_83.loops){
_84=function(_8c,arr,bag){
var ret=_21(0,arr),te,x=0;
var _8d=_8c.getElementsByTagName(_83.getTag());
while((te=_8d[x++])){
if(_76(te,bag)){
ret.push(te);
}
}
return ret;
};
}else{
_86=_5c(_83,{el:1,tag:1,id:1});
_84=function(_8e,arr,bag){
var ret=_21(0,arr),te,x=0;
var _8f=_8e.getElementsByTagName(_83.getTag());
while((te=_8f[x++])){
if(_86(te,_8e)&&_76(te,bag)){
ret.push(te);
}
}
return ret;
};
}
}
}
}else{
var _90={el:1};
if(_87){
_90.tag=1;
}
_86=_5c(_83,_90);
if("+"==_85){
_84=_73(_86);
}else{
if("~"==_85){
_84=_77(_86);
}else{
if(">"==_85){
_84=_7a(_86);
}
}
}
}
return _81[_83.query]=_84;
};
var _91=function(_92,_93){
var _94=_21(_92),qp,x,te,qpl=_93.length,bag,ret;
for(var i=0;i<qpl;i++){
ret=[];
qp=_93[i];
x=_94.length-1;
if(x>0){
bag={};
ret.nozip=true;
}
var gef=_82(qp);
for(var j=0;(te=_94[j]);j++){
gef(te,ret,bag);
}
if(!ret.length){
break;
}
_94=ret;
}
return ret;
};
var _95={},_96={};
var _97=function(_98){
var _99=_b(_4(_98));
if(_99.length==1){
var tef=_82(_99[0]);
return function(_9a){
var r=tef(_9a,[]);
if(r){
r.nozip=true;
}
return r;
};
}
return function(_9b){
return _91(_9b,_99);
};
};
var nua=navigator.userAgent;
var wk="WebKit/";
var _9c=(_1.isWebKit&&(nua.indexOf(wk)>0)&&(parseFloat(nua.split(wk)[1])>528));
var _9d=_1.isIE?"commentStrip":"nozip";
var qsa="querySelectorAll";
var _9e=(!!_6()[qsa]&&(!_1.isSafari||(_1.isSafari>3.1)||_9c));
var _9f=/n\+\d|([^ ])?([>~+])([^ =])?/g;
var _a0=function(_a1,pre,ch,_a2){
return ch?(pre?pre+" ":"")+ch+(_a2?" "+_a2:""):_a1;
};
var _a3=function(_a4,_a5){
_a4=_a4.replace(_9f,_a0);
if(_9e){
var _a6=_96[_a4];
if(_a6&&!_a5){
return _a6;
}
}
var _a7=_95[_a4];
if(_a7){
return _a7;
}
var qcz=_a4.charAt(0);
var _a8=(-1==_a4.indexOf(" "));
if((_a4.indexOf("#")>=0)&&(_a8)){
_a5=true;
}
var _a9=(_9e&&(!_a5)&&(_8.indexOf(qcz)==-1)&&(!_1.isIE||(_a4.indexOf(":")==-1))&&(!(_7&&(_a4.indexOf(".")>=0)))&&(_a4.indexOf(":contains")==-1)&&(_a4.indexOf(":checked")==-1)&&(_a4.indexOf("|=")==-1));
if(_a9){
var tq=(_8.indexOf(_a4.charAt(_a4.length-1))>=0)?(_a4+" *"):_a4;
return _96[_a4]=function(_aa){
try{
if(!((9==_aa.nodeType)||_a8)){
throw "";
}
var r=_aa[qsa](tq);
r[_9d]=true;
return r;
}
catch(e){
return _a3(_a4,true)(_aa);
}
};
}else{
var _ab=_a4.split(/\s*,\s*/);
return _95[_a4]=((_ab.length<2)?_97(_a4):function(_ac){
var _ad=0,ret=[],tp;
while((tp=_ab[_ad++])){
ret=ret.concat(_97(tp)(_ac));
}
return ret;
});
}
};
var _ae=0;
var _af=_1.isIE?function(_b0){
if(_9){
return (_b0.getAttribute("_uid")||_b0.setAttribute("_uid",++_ae)||_ae);
}else{
return _b0.uniqueID;
}
}:function(_b1){
return (_b1._uid||(_b1._uid=++_ae));
};
var _76=function(_b2,bag){
if(!bag){
return 1;
}
var id=_af(_b2);
if(!bag[id]){
return bag[id]=1;
}
return 0;
};
var _b3="_zipIdx";
var _b4=function(arr){
if(arr&&arr.nozip){
return arr;
}
var ret=[];
if(!arr||!arr.length){
return ret;
}
if(arr[0]){
ret.push(arr[0]);
}
if(arr.length<2){
return ret;
}
_ae++;
if(_1.isIE&&_9){
var _b5=_ae+"";
arr[0].setAttribute(_b3,_b5);
for(var x=1,te;te=arr[x];x++){
if(arr[x].getAttribute(_b3)!=_b5){
ret.push(te);
}
te.setAttribute(_b3,_b5);
}
}else{
if(_1.isIE&&arr.commentStrip){
try{
for(var x=1,te;te=arr[x];x++){
if(_22(te)){
ret.push(te);
}
}
}
catch(e){
}
}else{
if(arr[0]){
arr[0][_b3]=_ae;
}
for(var x=1,te;te=arr[x];x++){
if(arr[x][_b3]!=_ae){
ret.push(te);
}
te[_b3]=_ae;
}
}
}
return ret;
};
var _b6=function(_b7,_b8){
_b8=_b8||_6();
var od=_b8.ownerDocument||_b8.documentElement;
_9=(_b8.contentType&&_b8.contentType=="application/xml")||(_1.isOpera&&(_b8.doctype||od.toString()=="[object XMLDocument]"))||(!!od)&&(_1.isIE?od.xml:(_b8.xmlVersion||od.xmlVersion));
var r=_a3(_b7)(_b8);
if(r&&r.nozip){
return r;
}
return _b4(r);
};
_b6.filter=function(_b9,_ba,_bb){
var _bc=[],_bd=_b(_ba),_be=(_bd.length==1&&!/[^\w#\.]/.test(_ba))?_5c(_bd[0]):function(_bf){
return _1.query(_ba,_bb).indexOf(_bf)!=-1;
};
for(var x=0,te;te=_b9[x];x++){
if(_be(te)){
_bc.push(te);
}
}
return _bc;
};
return _b6;
});

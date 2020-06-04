/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/selector/acme",["../dom","../sniff","../_base/array","../_base/lang","../_base/window"],function(_1,_2,_3,_4,_5){
var _6=_4.trim;
var _7=_3.forEach;
var _8=function(){
return _5.doc;
};
var _9=(_8().compatMode)=="BackCompat";
var _a=">~+";
var _b=false;
var _c=function(){
return true;
};
var _d=function(_e){
if(_a.indexOf(_e.slice(-1))>=0){
_e+=" * ";
}else{
_e+=" ";
}
var ts=function(s,e){
return _6(_e.slice(s,e));
};
var _f=[];
var _10=-1,_11=-1,_12=-1,_13=-1,_14=-1,_15=-1,_16=-1,_17,lc="",cc="",_18;
var x=0,ql=_e.length,_19=null,_1a=null;
var _1b=function(){
if(_16>=0){
var tv=(_16==x)?null:ts(_16,x);
_19[(_a.indexOf(tv)<0)?"tag":"oper"]=tv;
_16=-1;
}
};
var _1c=function(){
if(_15>=0){
_19.id=ts(_15,x).replace(/\\/g,"");
_15=-1;
}
};
var _1d=function(){
if(_14>=0){
_19.classes.push(ts(_14+1,x).replace(/\\/g,""));
_14=-1;
}
};
var _1e=function(){
_1c();
_1b();
_1d();
};
var _1f=function(){
_1e();
if(_13>=0){
_19.pseudos.push({name:ts(_13+1,x)});
}
_19.loops=(_19.pseudos.length||_19.attrs.length||_19.classes.length);
_19.oquery=_19.query=ts(_18,x);
_19.otag=_19.tag=(_19["oper"])?null:(_19.tag||"*");
if(_19.tag){
_19.tag=_19.tag.toUpperCase();
}
if(_f.length&&(_f[_f.length-1].oper)){
_19.infixOper=_f.pop();
_19.query=_19.infixOper.query+" "+_19.query;
}
_f.push(_19);
_19=null;
};
for(;lc=cc,cc=_e.charAt(x),x<ql;x++){
if(lc=="\\"){
continue;
}
if(!_19){
_18=x;
_19={query:null,pseudos:[],attrs:[],classes:[],tag:null,oper:null,id:null,getTag:function(){
return _b?this.otag:this.tag;
}};
_16=x;
}
if(_17){
if(cc==_17){
_17=null;
}
continue;
}else{
if(cc=="'"||cc=="\""){
_17=cc;
continue;
}
}
if(_10>=0){
if(cc=="]"){
if(!_1a.attr){
_1a.attr=ts(_10+1,x);
}else{
_1a.matchFor=ts((_12||_10+1),x);
}
var cmf=_1a.matchFor;
if(cmf){
if((cmf.charAt(0)=="\"")||(cmf.charAt(0)=="'")){
_1a.matchFor=cmf.slice(1,-1);
}
}
if(_1a.matchFor){
_1a.matchFor=_1a.matchFor.replace(/\\/g,"");
}
_19.attrs.push(_1a);
_1a=null;
_10=_12=-1;
}else{
if(cc=="="){
var _20=("|~^$*".indexOf(lc)>=0)?lc:"";
_1a.type=_20+cc;
_1a.attr=ts(_10+1,x-_20.length);
_12=x+1;
}
}
}else{
if(_11>=0){
if(cc==")"){
if(_13>=0){
_1a.value=ts(_11+1,x);
}
_13=_11=-1;
}
}else{
if(cc=="#"){
_1e();
_15=x+1;
}else{
if(cc=="."){
_1e();
_14=x;
}else{
if(cc==":"){
_1e();
_13=x;
}else{
if(cc=="["){
_1e();
_10=x;
_1a={};
}else{
if(cc=="("){
if(_13>=0){
_1a={name:ts(_13+1,x),value:null};
_19.pseudos.push(_1a);
}
_11=x;
}else{
if((cc==" ")&&(lc!=cc)){
_1f();
}
}
}
}
}
}
}
}
}
return _f;
};
var _21=function(_22,_23){
if(!_22){
return _23;
}
if(!_23){
return _22;
}
return function(){
return _22.apply(window,arguments)&&_23.apply(window,arguments);
};
};
var _24=function(i,arr){
var r=arr||[];
if(i){
r.push(i);
}
return r;
};
var _25=function(n){
return (1==n.nodeType);
};
var _26="";
var _27=function(_28,_29){
if(!_28){
return _26;
}
if(_29=="class"){
return _28.className||_26;
}
if(_29=="for"){
return _28.htmlFor||_26;
}
if(_29=="style"){
return _28.style.cssText||_26;
}
return (_b?_28.getAttribute(_29):_28.getAttribute(_29,2))||_26;
};
var _2a={"*=":function(_2b,_2c){
return function(_2d){
return (_27(_2d,_2b).indexOf(_2c)>=0);
};
},"^=":function(_2e,_2f){
return function(_30){
return (_27(_30,_2e).indexOf(_2f)==0);
};
},"$=":function(_31,_32){
return function(_33){
var ea=" "+_27(_33,_31);
var _34=ea.lastIndexOf(_32);
return _34>-1&&(_34==(ea.length-_32.length));
};
},"~=":function(_35,_36){
var _37=" "+_36+" ";
return function(_38){
var ea=" "+_27(_38,_35)+" ";
return (ea.indexOf(_37)>=0);
};
},"|=":function(_39,_3a){
var _3b=_3a+"-";
return function(_3c){
var ea=_27(_3c,_39);
return ((ea==_3a)||(ea.indexOf(_3b)==0));
};
},"=":function(_3d,_3e){
return function(_3f){
return (_27(_3f,_3d)==_3e);
};
}};
var _40=_8().documentElement;
var _41=!(_40.nextElementSibling||"nextElementSibling" in _40);
var _42=!_41?"nextElementSibling":"nextSibling";
var _43=!_41?"previousElementSibling":"previousSibling";
var _44=(_41?_25:_c);
var _45=function(_46){
while(_46=_46[_43]){
if(_44(_46)){
return false;
}
}
return true;
};
var _47=function(_48){
while(_48=_48[_42]){
if(_44(_48)){
return false;
}
}
return true;
};
var _49=function(_4a){
var _4b=_4a.parentNode;
_4b=_4b.nodeType!=7?_4b:_4b.nextSibling;
var i=0,_4c=_4b.children||_4b.childNodes,ci=(_4a["_i"]||_4a.getAttribute("_i")||-1),cl=(_4b["_l"]||(typeof _4b.getAttribute!=="undefined"?_4b.getAttribute("_l"):-1));
if(!_4c){
return -1;
}
var l=_4c.length;
if(cl==l&&ci>=0&&cl>=0){
return ci;
}
if(_2("ie")&&typeof _4b.setAttribute!=="undefined"){
_4b.setAttribute("_l",l);
}else{
_4b["_l"]=l;
}
ci=-1;
for(var te=_4b["firstElementChild"]||_4b["firstChild"];te;te=te[_42]){
if(_44(te)){
if(_2("ie")){
te.setAttribute("_i",++i);
}else{
te["_i"]=++i;
}
if(_4a===te){
ci=i;
}
}
}
return ci;
};
var _4d=function(_4e){
return !((_49(_4e))%2);
};
var _4f=function(_50){
return ((_49(_50))%2);
};
var _51={"checked":function(_52,_53){
return function(_54){
return !!("checked" in _54?_54.checked:_54.selected);
};
},"disabled":function(_55,_56){
return function(_57){
return _57.disabled;
};
},"enabled":function(_58,_59){
return function(_5a){
return !_5a.disabled;
};
},"first-child":function(){
return _45;
},"last-child":function(){
return _47;
},"only-child":function(_5b,_5c){
return function(_5d){
return _45(_5d)&&_47(_5d);
};
},"empty":function(_5e,_5f){
return function(_60){
var cn=_60.childNodes;
var cnl=_60.childNodes.length;
for(var x=cnl-1;x>=0;x--){
var nt=cn[x].nodeType;
if((nt===1)||(nt==3)){
return false;
}
}
return true;
};
},"contains":function(_61,_62){
var cz=_62.charAt(0);
if(cz=="\""||cz=="'"){
_62=_62.slice(1,-1);
}
return function(_63){
return (_63.innerHTML.indexOf(_62)>=0);
};
},"not":function(_64,_65){
var p=_d(_65)[0];
var _66={el:1};
if(p.tag!="*"){
_66.tag=1;
}
if(!p.classes.length){
_66.classes=1;
}
var ntf=_67(p,_66);
return function(_68){
return (!ntf(_68));
};
},"nth-child":function(_69,_6a){
var pi=parseInt;
if(_6a=="odd"){
return _4f;
}else{
if(_6a=="even"){
return _4d;
}
}
if(_6a.indexOf("n")!=-1){
var _6b=_6a.split("n",2);
var _6c=_6b[0]?((_6b[0]=="-")?-1:pi(_6b[0])):1;
var idx=_6b[1]?pi(_6b[1]):0;
var lb=0,ub=-1;
if(_6c>0){
if(idx<0){
idx=(idx%_6c)&&(_6c+(idx%_6c));
}else{
if(idx>0){
if(idx>=_6c){
lb=idx-idx%_6c;
}
idx=idx%_6c;
}
}
}else{
if(_6c<0){
_6c*=-1;
if(idx>0){
ub=idx;
idx=idx%_6c;
}
}
}
if(_6c>0){
return function(_6d){
var i=_49(_6d);
return (i>=lb)&&(ub<0||i<=ub)&&((i%_6c)==idx);
};
}else{
_6a=idx;
}
}
var _6e=pi(_6a);
return function(_6f){
return (_49(_6f)==_6e);
};
}};
var _70=(_2("ie")<9||_2("ie")==9&&_2("quirks"))?function(_71){
var clc=_71.toLowerCase();
if(clc=="class"){
_71="className";
}
return function(_72){
return (_b?_72.getAttribute(_71):_72[_71]||_72[clc]);
};
}:function(_73){
return function(_74){
return (_74&&_74.getAttribute&&_74.hasAttribute(_73));
};
};
var _67=function(_75,_76){
if(!_75){
return _c;
}
_76=_76||{};
var ff=null;
if(!("el" in _76)){
ff=_21(ff,_25);
}
if(!("tag" in _76)){
if(_75.tag!="*"){
ff=_21(ff,function(_77){
return (_77&&((_b?_77.tagName:_77.tagName.toUpperCase())==_75.getTag()));
});
}
}
if(!("classes" in _76)){
_7(_75.classes,function(_78,idx,arr){
var re=new RegExp("(?:^|\\s)"+_78+"(?:\\s|$)");
ff=_21(ff,function(_79){
return re.test(_79.className);
});
ff.count=idx;
});
}
if(!("pseudos" in _76)){
_7(_75.pseudos,function(_7a){
var pn=_7a.name;
if(_51[pn]){
ff=_21(ff,_51[pn](pn,_7a.value));
}
});
}
if(!("attrs" in _76)){
_7(_75.attrs,function(_7b){
var _7c;
var a=_7b.attr;
if(_7b.type&&_2a[_7b.type]){
_7c=_2a[_7b.type](a,_7b.matchFor);
}else{
if(a.length){
_7c=_70(a);
}
}
if(_7c){
ff=_21(ff,_7c);
}
});
}
if(!("id" in _76)){
if(_75.id){
ff=_21(ff,function(_7d){
return (!!_7d&&(_7d.id==_75.id));
});
}
}
if(!ff){
if(!("default" in _76)){
ff=_c;
}
}
return ff;
};
var _7e=function(_7f){
return function(_80,ret,bag){
while(_80=_80[_42]){
if(_41&&(!_25(_80))){
continue;
}
if((!bag||_81(_80,bag))&&_7f(_80)){
ret.push(_80);
}
break;
}
return ret;
};
};
var _82=function(_83){
return function(_84,ret,bag){
var te=_84[_42];
while(te){
if(_44(te)){
if(bag&&!_81(te,bag)){
break;
}
if(_83(te)){
ret.push(te);
}
}
te=te[_42];
}
return ret;
};
};
var _85=function(_86,_87){
var _88=function(_89){
var _8a=[];
try{
_8a=Array.prototype.slice.call(_89);
}
catch(e){
for(var i=0,len=_89.length;i<len;i++){
_8a.push(_89[i]);
}
}
return _8a;
};
_86=_86||_c;
return function(_8b,ret,bag){
var te,x=0,_8c=[];
_8c=_88(_8b.children||_8b.childNodes);
if(_87){
_3.forEach(_8c,function(_8d){
if(_8d.nodeType===1){
_8c=_8c.concat(_88(_8d.getElementsByTagName("*")));
}
});
}
while(te=_8c[x++]){
if(_44(te)&&(!bag||_81(te,bag))&&(_86(te,x))){
ret.push(te);
}
}
return ret;
};
};
var _8e=function(_8f,_90){
var pn=_8f.parentNode;
while(pn){
if(pn==_90){
break;
}
pn=pn.parentNode;
}
return !!pn;
};
var _91={};
var _92=function(_93){
var _94=_91[_93.query];
if(_94){
return _94;
}
var io=_93.infixOper;
var _95=(io?io.oper:"");
var _96=_67(_93,{el:1});
var qt=_93.tag;
var _97=("*"==qt);
var ecs=_8()["getElementsByClassName"];
if(!_95){
if(_93.id){
_96=(!_93.loops&&_97)?_c:_67(_93,{el:1,id:1});
_94=function(_98,arr){
var te=_1.byId(_93.id,(_98.ownerDocument||_98));
if(_98.ownerDocument&&!_8e(_98,_98.ownerDocument)){
var _99=_98.nodeType===11?_98.childNodes:[_98];
_3.some(_99,function(_9a){
var _9b=_85(function(_9c){
return _9c.id===_93.id;
},true)(_9a,[]);
if(_9b.length){
te=_9b[0];
return false;
}
});
}
if(!te||!_96(te)){
return;
}
if(9==_98.nodeType){
return _24(te,arr);
}else{
if(_8e(te,_98)){
return _24(te,arr);
}
}
};
}else{
if(ecs&&/\{\s*\[native code\]\s*\}/.test(String(ecs))&&_93.classes.length&&!_9){
_96=_67(_93,{el:1,classes:1,id:1});
var _9d=_93.classes.join(" ");
_94=function(_9e,arr,bag){
var ret=_24(0,arr),te,x=0;
var _9f=_9e.getElementsByClassName(_9d);
while((te=_9f[x++])){
if(_96(te,_9e)&&_81(te,bag)){
ret.push(te);
}
}
return ret;
};
}else{
if(!_97&&!_93.loops){
_94=function(_a0,arr,bag){
var ret=_24(0,arr),te,x=0;
var tag=_93.getTag(),_a1=tag?_a0.getElementsByTagName(tag):[];
while((te=_a1[x++])){
if(_81(te,bag)){
ret.push(te);
}
}
return ret;
};
}else{
_96=_67(_93,{el:1,tag:1,id:1});
_94=function(_a2,arr,bag){
var ret=_24(0,arr),te,x=0;
var tag=_93.getTag(),_a3=tag?_a2.getElementsByTagName(tag):[];
while((te=_a3[x++])){
if(_96(te,_a2)&&_81(te,bag)){
ret.push(te);
}
}
return ret;
};
}
}
}
}else{
var _a4={el:1};
if(_97){
_a4.tag=1;
}
_96=_67(_93,_a4);
if("+"==_95){
_94=_7e(_96);
}else{
if("~"==_95){
_94=_82(_96);
}else{
if(">"==_95){
_94=_85(_96);
}
}
}
}
return _91[_93.query]=_94;
};
var _a5=function(_a6,_a7){
var _a8=_24(_a6),qp,x,te,qpl=_a7.length,bag,ret;
for(var i=0;i<qpl;i++){
ret=[];
qp=_a7[i];
x=_a8.length-1;
if(x>0){
bag={};
ret.nozip=true;
}
var gef=_92(qp);
for(var j=0;(te=_a8[j]);j++){
gef(te,ret,bag);
}
if(!ret.length){
break;
}
_a8=ret;
}
return ret;
};
var _a9={},_aa={};
var _ab=function(_ac){
var _ad=_d(_6(_ac));
if(_ad.length==1){
var tef=_92(_ad[0]);
return function(_ae){
var r=tef(_ae,[]);
if(r){
r.nozip=true;
}
return r;
};
}
return function(_af){
return _a5(_af,_ad);
};
};
var _b0=_2("ie")?"commentStrip":"nozip";
var qsa="querySelectorAll";
var _b1=!!_8()[qsa];
var _b2=/\\[>~+]|n\+\d|([^ \\])?([>~+])([^ =])?/g;
var _b3=function(_b4,pre,ch,_b5){
return ch?(pre?pre+" ":"")+ch+(_b5?" "+_b5:""):_b4;
};
var _b6=/([^[]*)([^\]]*])?/g;
var _b7=function(_b8,_b9,att){
return _b9.replace(_b2,_b3)+(att||"");
};
var _ba=function(_bb,_bc){
_bb=_bb.replace(_b6,_b7);
if(_b1){
var _bd=_aa[_bb];
if(_bd&&!_bc){
return _bd;
}
}
var _be=_a9[_bb];
if(_be){
return _be;
}
var qcz=_bb.charAt(0);
var _bf=(-1==_bb.indexOf(" "));
if((_bb.indexOf("#")>=0)&&(_bf)){
_bc=true;
}
var _c0=(_b1&&(!_bc)&&(_a.indexOf(qcz)==-1)&&(!_2("ie")||(_bb.indexOf(":")==-1))&&(!(_9&&(_bb.indexOf(".")>=0)))&&(_bb.indexOf(":contains")==-1)&&(_bb.indexOf(":checked")==-1)&&(_bb.indexOf("|=")==-1));
if(_c0){
var tq=(_a.indexOf(_bb.charAt(_bb.length-1))>=0)?(_bb+" *"):_bb;
return _aa[_bb]=function(_c1){
if(9==_c1.nodeType||_bf){
try{
var r=_c1[qsa](tq);
r[_b0]=true;
return r;
}
catch(e){
}
}
return _ba(_bb,true)(_c1);
};
}else{
var _c2=_bb.match(/([^\s,](?:"(?:\\.|[^"])+"|'(?:\\.|[^'])+'|[^,])*)/g);
return _a9[_bb]=((_c2.length<2)?_ab(_bb):function(_c3){
var _c4=0,ret=[],tp;
while((tp=_c2[_c4++])){
ret=ret.concat(_ab(tp)(_c3));
}
return ret;
});
}
};
var _c5=0;
var _c6=_2("ie")?function(_c7){
if(_b){
return (_c7.getAttribute("_uid")||_c7.setAttribute("_uid",++_c5)||_c5);
}else{
return _c7.uniqueID;
}
}:function(_c8){
return (_c8._uid||(_c8._uid=++_c5));
};
var _81=function(_c9,bag){
if(!bag){
return 1;
}
var id=_c6(_c9);
if(!bag[id]){
return bag[id]=1;
}
return 0;
};
var _ca="_zipIdx";
var _cb=function(arr){
if(arr&&arr.nozip){
return arr;
}
if(!arr||!arr.length){
return [];
}
if(arr.length<2){
return [arr[0]];
}
var ret=[];
_c5++;
var x,te;
if(_2("ie")&&_b){
var _cc=_c5+"";
for(x=0;x<arr.length;x++){
if((te=arr[x])&&te.getAttribute(_ca)!=_cc){
ret.push(te);
te.setAttribute(_ca,_cc);
}
}
}else{
if(_2("ie")&&arr.commentStrip){
try{
for(x=0;x<arr.length;x++){
if((te=arr[x])&&_25(te)){
ret.push(te);
}
}
}
catch(e){
}
}else{
for(x=0;x<arr.length;x++){
if((te=arr[x])&&te[_ca]!=_c5){
ret.push(te);
te[_ca]=_c5;
}
}
}
}
return ret;
};
var _cd=function(_ce,_cf){
_cf=_cf||_8();
var od=_cf.ownerDocument||_cf;
_b=(od.createElement("div").tagName==="div");
var r=_ba(_ce)(_cf);
if(r&&r.nozip){
return r;
}
return _cb(r);
};
_cd.filter=function(_d0,_d1,_d2){
var _d3=[],_d4=_d(_d1),_d5=(_d4.length==1&&!/[^\w#\.]/.test(_d1))?_67(_d4[0]):function(_d6){
return _3.indexOf(_cd(_d1,_1.byId(_d2)),_d6)!=-1;
};
for(var x=0,te;te=_d0[x];x++){
if(_d5(te)){
_d3.push(te);
}
}
return _d3;
};
return _cd;
});

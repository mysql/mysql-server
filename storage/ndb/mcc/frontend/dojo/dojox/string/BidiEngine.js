//>>built
define("dojox/string/BidiEngine",["dojo/_base/lang","dojo/_base/declare","dojo/Stateful"],function(_1,_2,_3){
_1.getObject("string",true,dojox);
var _4=_2("dojox.string.BidiEngine",_3,{inputFormat:"ILYNN",outputFormat:"VLNNN",sourceToTarget:[],targetToSource:[],levels:[],bidiTransform:function(_5,_6,_7){
this.sourceToTarget=[];
this.targetToSource=[];
if(!_5){
return "";
}
_8(this.sourceToTarget,this.targetToSource,_5.length);
if(!this.checkParameters(_6,_7)){
return _5;
}
_6=this.inputFormat;
_7=this.outputFormat;
var _9=_5;
var _a=_b;
var _c=_d(_6.charAt(1)),_e=_d(_7.charAt(1)),_f=(_6.charAt(0)==="I")?"L":_6.charAt(0),_10=(_7.charAt(0)==="I")?"L":_7.charAt(0),_11=_f+_c,_12=_10+_e,_13=_6.charAt(2)+_7.charAt(2);
_a.defInFormat=_11;
_a.defOutFormat=_12;
_a.defSwap=_13;
var _14=_15(_5,_11,_12,_13,_a),_16=false;
if(_7.charAt(1)==="R"){
_16=true;
}else{
if(_7.charAt(1)==="C"||_7.charAt(1)==="D"){
_16=this.checkContextual(_14);
}
}
this.sourceToTarget=_17;
this.targetToSource=_18(this.sourceToTarget);
_19=this.targetToSource;
if(_6.charAt(3)===_7.charAt(3)){
_9=_14;
}else{
if(_7.charAt(3)==="S"){
_9=_1a(_16,_14,true);
}else{
_9=_1b(_14,_16,true);
}
}
this.sourceToTarget=_17;
this.targetToSource=_19;
this.levels=_1c;
return _9;
},_inputFormatSetter:function(_1d){
if(!_1e.test(_1d)){
throw new Error("dojox/string/BidiEngine: the bidi layout string is wrong!");
}
this.inputFormat=_1d;
},_outputFormatSetter:function(_1f){
if(!_1e.test(_1f)){
throw new Error("dojox/string/BidiEngine: the bidi layout string is wrong!");
}
this.outputFormat=_1f;
},checkParameters:function(_20,_21){
if(!_20){
_20=this.inputFormat;
}else{
this.set("inputFormat",_20);
}
if(!_21){
_21=this.outputFormat;
}else{
this.set("outputFormat",_21);
}
if(_20===_21){
return false;
}
return true;
},checkContextual:function(_22){
var dir=_23(_22);
if(dir!=="ltr"&&dir!=="rtl"){
try{
dir=document.dir.toLowerCase();
}
catch(e){
}
if(dir!=="ltr"&&dir!=="rtl"){
dir="ltr";
}
}
return dir;
},hasBidiChar:function(_24){
return _25.test(_24);
}});
function _15(_26,_27,_28,_29,bdx){
var _2a=_2b(_26,{inFormat:_27,outFormat:_28,swap:_29},bdx);
if(_2a.inFormat===_2a.outFormat){
return _26;
}
_27=_2a.inFormat;
_28=_2a.outFormat;
_29=_2a.swap;
var _2c=_27.substring(0,1),_2d=_27.substring(1,4),_2e=_28.substring(0,1),_2f=_28.substring(1,4);
bdx.inFormat=_27;
bdx.outFormat=_28;
bdx.swap=_29;
if((_2c==="L")&&(_28==="VLTR")){
if(_2d==="LTR"){
bdx.dir=LTR;
return _30(_26,bdx);
}
if(_2d==="RTL"){
bdx.dir=RTL;
return _30(_26,bdx);
}
}
if((_2c==="V")&&(_2e==="V")){
bdx.dir=_2d==="RTL"?RTL:LTR;
return _31(_26,bdx);
}
if((_2c==="L")&&(_28==="VRTL")){
if(_2d==="LTR"){
bdx.dir=LTR;
_26=_30(_26,bdx);
}else{
bdx.dir=RTL;
_26=_30(_26,bdx);
}
return _31(_26);
}
if((_27==="VLTR")&&(_28==="LLTR")){
bdx.dir=LTR;
return _30(_26,bdx);
}
if((_2c==="V")&&(_2e==="L")&&(_2d!==_2f)){
_26=_31(_26);
return (_2d==="RTL")?_15(_26,"LLTR","VLTR",_29,bdx):_15(_26,"LRTL","VRTL",_29,bdx);
}
if((_27==="VRTL")&&(_28==="LRTL")){
return _15(_26,"LRTL","VRTL",_29,bdx);
}
if((_2c==="L")&&(_2e==="L")){
var _32=bdx.swap;
bdx.swap=_32.substr(0,1)+"N";
if(_2d==="RTL"){
bdx.dir=RTL;
_26=_30(_26,bdx);
bdx.swap="N"+_32.substr(1,2);
bdx.dir=LTR;
_26=_30(_26,bdx);
}else{
bdx.dir=LTR;
_26=_30(_26,bdx);
bdx.swap="N"+_32.substr(1,2);
_26=_15(_26,"VLTR","LRTL",bdx.swap,bdx);
}
return _26;
}
};
function _2b(_33,_34,bdx){
if(_34.inFormat===undefined){
_34.inFormat=bdx.defInFormat;
}
if(_34.outFormat===undefined){
_34.outFormat=bdx.defOutFormat;
}
if(_34.swap===undefined){
_34.swap=bdx.defSwap;
}
if(_34.inFormat===_34.outFormat){
return _34;
}
var dir,_35=_34.inFormat.substring(0,1),_36=_34.inFormat.substring(1,4),_37=_34.outFormat.substring(0,1),_38=_34.outFormat.substring(1,4);
if(_36.charAt(0)==="C"){
dir=_23(_33);
if(dir==="ltr"||dir==="rtl"){
_36=dir.toUpperCase();
}else{
_36=_34.inFormat.charAt(2)==="L"?"LTR":"RTL";
}
_34.inFormat=_35+_36;
}
if(_38.charAt(0)==="C"){
dir=_23(_33);
if(dir==="rtl"){
_38="RTL";
}else{
if(dir==="ltr"){
dir=_39(_33);
_38=dir.toUpperCase();
}else{
_38=_34.outFormat.charAt(2)==="L"?"LTR":"RTL";
}
}
_34.outFormat=_37+_38;
}
return _34;
};
function _1a(rtl,_3a,_3b){
if(_3a.length===0){
return;
}
if(rtl===undefined){
rtl=true;
}
if(_3b===undefined){
_3b=true;
}
_3a=String(_3a);
var _3c=_3a.split(""),Ix=0,_3d=+1,_3e=_3c.length;
if(!rtl){
Ix=_3c.length-1;
_3d=-1;
_3e=1;
}
var _3f=_40(_3c,Ix,_3d,_3e,_3b);
var _41="";
for(var idx=0;idx<_3c.length;idx++){
if(!(_3b&&_42(_3f,_3f.length,idx)>-1)){
_41+=_3c[idx];
}else{
_43(_19,idx,!rtl,-1);
_17.splice(idx,1);
}
}
return _41;
};
function _40(_44,Ix,_45,_46,_47){
var _48=0,_49=[],_4a=0;
for(var _4b=Ix;_4b*_45<_46;_4b=_4b+_45){
if(_4c(_44[_4b])||_4d(_44[_4b])){
if(_44[_4b]==="ل"&&_4e(_44,(_4b+_45),_45,_46)){
_44[_4b]=(_48===0)?_4f(_44[_4b+_45],_50):_4f(_44[_4b+_45],_51);
_4b+=_45;
_52(_44,_4b,_45,_46);
if(_47){
_49[_4a]=_4b;
_4a++;
}
_48=0;
continue;
}
var _53=_44[_4b];
if(_48===1){
_44[_4b]=(_54(_44,(_4b+_45),_45,_46))?_55(_44[_4b]):_56(_44[_4b],_57);
}else{
if(_54(_44,(_4b+_45),_45,_46)===true){
_44[_4b]=_56(_44[_4b],_58);
}else{
_44[_4b]=_56(_44[_4b],_59);
}
}
if(!_4d(_53)){
_48=1;
}
if(_5a(_53)===true){
_48=0;
}
}else{
_48=0;
}
}
return _49;
};
function _23(_5b){
var fdc=/[A-Za-z\u05d0-\u065f\u066a-\u06ef\u06fa-\u07ff\ufb1d-\ufdff\ufe70-\ufefc]/.exec(_5b);
return fdc?(fdc[0]<="z"?"ltr":"rtl"):"";
};
function _39(_5c){
var _5d=_5c.split("");
_5d.reverse();
return _23(_5d.join(""));
};
function _1b(_5e,rtl,_5f){
if(_5e.length===0){
return;
}
if(_5f===undefined){
_5f=true;
}
if(rtl===undefined){
rtl=true;
}
_5e=String(_5e);
var _60="",_61=[];
_61=_5e.split("");
for(var i=0;i<_5e.length;i++){
var _62=false;
if(_61[i]>="ﹰ"&&_61[i]<"﻿"){
var _63=_5e.charCodeAt(i);
if(_61[i]>="ﻵ"&&_61[i]<="ﻼ"){
if(rtl){
if(i>0&&_5f&&_61[i-1]===" "){
_60=_60.substring(0,_60.length-1)+"ل";
}else{
_60+="ل";
_62=true;
}
_60+=_64[(_63-65269)/2];
}else{
_60+=_64[(_63-65269)/2];
_60+="ل";
if(i+1<_5e.length&&_5f&&_61[i+1]===" "){
i++;
}else{
_62=true;
}
}
if(_62){
_43(_19,i,true,1);
_17.splice(i,0,_17[i]);
}
}else{
_60+=_65[_63-65136];
}
}else{
_60+=_61[i];
}
}
return _60;
};
function _30(str,bdx){
var _66=str.split(""),_67=[];
_68(_66,_67,bdx);
_69(_66,_67,bdx);
_6a(2,_66,_67,bdx);
_6a(1,_66,_67,bdx);
_1c=_67;
return _66.join("");
};
function _68(_6b,_6c,bdx){
var len=_6b.length,_6d=bdx.dir?_6e:_6f,_70=null,_71=null,_72=null,_73=0,_74=null,_75=null,_76=-1,i=null,ix=null,_77=[],_78=[];
bdx.hiLevel=bdx.dir;
bdx.lastArabic=false;
bdx.hasUbatAl=false;
bdx.hasUbatB=false;
bdx.hasUbatS=false;
for(i=0;i<len;i++){
_77[i]=_79(_6b[i]);
}
for(ix=0;ix<len;ix++){
_70=_73;
_78[ix]=_71=_7a(_6b,_77,_78,ix,bdx);
_73=_6d[_70][_71];
_74=_73&240;
_73&=15;
_6c[ix]=_72=_6d[_73][_7b];
if(_74>0){
if(_74===16){
for(i=_76;i<ix;i++){
_6c[i]=1;
}
_76=-1;
}else{
_76=-1;
}
}
_75=_6d[_73][_7c];
if(_75){
if(_76===-1){
_76=ix;
}
}else{
if(_76>-1){
for(i=_76;i<ix;i++){
_6c[i]=_72;
}
_76=-1;
}
}
if(_77[ix]===_7d){
_6c[ix]=0;
}
bdx.hiLevel|=_72;
}
if(bdx.hasUbatS){
_7e(_77,_6c,len,bdx);
}
};
function _7e(_7f,_80,len,bdx){
for(var i=0;i<len;i++){
if(_7f[i]===_81){
_80[i]=bdx.dir;
for(var j=i-1;j>=0;j--){
if(_7f[j]===_82){
_80[j]=bdx.dir;
}else{
break;
}
}
}
}
};
function _69(_83,_84,bdx){
if(bdx.hiLevel===0||bdx.swap.substr(0,1)===bdx.swap.substr(1,2)){
return;
}
for(var i=0;i<_83.length;i++){
if(_84[i]===1){
_83[i]=_85(_83[i]);
}
}
};
function _79(ch){
var uc=ch.charCodeAt(0),hi=_86[uc>>8];
return (hi<_87)?hi:_88[hi-_87][uc&255];
};
function _31(str,bdx){
var _89=str.split("");
if(bdx){
var _8a=[];
_68(_89,_8a,bdx);
_1c=_8a;
}
_89.reverse();
_17.reverse();
return _89.join("");
};
function _42(_8b,_8c,idx){
for(var i=0;i<_8c;i++){
if(_8b[i]===idx){
return i;
}
}
return -1;
};
function _4c(c){
for(var i=0;i<_8d.length;i++){
if(c>=_8d[i]&&c<=_8e[i]){
return true;
}
}
return false;
};
function _54(_8f,_90,_91,_92){
while(((_90)*_91)<_92&&_4d(_8f[_90])){
_90+=_91;
}
if(((_90)*_91)<_92&&_4c(_8f[_90])){
return true;
}
return false;
};
function _4e(_93,_94,_95,_96){
while(((_94)*_95)<_96&&_4d(_93[_94])){
_94+=_95;
}
var c=" ";
if(((_94)*_95)<_96){
c=_93[_94];
}else{
return false;
}
for(var i=0;i<_64.length;i++){
if(_64[i]===c){
return true;
}
}
return false;
};
function _6a(lev,_97,_98,bdx){
if(bdx.hiLevel<lev){
return;
}
if(lev===1&&bdx.dir===RTL&&!bdx.hasUbatB){
_97.reverse();
_17.reverse();
return;
}
var len=_97.length,_99=0,end,lo,hi,tmp;
while(_99<len){
if(_98[_99]>=lev){
end=_99+1;
while(end<len&&_98[end]>=lev){
end++;
}
for(lo=_99,hi=end-1;lo<hi;lo++,hi--){
tmp=_97[lo];
_97[lo]=_97[hi];
_97[hi]=tmp;
tmp=_17[lo];
_17[lo]=_17[hi];
_17[hi]=tmp;
}
_99=end;
}
_99++;
}
};
function _7a(_9a,_9b,_9c,ix,bdx){
var _9d=_9b[ix],_9e={UBAT_L:function(){
bdx.lastArabic=false;
return _b9;
},UBAT_R:function(){
bdx.lastArabic=false;
return _ba;
},UBAT_ON:function(){
return _a4;
},UBAT_AN:function(){
return _bc;
},UBAT_EN:function(){
return bdx.lastArabic?_bc:_bb;
},UBAT_AL:function(){
bdx.lastArabic=true;
bdx.hasUbatAl=true;
return _ba;
},UBAT_WS:function(){
return _a4;
},UBAT_CS:function(){
var _9f,_a0;
if(ix<1||(ix+1)>=_9b.length||((_9f=_9c[ix-1])!==_bb&&_9f!==_bc)||((_a0=_9b[ix+1])!==_bb&&_a0!==_bc)){
return _a4;
}
if(bdx.lastArabic){
_a0=_bc;
}
return _a0===_9f?_a0:_a4;
},UBAT_ES:function(){
var _a1=ix>0?_9c[ix-1]:_7d;
if(_a1===_bb&&(ix+1)<_9b.length&&_9b[ix+1]===_bb){
return _bb;
}
return _a4;
},UBAT_ET:function(){
if(ix>0&&_9c[ix-1]===_bb){
return _bb;
}
if(bdx.lastArabic){
return _a4;
}
var i=ix+1,len=_9b.length;
while(i<len&&_9b[i]===_c0){
i++;
}
if(i<len&&_9b[i]===_bb){
return _bb;
}
return _a4;
},UBAT_NSM:function(){
if(bdx.inFormat==="VLTR"){
var len=_9b.length,i=ix+1;
while(i<len&&_9b[i]===_c1){
i++;
}
if(i<len){
var c=_9a[ix],_a2=(c>=1425&&c<=2303)||c===64286,_a3=_9b[i];
if(_a2&&(_a3===_ba||_a3===_bd)){
return _ba;
}
}
}
if(ix<1||_9b[ix-1]===_7d){
return _a4;
}
return _9c[ix-1];
},UBAT_B:function(){
bdx.lastArabic=true;
bdx.hasUbatB=true;
return bdx.dir;
},UBAT_S:function(){
bdx.hasUbatS=true;
return _a4;
},UBAT_LRE:function(){
bdx.lastArabic=false;
return _a4;
},UBAT_RLE:function(){
bdx.lastArabic=false;
return _a4;
},UBAT_LRO:function(){
bdx.lastArabic=false;
return _a4;
},UBAT_RLO:function(){
bdx.lastArabic=false;
return _a4;
},UBAT_PDF:function(){
bdx.lastArabic=false;
return _a4;
},UBAT_BN:function(){
return _a4;
}};
return _9e[_a5[_9d]]();
};
function _85(c){
var mid,low=0,_a6=_a7.length-1;
while(low<=_a6){
mid=Math.floor((low+_a6)/2);
if(c<_a7[mid][0]){
_a6=mid-1;
}else{
if(c>_a7[mid][0]){
low=mid+1;
}else{
return _a7[mid][1];
}
}
}
return c;
};
function _5a(c){
for(var i=0;i<_a8.length;i++){
if(_a8[i]===c){
return true;
}
}
return false;
};
function _55(c){
for(var i=0;i<_a9.length;i++){
if(c===_a9[i]){
return _aa[i];
}
}
return c;
};
function _56(c,_ab){
for(var i=0;i<_a9.length;i++){
if(c===_a9[i]){
return _ab[i];
}
}
return c;
};
function _4d(c){
return (c>="ً"&&c<="ٕ")?true:false;
};
function _d(oc){
if(oc==="L"){
return "LTR";
}
if(oc==="R"){
return "RTL";
}
if(oc==="C"){
return "CLR";
}
if(oc==="D"){
return "CRL";
}
};
function _52(_ac,_ad,_ae,_af){
while(((_ad)*_ae)<_af&&_4d(_ac[_ad])){
_ad+=_ae;
}
if(((_ad)*_ae)<_af){
_ac[_ad]=" ";
return true;
}
return false;
};
function _4f(_b0,_b1){
for(var i=0;i<_64.length;i++){
if(_b0===_64[i]){
return _b1[i];
}
}
return _b0;
};
function _8(_b2,_b3,_b4){
_17=[];
_1c=[];
for(var i=0;i<_b4;i++){
_b2[i]=i;
_b3[i]=i;
_17[i]=i;
}
};
function _18(_b5){
var map=new Array(_b5.length);
for(var i=0;i<_b5.length;i++){
map[_b5[i]]=i;
}
return map;
};
function _43(map,_b6,_b7,_b8){
for(var i=0;i<map.length;i++){
if(map[i]>_b6||(!_b7&&map[i]===_b6)){
map[i]+=_b8;
}
}
};
var _17=[];
var _19=[];
var _1c=[];
var _b={dir:0,defInFormat:"LLTR",defoutFormat:"VLTR",defSwap:"YN",inFormat:"LLTR",outFormat:"VLTR",swap:"YN",hiLevel:0,lastArabic:false,hasUbatAl:false,hasBlockSep:false,hasSegSep:false};
var _7b=5;
var _7c=6;
var LTR=0;
var RTL=1;
var _1e=/^[(I|V)][(L|R|C|D)][(Y|N)][(S|N)][N]$/;
var _25=/[\u0591-\u06ff\ufb1d-\ufefc]/;
var _a7=[["(",")"],[")","("],["<",">"],[">","<"],["[","]"],["]","["],["{","}"],["}","{"],["«","»"],["»","«"],["‹","›"],["›","‹"],["⁽","⁾"],["⁾","⁽"],["₍","₎"],["₎","₍"],["≤","≥"],["≥","≤"],["〈","〉"],["〉","〈"],["﹙","﹚"],["﹚","﹙"],["﹛","﹜"],["﹜","﹛"],["﹝","﹞"],["﹞","﹝"],["﹤","﹥"],["﹥","﹤"]];
var _64=["آ","أ","إ","ا"];
var _50=["ﻵ","ﻷ","ﻹ","ﻻ"];
var _51=["ﻶ","ﻸ","ﻺ","ﻼ"];
var _a9=["ا","ب","ت","ث","ج","ح","خ","د","ذ","ر","ز","س","ش","ص","ض","ط","ظ","ع","غ","ف","ق","ك","ل","م","ن","ه","و","ي","إ","أ","آ","ة","ى","ل","م","ن","ه","و","ي","إ","أ","آ","ة","ى","ی","ئ","ؤ"];
var _59=["ﺍ","ﺏ","ﺕ","ﺙ","ﺝ","ﺡ","ﺥ","ﺩ","ﺫ","ﺭ","ﺯ","ﺱ","ﺵ","ﺹ","ﺽ","ﻁ","ﻅ","ﻉ","ﻍ","ﻑ","ﻕ","ﻙ","ﻝ","ﻡ","ﻥ","ﻩ","ﻭ","ﻱ","ﺇ","ﺃ","ﺁ","ﺓ","ﻯ","ﯼ","ﺉ","ﺅ","ﹰ","ﹲ","ﹴ","ﹶ","ﹸ","ﹺ","ﹼ","ﹾ","ﺀ","ﺉ","ﺅ"];
var _57=["ﺎ","ﺐ","ﺖ","ﺚ","ﺞ","ﺢ","ﺦ","ﺪ","ﺬ","ﺮ","ﺰ","ﺲ","ﺶ","ﺺ","ﺾ","ﻂ","ﻆ","ﻊ","ﻎ","ﻒ","ﻖ","ﻚ","ﻞ","ﻢ","ﻦ","ﻪ","ﻮ","ﻲ","ﺈ","ﺄ","ﺂ","ﺔ","ﻰ","ﯽ","ﺊ","ﺆ","ﹰ","ﹲ","ﹴ","ﹶ","ﹸ","ﹺ","ﹼ","ﹾ","ﺀ","ﺊ","ﺆ"];
var _aa=["ﺎ","ﺒ","ﺘ","ﺜ","ﺠ","ﺤ","ﺨ","ﺪ","ﺬ","ﺮ","ﺰ","ﺴ","ﺸ","ﺼ","ﻀ","ﻄ","ﻈ","ﻌ","ﻐ","ﻔ","ﻘ","ﻜ","ﻠ","ﻤ","ﻨ","ﻬ","ﻮ","ﻴ","ﺈ","ﺄ","ﺂ","ﺔ","ﻰ","ﯿ","ﺌ","ﺆ","ﹱ","ﹲ","ﹴ","ﹷ","ﹹ","ﹻ","ﹽ","ﹿ","ﺀ","ﺌ","ﺆ"];
var _58=["ﺍ","ﺑ","ﺗ","ﺛ","ﺟ","ﺣ","ﺧ","ﺩ","ﺫ","ﺭ","ﺯ","ﺳ","ﺷ","ﺻ","ﺿ","ﻃ","ﻇ","ﻋ","ﻏ","ﻓ","ﻗ","ﻛ","ﻟ","ﻣ","ﻧ","ﻫ","ﻭ","ﻳ","ﺇ","ﺃ","ﺁ","ﺓ","ﻯ","ﯾ","ﺋ","ﺅ","ﹰ","ﹲ","ﹴ","ﹶ","ﹸ","ﹺ","ﹼ","ﹾ","ﺀ","ﺋ","ﺅ"];
var _a8=["ء","آ","أ","ؤ","إ","ا","ة","د","ذ","ر","ز","و","ى"];
var _65=["ً","ً","ٌ","؟","ٍ","؟","َ","َ","ُ","ُ","ِ","ِ","ّ","ّ","ْ","ْ","ء","آ","آ","أ","أ","ؤ","ؤ","إ","إ","ئ","ئ","ئ","ئ","ا","ا","ب","ب","ب","ب","ة","ة","ت","ت","ت","ت","ث","ث","ث","ث","ج","ج","ج","ج","ح","ح","ح","ح","خ","خ","خ","خ","د","د","ذ","ذ","ر","ر","ز","ز","س","س","س","س","ش","ش","ش","ش","ص","ص","ص","ص","ض","ض","ض","ض","ط","ط","ط","ط","ظ","ظ","ظ","ظ","ع","ع","ع","ع","غ","غ","غ","غ","ف","ف","ف","ف","ق","ق","ق","ق","ك","ك","ك","ك","ل","ل","ل","ل","م","م","م","م","ن","ن","ن","ن","ه","ه","ه","ه","و","و","ى","ى","ي","ي","ي","ي","ﻵ","ﻶ","ﻷ","ﻸ","ﻹ","ﻺ","ﻻ","ﻼ","؟","؟","؟"];
var _8d=["ء","ف"];
var _8e=["غ","ي"];
var _6f=[[0,3,0,1,0,0,0],[0,3,0,1,2,2,0],[0,3,0,17,2,0,1],[0,3,5,5,4,1,0],[0,3,21,21,4,0,1],[0,3,5,5,4,2,0]];
var _6e=[[2,0,1,1,0,1,0],[2,0,1,1,0,2,0],[2,0,2,1,3,2,0],[2,0,2,33,3,1,1]];
var _b9=0;
var _ba=1;
var _bb=2;
var _bc=3;
var _a4=4;
var _7d=5;
var _81=6;
var _bd=7;
var _82=8;
var _be=9;
var _bf=10;
var _c0=11;
var _c1=12;
var _c2=13;
var _c3=14;
var _c4=15;
var _c5=16;
var _c6=17;
var _c7=18;
var _a5=["UBAT_L","UBAT_R","UBAT_EN","UBAT_AN","UBAT_ON","UBAT_B","UBAT_S","UBAT_AL","UBAT_WS","UBAT_CS","UBAT_ES","UBAT_ET","UBAT_NSM","UBAT_LRE","UBAT_RLE","UBAT_PDF","UBAT_LRO","UBAT_RLO","UBAT_BN"];
var _87=100;
var _c8=_87+0;
var _c9=_87+1;
var _ca=_87+2;
var _cb=_87+3;
var _cc=_87+4;
var _cd=_87+5;
var _ce=_87+6;
var _cf=_87+7;
var L=_b9;
var R=_ba;
var EN=_bb;
var AN=_bc;
var ON=_a4;
var B=_7d;
var S=_81;
var AL=_bd;
var WS=_82;
var CS=_be;
var ES=_bf;
var ET=_c0;
var NSM=_c1;
var LRE=_c2;
var RLE=_c3;
var PDF=_c4;
var LRO=_c5;
var RLO=_c6;
var BN=_c7;
var _86=[_c8,L,L,L,L,_c9,_ca,_cb,R,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,_cc,ON,ON,ON,L,ON,L,ON,L,ON,ON,ON,L,L,ON,ON,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,ON,ON,L,L,ON,ON,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,_cd,AL,AL,_ce,_cf];
var _88=[[BN,BN,BN,BN,BN,BN,BN,BN,BN,S,B,S,WS,B,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,B,B,B,S,WS,ON,ON,ET,ET,ET,ON,ON,ON,ON,ON,ES,CS,ES,CS,CS,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,CS,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,BN,BN,BN,BN,BN,BN,B,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,CS,ON,ET,ET,ET,ET,ON,ON,ON,ON,L,ON,ON,BN,ON,ON,ET,ET,EN,EN,ON,L,ON,ON,ON,EN,L,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,L,L,L,L,L,L,L,L],[L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,L,L,L,L,L,L,L,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,L,ON,ON,ON,ON,ON,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,R,NSM,R,NSM,NSM,R,NSM,NSM,R,NSM,ON,ON,ON,ON,ON,ON,ON,ON,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,ON,ON,ON,ON,ON,R,R,R,R,R,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON],[AN,AN,AN,AN,ON,ON,ON,ON,AL,ET,ET,AL,CS,AL,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AL,ON,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AN,AN,AN,AN,AN,AN,AN,AN,AN,AN,ET,AN,AN,AL,AL,AL,NSM,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AN,ON,NSM,NSM,NSM,NSM,NSM,NSM,AL,AL,NSM,NSM,ON,NSM,NSM,NSM,NSM,AL,AL,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,AL,AL,AL,AL,AL,AL],[AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,ON,AL,AL,NSM,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AL,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,R,R,ON,ON,ON,ON,R,ON,ON,ON,ON,ON],[WS,WS,WS,WS,WS,WS,WS,WS,WS,WS,WS,BN,BN,BN,L,R,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,WS,B,LRE,RLE,PDF,LRO,RLO,CS,ET,ET,ET,ET,ET,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,CS,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,WS,BN,BN,BN,BN,BN,ON,ON,ON,ON,ON,BN,BN,BN,BN,BN,BN,EN,L,ON,ON,EN,EN,EN,EN,EN,EN,ES,ES,ON,ON,ON,L,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,ES,ES,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON],[L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,ON,ON,ON,ON,ON,R,NSM,R,R,R,R,R,R,R,R,R,R,ES,R,R,R,R,R,R,R,R,R,R,R,R,R,ON,R,R,R,R,R,ON,R,ON,R,R,ON,R,R,ON,R,R,R,R,R,R,R,R,R,R,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL],[NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,CS,ON,CS,ON,ON,CS,ON,ON,ON,ON,ON,ON,ON,ON,ON,ET,ON,ON,ES,ES,ON,ON,ON,ON,ON,ET,ET,ON,ON,ON,ON,ON,AL,AL,AL,AL,AL,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,ON,ON,BN],[ON,ON,ON,ET,ET,ET,ON,ON,ON,ON,ON,ES,CS,ES,CS,CS,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,CS,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,L,L,L,L,L,L,ON,ON,L,L,L,L,L,L,ON,ON,L,L,L,L,L,L,ON,ON,L,L,L,ON,ON,ON,ET,ET,ON,ON,ON,ET,ET,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON]];
return _4;
});

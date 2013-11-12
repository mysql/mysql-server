//>>built
define("dojox/string/BidiEngine",["dojo/_base/lang","dojo/_base/declare"],function(_1,_2){
_1.getObject("string",true,dojox);
_2("dojox.string.BidiEngine",null,{bidiTransform:function(_3,_4,_5){
if(!_3){
return "";
}
if(!_4&&!_5){
return _3;
}
var _6=/^[(I|V)][(L|R|C|D)][(Y|N)][(S|N)][N]$/;
if(!_6.test(_4)||!_6.test(_5)){
throw new Error("dojox.string.BidiEngine: the bidi layout string is wrong!");
}
if(_4==_5){
return _3;
}
var _7=_8(_4.charAt(1)),_9=_8(_5.charAt(1)),_a=(_4.charAt(0)=="I")?"L":_4.charAt(0),_b=(_5.charAt(0)=="I")?"L":_5.charAt(0),_c=_a+_7,_d=_b+_9,_e=_4.charAt(2)+_5.charAt(2);
if(_c){
_f.defInFormat=_c;
}
if(_d){
_f.defOutFormat=_d;
}
if(_e){
_f.defSwap=_e;
}
var _10=_11(_3,_a+_7,_b+_9,_4.charAt(2)+_5.charAt(2)),_12=false;
if(_5.charAt(1)=="R"){
_12=true;
}else{
if(_5.charAt(1)=="C"||_5.charAt(1)=="D"){
_12=this.checkContextual(_10);
}
}
if(_4.charAt(3)==_5.charAt(3)){
return _10;
}else{
if(_5.charAt(3)=="S"){
return _13(_12,_10,true);
}
}
if(_5.charAt(3)=="N"){
return _14(_10,_12,true);
}
},checkContextual:function(_15){
var dir=_16(_15);
if(dir!="ltr"&&dir!="rtl"){
dir=document.dir.toLowerCase();
if(dir!="ltr"&&dir!="rtl"){
dir="ltr";
}
}
return dir;
},hasBidiChar:function(_17){
var _18=null,uc=null,hi=null;
for(var i=0;i<_17.length;i++){
uc=_17.charAt(i).charCodeAt(0);
hi=_19[uc>>8];
_18=hi<_1a?hi:_1b[hi-_1a][uc&255];
if(_18==_1c||_18==_1d){
return true;
}
if(_18==_1e){
break;
}
}
return false;
}});
function _11(_1f,_20,_21,_22){
if(_20==undefined){
_20=_f.defInFormat;
}
if(_21==undefined){
_21=_f.defOutFormat;
}
if(_22==undefined){
_22=_f.defSwap;
}
if(_20==_21){
return _1f;
}
var dir,_23=_20.substring(0,1),_24=_20.substring(1,4),_25=_21.substring(0,1),_26=_21.substring(1,4);
if(_24.charAt(0)=="C"){
dir=_16(_1f);
if(dir=="ltr"||dir=="rtl"){
_24=dir.toUpperCase();
}else{
_24=_20.charAt(2)=="L"?"LTR":"RTL";
}
_20=_23+_24;
}
if(_26.charAt(0)=="C"){
dir=_16(_1f);
if(dir=="rtl"){
_26="RTL";
}else{
if(dir=="ltr"){
dir=_27(_1f);
_26=dir.toUpperCase();
}else{
_26=_21.charAt(2)=="L"?"LTR":"RTL";
}
}
_21=_25+_26;
}
if(_20==_21){
return _1f;
}
_f.inFormat=_20;
_f.outFormat=_21;
_f.swap=_22;
if((_23=="L")&&(_21=="VLTR")){
if(_24=="LTR"){
_f.dir=LTR;
return _28(_1f);
}
if(_24=="RTL"){
_f.dir=RTL;
return _28(_1f);
}
}
if((_23=="V")&&(_25=="V")){
return _29(_1f);
}
if((_23=="L")&&(_21=="VRTL")){
if(_24=="LTR"){
_f.dir=LTR;
_1f=_28(_1f);
}else{
_f.dir=RTL;
_1f=_28(_1f);
}
return _29(_1f);
}
if((_20=="VLTR")&&(_21=="LLTR")){
_f.dir=LTR;
return _28(_1f);
}
if((_23=="V")&&(_25=="L")&&(_24!=_26)){
_1f=_29(_1f);
return (_24=="RTL")?_11(_1f,"LLTR","VLTR",_22):_11(_1f,"LRTL","VRTL",_22);
}
if((_20=="VRTL")&&(_21=="LRTL")){
return _11(_1f,"LRTL","VRTL",_22);
}
if((_23=="L")&&(_25=="L")){
var _2a=_f.swap;
_f.swap=_2a.substr(0,1)+"N";
if(_24=="RTL"){
_f.dir=RTL;
_1f=_28(_1f);
_f.swap="N"+_2a.substr(1,2);
_f.dir=LTR;
_1f=_28(_1f);
}else{
_f.dir=LTR;
_1f=_28(_1f);
_f.swap="N"+_2a.substr(1,2);
_1f=_11(_1f,"VLTR","LRTL",_f.swap);
}
return _1f;
}
};
function _13(rtl,_2b,_2c){
if(_2b.length==0){
return;
}
if(rtl==undefined){
rtl=true;
}
if(_2c==undefined){
_2c=true;
}
_2b=new String(_2b);
var _2d=_2b.split(""),Ix=0,_2e=+1,_2f=_2d.length;
if(!rtl){
Ix=_2d.length-1;
_2e=-1;
_2f=1;
}
var _30=0,_31=[],_32=0;
for(var _33=Ix;_33*_2e<_2f;_33=_33+_2e){
if(_34(_2d[_33])||_35(_2d[_33])){
if(_2d[_33]=="ل"){
if(_36(_2d,(_33+_2e),_2e,_2f)){
_2d[_33]=(_30==0)?_37(_2d[_33+_2e],_38):_37(_2d[_33+_2e],_39);
_33+=_2e;
_3a(_2d,_33,_2e,_2f);
if(_2c){
_31[_32]=_33;
_32++;
}
_30=0;
continue;
}
}
var _3b=_2d[_33];
if(_30==1){
_2d[_33]=(_3c(_2d,(_33+_2e),_2e,_2f))?_3d(_2d[_33]):_3e(_2d[_33],_3f);
}else{
if(_3c(_2d,(_33+_2e),_2e,_2f)==true){
_2d[_33]=_3e(_2d[_33],_40);
}else{
_2d[_33]=_3e(_2d[_33],_41);
}
}
if(!_35(_3b)){
_30=1;
}
if(_42(_3b)==true){
_30=0;
}
}else{
_30=0;
}
}
var _43="";
for(idx=0;idx<_2d.length;idx++){
if(!(_2c&&_44(_31,_31.length,idx)>-1)){
_43+=_2d[idx];
}
}
return _43;
};
function _16(_45){
var _46=null,uc=null,hi=null;
for(var i=0;i<_45.length;i++){
uc=_45.charAt(i).charCodeAt(0);
hi=_19[uc>>8];
_46=hi<_1a?hi:_1b[hi-_1a][uc&255];
if(_46==_1c||_46==_1d){
return "rtl";
}
if(_46==_47){
return "ltr";
}
if(_46==_1e){
break;
}
}
return "";
};
function _27(_48){
var _49=null;
for(var i=_48.length-1;i>=0;i--){
_49=_4a(_48.charAt(i));
if(_49==_1c||_49==_1d){
return "rtl";
}
if(_49==_47){
return "ltr";
}
if(_49==_1e){
break;
}
}
return "";
};
function _14(_4b,rtl,_4c){
if(_4b.length==0){
return;
}
if(_4c==undefined){
_4c=true;
}
if(rtl==undefined){
rtl=true;
}
_4b=new String(_4b);
var _4d="",_4e=[],_4f="";
if(_4c){
for(var j=0;j<_4b.length;j++){
if(_4b.charAt(j)==" "){
if(rtl){
if(j>0){
if(_4b.charAt(j-1)>="ﻵ"&&_4b.charAt(j-1)<="ﻼ"){
continue;
}
}
}else{
if(j+1<_4b.length){
if(_4b.charAt(j+1)>="ﻵ"&&_4b.charAt(j+1)<="ﻼ"){
continue;
}
}
}
}
_4f+=_4b.charAt(j);
}
}else{
_4f=new String(_4b);
}
_4e=_4f.split("");
for(var i=0;i<_4f.length;i++){
if(_4e[i]>="ﹰ"&&_4e[i]<"﻿"){
var _50=_4f.charCodeAt(i);
if(_4e[i]>="ﻵ"&&_4e[i]<="ﻼ"){
if(rtl){
_4d+="ل";
_4d+=_51[parseInt((_50-65269)/2)];
}else{
_4d+=_51[parseInt((_50-65269)/2)];
_4d+="ل";
}
}else{
_4d+=_52[_50-65136];
}
}else{
_4d+=_4e[i];
}
}
return _4d;
};
function _28(str){
var _53=str.split(""),_54=[];
_55(_53,_54);
_56(_53,_54);
_57(2,_53,_54);
_57(1,_53,_54);
return _53.join("");
};
function _55(_58,_59){
var len=_58.length,_5a=_f.dir?_5b:_5c,_5d=null,_5e=null,_5f=null,_60=0,_61=null,_62=null,_63=-1,i=null,ix=null,_64=[],_65=[];
_f.hiLevel=_f.dir;
_f.lastArabic=false;
_f.hasUBAT_AL=false,_f.hasUBAT_B=false;
_f.hasUBAT_S=false;
for(i=0;i<len;i++){
_64[i]=_4a(_58[i]);
}
for(ix=0;ix<len;ix++){
_5d=_60;
_65[ix]=_5e=_66(_58,_64,_65,ix);
_60=_5a[_5d][_5e];
_61=_60&240;
_60&=15;
_59[ix]=_5f=_5a[_60][_67];
if(_61>0){
if(_61==16){
for(i=_63;i<ix;i++){
_59[i]=1;
}
_63=-1;
}else{
_63=-1;
}
}
_62=_5a[_60][_68];
if(_62){
if(_63==-1){
_63=ix;
}
}else{
if(_63>-1){
for(i=_63;i<ix;i++){
_59[i]=_5f;
}
_63=-1;
}
}
if(_64[ix]==_1e){
_59[ix]=0;
}
_f.hiLevel|=_5f;
}
if(_f.hasUBAT_S){
for(i=0;i<len;i++){
if(_64[i]==_69){
_59[i]=_f.dir;
for(var j=i-1;j>=0;j--){
if(_64[j]==_6a){
_59[j]=_f.dir;
}else{
break;
}
}
}
}
}
};
function _56(_6b,_6c){
if(_f.hiLevel==0||_f.swap.substr(0,1)==_f.swap.substr(1,2)){
return;
}
for(var i=0;i<_6b.length;i++){
if(_6c[i]==1){
_6b[i]=_6d(_6b[i]);
}
}
};
function _4a(ch){
var uc=ch.charCodeAt(0),hi=_19[uc>>8];
return (hi<_1a)?hi:_1b[hi-_1a][uc&255];
};
function _29(str){
var _6e=str.split("");
_6e.reverse();
return _6e.join("");
};
function _44(_6f,_70,idx){
var _71=-1;
for(var i=0;i<_70;i++){
if(_6f[i]==idx){
return i;
}
}
return -1;
};
function _34(c){
for(var i=0;i<_72.length;i++){
if(c>=_72[i]&&c<=_73[i]){
return true;
}
}
return false;
};
function _3c(_74,_75,_76,_77){
while(((_75)*_76)<_77&&_35(_74[_75])){
_75+=_76;
}
if(((_75)*_76)<_77&&_34(_74[_75])){
return true;
}
return false;
};
function _36(_78,_79,_7a,_7b){
while(((_79)*_7a)<_7b&&_35(_78[_79])){
_79+=_7a;
}
var c=" ";
if(((_79)*_7a)<_7b){
c=_78[_79];
}else{
return false;
}
for(var i=0;i<_51.length;i++){
if(_51[i]==c){
return true;
}
}
return false;
};
function _57(lev,_7c,_7d){
if(_f.hiLevel<lev){
return;
}
if(lev==1&&_f.dir==RTL&&!_f.hasUBAT_B){
_7c.reverse();
return;
}
var len=_7c.length,_7e=0,end,lo,hi,tmp;
while(_7e<len){
if(_7d[_7e]>=lev){
end=_7e+1;
while(end<len&&_7d[end]>=lev){
end++;
}
for(lo=_7e,hi=end-1;lo<hi;lo++,hi--){
tmp=_7c[lo];
_7c[lo]=_7c[hi];
_7c[hi]=tmp;
}
_7e=end;
}
_7e++;
}
};
function _66(_7f,_80,_81,ix){
var _82=_80[ix],_83,_84,len,i;
switch(_82){
case _47:
case _1c:
_f.lastArabic=false;
case _85:
case _86:
return _82;
case _87:
return _f.lastArabic?_86:_87;
case _1d:
_f.lastArabic=true;
_f.hasUBAT_AL=true;
return _1c;
case _6a:
return _85;
case _88:
if(ix<1||(ix+1)>=_80.length||((_83=_81[ix-1])!=_87&&_83!=_86)||((_84=_80[ix+1])!=_87&&_84!=_86)){
return _85;
}
if(_f.lastArabic){
_84=_86;
}
return _84==_83?_84:_85;
case _89:
_83=ix>0?_81[ix-1]:_1e;
if(_83==_87&&(ix+1)<_80.length&&_80[ix+1]==_87){
return _87;
}
return _85;
case _8a:
if(ix>0&&_81[ix-1]==_87){
return _87;
}
if(_f.lastArabic){
return _85;
}
i=ix+1;
len=_80.length;
while(i<len&&_80[i]==_8a){
i++;
}
if(i<len&&_80[i]==_87){
return _87;
}
return _85;
case _8b:
if(_f.inFormat=="VLTR"){
len=_80.length;
i=ix+1;
while(i<len&&_80[i]==_8b){
i++;
}
if(i<len){
var c=_7f[ix],_8c=(c>=1425&&c<=2303)||c==64286;
_83=_80[i];
if(_8c&&(_83==_1c||_83==_1d)){
return _1c;
}
}
}
if(ix<1||(_83=_80[ix-1])==_1e){
return _85;
}
return _81[ix-1];
case _1e:
lastArabic=false;
_f.hasUBAT_B=true;
return _f.dir;
case _69:
_f.hasUBAT_S=true;
return _85;
case _8d:
case _8e:
case _8f:
case _90:
case _91:
lastArabic=false;
case _92:
return _85;
}
};
function _6d(c){
var mid,low=0,_93=_94.length-1;
while(low<=_93){
mid=Math.floor((low+_93)/2);
if(c<_94[mid][0]){
_93=mid-1;
}else{
if(c>_94[mid][0]){
low=mid+1;
}else{
return _94[mid][1];
}
}
}
return c;
};
function _42(c){
for(var i=0;i<_95.length;i++){
if(_95[i]==c){
return true;
}
}
return false;
};
function _3d(c){
for(var i=0;i<_96.length;i++){
if(c==_96[i]){
return _97[i];
}
}
return c;
};
function _3e(c,_98){
for(var i=0;i<_96.length;i++){
if(c==_96[i]){
return _98[i];
}
}
return c;
};
function _35(c){
return (c>="ً"&&c<="ٕ")?true:false;
};
function _8(oc){
if(oc=="L"){
return "LTR";
}
if(oc=="R"){
return "RTL";
}
if(oc=="C"){
return "CLR";
}
if(oc=="D"){
return "CRL";
}
};
function _3a(_99,_9a,_9b,_9c){
while(((_9a)*_9b)<_9c&&_35(_99[_9a])){
_9a+=_9b;
}
if(((_9a)*_9b)<_9c){
_99[_9a]=" ";
return true;
}
return false;
};
function _37(_9d,_9e){
for(var i=0;i<_51.length;i++){
if(_9d==_51[i]){
return _9e[i];
}
}
return _9d;
};
function _9f(_a0){
for(var i=0;i<_51.length;i++){
if(_51[i]==_a0){
return _51[i];
}
}
return 0;
};
var _f={dir:0,defInFormat:"LLTR",defoutFormat:"VLTR",defSwap:"YN",inFormat:"LLTR",outFormat:"VLTR",swap:"YN",hiLevel:0,lastArabic:false,hasUBAT_AL:false,hasBlockSep:false,hasSegSep:false};
var _67=5;
var _68=6;
var LTR=0;
var RTL=1;
var _94=[["(",")"],[")","("],["<",">"],[">","<"],["[","]"],["]","["],["{","}"],["}","{"],["«","»"],["»","«"],["‹","›"],["›","‹"],["⁽","⁾"],["⁾","⁽"],["₍","₎"],["₎","₍"],["≤","≥"],["≥","≤"],["〈","〉"],["〉","〈"],["﹙","﹚"],["﹚","﹙"],["﹛","﹜"],["﹜","﹛"],["﹝","﹞"],["﹞","﹝"],["﹤","﹥"],["﹥","﹤"]];
var _51=["آ","أ","إ","ا"];
var _a1=[65153,65154,65155,65156,65159,65160,65165,65166];
var _a2=[65245,65246,65247,65248];
var _38=["ﻵ","ﻷ","ﻹ","ﻻ"];
var _39=["ﻶ","ﻸ","ﻺ","ﻼ"];
var _96=["ا","ب","ت","ث","ج","ح","خ","د","ذ","ر","ز","س","ش","ص","ض","ط","ظ","ع","غ","ف","ق","ك","ل","م","ن","ه","و","ي","إ","أ","آ","ة","ى","ی","ئ","ؤ","ً","ٌ","ٍ","َ","ُ","ِ","ّ","ْ","ء"];
var _41=["ﺍ","ﺏ","ﺕ","ﺙ","ﺝ","ﺡ","ﺥ","ﺩ","ﺫ","ﺭ","ﺯ","ﺱ","ﺵ","ﺹ","ﺽ","ﻁ","ﻅ","ﻉ","ﻍ","ﻑ","ﻕ","ﻙ","ﻝ","ﻡ","ﻥ","ﻩ","ﻭ","ﻱ","ﺇ","ﺃ","ﺁ","ﺓ","ﻯ","ﯼ","ﺉ","ﺅ","ﹰ","ﹲ","ﹴ","ﹶ","ﹸ","ﹺ","ﹼ","ﹾ","ﺀ"];
var _3f=["ﺎ","ﺐ","ﺖ","ﺚ","ﺞ","ﺢ","ﺦ","ﺪ","ﺬ","ﺮ","ﺰ","ﺲ","ﺶ","ﺺ","ﺾ","ﻂ","ﻆ","ﻊ","ﻎ","ﻒ","ﻖ","ﻚ","ﻞ","ﻢ","ﻦ","ﻪ","ﻮ","ﻲ","ﺈ","ﺄ","ﺂ","ﺔ","ﻰ","ﯽ","ﺊ","ﺆ","ﹰ","ﹲ","ﹴ","ﹶ","ﹸ","ﹺ","ﹼ","ﹾ","ﺀ"];
var _97=["ﺎ","ﺒ","ﺘ","ﺜ","ﺠ","ﺤ","ﺨ","ﺪ","ﺬ","ﺮ","ﺰ","ﺴ","ﺸ","ﺼ","ﻀ","ﻄ","ﻈ","ﻌ","ﻐ","ﻔ","ﻘ","ﻜ","ﻠ","ﻤ","ﻨ","ﻬ","ﻮ","ﻴ","ﺈ","ﺄ","ﺂ","ﺔ","ﻰ","ﯿ","ﺌ","ﺆ","ﹱ","ﹲ","ﹴ","ﹷ","ﹹ","ﹻ","ﹽ","ﹿ","ﺀ"];
var _40=["ﺍ","ﺑ","ﺗ","ﺛ","ﺟ","ﺣ","ﺧ","ﺩ","ﺫ","ﺭ","ﺯ","ﺳ","ﺷ","ﺻ","ﺿ","ﻃ","ﻇ","ﻋ","ﻏ","ﻓ","ﻗ","ﻛ","ﻟ","ﻣ","ﻧ","ﻫ","ﻭ","ﻳ","ﺇ","ﺃ","ﺁ","ﺓ","ﻯ","ﯾ","ﺋ","ﺅ","ﹰ","ﹲ","ﹴ","ﹶ","ﹸ","ﹺ","ﹼ","ﹾ","ﺀ"];
var _95=["ء","ا","د","ذ","ر","ز","و","آ","ة","ئ","ؤ","إ","ٵ","أ"];
var _52=["ً","ً","ٌ","؟","ٍ","؟","َ","َ","ُ","ُ","ِ","ِ","ّ","ّ","ْ","ْ","ء","آ","آ","أ","أ","ؤ","ؤ","إ","إ","ئ","ئ","ئ","ئ","ا","ا","ب","ب","ب","ب","ة","ة","ت","ت","ت","ت","ث","ث","ث","ث","ج","ج","ج","ج","ح","ح","ح","ح","خ","خ","خ","خ","د","د","ذ","ذ","ر","ر","ز","ز","س","س","س","س","ش","ش","ش","ش","ص","ص","ص","ص","ض","ض","ض","ض","ط","ط","ط","ط","ظ","ظ","ظ","ظ","ع","ع","ع","ع","غ","غ","غ","غ","ف","ف","ف","ف","ق","ق","ق","ق","ك","ك","ك","ك","ل","ل","ل","ل","م","م","م","م","ن","ن","ن","ن","ه","ه","ه","ه","و","و","ى","ى","ي","ي","ي","ي","ﻵ","ﻶ","ﻷ","ﻸ","ﻹ","ﻺ","ﻻ","ﻼ","؟","؟","؟"];
var _72=["ء","ف"];
var _73=["غ","ي"];
var _a3=[1+32+256*17,1+32+256*19,1+256*21,1+32+256*23,1+2+256*25,1+32+256*29,1+2+256*31,1+256*35,1+2+256*37,1+2+256*41,1+2+256*45,1+2+256*49,1+2+256*53,1+256*57,1+256*59,1+256*61,1+256*63,1+2+256*65,1+2+256*69,1+2+256*73,1+2+256*77,1+2+256*81,1+2+256*85,1+2+256*89,1+2+256*93,0,0,0,0,0,1+2,1+2+256*97,1+2+256*101,1+2+256*105,1+2+16+256*109,1+2+256*113,1+2+256*117,1+2+256*121,1+256*125,1+256*127,1+2+256*129,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,1+256*133,1+256*135,1+256*137,1+256*139,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,1+32,1+32,0,1+32,1,1,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1,1+2,1,1,1,1,1,1,1,1,1,1,1+2,1,1+2,1+2,1+2,1+2,1,1];
var _a4=[1+2,1+2,1+2,0,1+2,0,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,1+2,0,0+32,1+32,0+32,1+32,0,1,0+32,1+32,0,2,1+2,1,0+32,1+32,0,2,1+2,1,0,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,1,0,1,0,1,0,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0+16,2+16,1+2+16,1+16,0,2,1+2,1,0,2,1+2,1,0,2,1+2,1,0,1,0,1,0,2,1+2,1,0,1,0,1,0,1,0,1];
var _5c=[[0,3,0,1,0,0,0],[0,3,0,1,2,2,0],[0,3,0,17,2,0,1],[0,3,5,5,4,1,0],[0,3,21,21,4,0,1],[0,3,5,5,4,2,0]];
var _5b=[[2,0,1,1,0,1,0],[2,0,1,1,0,2,0],[2,0,2,1,3,2,0],[2,0,2,33,3,1,1]];
var _47=0;
var _1c=1;
var _87=2;
var _86=3;
var _85=4;
var _1e=5;
var _69=6;
var _1d=7;
var _6a=8;
var _88=9;
var _89=10;
var _8a=11;
var _8b=12;
var _8d=13;
var _8e=14;
var _91=15;
var _8f=16;
var _90=17;
var _92=18;
var _1a=100;
var _a5=_1a+0;
var _a6=_1a+1;
var _a7=_1a+2;
var _a8=_1a+3;
var _a9=_1a+4;
var _aa=_1a+5;
var _ab=_1a+6;
var _ac=_1a+7;
var L=_47;
var R=_1c;
var EN=_87;
var AN=_86;
var ON=_85;
var B=_1e;
var S=_69;
var AL=_1d;
var WS=_6a;
var CS=_88;
var ES=_89;
var ET=_8a;
var NSM=_8b;
var LRE=_8d;
var RLE=_8e;
var PDF=_91;
var LRO=_8f;
var RLO=_90;
var BN=_92;
var _19=[_a5,L,L,L,L,_a6,_a7,_a8,R,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,_a9,ON,ON,ON,L,ON,L,ON,L,ON,ON,ON,L,L,ON,ON,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,ON,ON,L,L,ON,ON,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,_aa,AL,AL,_ab,_ac];
delete _a5;
delete _a6;
delete _a7;
delete _a8;
delete _a9;
delete _aa;
delete _ab;
delete _ac;
var _1b=[[BN,BN,BN,BN,BN,BN,BN,BN,BN,S,B,S,WS,B,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,B,B,B,S,WS,ON,ON,ET,ET,ET,ON,ON,ON,ON,ON,ES,CS,ES,CS,CS,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,CS,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,BN,BN,BN,BN,BN,BN,B,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,BN,CS,ON,ET,ET,ET,ET,ON,ON,ON,ON,L,ON,ON,BN,ON,ON,ET,ET,EN,EN,ON,L,ON,ON,ON,EN,L,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,L,L,L,L,L,L,L,L],[L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,L,L,L,L,L,L,L,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,L,ON,ON,ON,ON,ON,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,R,NSM,R,NSM,NSM,R,NSM,NSM,R,NSM,ON,ON,ON,ON,ON,ON,ON,ON,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,ON,ON,ON,ON,ON,R,R,R,R,R,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON],[AN,AN,AN,AN,ON,ON,ON,ON,AL,ET,ET,AL,CS,AL,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AL,ON,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AN,AN,AN,AN,AN,AN,AN,AN,AN,AN,ET,AN,AN,AL,AL,AL,NSM,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AN,ON,NSM,NSM,NSM,NSM,NSM,NSM,AL,AL,NSM,NSM,ON,NSM,NSM,NSM,NSM,AL,AL,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,AL,AL,AL,AL,AL,AL],[AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,ON,AL,AL,NSM,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,AL,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,R,R,ON,ON,ON,ON,R,ON,ON,ON,ON,ON],[WS,WS,WS,WS,WS,WS,WS,WS,WS,WS,WS,BN,BN,BN,L,R,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,WS,B,LRE,RLE,PDF,LRO,RLO,CS,ET,ET,ET,ET,ET,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,CS,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,WS,BN,BN,BN,BN,BN,ON,ON,ON,ON,ON,BN,BN,BN,BN,BN,BN,EN,L,ON,ON,EN,EN,EN,EN,EN,EN,ES,ES,ON,ON,ON,L,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,ES,ES,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ET,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON],[L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,ON,ON,ON,ON,ON,R,NSM,R,R,R,R,R,R,R,R,R,R,ES,R,R,R,R,R,R,R,R,R,R,R,R,R,ON,R,R,R,R,R,ON,R,ON,R,R,ON,R,R,ON,R,R,R,R,R,R,R,R,R,R,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL],[NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,NSM,NSM,NSM,NSM,NSM,NSM,NSM,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,CS,ON,CS,ON,ON,CS,ON,ON,ON,ON,ON,ON,ON,ON,ON,ET,ON,ON,ES,ES,ON,ON,ON,ON,ON,ET,ET,ON,ON,ON,ON,ON,AL,AL,AL,AL,AL,ON,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,AL,ON,ON,BN],[ON,ON,ON,ET,ET,ET,ON,ON,ON,ON,ON,ES,CS,ES,CS,CS,EN,EN,EN,EN,EN,EN,EN,EN,EN,EN,CS,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,L,ON,ON,ON,L,L,L,L,L,L,ON,ON,L,L,L,L,L,L,ON,ON,L,L,L,L,L,L,ON,ON,L,L,L,ON,ON,ON,ET,ET,ON,ON,ON,ET,ET,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON,ON]];
delete L;
delete R;
delete EN;
delete AN;
delete ON;
delete B;
delete S;
delete AL;
delete WS;
delete CS;
delete ES;
delete ET;
delete NSM;
delete LRE;
delete RLE;
delete PDF;
delete LRO;
delete RLO;
delete BN;
return dojox.string.BidiEngine;
});

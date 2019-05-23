//>>built
define("dojox/validate/br",["dojo/_base/lang","./_base"],function(_1,_2){
var br=_1.getObject("br",true,_2);
br.isValidCnpj=function(_3){
if(!_1.isString(_3)){
if(!_3){
return false;
}
_3=_3+"";
while(_3.length<14){
_3="0"+_3;
}
}
var _4={format:["##.###.###/####-##","########/####-##","############-##","##############"]};
if(_2.isNumberFormat(_3,_4)){
_3=_3.replace("/","").replace(/\./g,"").replace("-","");
var _5=[];
var dv=[];
var i,j,_6;
for(i=0;i<10;i++){
_6="";
for(j=0;j<_3.length;j++){
_6+=""+i;
}
if(_3===_6){
return false;
}
}
for(i=0;i<12;i++){
_5.push(parseInt(_3.charAt(i),10));
}
for(i=12;i<14;i++){
dv.push(parseInt(_3.charAt(i),10));
}
var _7=[9,8,7,6,5,4,3,2,9,8,7,6].reverse();
var _8=0;
for(i=0;i<_5.length;i++){
_8+=_5[i]*_7[i];
}
var _9=_a(_8);
if(_9==dv[0]){
_8=0;
_7=[9,8,7,6,5,4,3,2,9,8,7,6,5].reverse();
_5.push(_9);
for(i=0;i<_5.length;i++){
_8+=_5[i]*_7[i];
}
var _b=_a(_8);
if(_b===dv[1]){
return true;
}
}
}
return false;
};
br.computeCnpjDv=function(_c){
if(!_1.isString(_c)){
if(!_c){
return "";
}
_c=_c+"";
while(_c.length<12){
_c="0"+_c;
}
}
var _d={format:["##.###.###/####","########/####","############"]};
if(_2.isNumberFormat(_c,_d)){
_c=_c.replace("/","").replace(/\./g,"");
var _e=[];
var i,j,_f;
for(i=0;i<10;i++){
_f="";
for(j=0;j<_c.length;j++){
_f+=""+i;
}
if(_c===_f){
return "";
}
}
for(i=0;i<_c.length;i++){
_e.push(parseInt(_c.charAt(i),10));
}
var _10=[9,8,7,6,5,4,3,2,9,8,7,6].reverse();
var sum=0;
for(i=0;i<_e.length;i++){
sum+=_e[i]*_10[i];
}
var dv0=_a(sum);
sum=0;
_10=[9,8,7,6,5,4,3,2,9,8,7,6,5].reverse();
_e.push(dv0);
for(i=0;i<_e.length;i++){
sum+=_e[i]*_10[i];
}
var dv1=_a(sum);
return (""+dv0)+dv1;
}
return "";
};
br.isValidCpf=function(_11){
if(!_1.isString(_11)){
if(!_11){
return false;
}
_11=_11+"";
while(_11.length<11){
_11="0"+_11;
}
}
var _12={format:["###.###.###-##","#########-##","###########"]};
if(_2.isNumberFormat(_11,_12)){
_11=_11.replace("-","").replace(/\./g,"");
var cpf=[];
var dv=[];
var i,j,tmp;
for(i=0;i<10;i++){
tmp="";
for(j=0;j<_11.length;j++){
tmp+=""+i;
}
if(_11===tmp){
return false;
}
}
for(i=0;i<9;i++){
cpf.push(parseInt(_11.charAt(i),10));
}
for(i=9;i<12;i++){
dv.push(parseInt(_11.charAt(i),10));
}
var _13=[9,8,7,6,5,4,3,2,1].reverse();
var sum=0;
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_13[i];
}
var dv0=_a(sum);
if(dv0==dv[0]){
sum=0;
_13=[9,8,7,6,5,4,3,2,1,0].reverse();
cpf.push(dv0);
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_13[i];
}
var dv1=_a(sum);
if(dv1===dv[1]){
return true;
}
}
}
return false;
};
br.computeCpfDv=function(_14){
if(!_1.isString(_14)){
if(!_14){
return "";
}
_14=_14+"";
while(_14.length<9){
_14="0"+_14;
}
}
var _15={format:["###.###.###","#########"]};
if(_2.isNumberFormat(_14,_15)){
_14=_14.replace(/\./g,"");
var cpf=[];
for(i=0;i<10;i++){
tmp="";
for(j=0;j<_14.length;j++){
tmp+=""+i;
}
if(_14===tmp){
return "";
}
}
for(i=0;i<_14.length;i++){
cpf.push(parseInt(_14.charAt(i),10));
}
var _16=[9,8,7,6,5,4,3,2,1].reverse();
var sum=0;
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_16[i];
}
var dv0=_a(sum);
sum=0;
_16=[9,8,7,6,5,4,3,2,1,0].reverse();
cpf.push(dv0);
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_16[i];
}
var dv1=_a(sum);
return (""+dv0)+dv1;
}
return "";
};
var _a=function(sum){
var dv=sum%11;
if(dv===10){
dv=0;
}
return dv;
};
return br;
});

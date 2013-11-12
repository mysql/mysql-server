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
var _9=_8%11;
if(_9==dv[0]){
_8=0;
_7=[9,8,7,6,5,4,3,2,9,8,7,6,5].reverse();
_5.push(_9);
for(i=0;i<_5.length;i++){
_8+=_5[i]*_7[i];
}
var _a=_8%11;
if(_a===dv[1]){
return true;
}
}
}
return false;
};
br.computeCnpjDv=function(_b){
if(!_1.isString(_b)){
if(!_b){
return "";
}
_b=_b+"";
while(_b.length<12){
_b="0"+_b;
}
}
var _c={format:["##.###.###/####","########/####","############"]};
if(_2.isNumberFormat(_b,_c)){
_b=_b.replace("/","").replace(/\./g,"");
var _d=[];
var i,j,_e;
for(i=0;i<10;i++){
_e="";
for(j=0;j<_b.length;j++){
_e+=""+i;
}
if(_b===_e){
return "";
}
}
for(i=0;i<_b.length;i++){
_d.push(parseInt(_b.charAt(i),10));
}
var _f=[9,8,7,6,5,4,3,2,9,8,7,6].reverse();
var sum=0;
for(i=0;i<_d.length;i++){
sum+=_d[i]*_f[i];
}
var dv0=sum%11;
sum=0;
_f=[9,8,7,6,5,4,3,2,9,8,7,6,5].reverse();
_d.push(dv0);
for(i=0;i<_d.length;i++){
sum+=_d[i]*_f[i];
}
var dv1=sum%11;
return (""+dv0)+dv1;
}
return "";
};
br.isValidCpf=function(_10){
if(!_1.isString(_10)){
if(!_10){
return false;
}
_10=_10+"";
while(_10.length<11){
_10="0"+_10;
}
}
var _11={format:["###.###.###-##","#########-##","###########"]};
if(_2.isNumberFormat(_10,_11)){
_10=_10.replace("-","").replace(/\./g,"");
var cpf=[];
var dv=[];
var i,j,tmp;
for(i=0;i<10;i++){
tmp="";
for(j=0;j<_10.length;j++){
tmp+=""+i;
}
if(_10===tmp){
return false;
}
}
for(i=0;i<9;i++){
cpf.push(parseInt(_10.charAt(i),10));
}
for(i=9;i<12;i++){
dv.push(parseInt(_10.charAt(i),10));
}
var _12=[9,8,7,6,5,4,3,2,1].reverse();
var sum=0;
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_12[i];
}
var dv0=sum%11;
if(dv0==dv[0]){
sum=0;
_12=[9,8,7,6,5,4,3,2,1,0].reverse();
cpf.push(dv0);
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_12[i];
}
var dv1=sum%11;
if(dv1===dv[1]){
return true;
}
}
}
return false;
};
br.computeCpfDv=function(_13){
if(!_1.isString(_13)){
if(!_13){
return "";
}
_13=_13+"";
while(_13.length<9){
_13="0"+_13;
}
}
var _14={format:["###.###.###","#########"]};
if(_2.isNumberFormat(_13,_14)){
_13=_13.replace(/\./g,"");
var cpf=[];
for(i=0;i<10;i++){
tmp="";
for(j=0;j<_13.length;j++){
tmp+=""+i;
}
if(_13===tmp){
return "";
}
}
for(i=0;i<_13.length;i++){
cpf.push(parseInt(_13.charAt(i),10));
}
var _15=[9,8,7,6,5,4,3,2,1].reverse();
var sum=0;
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_15[i];
}
var dv0=sum%11;
sum=0;
_15=[9,8,7,6,5,4,3,2,1,0].reverse();
cpf.push(dv0);
for(i=0;i<cpf.length;i++){
sum+=cpf[i]*_15[i];
}
var dv1=sum%11;
return (""+dv0)+dv1;
}
return "";
};
return br;
});

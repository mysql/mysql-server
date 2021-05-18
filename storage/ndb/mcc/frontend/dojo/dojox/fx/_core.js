//>>built
define("dojox/fx/_core",["dojo/_base/lang","dojo/_base/array","./_base"],function(_1,_2,_3){
var _4=function(_5,_6){
this.start=_5;
this.end=_6;
var _7=_1.isArray(_5),d=(_7?[]:_6-_5);
if(_7){
_2.forEach(this.start,function(s,i){
d[i]=this.end[i]-s;
},this);
this.getValue=function(n){
var _8=[];
_2.forEach(this.start,function(s,i){
_8[i]=(d[i]*n)+s;
},this);
return _8;
};
}else{
this.getValue=function(n){
return (d*n)+this.start;
};
}
};
_3._Line=_4;
return _4;
});

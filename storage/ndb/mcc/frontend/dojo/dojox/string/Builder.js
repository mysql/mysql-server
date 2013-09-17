//>>built
define("dojox/string/Builder",["dojo/_base/lang"],function(_1){
_1.getObject("string",true,dojox).Builder=function(_2){
var b="";
this.length=0;
this.append=function(s){
if(arguments.length>1){
var _3="",l=arguments.length;
switch(l){
case 9:
_3=""+arguments[8]+_3;
case 8:
_3=""+arguments[7]+_3;
case 7:
_3=""+arguments[6]+_3;
case 6:
_3=""+arguments[5]+_3;
case 5:
_3=""+arguments[4]+_3;
case 4:
_3=""+arguments[3]+_3;
case 3:
_3=""+arguments[2]+_3;
case 2:
b+=""+arguments[0]+arguments[1]+_3;
break;
default:
var i=0;
while(i<arguments.length){
_3+=arguments[i++];
}
b+=_3;
}
}else{
b+=s;
}
this.length=b.length;
return this;
};
this.concat=function(s){
return this.append.apply(this,arguments);
};
this.appendArray=function(_4){
return this.append.apply(this,_4);
};
this.clear=function(){
b="";
this.length=0;
return this;
};
this.replace=function(_5,_6){
b=b.replace(_5,_6);
this.length=b.length;
return this;
};
this.remove=function(_7,_8){
if(_8===undefined){
_8=b.length;
}
if(_8==0){
return this;
}
b=b.substr(0,_7)+b.substr(_7+_8);
this.length=b.length;
return this;
};
this.insert=function(_9,_a){
if(_9==0){
b=_a+b;
}else{
b=b.slice(0,_9)+_a+b.slice(_9);
}
this.length=b.length;
return this;
};
this.toString=function(){
return b;
};
if(_2){
this.append(_2);
}
};
return dojox.string.Builder;
});

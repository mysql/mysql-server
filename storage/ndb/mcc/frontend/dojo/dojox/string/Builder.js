//>>built
define("dojox/string/Builder",["dojo/_base/lang"],function(_1){
var _2=_1.getObject("string",true,dojox).Builder=function(_3){
var b="";
this.length=0;
this.append=function(s){
if(arguments.length>1){
var _4="",l=arguments.length;
switch(l){
case 9:
_4=""+arguments[8]+_4;
case 8:
_4=""+arguments[7]+_4;
case 7:
_4=""+arguments[6]+_4;
case 6:
_4=""+arguments[5]+_4;
case 5:
_4=""+arguments[4]+_4;
case 4:
_4=""+arguments[3]+_4;
case 3:
_4=""+arguments[2]+_4;
case 2:
b+=""+arguments[0]+arguments[1]+_4;
break;
default:
var i=0;
while(i<arguments.length){
_4+=arguments[i++];
}
b+=_4;
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
this.appendArray=function(_5){
return this.append.apply(this,_5);
};
this.clear=function(){
b="";
this.length=0;
return this;
};
this.replace=function(_6,_7){
b=b.replace(_6,_7);
this.length=b.length;
return this;
};
this.remove=function(_8,_9){
if(_9===undefined){
_9=b.length;
}
if(_9==0){
return this;
}
b=b.substr(0,_8)+b.substr(_8+_9);
this.length=b.length;
return this;
};
this.insert=function(_a,_b){
if(_a==0){
b=_b+b;
}else{
b=b.slice(0,_a)+_b+b.slice(_a);
}
this.length=b.length;
return this;
};
this.toString=function(){
return b;
};
if(_3){
this.append(_3);
}
};
return _2;
});

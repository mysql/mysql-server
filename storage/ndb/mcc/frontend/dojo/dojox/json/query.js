//>>built
define("dojox/json/query",["dojo/_base/kernel","dojox","dojo/_base/array"],function(_1,_2){
_1.getObject("json",true,_2);
_2.json._slice=function(_3,_4,_5,_6){
var _7=_3.length,_8=[];
_5=_5||_7;
_4=(_4<0)?Math.max(0,_4+_7):Math.min(_7,_4);
_5=(_5<0)?Math.max(0,_5+_7):Math.min(_7,_5);
for(var i=_4;i<_5;i+=_6){
_8.push(_3[i]);
}
return _8;
};
_2.json._find=function e(_9,_a){
var _b=[];
function _c(_d){
if(_a){
if(_a===true&&!(_d instanceof Array)){
_b.push(_d);
}else{
if(_d[_a]){
_b.push(_d[_a]);
}
}
}
for(var i in _d){
var _e=_d[i];
if(!_a){
_b.push(_e);
}else{
if(_e&&typeof _e=="object"){
_c(_e);
}
}
}
};
if(_a instanceof Array){
if(_a.length==1){
return _9[_a[0]];
}
for(var i=0;i<_a.length;i++){
_b.push(_9[_a[i]]);
}
}else{
_c(_9);
}
return _b;
};
_2.json._distinctFilter=function(_f,_10){
var _11=[];
var _12={};
for(var i=0,l=_f.length;i<l;++i){
var _13=_f[i];
if(_10(_13,i,_f)){
if((typeof _13=="object")&&_13){
if(!_13.__included){
_13.__included=true;
_11.push(_13);
}
}else{
if(!_12[_13+typeof _13]){
_12[_13+typeof _13]=true;
_11.push(_13);
}
}
}
}
for(i=0,l=_11.length;i<l;++i){
if(_11[i]){
delete _11[i].__included;
}
}
return _11;
};
return _2.json.query=function(_14,obj){
var _15=0;
var str=[];
_14=_14.replace(/"(\\.|[^"\\])*"|'(\\.|[^'\\])*'|[\[\]]/g,function(t){
_15+=t=="["?1:t=="]"?-1:0;
return (t=="]"&&_15>0)?"`]":(t.charAt(0)=="\""||t.charAt(0)=="'")?"`"+(str.push(t)-1):t;
});
var _16="";
function _17(_18){
_16=_18+"("+_16;
};
function _19(t,a,b,c,d,e,f,g){
return str[g].match(/[\*\?]/)||f=="~"?"/^"+str[g].substring(1,str[g].length-1).replace(/\\([btnfr\\"'])|([^\w\*\?])/g,"\\$1$2").replace(/([\*\?])/g,"[\\w\\W]$1")+(f=="~"?"$/i":"$/")+".test("+a+")":t;
};
_14.replace(/(\]|\)|push|pop|shift|splice|sort|reverse)\s*\(/,function(){
throw new Error("Unsafe function call");
});
_14=_14.replace(/([^<>=]=)([^=])/g,"$1=$2").replace(/@|(\.\s*)?[a-zA-Z\$_]+(\s*:)?/g,function(t){
return t.charAt(0)=="."?t:t=="@"?"$obj":(t.match(/:|^(\$|Math|true|false|null)$/)?"":"$obj.")+t;
}).replace(/\.?\.?\[(`\]|[^\]])*\]|\?.*|\.\.([\w\$_]+)|\.\*/g,function(t,a,b){
var _1a=t.match(/^\.?\.?(\[\s*\^?\?|\^?\?|\[\s*==)(.*?)\]?$/);
if(_1a){
var _1b="";
if(t.match(/^\./)){
_17("dojox.json._find");
_1b=",true)";
}
_17(_1a[1].match(/\=/)?"dojo.map":_1a[1].match(/\^/)?"dojox.json._distinctFilter":"dojo.filter");
return _1b+",function($obj){return "+_1a[2]+"})";
}
_1a=t.match(/^\[\s*([\/\\].*)\]/);
if(_1a){
return ".concat().sort(function(a,b){"+_1a[1].replace(/\s*,?\s*([\/\\])\s*([^,\\\/]+)/g,function(t,a,b){
return "var av= "+b.replace(/\$obj/,"a")+",bv= "+b.replace(/\$obj/,"b")+";if(av>bv||bv==null){return "+(a=="/"?1:-1)+";}\n"+"if(bv>av||av==null){return "+(a=="/"?-1:1)+";}\n";
})+"return 0;})";
}
_1a=t.match(/^\[(-?[0-9]*):(-?[0-9]*):?(-?[0-9]*)\]/);
if(_1a){
_17("dojox.json._slice");
return ","+(_1a[1]||0)+","+(_1a[2]||0)+","+(_1a[3]||1)+")";
}
if(t.match(/^\.\.|\.\*|\[\s*\*\s*\]|,/)){
_17("dojox.json._find");
return (t.charAt(1)=="."?",'"+b+"'":t.match(/,/)?","+t:"")+")";
}
return t;
}).replace(/(\$obj\s*((\.\s*[\w_$]+\s*)|(\[\s*`([0-9]+)\s*`\]))*)(==|~)\s*`([0-9]+)/g,_19).replace(/`([0-9]+)\s*(==|~)\s*(\$obj\s*((\.\s*[\w_$]+)|(\[\s*`([0-9]+)\s*`\]))*)/g,function(t,a,b,c,d,e,f,g){
return _19(t,c,d,e,f,g,b,a);
});
_14=_16+(_14.charAt(0)=="$"?"":"$")+_14.replace(/`([0-9]+|\])/g,function(t,a){
return a=="]"?"]":str[a];
});
var _1c=eval("1&&function($,$1,$2,$3,$4,$5,$6,$7,$8,$9){var $obj=$;return "+_14+"}");
for(var i=0;i<arguments.length-1;i++){
arguments[i]=arguments[i+1];
}
return obj?_1c.apply(this,arguments):_1c;
};
});

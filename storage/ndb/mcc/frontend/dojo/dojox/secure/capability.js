//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.secure.capability");
_3.secure.badProps=/^__|^(apply|call|callee|caller|constructor|eval|prototype|this|unwatch|valueOf|watch)$|__$/;
_3.secure.capability={keywords:["break","case","catch","const","continue","debugger","default","delete","do","else","enum","false","finally","for","function","if","in","instanceof","new","null","yield","return","switch","throw","true","try","typeof","var","void","while"],validate:function(_4,_5,_6){
var _7=this.keywords;
for(var i=0;i<_7.length;i++){
_6[_7[i]]=true;
}
var _8="|this| keyword in object literal without a Class call";
var _9=[];
if(_4.match(/[\u200c-\u200f\u202a-\u202e\u206a-\u206f\uff00-\uffff]/)){
throw new Error("Illegal unicode characters detected");
}
if(_4.match(/\/\*@cc_on/)){
throw new Error("Conditional compilation token is not allowed");
}
_4=_4.replace(/\\["'\\\/bfnrtu]/g,"@").replace(/\/\/.*|\/\*[\w\W]*?\*\/|("[^"]*")|('[^']*')/g,function(t){
return t.match(/^\/\/|^\/\*/)?" ":"0";
}).replace(/\.\s*([a-z\$_A-Z][\w\$_]*)|([;,{])\s*([a-z\$_A-Z][\w\$_]*\s*):/g,function(t,_a,_b,_c){
_a=_a||_c;
if(/^__|^(apply|call|callee|caller|constructor|eval|prototype|this|unwatch|valueOf|watch)$|__$/.test(_a)){
throw new Error("Illegal property name "+_a);
}
return (_b&&(_b+"0:"))||"~";
});
_4.replace(/([^\[][\]\}]\s*=)|((\Wreturn|\S)\s*\[\s*\+?)|([^=!][=!]=[^=])/g,function(_d){
if(!_d.match(/((\Wreturn|[=\&\|\:\?\,])\s*\[)|\[\s*\+$/)){
throw new Error("Illegal operator "+_d.substring(1));
}
});
_4=_4.replace(new RegExp("("+_5.join("|")+")[\\s~]*\\(","g"),function(_e){
return "new(";
});
function _f(_10,_11){
var _12={};
_10.replace(/#\d+/g,function(b){
var _13=_9[b.substring(1)];
for(var i in _13){
if(i==_8){
throw i;
}
if(i=="this"&&_13[":method"]&&_13["this"]==1){
i=_8;
}
if(i!=":method"){
_12[i]=2;
}
}
});
_10.replace(/(\W|^)([a-z_\$A-Z][\w_\$]*)/g,function(t,a,_14){
if(_14.charAt(0)=="_"){
throw new Error("Names may not start with _");
}
_12[_14]=1;
});
return _12;
};
var _15,_16;
function _17(t,_18,a,b,_19,_1a){
_1a.replace(/(^|,)0:\s*function#(\d+)/g,function(t,a,b){
var _1b=_9[b];
_1b[":method"]=1;
});
_1a=_1a.replace(/(^|[^_\w\$])Class\s*\(\s*([_\w\$]+\s*,\s*)*#(\d+)/g,function(t,p,a,b){
var _1c=_9[b];
delete _1c[_8];
return (p||"")+(a||"")+"#"+b;
});
_16=_f(_1a,_18);
function _1d(t,a,b,_1e){
_1e.replace(/,?([a-z\$A-Z][_\w\$]*)/g,function(t,_1f){
if(_1f=="Class"){
throw new Error("Class is reserved");
}
delete _16[_1f];
});
};
if(_18){
_1d(t,a,a,_19);
}
_1a.replace(/(\W|^)(var) ([ \t,_\w\$]+)/g,_1d);
return (a||"")+(b||"")+"#"+(_9.push(_16)-1);
};
do{
_15=_4.replace(/((function|catch)(\s+[_\w\$]+)?\s*\(([^\)]*)\)\s*)?{([^{}]*)}/g,_17);
}while(_15!=_4&&(_4=_15));
_17(0,0,0,0,0,_4);
for(i in _16){
if(!(i in _6)){
throw new Error("Illegal reference to "+i);
}
}
}};
});

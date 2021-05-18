//>>built
define("dojox/lang/observable",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.observable");
_1.experimental("dojox.lang.observable");
_3.lang.observable=function(_4,_5,_6,_7){
return _3.lang.makeObservable(_5,_6,_7)(_4);
};
_3.lang.makeObservable=function(_8,_9,_a,_b){
_b=_b||{};
_a=_a||function(_c,_d,_e,_f){
return _d[_e].apply(_c,_f);
};
function _10(_11,_12,i){
return function(){
return _a(_11,_12,i,arguments);
};
};
if(_3.lang.lettableWin){
var _13=_3.lang.makeObservable;
_13.inc=(_13.inc||0)+1;
var _14="gettable_"+_13.inc;
_3.lang.lettableWin[_14]=_8;
var _15="settable_"+_13.inc;
_3.lang.lettableWin[_15]=_9;
var _16={};
return function(_17){
if(_17.__observable){
return _17.__observable;
}
if(_17.data__){
throw new Error("Can wrap an object that is already wrapped");
}
var _18=[],i,l;
for(i in _b){
_18.push(i);
}
var _19={type:1,event:1};
for(i in _17){
if(i.match(/^[a-zA-Z][\w\$_]*$/)&&!(i in _b)&&!(i in _19)){
_18.push(i);
}
}
var _1a=_18.join(",");
var _1b,_1c=_16[_1a];
if(!_1c){
var _1d="dj_lettable_"+(_13.inc++);
var _1e=_1d+"_dj_getter";
var _1f=["Class "+_1d,"\tPublic data__"];
for(i=0,l=_18.length;i<l;i++){
_1b=_18[i];
var _20=typeof _17[_1b];
if(_20=="function"||_b[_1b]){
_1f.push("  Public "+_1b);
}else{
if(_20!="object"){
_1f.push("\tPublic Property Let "+_1b+"(val)","\t\tCall "+_15+"(me.data__,\""+_1b+"\",val)","\tEnd Property","\tPublic Property Get "+_1b,"\t\t"+_1b+" = "+_14+"(me.data__,\""+_1b+"\")","\tEnd Property");
}
}
}
_1f.push("End Class");
_1f.push("Function "+_1e+"()","\tDim tmp","\tSet tmp = New "+_1d,"\tSet "+_1e+" = tmp","End Function");
_3.lang.lettableWin.vbEval(_1f.join("\n"));
_16[_1a]=_1c=function(){
return _3.lang.lettableWin.construct(_1e);
};
}
var _21=_1c();
_21.data__=_17;
try{
_17.__observable=_21;
}
catch(e){
}
for(i=0,l=_18.length;i<l;i++){
_1b=_18[i];
try{
var val=_17[_1b];
}
catch(e){
}
if(typeof val=="function"||_b[_1b]){
_21[_1b]=_10(_21,_17,_1b);
}
}
return _21;
};
}else{
return function(_22){
if(_22.__observable){
return _22.__observable;
}
var _23=_22 instanceof Array?[]:{};
_23.data__=_22;
for(var i in _22){
if(i.charAt(0)!="_"){
if(typeof _22[i]=="function"){
_23[i]=_10(_23,_22,i);
}else{
if(typeof _22[i]!="object"){
(function(i){
_23.__defineGetter__(i,function(){
return _8(_22,i);
});
_23.__defineSetter__(i,function(_24){
return _9(_22,i,_24);
});
})(i);
}
}
}
}
for(i in _b){
_23[i]=_10(_23,_22,i);
}
_22.__observable=_23;
return _23;
};
}
};
if(!{}.__defineGetter__){
if(_1.isIE){
var _25;
if(document.body){
_25=document.createElement("iframe");
document.body.appendChild(_25);
}else{
document.write("<iframe id='dj_vb_eval_frame'></iframe>");
_25=document.getElementById("dj_vb_eval_frame");
}
_25.style.display="none";
var doc=_25.contentWindow.document;
_3.lang.lettableWin=_25.contentWindow;
doc.write("<html><head><script language=\"VBScript\" type=\"text/VBScript\">"+"Function vb_global_eval(code)"+"ExecuteGlobal(code)"+"End Function"+"</script>"+"<script type=\"text/javascript\">"+"function vbEval(code){ \n"+"return vb_global_eval(code);"+"}"+"function construct(name){ \n"+"return window[name]();"+"}"+"</script>"+"</head><body>vb-eval</body></html>");
doc.close();
}else{
throw new Error("This browser does not support getters and setters");
}
}
_3.lang.ReadOnlyProxy=_3.lang.makeObservable(function(obj,i){
return obj[i];
},function(obj,i,_26){
});
});

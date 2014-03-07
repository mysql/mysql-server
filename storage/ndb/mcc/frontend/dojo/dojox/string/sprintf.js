//>>built
define("dojox/string/sprintf",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/sniff","./tokenize"],function(_1,_2,_3,_4){
var _5=_2.getObject("string",true,dojox);
_5.sprintf=function(_6,_7){
for(var _8=[],i=1;i<arguments.length;i++){
_8.push(arguments[i]);
}
var _9=new _5.sprintf.Formatter(_6);
return _9.format.apply(_9,_8);
};
_5.sprintf.Formatter=function(_a){
var _b=[];
this._mapped=false;
this._format=_a;
this._tokens=_4(_a,this._re,this._parseDelim,this);
};
_2.extend(_5.sprintf.Formatter,{_re:/\%(?:\(([\w_]+)\)|([1-9]\d*)\$)?([0 +\-\#]*)(\*|\d+)?(\.)?(\*|\d+)?[hlL]?([\%scdeEfFgGiouxX])/g,_parseDelim:function(_c,_d,_e,_f,_10,_11,_12){
if(_c){
this._mapped=true;
}
return {mapping:_c,intmapping:_d,flags:_e,_minWidth:_f,period:_10,_precision:_11,specifier:_12};
},_specifiers:{b:{base:2,isInt:true},o:{base:8,isInt:true},x:{base:16,isInt:true},X:{extend:["x"],toUpper:true},d:{base:10,isInt:true},i:{extend:["d"]},u:{extend:["d"],isUnsigned:true},c:{setArg:function(_13){
if(!isNaN(_13.arg)){
var num=parseInt(_13.arg);
if(num<0||num>127){
throw new Error("invalid character code passed to %c in sprintf");
}
_13.arg=isNaN(num)?""+num:String.fromCharCode(num);
}
}},s:{setMaxWidth:function(_14){
_14.maxWidth=(_14.period==".")?_14.precision:-1;
}},e:{isDouble:true,doubleNotation:"e"},E:{extend:["e"],toUpper:true},f:{isDouble:true,doubleNotation:"f"},F:{extend:["f"]},g:{isDouble:true,doubleNotation:"g"},G:{extend:["g"],toUpper:true}},format:function(_15){
if(this._mapped&&typeof _15!="object"){
throw new Error("format requires a mapping");
}
var str="";
var _16=0;
for(var i=0,_17;i<this._tokens.length;i++){
_17=this._tokens[i];
if(typeof _17=="string"){
str+=_17;
}else{
if(this._mapped){
if(typeof _15[_17.mapping]=="undefined"){
throw new Error("missing key "+_17.mapping);
}
_17.arg=_15[_17.mapping];
}else{
if(_17.intmapping){
var _16=parseInt(_17.intmapping)-1;
}
if(_16>=arguments.length){
throw new Error("got "+arguments.length+" printf arguments, insufficient for '"+this._format+"'");
}
_17.arg=arguments[_16++];
}
if(!_17.compiled){
_17.compiled=true;
_17.sign="";
_17.zeroPad=false;
_17.rightJustify=false;
_17.alternative=false;
var _18={};
for(var fi=_17.flags.length;fi--;){
var _19=_17.flags.charAt(fi);
_18[_19]=true;
switch(_19){
case " ":
_17.sign=" ";
break;
case "+":
_17.sign="+";
break;
case "0":
_17.zeroPad=(_18["-"])?false:true;
break;
case "-":
_17.rightJustify=true;
_17.zeroPad=false;
break;
case "#":
_17.alternative=true;
break;
default:
throw Error("bad formatting flag '"+_17.flags.charAt(fi)+"'");
}
}
_17.minWidth=(_17._minWidth)?parseInt(_17._minWidth):0;
_17.maxWidth=-1;
_17.toUpper=false;
_17.isUnsigned=false;
_17.isInt=false;
_17.isDouble=false;
_17.precision=1;
if(_17.period=="."){
if(_17._precision){
_17.precision=parseInt(_17._precision);
}else{
_17.precision=0;
}
}
var _1a=this._specifiers[_17.specifier];
if(typeof _1a=="undefined"){
throw new Error("unexpected specifier '"+_17.specifier+"'");
}
if(_1a.extend){
_2.mixin(_1a,this._specifiers[_1a.extend]);
delete _1a.extend;
}
_2.mixin(_17,_1a);
}
if(typeof _17.setArg=="function"){
_17.setArg(_17);
}
if(typeof _17.setMaxWidth=="function"){
_17.setMaxWidth(_17);
}
if(_17._minWidth=="*"){
if(this._mapped){
throw new Error("* width not supported in mapped formats");
}
_17.minWidth=parseInt(arguments[_16++]);
if(isNaN(_17.minWidth)){
throw new Error("the argument for * width at position "+_16+" is not a number in "+this._format);
}
if(_17.minWidth<0){
_17.rightJustify=true;
_17.minWidth=-_17.minWidth;
}
}
if(_17._precision=="*"&&_17.period=="."){
if(this._mapped){
throw new Error("* precision not supported in mapped formats");
}
_17.precision=parseInt(arguments[_16++]);
if(isNaN(_17.precision)){
throw Error("the argument for * precision at position "+_16+" is not a number in "+this._format);
}
if(_17.precision<0){
_17.precision=1;
_17.period="";
}
}
if(_17.isInt){
if(_17.period=="."){
_17.zeroPad=false;
}
this.formatInt(_17);
}else{
if(_17.isDouble){
if(_17.period!="."){
_17.precision=6;
}
this.formatDouble(_17);
}
}
this.fitField(_17);
str+=""+_17.arg;
}
}
return str;
},_zeros10:"0000000000",_spaces10:"          ",formatInt:function(_1b){
var i=parseInt(_1b.arg);
if(!isFinite(i)){
if(typeof _1b.arg!="number"){
throw new Error("format argument '"+_1b.arg+"' not an integer; parseInt returned "+i);
}
i=0;
}
if(i<0&&(_1b.isUnsigned||_1b.base!=10)){
i=4294967295+i+1;
}
if(i<0){
_1b.arg=(-i).toString(_1b.base);
this.zeroPad(_1b);
_1b.arg="-"+_1b.arg;
}else{
_1b.arg=i.toString(_1b.base);
if(!i&&!_1b.precision){
_1b.arg="";
}else{
this.zeroPad(_1b);
}
if(_1b.sign){
_1b.arg=_1b.sign+_1b.arg;
}
}
if(_1b.base==16){
if(_1b.alternative){
_1b.arg="0x"+_1b.arg;
}
_1b.arg=_1b.toUpper?_1b.arg.toUpperCase():_1b.arg.toLowerCase();
}
if(_1b.base==8){
if(_1b.alternative&&_1b.arg.charAt(0)!="0"){
_1b.arg="0"+_1b.arg;
}
}
},formatDouble:function(_1c){
var f=parseFloat(_1c.arg);
if(!isFinite(f)){
if(typeof _1c.arg!="number"){
throw new Error("format argument '"+_1c.arg+"' not a float; parseFloat returned "+f);
}
f=0;
}
switch(_1c.doubleNotation){
case "e":
_1c.arg=f.toExponential(_1c.precision);
break;
case "f":
_1c.arg=f.toFixed(_1c.precision);
break;
case "g":
if(Math.abs(f)<0.0001){
_1c.arg=f.toExponential(_1c.precision>0?_1c.precision-1:_1c.precision);
}else{
_1c.arg=f.toPrecision(_1c.precision);
}
if(!_1c.alternative){
_1c.arg=_1c.arg.replace(/(\..*[^0])0*/,"$1");
_1c.arg=_1c.arg.replace(/\.0*e/,"e").replace(/\.0$/,"");
}
break;
default:
throw new Error("unexpected double notation '"+_1c.doubleNotation+"'");
}
_1c.arg=_1c.arg.replace(/e\+(\d)$/,"e+0$1").replace(/e\-(\d)$/,"e-0$1");
if(_3("opera")){
_1c.arg=_1c.arg.replace(/^\./,"0.");
}
if(_1c.alternative){
_1c.arg=_1c.arg.replace(/^(\d+)$/,"$1.");
_1c.arg=_1c.arg.replace(/^(\d+)e/,"$1.e");
}
if(f>=0&&_1c.sign){
_1c.arg=_1c.sign+_1c.arg;
}
_1c.arg=_1c.toUpper?_1c.arg.toUpperCase():_1c.arg.toLowerCase();
},zeroPad:function(_1d,_1e){
_1e=(arguments.length==2)?_1e:_1d.precision;
if(typeof _1d.arg!="string"){
_1d.arg=""+_1d.arg;
}
var _1f=_1e-10;
while(_1d.arg.length<_1f){
_1d.arg=(_1d.rightJustify)?_1d.arg+this._zeros10:this._zeros10+_1d.arg;
}
var pad=_1e-_1d.arg.length;
_1d.arg=(_1d.rightJustify)?_1d.arg+this._zeros10.substring(0,pad):this._zeros10.substring(0,pad)+_1d.arg;
},fitField:function(_20){
if(_20.maxWidth>=0&&_20.arg.length>_20.maxWidth){
return _20.arg.substring(0,_20.maxWidth);
}
if(_20.zeroPad){
this.zeroPad(_20,_20.minWidth);
return;
}
this.spacePad(_20);
},spacePad:function(_21,_22){
_22=(arguments.length==2)?_22:_21.minWidth;
if(typeof _21.arg!="string"){
_21.arg=""+_21.arg;
}
var _23=_22-10;
while(_21.arg.length<_23){
_21.arg=(_21.rightJustify)?_21.arg+this._spaces10:this._spaces10+_21.arg;
}
var pad=_22-_21.arg.length;
_21.arg=(_21.rightJustify)?_21.arg+this._spaces10.substring(0,pad):this._spaces10.substring(0,pad)+_21.arg;
}});
return _5.sprintf;
});

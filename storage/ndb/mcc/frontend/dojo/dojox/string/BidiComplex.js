//>>built
define("dojox/string/BidiComplex",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/_base/sniff","dojo/keys"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.string.BidiComplex");
var _7=_2.getObject("string.BidiComplex",true,dojox);
var _8=[];
_7.attachInput=function(_9,_a){
_9.alt=_a;
_4.connect(_9,"onkeydown",this,"_ceKeyDown");
_4.connect(_9,"onkeyup",this,"_ceKeyUp");
_4.connect(_9,"oncut",this,"_ceCutText");
_4.connect(_9,"oncopy",this,"_ceCopyText");
_9.value=_7.createDisplayString(_9.value,_9.alt);
};
_7.createDisplayString=function(_b,_c){
_b=_7.stripSpecialCharacters(_b);
var _d=_7._parse(_b,_c);
var _e="‪"+_b;
var _f=1;
_3.forEach(_d,function(n){
if(n!=null){
var _10=_e.substring(0,n+_f);
var _11=_e.substring(n+_f,_e.length);
_e=_10+"‎"+_11;
_f++;
}
});
return _e;
};
_7.stripSpecialCharacters=function(str){
return str.replace(/[\u200E\u200F\u202A-\u202E]/g,"");
};
_7._ceKeyDown=function(_12){
var _13=_5("ie")?_12.srcElement:_12.target;
_8=_13.value;
};
_7._ceKeyUp=function(_14){
var LRM="‎";
var _15=_5("ie")?_14.srcElement:_14.target;
var _16=_15.value;
var _17=_14.keyCode;
if((_17==_6.HOME)||(_17==_6.END)||(_17==_6.SHIFT)){
return;
}
var _18,_19;
var _1a=_7._getCaretPos(_14,_15);
if(_1a){
_18=_1a[0];
_19=_1a[1];
}
if(_5("ie")){
var _1b=_18,_1c=_19;
if(_17==_6.LEFT_ARROW){
if((_16.charAt(_19-1)==LRM)&&(_18==_19)){
_7._setSelectedRange(_15,_18-1,_19-1);
}
return;
}
if(_17==_6.RIGHT_ARROW){
if(_16.charAt(_19-1)==LRM){
_1c=_19+1;
if(_18==_19){
_1b=_18+1;
}
}
_7._setSelectedRange(_15,_1b,_1c);
return;
}
}else{
if(_17==_6.LEFT_ARROW){
if(_16.charAt(_19-1)==LRM){
_7._setSelectedRange(_15,_18-1,_19-1);
}
return;
}
if(_17==_6.RIGHT_ARROW){
if(_16.charAt(_19-1)==LRM){
_7._setSelectedRange(_15,_18+1,_19+1);
}
return;
}
}
var _1d=_7.createDisplayString(_16,_15.alt);
if(_16!=_1d){
window.status=_16+" c="+_19;
_15.value=_1d;
if((_17==_6.DELETE)&&(_1d.charAt(_19)==LRM)){
_15.value=_1d.substring(0,_19)+_1d.substring(_19+2,_1d.length);
}
if(_17==_6.DELETE){
_7._setSelectedRange(_15,_18,_19);
}else{
if(_17==_6.BACKSPACE){
if((_8.length>=_19)&&(_8.charAt(_19-1)==LRM)){
_7._setSelectedRange(_15,_18-1,_19-1);
}else{
_7._setSelectedRange(_15,_18,_19);
}
}else{
if(_15.value.charAt(_19)!=LRM){
_7._setSelectedRange(_15,_18+1,_19+1);
}
}
}
}
};
_7._processCopy=function(_1e,_1f,_20){
if(_1f==null){
if(_5("ie")){
var _21=document.selection.createRange();
_1f=_21.text;
}else{
_1f=_1e.value.substring(_1e.selectionStart,_1e.selectionEnd);
}
}
var _22=_7.stripSpecialCharacters(_1f);
if(_5("ie")){
window.clipboardData.setData("Text",_22);
}
return true;
};
_7._ceCopyText=function(_23){
if(_5("ie")){
_23.returnValue=false;
}
return _7._processCopy(_23,null,false);
};
_7._ceCutText=function(_24){
var ret=_7._processCopy(_24,null,false);
if(!ret){
return false;
}
if(_5("ie")){
document.selection.clear();
}else{
var _25=_24.selectionStart;
_24.value=_24.value.substring(0,_25)+_24.value.substring(_24.selectionEnd);
_24.setSelectionRange(_25,_25);
}
return true;
};
_7._getCaretPos=function(_26,_27){
if(_5("ie")){
var _28=0,_29=document.selection.createRange().duplicate(),_2a=_29.duplicate(),_2b=_29.text.length;
if(_27.type=="textarea"){
_2a.moveToElementText(_27);
}else{
_2a.expand("textedit");
}
while(_29.compareEndPoints("StartToStart",_2a)>0){
_29.moveStart("character",-1);
++_28;
}
return [_28,_28+_2b];
}
return [_26.target.selectionStart,_26.target.selectionEnd];
};
_7._setSelectedRange=function(_2c,_2d,_2e){
if(_5("ie")){
var _2f=_2c.createTextRange();
if(_2f){
if(_2c.type=="textarea"){
_2f.moveToElementText(_2c);
}else{
_2f.expand("textedit");
}
_2f.collapse();
_2f.moveEnd("character",_2e);
_2f.moveStart("character",_2d);
_2f.select();
}
}else{
_2c.selectionStart=_2d;
_2c.selectionEnd=_2e;
}
};
var _30=function(c){
return (c>="0"&&c<="9")||(c>"ÿ");
};
var _31=function(c){
return (c>="A"&&c<="Z")||(c>="a"&&c<="z");
};
var _32=function(_33,i,_34){
while(i>0){
if(i==_34){
return false;
}
i--;
if(_30(_33.charAt(i))){
return true;
}
if(_31(_33.charAt(i))){
return false;
}
}
return false;
};
_7._parse=function(str,_35){
var _36=-1,_37=[];
var _38={FILE_PATH:"/\\:.",URL:"/:.?=&#",XPATH:"/\\:.<>=[]",EMAIL:"<>@.,;"}[_35];
switch(_35){
case "FILE_PATH":
case "URL":
case "XPATH":
_3.forEach(str,function(ch,i){
if(_38.indexOf(ch)>=0&&_32(str,i,_36)){
_36=i;
_37.push(i);
}
});
break;
case "EMAIL":
var _39=false;
_3.forEach(str,function(ch,i){
if(ch=="\""){
if(_32(str,i,_36)){
_36=i;
_37.push(i);
}
i++;
var i1=str.indexOf("\"",i);
if(i1>=i){
i=i1;
}
if(_32(str,i,_36)){
_36=i;
_37.push(i);
}
}
if(_38.indexOf(ch)>=0&&_32(str,i,_36)){
_36=i;
_37.push(i);
}
});
}
return _37;
};
return _7;
});

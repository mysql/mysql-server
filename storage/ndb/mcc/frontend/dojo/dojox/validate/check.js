//>>built
define("dojox/validate/check",["dojo/_base/kernel","dojo/_base/lang","./_base"],function(_1,_2,_3){
_1.experimental("dojox.validate.check");
_3.check=function(_4,_5){
var _6=[];
var _7=[];
var _8={isSuccessful:function(){
return (!this.hasInvalid()&&!this.hasMissing());
},hasMissing:function(){
return (_6.length>0);
},getMissing:function(){
return _6;
},isMissing:function(_9){
for(var i=0;i<_6.length;i++){
if(_9==_6[i]){
return true;
}
}
return false;
},hasInvalid:function(){
return (_7.length>0);
},getInvalid:function(){
return _7;
},isInvalid:function(_a){
for(var i=0;i<_7.length;i++){
if(_a==_7[i]){
return true;
}
}
return false;
}};
var _b=function(_c,_d){
return (typeof _d[_c]=="undefined");
};
if(_5.trim instanceof Array){
for(var i=0;i<_5.trim.length;i++){
var _e=_4[_5.trim[i]];
if(_b("type",_e)||_e.type!="text"&&_e.type!="textarea"&&_e.type!="password"){
continue;
}
_e.value=_e.value.replace(/(^\s*|\s*$)/g,"");
}
}
if(_5.uppercase instanceof Array){
for(var i=0;i<_5.uppercase.length;i++){
var _e=_4[_5.uppercase[i]];
if(_b("type",_e)||_e.type!="text"&&_e.type!="textarea"&&_e.type!="password"){
continue;
}
_e.value=_e.value.toUpperCase();
}
}
if(_5.lowercase instanceof Array){
for(var i=0;i<_5.lowercase.length;i++){
var _e=_4[_5.lowercase[i]];
if(_b("type",_e)||_e.type!="text"&&_e.type!="textarea"&&_e.type!="password"){
continue;
}
_e.value=_e.value.toLowerCase();
}
}
if(_5.ucfirst instanceof Array){
for(var i=0;i<_5.ucfirst.length;i++){
var _e=_4[_5.ucfirst[i]];
if(_b("type",_e)||_e.type!="text"&&_e.type!="textarea"&&_e.type!="password"){
continue;
}
_e.value=_e.value.replace(/\b\w+\b/g,function(_f){
return _f.substring(0,1).toUpperCase()+_f.substring(1).toLowerCase();
});
}
}
if(_5.digit instanceof Array){
for(var i=0;i<_5.digit.length;i++){
var _e=_4[_5.digit[i]];
if(_b("type",_e)||_e.type!="text"&&_e.type!="textarea"&&_e.type!="password"){
continue;
}
_e.value=_e.value.replace(/\D/g,"");
}
}
if(_5.required instanceof Array){
for(var i=0;i<_5.required.length;i++){
if(!_2.isString(_5.required[i])){
continue;
}
var _e=_4[_5.required[i]];
if(!_b("type",_e)&&(_e.type=="text"||_e.type=="textarea"||_e.type=="password"||_e.type=="file")&&/^\s*$/.test(_e.value)){
_6[_6.length]=_e.name;
}else{
if(!_b("type",_e)&&(_e.type=="select-one"||_e.type=="select-multiple")&&(_e.selectedIndex==-1||/^\s*$/.test(_e.options[_e.selectedIndex].value))){
_6[_6.length]=_e.name;
}else{
if(_e instanceof Array){
var _10=false;
for(var j=0;j<_e.length;j++){
if(_e[j].checked){
_10=true;
}
}
if(!_10){
_6[_6.length]=_e[0].name;
}
}
}
}
}
}
if(_5.required instanceof Array){
for(var i=0;i<_5.required.length;i++){
if(!_2.isObject(_5.required[i])){
continue;
}
var _e,_11;
for(var _12 in _5.required[i]){
_e=_4[_12];
_11=_5.required[i][_12];
}
if(_e instanceof Array){
var _10=0;
for(var j=0;j<_e.length;j++){
if(_e[j].checked){
_10++;
}
}
if(_10<_11){
_6[_6.length]=_e[0].name;
}
}else{
if(!_b("type",_e)&&_e.type=="select-multiple"){
var _13=0;
for(var j=0;j<_e.options.length;j++){
if(_e.options[j].selected&&!/^\s*$/.test(_e.options[j].value)){
_13++;
}
}
if(_13<_11){
_6[_6.length]=_e.name;
}
}
}
}
}
if(_2.isObject(_5.dependencies)){
for(_12 in _5.dependencies){
var _e=_4[_12];
if(_b("type",_e)){
continue;
}
if(_e.type!="text"&&_e.type!="textarea"&&_e.type!="password"){
continue;
}
if(/\S+/.test(_e.value)){
continue;
}
if(_8.isMissing(_e.name)){
continue;
}
var _14=_4[_5.dependencies[_12]];
if(_14.type!="text"&&_14.type!="textarea"&&_14.type!="password"){
continue;
}
if(/^\s*$/.test(_14.value)){
continue;
}
_6[_6.length]=_e.name;
}
}
if(_2.isObject(_5.constraints)){
for(_12 in _5.constraints){
var _e=_4[_12];
if(!_e){
continue;
}
if(!_b("tagName",_e)&&(_e.tagName.toLowerCase().indexOf("input")>=0||_e.tagName.toLowerCase().indexOf("textarea")>=0)&&/^\s*$/.test(_e.value)){
continue;
}
var _15=true;
if(_2.isFunction(_5.constraints[_12])){
_15=_5.constraints[_12](_e.value);
}else{
if(_2.isArray(_5.constraints[_12])){
if(_2.isArray(_5.constraints[_12][0])){
for(var i=0;i<_5.constraints[_12].length;i++){
_15=_3.evaluateConstraint(_5,_5.constraints[_12][i],_12,_e);
if(!_15){
break;
}
}
}else{
_15=_3.evaluateConstraint(_5,_5.constraints[_12],_12,_e);
}
}
}
if(!_15){
_7[_7.length]=_e.name;
}
}
}
if(_2.isObject(_5.confirm)){
for(_12 in _5.confirm){
var _e=_4[_12];
var _14=_4[_5.confirm[_12]];
if(_b("type",_e)||_b("type",_14)||(_e.type!="text"&&_e.type!="textarea"&&_e.type!="password")||(_14.type!=_e.type)||(_14.value==_e.value)||(_8.isInvalid(_e.name))||(/^\s*$/.test(_14.value))){
continue;
}
_7[_7.length]=_e.name;
}
}
return _8;
};
_3.evaluateConstraint=function(_16,_17,_18,_19){
var _1a=_17[0];
var _1b=_17.slice(1);
_1b.unshift(_19.value);
if(typeof _1a!="undefined"){
return _1a.apply(null,_1b);
}
return false;
};
return _3.check;
});

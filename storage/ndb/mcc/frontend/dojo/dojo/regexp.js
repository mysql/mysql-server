/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/regexp",["./_base/kernel","./_base/lang"],function(_1,_2){
_2.getObject("regexp",true,_1);
_1.regexp.escapeString=function(_3,_4){
return _3.replace(/([\.$?*|{}\(\)\[\]\\\/\+^])/g,function(ch){
if(_4&&_4.indexOf(ch)!=-1){
return ch;
}
return "\\"+ch;
});
};
_1.regexp.buildGroupRE=function(_5,re,_6){
if(!(_5 instanceof Array)){
return re(_5);
}
var b=[];
for(var i=0;i<_5.length;i++){
b.push(re(_5[i]));
}
return _1.regexp.group(b.join("|"),_6);
};
_1.regexp.group=function(_7,_8){
return "("+(_8?"?:":"")+_7+")";
};
return _1.regexp;
});

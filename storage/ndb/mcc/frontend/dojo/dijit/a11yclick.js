//>>built
define("dijit/a11yclick",["dojo/_base/array","dojo/_base/declare","dojo/has","dojo/keys","dojo/on"],function(_1,_2,_3,_4,on){
function _5(_6){
do{
if(_6.dojoClick){
return true;
}
}while(_6=_6.parentNode);
};
function _7(e){
return (e.keyCode===_4.ENTER||e.keyCode===_4.SPACE)&&!e.ctrlKey&&!e.shiftKey&&!e.altKey&&!e.metaKey&&!/input|button/i.test(e.target.nodeName)&&_5(e.target);
};
var _8;
on(document,"keydown",function(e){
if(_7(e)){
_8=e.target;
e.preventDefault();
}
});
on(document,"keyup",function(e){
if(_7(e)&&e.target==_8){
_8=null;
on.emit(e.target,"click",{cancelable:true,bubbles:true});
}
});
if(_3("touch")){
var _9;
on(document,"touchend",function(e){
var _a=e.target;
if(_5(_a)){
var _b=on.once(_a,"click",function(e){
if(_9){
clearTimeout(_9);
_9=null;
}
});
if(_9){
clearTimeout(_9);
}
_9=setTimeout(function(){
_9=null;
_b.remove();
on.emit(_a,"click",{cancelable:true,bubbles:true});
},600);
}
});
}
return function(_c,_d){
_c.dojoClick=true;
return on(_c,"click",_d);
};
});

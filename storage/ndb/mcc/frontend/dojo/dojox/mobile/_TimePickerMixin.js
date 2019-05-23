//>>built
define("dojox/mobile/_TimePickerMixin",["dojo/_base/declare","dojo/dom-class","dojo/date/locale"],function(_1,_2,_3){
return _1("dojox.mobile._TimePickerMixin",null,{reset:function(){
var _4=new Date(),h=_4.getHours()+"",m=_4.getMinutes();
m=(m<10?"0":"")+m;
this.set("colors",[h,m]);
if(this.values){
this.set("values",this.values);
this.values=null;
}else{
if(this.values12){
this.set("values12",this.values12);
this.values12=null;
}else{
this.set("values",[h,m]);
}
}
},_getDateAttr:function(){
var v=this.get("values");
return _3.parse(v[0]+":"+v[1],{timePattern:"H:m",selector:"time"});
}});
});

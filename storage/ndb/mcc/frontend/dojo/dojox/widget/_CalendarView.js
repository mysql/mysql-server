//>>built
define("dojox/widget/_CalendarView",["dojo/_base/declare","dijit/_WidgetBase","dojo/dom-construct","dojo/query","dojo/date","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.widget._CalendarView",_2,{headerClass:"",useHeader:true,cloneClass:function(_7,n,_8){
var _9=_4(_7,this.domNode)[0];
var i;
if(!_8){
for(i=0;i<n;i++){
_9.parentNode.appendChild(_9.cloneNode(true));
}
}else{
var _a=_4(_7,this.domNode)[0];
for(i=0;i<n;i++){
_9.parentNode.insertBefore(_9.cloneNode(true),_a);
}
}
},_setText:function(_b,_c){
if(_b.innerHTML!=_c){
_3.empty(_b);
_b.appendChild(_6.doc.createTextNode(_c));
}
},getHeader:function(){
return this.header||(this.header=_3.create("span",{"class":this.headerClass}));
},onValueSelected:function(_d){
},adjustDate:function(_e,_f){
return _5.add(_e,this.datePart,_f);
},onDisplay:function(){
},onBeforeDisplay:function(){
},onBeforeUnDisplay:function(){
}});
});

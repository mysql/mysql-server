//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/Declaration"],function(_1,_2,_3){
_2.provide("dojox.widget.Iterator");
_2.require("dijit.Declaration");
_2.experimental("dojox.widget.Iterator");
_2.declare("dojox.widget.Iterator",[_1.Declaration],{constructor:(function(){
var _4=0;
return function(){
this.attrs=[];
this.children=[];
this.widgetClass="dojox.widget.Iterator._classes._"+(_4++);
};
})(),start:0,fetchMax:1000,query:{name:"*"},attrs:[],defaultValue:"",widgetCtor:null,dataValues:[],data:null,store:null,_srcIndex:0,_srcParent:null,_setSrcIndex:function(s){
this._srcIndex=0;
this._srcParent=s.parentNode;
var ts=s;
while(ts.previousSibling){
this._srcIndex++;
ts=ts.previousSibling;
}
},postscript:function(p,s){
this._setSrcIndex(s);
this.inherited("postscript",arguments);
var wc=this.widgetCtor=_2.getObject(this.widgetClass);
this.attrs=_2.map(wc.prototype.templateString.match(/\$\{([^\s\:\}]+)(?:\:([^\s\:\}]+))?\}/g),function(s){
return s.slice(2,-1);
});
_2.forEach(this.attrs,function(m){
wc.prototype[m]="";
});
this.update();
},clear:function(){
if(this.children.length){
this._setSrcIndex(this.children[0].domNode);
}
_2.forEach(this.children,"item.destroy();");
this.children=[];
},update:function(){
if(this.store){
this.fetch();
}else{
this.onDataAvailable(this.data||this.dataValues);
}
},_addItem:function(_5,_6){
if(_2.isString(_5)){
_5={value:_5};
}
var _7=new this.widgetCtor(_5);
this.children.push(_7);
_2.place(_7.domNode,this._srcParent,this._srcIndex+_6);
},getAttrValuesObj:function(_8){
var _9={};
if(_2.isString(_8)){
_2.forEach(this.attrs,function(_a){
_9[_a]=(_a=="value")?_8:this.defaultValue;
},this);
}else{
_2.forEach(this.attrs,function(_b){
if(this.store){
_9[_b]=this.store.getValue(_8,_b)||this.defaultValue;
}else{
_9[_b]=_8[_b]||this.defaultValue;
}
},this);
}
return _9;
},onDataAvailable:function(_c){
this.clear();
_2.forEach(_c,function(_d,_e){
this._addItem(this.getAttrValuesObj(_d),_e);
},this);
},fetch:function(_f,_10,end){
this.store.fetch({query:_f||this.query,start:_10||this.start,count:end||this.fetchMax,onComplete:_2.hitch(this,"onDataAvailable")});
}});
_3.widget.Iterator._classes={};
});

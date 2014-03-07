//>>built
define("dojox/highlight/widget/Code",["dojo","dijit","dijit/_Widget","dijit/_Templated","dojox/highlight"],function(_1,_2){
_1.declare("dojox.highlight.widget.Code",[_2._Widget,_2._Templated],{url:"",range:null,style:"",listType:"1",lang:"",templateString:"<div class=\"formatted\" style=\"${style}\">"+"<div class=\"titleBar\"></div>"+"<ol type=\"${listType}\" dojoAttachPoint=\"codeList\" class=\"numbers\"></ol>"+"<div style=\"display:none\" dojoAttachPoint=\"containerNode\"></div>"+"</div>",postCreate:function(){
this.inherited(arguments);
if(this.url){
_1.xhrGet({url:this.url,load:_1.hitch(this,"_populate"),error:_1.hitch(this,"_loadError")});
}else{
this._populate(this.containerNode.innerHTML);
}
},_populate:function(_3){
this.containerNode.innerHTML="<pre><code class='"+this.lang+"'>"+_3.replace(/\</g,"&lt;")+"</code></pre>";
_1.query("pre > code",this.containerNode).forEach(dojox.highlight.init);
var _4=this.containerNode.innerHTML.split("\n");
_1.forEach(_4,function(_5,i){
var li=_1.doc.createElement("li");
_1.addClass(li,(i%2!==0?"even":"odd"));
_5="<pre><code>"+_5+"&nbsp;</code></pre>";
_5=_5.replace(/\t/g," &nbsp; ");
li.innerHTML=_5;
this.codeList.appendChild(li);
},this);
this._lines=_1.query("li",this.codeList);
this._updateView();
},setRange:function(_6){
if(_1.isArray(_6)){
this.range=_6;
this._updateView();
}
},_updateView:function(){
if(this.range){
var r=this.range;
this._lines.style({display:"none"}).filter(function(n,i){
return (i+1>=r[0]&&i+1<=r[1]);
}).style({display:""});
_1.attr(this.codeList,"start",r[0]);
}
},_loadError:function(_7){
console.warn("loading: ",this.url," FAILED",_7);
}});
});

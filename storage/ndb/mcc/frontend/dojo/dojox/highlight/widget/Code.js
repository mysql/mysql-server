//>>built
define("dojox/highlight/widget/Code",["dojo/_base/declare","dojo/_base/lang","dojo/_base/array","dojo/query","dojo/dom-class","dojo/dom-attr","dojo/dom-construct","dojo/request/xhr","dijit/_Widget","dijit/_Templated","dojox/highlight"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _1("dojox.highlight.widget.Code",[_9,_a],{url:"",range:null,style:"",listType:"1",lang:"",templateString:"<div class=\"formatted\" style=\"${style}\">"+"<div class=\"titleBar\"></div>"+"<ol type=\"${listType}\" dojoAttachPoint=\"codeList\" class=\"numbers\"></ol>"+"<div style=\"display:none\" dojoAttachPoint=\"containerNode\"></div>"+"</div>",postCreate:function(){
this.inherited(arguments);
if(this.url){
_8(this.url,{}).then(_2.hitch(this,"_populate"),_2.hitch(this,"_loadError"));
}else{
this._populate(this.containerNode.innerHTML);
}
},_populate:function(_b){
this.containerNode.innerHTML="<pre><code class='"+this.lang+"'>"+_b.replace(/\</g,"&lt;")+"</code></pre>";
_4("pre > code",this.containerNode).forEach(dojox.highlight.init);
var _c=this.containerNode.innerHTML.split("\n");
_3.forEach(_c,function(_d,i){
var li=_7.create("li");
_5.add(li,(i%2!==0?"even":"odd"));
_d="<pre><code>"+_d+"&nbsp;</code></pre>";
_d=_d.replace(/\t/g," &nbsp; ");
li.innerHTML=_d;
this.codeList.appendChild(li);
},this);
this._lines=_4("li",this.codeList);
this._updateView();
},setRange:function(_e){
if(_e instanceof Array){
this.range=_e;
this._updateView();
}
},_updateView:function(){
if(this.range){
var r=this.range;
this._lines.style({display:"none"}).filter(function(n,i){
return (i+1>=r[0]&&i+1<=r[1]);
}).style({display:""});
_6.set(this.codeList,"start",r[0]);
}
},_loadError:function(_f){
console.warn("loading: ",this.url," FAILED",_f);
}});
});

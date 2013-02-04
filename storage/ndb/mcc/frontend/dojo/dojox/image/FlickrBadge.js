//>>built
define("dojox/image/FlickrBadge",["dojo","dojox/main","dojox/image/Badge","dojox/data/FlickrRestStore"],function(_1,_2){
_1.getObject("image",true,_2);
return _1.declare("dojox.image.FlickrBadge",_2.image.Badge,{children:"a.flickrImage",userid:"",username:"",setid:"",tags:"",searchText:"",target:"",apikey:"8c6803164dbc395fb7131c9d54843627",_store:null,postCreate:function(){
if(this.username&&!this.userid){
var _3=_1.io.script.get({url:"http://www.flickr.com/services/rest/",preventCache:true,content:{format:"json",method:"flickr.people.findByUsername",api_key:this.apikey,username:this.username},callbackParamName:"jsoncallback"});
_3.addCallback(this,function(_4){
if(_4.user&&_4.user.nsid){
this.userid=_4.user.nsid;
if(!this._started){
this.startup();
}
}
});
}
},startup:function(){
if(this._started){
return;
}
if(this.userid){
var _5={userid:this.userid};
if(this.setid){
_5["setid"]=this.setid;
}
if(this.tags){
_5.tags=this.tags;
}
if(this.searchText){
_5.text=this.searchText;
}
var _6=arguments;
this._store=new _2.data.FlickrRestStore({apikey:this.apikey});
this._store.fetch({count:this.cols*this.rows,query:_5,onComplete:_1.hitch(this,function(_7){
_1.forEach(_7,function(_8){
var a=_1.doc.createElement("a");
_1.addClass(a,"flickrImage");
a.href=this._store.getValue(_8,"link");
if(this.target){
a.target=this.target;
}
var _9=_1.doc.createElement("img");
_9.src=this._store.getValue(_8,"imageUrlThumb");
_1.style(_9,{width:"100%",height:"100%"});
a.appendChild(_9);
this.domNode.appendChild(a);
},this);
_2.image.Badge.prototype.startup.call(this,_6);
})});
}
}});
});

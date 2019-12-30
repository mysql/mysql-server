//>>built
define("dojox/mobile/Video",["dojo/_base/declare","dojo/_base/sniff","./Audio"],function(_1,_2,_3){
return _1("dojox.mobile.Video",_3,{width:"200px",height:"150px",_tag:"video",_getEmbedRegExp:function(){
return _2("ff")?/video\/mp4/i:_2.isIE>=9?/video\/webm/i:null;
}});
});

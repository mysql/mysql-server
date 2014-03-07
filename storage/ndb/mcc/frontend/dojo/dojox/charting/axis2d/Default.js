//>>built
define("dojox/charting/axis2d/Default",["dojo/_base/lang","dojo/_base/array","dojo/_base/sniff","dojo/_base/declare","dojo/_base/connect","dojo/_base/html","dojo/dom-geometry","./Invisible","../scaler/common","../scaler/linear","./common","dojox/gfx","dojox/lang/utils"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,g,du){
var _c=4,_d=45;
return _4("dojox.charting.axis2d.Default",_8,{defaultParams:{vertical:false,fixUpper:"none",fixLower:"none",natural:false,leftBottom:true,includeZero:false,fixed:true,majorLabels:true,minorTicks:true,minorLabels:true,microTicks:false,rotation:0,htmlLabels:true,enableCache:false},optionalParams:{min:0,max:1,from:0,to:1,majorTickStep:4,minorTickStep:2,microTickStep:1,labels:[],labelFunc:null,maxLabelSize:0,maxLabelCharCount:0,trailingSymbol:null,stroke:{},majorTick:{},minorTick:{},microTick:{},tick:{},font:"",fontColor:"",title:"",titleGap:0,titleFont:"",titleFontColor:"",titleOrientation:""},constructor:function(_e,_f){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_f);
du.updateWithPattern(this.opt,_f,this.optionalParams);
if(this.opt.enableCache){
this._textFreePool=[];
this._lineFreePool=[];
this._textUsePool=[];
this._lineUsePool=[];
}
},getOffsets:function(){
var s=this.scaler,_10={l:0,r:0,t:0,b:0};
if(!s){
return _10;
}
var o=this.opt,_11=0,a,b,c,d,gl=_9.getNumericLabel,_12=0,ma=s.major,mi=s.minor,ta=this.chart.theme.axis,_13=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_14=o.titleFont||(ta.tick&&ta.tick.titleFont),_15=(o.titleGap==0)?0:o.titleGap||(ta.tick&&ta.tick.titleGap)||15,_16=this.chart.theme.getTick("major",o),_17=this.chart.theme.getTick("minor",o),_18=_13?g.normalizedLength(g.splitFontString(_13).size):0,_19=_14?g.normalizedLength(g.splitFontString(_14).size):0,_1a=o.rotation%360,_1b=o.leftBottom,_1c=Math.abs(Math.cos(_1a*Math.PI/180)),_1d=Math.abs(Math.sin(_1a*Math.PI/180));
this.trailingSymbol=(o.trailingSymbol===undefined||o.trailingSymbol===null)?this.trailingSymbol:o.trailingSymbol;
if(_1a<0){
_1a+=360;
}
if(_18){
if(this.labels){
_11=this._groupLabelWidth(this.labels,_13,o.maxLabelCharCount);
}else{
_11=this._groupLabelWidth([gl(ma.start,ma.prec,o),gl(ma.start+ma.count*ma.tick,ma.prec,o),gl(mi.start,mi.prec,o),gl(mi.start+mi.count*mi.tick,mi.prec,o)],_13,o.maxLabelCharCount);
}
_11=o.maxLabelSize?Math.min(o.maxLabelSize,_11):_11;
if(this.vertical){
var _1e=_1b?"l":"r";
switch(_1a){
case 0:
case 180:
_10[_1e]=_11;
_10.t=_10.b=_18/2;
break;
case 90:
case 270:
_10[_1e]=_18;
_10.t=_10.b=_11/2;
break;
default:
if(_1a<=_d||(180<_1a&&_1a<=(180+_d))){
_10[_1e]=_18*_1d/2+_11*_1c;
_10[_1b?"t":"b"]=_18*_1c/2+_11*_1d;
_10[_1b?"b":"t"]=_18*_1c/2;
}else{
if(_1a>(360-_d)||(180>_1a&&_1a>(180-_d))){
_10[_1e]=_18*_1d/2+_11*_1c;
_10[_1b?"b":"t"]=_18*_1c/2+_11*_1d;
_10[_1b?"t":"b"]=_18*_1c/2;
}else{
if(_1a<90||(180<_1a&&_1a<270)){
_10[_1e]=_18*_1d+_11*_1c;
_10[_1b?"t":"b"]=_18*_1c+_11*_1d;
}else{
_10[_1e]=_18*_1d+_11*_1c;
_10[_1b?"b":"t"]=_18*_1c+_11*_1d;
}
}
}
break;
}
_10[_1e]+=_c+Math.max(_16.length,_17.length)+(o.title?(_19+_15):0);
}else{
var _1e=_1b?"b":"t";
switch(_1a){
case 0:
case 180:
_10[_1e]=_18;
_10.l=_10.r=_11/2;
break;
case 90:
case 270:
_10[_1e]=_11;
_10.l=_10.r=_18/2;
break;
default:
if((90-_d)<=_1a&&_1a<=90||(270-_d)<=_1a&&_1a<=270){
_10[_1e]=_18*_1d/2+_11*_1c;
_10[_1b?"r":"l"]=_18*_1c/2+_11*_1d;
_10[_1b?"l":"r"]=_18*_1c/2;
}else{
if(90<=_1a&&_1a<=(90+_d)||270<=_1a&&_1a<=(270+_d)){
_10[_1e]=_18*_1d/2+_11*_1c;
_10[_1b?"l":"r"]=_18*_1c/2+_11*_1d;
_10[_1b?"r":"l"]=_18*_1c/2;
}else{
if(_1a<_d||(180<_1a&&_1a<(180-_d))){
_10[_1e]=_18*_1d+_11*_1c;
_10[_1b?"r":"l"]=_18*_1c+_11*_1d;
}else{
_10[_1e]=_18*_1d+_11*_1c;
_10[_1b?"l":"r"]=_18*_1c+_11*_1d;
}
}
}
break;
}
_10[_1e]+=_c+Math.max(_16.length,_17.length)+(o.title?(_19+_15):0);
}
}
if(_11){
this._cachedLabelWidth=_11;
}
return _10;
},cleanGroup:function(_1f){
if(this.opt.enableCache&&this.group){
this._lineFreePool=this._lineFreePool.concat(this._lineUsePool);
this._lineUsePool=[];
this._textFreePool=this._textFreePool.concat(this._textUsePool);
this._textUsePool=[];
}
this.inherited(arguments);
},createText:function(_20,_21,x,y,_22,_23,_24,_25,_26){
if(!this.opt.enableCache||_20=="html"){
return _b.createText[_20](this.chart,_21,x,y,_22,_23,_24,_25,_26);
}
var _27;
if(this._textFreePool.length>0){
_27=this._textFreePool.pop();
_27.setShape({x:x,y:y,text:_23,align:_22});
_21.add(_27);
}else{
_27=_b.createText[_20](this.chart,_21,x,y,_22,_23,_24,_25,_26);
}
this._textUsePool.push(_27);
return _27;
},createLine:function(_28,_29){
var _2a;
if(this.opt.enableCache&&this._lineFreePool.length>0){
_2a=this._lineFreePool.pop();
_2a.setShape(_29);
_28.add(_2a);
}else{
_2a=_28.createLine(_29);
}
if(this.opt.enableCache){
this._lineUsePool.push(_2a);
}
return _2a;
},render:function(dim,_2b){
if(!this.dirty){
return this;
}
var o=this.opt,ta=this.chart.theme.axis,_2c=o.leftBottom,_2d=o.rotation%360,_2e,_2f,_30,_31=0,_32,_33,_34,_35,_36,_37,_38=o.font||(ta.majorTick&&ta.majorTick.font)||(ta.tick&&ta.tick.font),_39=o.titleFont||(ta.tick&&ta.tick.titleFont),_3a=o.fontColor||(ta.majorTick&&ta.majorTick.fontColor)||(ta.tick&&ta.tick.fontColor)||"black",_3b=o.titleFontColor||(ta.tick&&ta.tick.titleFontColor)||"black",_3c=(o.titleGap==0)?0:o.titleGap||(ta.tick&&ta.tick.titleGap)||15,_3d=o.titleOrientation||(ta.tick&&ta.tick.titleOrientation)||"axis",_3e=this.chart.theme.getTick("major",o),_3f=this.chart.theme.getTick("minor",o),_40=this.chart.theme.getTick("micro",o),_41=Math.max(_3e.length,_3f.length,_40.length),_42="stroke" in o?o.stroke:ta.stroke,_43=_38?g.normalizedLength(g.splitFontString(_38).size):0,_44=Math.abs(Math.cos(_2d*Math.PI/180)),_45=Math.abs(Math.sin(_2d*Math.PI/180)),_46=_39?g.normalizedLength(g.splitFontString(_39).size):0;
if(_2d<0){
_2d+=360;
}
if(this.vertical){
_2e={y:dim.height-_2b.b};
_2f={y:_2b.t};
_30={y:(dim.height-_2b.b+_2b.t)/2};
_32=_43*_45+(this._cachedLabelWidth||0)*_44+_c+Math.max(_3e.length,_3f.length)+_46+_3c;
_33={x:0,y:-1};
_36={x:0,y:0};
_34={x:1,y:0};
_35={x:_c,y:0};
switch(_2d){
case 0:
_37="end";
_36.y=_43*0.4;
break;
case 90:
_37="middle";
_36.x=-_43;
break;
case 180:
_37="start";
_36.y=-_43*0.4;
break;
case 270:
_37="middle";
break;
default:
if(_2d<_d){
_37="end";
_36.y=_43*0.4;
}else{
if(_2d<90){
_37="end";
_36.y=_43*0.4;
}else{
if(_2d<(180-_d)){
_37="start";
}else{
if(_2d<(180+_d)){
_37="start";
_36.y=-_43*0.4;
}else{
if(_2d<270){
_37="start";
_36.x=_2c?0:_43*0.4;
}else{
if(_2d<(360-_d)){
_37="end";
_36.x=_2c?0:_43*0.4;
}else{
_37="end";
_36.y=_43*0.4;
}
}
}
}
}
}
}
if(_2c){
_2e.x=_2f.x=_2b.l;
_31=(_3d&&_3d=="away")?90:270;
_30.x=_2b.l-_32+(_31==270?_46:0);
_34.x=-1;
_35.x=-_35.x;
}else{
_2e.x=_2f.x=dim.width-_2b.r;
_31=(_3d&&_3d=="axis")?90:270;
_30.x=dim.width-_2b.r+_32-(_31==270?0:_46);
switch(_37){
case "start":
_37="end";
break;
case "end":
_37="start";
break;
case "middle":
_36.x+=_43;
break;
}
}
}else{
_2e={x:_2b.l};
_2f={x:dim.width-_2b.r};
_30={x:(dim.width-_2b.r+_2b.l)/2};
_32=_43*_44+(this._cachedLabelWidth||0)*_45+_c+Math.max(_3e.length,_3f.length)+_46+_3c;
_33={x:1,y:0};
_36={x:0,y:0};
_34={x:0,y:1};
_35={x:0,y:_c};
switch(_2d){
case 0:
_37="middle";
_36.y=_43;
break;
case 90:
_37="start";
_36.x=-_43*0.4;
break;
case 180:
_37="middle";
break;
case 270:
_37="end";
_36.x=_43*0.4;
break;
default:
if(_2d<(90-_d)){
_37="start";
_36.y=_2c?_43:0;
}else{
if(_2d<(90+_d)){
_37="start";
_36.x=-_43*0.4;
}else{
if(_2d<180){
_37="start";
_36.y=_2c?0:-_43;
}else{
if(_2d<(270-_d)){
_37="end";
_36.y=_2c?0:-_43;
}else{
if(_2d<(270+_d)){
_37="end";
_36.y=_2c?_43*0.4:0;
}else{
_37="end";
_36.y=_2c?_43:0;
}
}
}
}
}
}
if(_2c){
_2e.y=_2f.y=dim.height-_2b.b;
_31=(_3d&&_3d=="axis")?180:0;
_30.y=dim.height-_2b.b+_32-(_31?_46:0);
}else{
_2e.y=_2f.y=_2b.t;
_31=(_3d&&_3d=="away")?180:0;
_30.y=_2b.t-_32+(_31?0:_46);
_34.y=-1;
_35.y=-_35.y;
switch(_37){
case "start":
_37="end";
break;
case "end":
_37="start";
break;
case "middle":
_36.y-=_43;
break;
}
}
}
this.cleanGroup();
try{
var s=this.group,c=this.scaler,t=this.ticks,_47,f=_a.getTransformerFromModel(this.scaler),_48=(!o.title||!_31)&&!_2d&&this.opt.htmlLabels&&!_3("ie")&&!_3("opera")?"html":"gfx",dx=_34.x*_3e.length,dy=_34.y*_3e.length;
s.createLine({x1:_2e.x,y1:_2e.y,x2:_2f.x,y2:_2f.y}).setStroke(_42);
if(o.title){
var _49=_b.createText[_48](this.chart,s,_30.x,_30.y,"middle",o.title,_39,_3b);
if(_48=="html"){
this.htmlElements.push(_49);
}else{
_49.setTransform(g.matrix.rotategAt(_31,_30.x,_30.y));
}
}
if(t==null){
this.dirty=false;
return this;
}
_2.forEach(t.major,function(_4a){
var _4b=f(_4a.value),_4c,x=_2e.x+_33.x*_4b,y=_2e.y+_33.y*_4b;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_3e);
if(_4a.label){
var _4d=o.maxLabelCharCount?this.getTextWithLimitCharCount(_4a.label,_38,o.maxLabelCharCount):{text:_4a.label,truncated:false};
_4d=o.maxLabelSize?this.getTextWithLimitLength(_4d.text,_38,o.maxLabelSize,_4d.truncated):_4d;
_4c=this.createText(_48,s,x+dx+_35.x+(_2d?0:_36.x),y+dy+_35.y+(_2d?0:_36.y),_37,_4d.text,_38,_3a);
if(this.chart.truncateBidi&&_4d.truncated){
this.chart.truncateBidi(_4c,_4a.label,_48);
}
_4d.truncated&&this.labelTooltip(_4c,this.chart,_4a.label,_4d.text,_38,_48);
if(_48=="html"){
this.htmlElements.push(_4c);
}else{
if(_2d){
_4c.setTransform([{dx:_36.x,dy:_36.y},g.matrix.rotategAt(_2d,x+dx+_35.x,y+dy+_35.y)]);
}
}
}
},this);
dx=_34.x*_3f.length;
dy=_34.y*_3f.length;
_47=c.minMinorStep<=c.minor.tick*c.bounds.scale;
_2.forEach(t.minor,function(_4e){
var _4f=f(_4e.value),_50,x=_2e.x+_33.x*_4f,y=_2e.y+_33.y*_4f;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_3f);
if(_47&&_4e.label){
var _51=o.maxLabelCharCount?this.getTextWithLimitCharCount(_4e.label,_38,o.maxLabelCharCount):{text:_4e.label,truncated:false};
_51=o.maxLabelSize?this.getTextWithLimitLength(_51.text,_38,o.maxLabelSize,_51.truncated):_51;
_50=this.createText(_48,s,x+dx+_35.x+(_2d?0:_36.x),y+dy+_35.y+(_2d?0:_36.y),_37,_51.text,_38,_3a);
if(this.chart.getTextDir&&_51.truncated){
this.chart.truncateBidi(_50,_4e.label,_48);
}
_51.truncated&&this.labelTooltip(_50,this.chart,_4e.label,_51.text,_38,_48);
if(_48=="html"){
this.htmlElements.push(_50);
}else{
if(_2d){
_50.setTransform([{dx:_36.x,dy:_36.y},g.matrix.rotategAt(_2d,x+dx+_35.x,y+dy+_35.y)]);
}
}
}
},this);
dx=_34.x*_40.length;
dy=_34.y*_40.length;
_2.forEach(t.micro,function(_52){
var _53=f(_52.value),_54,x=_2e.x+_33.x*_53,y=_2e.y+_33.y*_53;
this.createLine(s,{x1:x,y1:y,x2:x+dx,y2:y+dy}).setStroke(_40);
},this);
}
catch(e){
}
this.dirty=false;
return this;
},labelTooltip:function(_55,_56,_57,_58,_59,_5a){
var _5b=["dijit/Tooltip"];
var _5c={type:"rect"},_5d=["above","below"],_5e=g._base._getTextBox(_58,{font:_59}).w||0,_5f=_59?g.normalizedLength(g.splitFontString(_59).size):0;
if(_5a=="html"){
_1.mixin(_5c,_6.coords(_55.firstChild,true));
_5c.width=Math.ceil(_5e);
_5c.height=Math.ceil(_5f);
this._events.push({shape:dojo,handle:_5.connect(_55.firstChild,"onmouseover",this,function(e){
require(_5b,function(_60){
_60.show(_57,_5c,_5d);
});
})});
this._events.push({shape:dojo,handle:_5.connect(_55.firstChild,"onmouseout",this,function(e){
require(_5b,function(_61){
_61.hide(_5c);
});
})});
}else{
var shp=_55.getShape(),lt=_6.coords(_56.node,true);
_5c=_1.mixin(_5c,{x:shp.x-_5e/2,y:shp.y});
_5c.x+=lt.x;
_5c.y+=lt.y;
_5c.x=Math.round(_5c.x);
_5c.y=Math.round(_5c.y);
_5c.width=Math.ceil(_5e);
_5c.height=Math.ceil(_5f);
this._events.push({shape:_55,handle:_55.connect("onmouseenter",this,function(e){
require(_5b,function(_62){
_62.show(_57,_5c,_5d);
});
})});
this._events.push({shape:_55,handle:_55.connect("onmouseleave",this,function(e){
require(_5b,function(_63){
_63.hide(_5c);
});
})});
}
}});
});

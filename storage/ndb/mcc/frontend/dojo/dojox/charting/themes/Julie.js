//>>built
define("dojox/charting/themes/Julie",["../Theme","dojox/gfx/gradutils","./common"],function(_1,_2){
var _3=dojox.charting.themes,g=_1.generateGradient,_4={type:"linear",space:"shape",x1:0,y1:0,x2:0,y2:100};
_3.Julie=new _1({seriesThemes:[{fill:g(_4,"#59a0bd","#497c91"),stroke:{color:"#22627d"}},{fill:g(_4,"#8d88c7","#6c6d8e"),stroke:{color:"#8a84c5"}},{fill:g(_4,"#85a54a","#768b4e"),stroke:{color:"#5b6d1f"}},{fill:g(_4,"#e8e667","#c6c361"),stroke:{color:"#918e38"}},{fill:g(_4,"#e9c756","#c7a223"),stroke:{color:"#947b30"}},{fill:g(_4,"#a05a5a","#815454"),stroke:{color:"#572828"}},{fill:g(_4,"#b17044","#72543e"),stroke:{color:"#74482e"}},{fill:g(_4,"#a5a5a5","#727272"),stroke:{color:"#535353"}},{fill:g(_4,"#9dc7d9","#59a0bd"),stroke:{color:"#22627d"}},{fill:g(_4,"#b7b3da","#8681b3"),stroke:{color:"#8a84c5"}},{fill:g(_4,"#a8c179","#85a54a"),stroke:{color:"#5b6d1f"}},{fill:g(_4,"#eeea99","#d6d456"),stroke:{color:"#918e38"}},{fill:g(_4,"#ebcf81","#e9c756"),stroke:{color:"#947b30"}},{fill:g(_4,"#c99999","#a05a5a"),stroke:{color:"#572828"}},{fill:g(_4,"#c28b69","#7d5437"),stroke:{color:"#74482e"}},{fill:g(_4,"#bebebe","#8c8c8c"),stroke:{color:"#535353"}},{fill:g(_4,"#c7e0e9","#92baca"),stroke:{color:"#22627d"}},{fill:g(_4,"#c9c6e4","#ada9d6"),stroke:{color:"#8a84c5"}},{fill:g(_4,"#c0d0a0","#98ab74"),stroke:{color:"#5b6d1f"}},{fill:g(_4,"#f0eebb","#dcd87c"),stroke:{color:"#918e38"}},{fill:g(_4,"#efdeb0","#ebcf81"),stroke:{color:"#947b30"}},{fill:g(_4,"#ddc0c0","#c99999"),stroke:{color:"#572828"}},{fill:g(_4,"#cfb09b","#c28b69"),stroke:{color:"#74482e"}},{fill:g(_4,"#d8d8d8","#bebebe"),stroke:{color:"#535353"}},{fill:g(_4,"#ddeff5","#a5c4cd"),stroke:{color:"#22627d"}},{fill:g(_4,"#dedcf0","#b3afd3"),stroke:{color:"#8a84c5"}},{fill:g(_4,"#dfe9ca","#c0d0a0"),stroke:{color:"#5b6d1f"}},{fill:g(_4,"#f8f7db","#e5e28f"),stroke:{color:"#918e38"}},{fill:g(_4,"#f7f0d8","#cfbd88"),stroke:{color:"#947b30"}},{fill:g(_4,"#eedede","#caafaf"),stroke:{color:"#572828"}},{fill:g(_4,"#e3cdbf","#cfb09b"),stroke:{color:"#74482e"}},{fill:g(_4,"#efefef","#cacaca"),stroke:{color:"#535353"}}]});
_3.Julie.next=function(_5,_6,_7){
if(_5=="line"||_5=="area"){
var s=this.seriesThemes[this._current%this.seriesThemes.length];
s.fill.space="plot";
var _8=_1.prototype.next.apply(this,arguments);
s.fill.space="shape";
return _8;
}
return _1.prototype.next.apply(this,arguments);
};
_3.Julie.post=function(_9,_a){
_9=_1.prototype.post.apply(this,arguments);
if(_a=="slice"&&_9.series.fill&&_9.series.fill.type=="radial"){
_9.series.fill=_2.reverse(_9.series.fill);
}
return _9;
};
return _3.Julie;
});

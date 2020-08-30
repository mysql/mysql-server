//>>built
define("dojox/store/DynamoDB",["dojo/_base/declare","dojo/Stateful","dojo/request","dojo/store/util/QueryResults","dojo/store/util/SimpleQueryEngine","dojo/_base/lang","dojo/_base/array","dojo/errors/RequestError","dojo/Deferred","../encoding/digests/SHA256","../encoding/digests/_base","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
function _d(_e,_f,_10){
_10=_10||_b.outputTypes.String;
return _a._hmac(_f,_e,_10);
};
function _11(_12,_13,_14){
this._signingDate=_12;
var _15=_d("AWS4"+_13,_12);
var _16=_d(_15,_14);
var _17=_d(_16,"dynamodb");
return _d(_17,"aws4_request");
};
function _18(_19){
function _1a(_1b){
_1b=String(_1b);
return _1b.length===1?"0"+_1b:_1b;
};
var _1c=String(_19.getUTCFullYear());
var _1d=_1a(_19.getUTCMonth()+1);
var day=_1a(_19.getUTCDate());
var _1e=_1a(_19.getUTCHours());
var _1f=_1a(_19.getUTCMinutes());
var _20=_1a(_19.getUTCSeconds());
return _1c+_1d+day+"T"+_1e+_1f+_20+"Z";
};
function _21(url){
if(typeof document!=="undefined"){
var a=document.createElement("a");
a.href=url;
return a.hostname;
}else{
url=_c.parse(url);
return url.hostname;
}
};
function _22(_23){
if(_23==null){
return "NULL";
}
var _24=typeof _23;
if(_24==="string"){
return "S";
}
if(_24==="boolean"){
return "BOOL";
}
if(_24==="number"){
return "N";
}
if(_23 instanceof Array){
return "L";
}
return "M";
};
function _25(_26){
var _27=_22(_26);
var _28={};
var _29;
switch(_27){
case "BOOL":
case "S":
_29=_26;
break;
case "N":
_29=String(_26);
break;
case "NULL":
_29=true;
break;
case "M":
_29={};
for(var key in _26){
_29[key]=_25(_26[key]);
}
break;
case "L":
_29=[];
for(var i=0;i<_26.length;i++){
_29[i]=_25(_26[i]);
}
break;
default:
throw new Error("Unknown Dynamo type: "+_27);
}
_28[_27]=_29;
return _28;
};
function _2a(_2b){
var _2c;
var _2d;
for(_2c in _2b){
_2d=_2b[_2c];
}
function _2e(_2f){
if(_2c==="N"){
return Number(_2f);
}
if(_2c==="NULL"){
return null;
}
if(_2c==="L"){
return _7.map(_2f,function(_30){
return _2a(_30);
});
}
if(_2c==="M"){
var _31={};
for(var key in _2f){
_31[key]=_2a(_2f[key]);
}
return _31;
}
return _2f;
};
if(_2c.charAt(1)==="S"){
_2c=_2c.charAt(0);
return _7.map(_2d,_2e);
}
return _2e(_2d);
};
function _32(_33){
var _34={};
for(var k in _33){
_34[k]=_25(_33[k]);
}
return _34;
};
function _35(_36){
if(!_36){
return null;
}
var _37={};
for(var k in _36){
_37[k]=_2a(_36[k]);
}
return _37;
};
return _1(_2,{tableName:"",attributesToGet:null,consistentRead:false,maxRetries:5,idProperty:"id",queryEngine:_5,region:null,endpointUrl:null,credentials:null,_signRequest:function(_38,_39){
_39=_18(_39||new Date());
var _3a=_39.slice(0,8);
var _3b=this.credentials.SecretAccessKey;
var _3c=this.credentials.AccessKeyId;
var _3d=this.credentials.SessionToken;
var _3e="";
var _3f="";
_38.headers["x-amz-date"]=_39;
if(_3d){
_3e="x-amz-security-token:"+_3d+"\n";
_3f=";x-amz-security-token";
_38.headers["x-amz-security-token"]=_3d;
}
var _40=_38.method+"\n/\n\n"+"host:"+_38.host+"\n"+"x-amz-date:"+_39+"\n"+_3e+"x-amz-target:"+_38.headers["x-amz-target"]+"\n"+"\n"+"host;x-amz-date"+_3f+";x-amz-target\n"+_a(_38.body,_b.outputTypes.Hex);
var key=_11(_3a,_3b,this.region);
var _41="AWS4-HMAC-SHA256\n"+_39+"\n"+_3a+"/"+this.region+"/dynamodb/aws4_request\n"+_a(_40,_b.outputTypes.Hex);
var _42=_d(key,_41,_b.outputTypes.Hex);
_38.headers.authorization="AWS4-HMAC-SHA256 Credential="+_3c+"/"+_3a+"/"+this.region+"/dynamodb/aws4_request,SignedHeaders=host;x-amz-date;x-amz-target"+_3f+",Signature="+_42;
},_rpc:function(_43,_44){
var _45;
var _46;
var dfd=new _9(function(){
_46&&_46.cancel.apply(_3,arguments);
clearTimeout(_45);
});
var _47={"Content-Type":"application/x-amz-json-1.0","x-amz-target":"DynamoDB_20120810."+_43};
_44.TableName=this.tableName;
_44=JSON.stringify(_44);
if(!this.endpointUrl){
this.endpointUrl="https://dynamodb."+this.region+".amazonaws.com";
}
if(this.credentials){
this._signRequest({body:_44,host:_21(this.endpointUrl),method:"POST",headers:_47});
}
var _48=this.endpointUrl;
var _49=this.maxRetries;
var _4a=0;
(function sendRequest(){
_46=_3.post(_48,{data:_44,headers:_47,handleAs:"json"}).then(_6.hitch(dfd,"resolve"),function(_4b){
var _4c=_4b.response;
if(++_4a===_49){
dfd.reject(_4b);
return;
}
if(_4c.status>=500||_4c.status<400||(_4c.status===400&&_4c.data&&_4c.data.__type&&(_4c.data.__type.indexOf("#ProvisionedThroughputExceededException")>-1||_4c.data.__type.indexOf("#ThrottlingException")>-1))){
_45=setTimeout(_4d,Math.pow(2,_4a)*50);
}else{
dfd.reject(_4b);
}
});
})();
return dfd.promise.then(function(_4e){
return _4e;
},function(_4f){
var _50=_4f.response&&_4f.response.data;
if(_50){
throw new _8(_50.__type+": "+_50.message,_4f.response);
}
throw _4f;
});
},_getKeyFromId:function(id){
var key={};
if(this.idProperty instanceof Array){
if(typeof id==="string"){
id=_7.map(id.split("/"),function(_51){
var _52=_51.charAt(0);
_51=_51.slice(1);
if(_52==="N"){
_51=+_51;
}
return _51;
});
}
_7.forEach(id,function(_53,_54){
key[this.idProperty[_54]]=_25(_53);
},this);
}else{
key[this.idProperty]=_25(id);
}
return key;
},get:function(id){
var _55={Key:this._getKeyFromId(id),ConsistentRead:this.consistentRead};
if(this.attributesToGet){
_55.AttributesToGet=this.attributesToGet;
}
return this._rpc("GetItem",_55).then(function(_56){
return _35(_56.Item);
});
},getIdentity:function(_57){
var id;
if(this.idProperty instanceof Array){
id=_7.map(this.idProperty,function(_58){
return _22(_57[_58])+_57[_58];
}).join("/");
}else{
id=_57[this.idProperty];
}
return id;
},query:function(_59,_5a){
function _5b(map){
var _5c={};
for(var key in map){
_5c[key]=_25(map[key]);
}
return _5c;
};
_5a=_5a||{};
var _5d={KeyConditions:{},ConsistentRead:this.consistentRead};
if(this.attributesToGet){
_5d.AttributesToGet=this.attributesToGet;
}
if(_5a.indexName){
_5d.IndexName=_5a.indexName;
}
if(_5a.sort&&_5a.sort.length){
if(_5a.sort.length>1){
throw new Error("Cannot sort by more than one dimension");
}
if(!_5d.IndexName){
_5d.IndexName=_5a.sort[0].attribute;
}
_5d.ScanIndexForward=!_5a.sort[0].descending;
}
for(var k in _59){
var _5e=_59[k];
_5d.KeyConditions[k]={AttributeValueList:_5e instanceof Array?_7.map(_5e,_25):[_25(_5e)],ComparisonOperator:"EQ"};
}
if(_5a.filter){
var _5f=_5a.filter;
_5d.FilterExpression=_5f.FilterExpression;
if(_5f.ExpressionAttributeValues){
_5d.ExpressionAttributeValues=_5b(_5f.ExpressionAttributeValues);
}
if(_5f.ExpressionAttributeNames){
_5d.ExpressionAttributeNames=_5f.ExpressionAttributeNames;
}
}
var dfd=new _9(function(){
_66&&_66.cancel.apply(_66,arguments);
});
var _60=_4(dfd.promise);
if(_5a.fetchTotal!==false){
_60.total=this._rpc("Query",_6.mixin({},_5d,{Select:"COUNT"})).then(function(_61){
return _61.Count;
});
}
var _62=this;
var _63=[];
var _64=typeof _5a.start==="number"?_5a.start:0;
var _65=(_64+(_5a.count||0))||Infinity;
var _66;
(function nextQuery(_67){
_5d.Limit=_65<Infinity?_65:undefined;
_5d.ExclusiveStartKey=_67;
_66=_62._rpc("Query",_5d).then(function(_68){
if(_68.Items.length){
var _69=_7.map(_68.Items.slice(_64),_35);
if(_64>0){
_64=Math.max(_64-_69.length,0);
}
_65-=_69.length;
_63=_63.concat(_69);
}
if(_65>0&&_68.LastEvaluatedKey){
_6a(_68.LastEvaluatedKey);
}else{
dfd.resolve(_63);
}
},_6.hitch(dfd,"reject"));
})(typeof _5a.start==="object"?_32(_5a.start):undefined);
return _60;
},remove:function(id,_6b){
_6b=_6b||{};
var _6c={Key:this._getKeyFromId(id),ReturnValues:"ALL_OLD"};
if(_6b.expected){
_6c.Expected={};
for(var k in _6b.expected){
_6c.Expected[k]={Value:_25(_6b.expected[k])};
}
}
return this._rpc("DeleteItem",_6c).then(function(_6d){
return _35(_6d.Attributes);
});
},put:function(_6e,_6f){
_6f=_6f||{};
var _70={Item:_32(_6e)};
if(_6f.id){
_6.mixin(_70.Item,this._getKeyFromId(_6f.id));
}
if(_6f.overwrite===false){
_70.Expected={};
var _71=this.idProperty instanceof Array?this.idProperty:[this.idProperty];
_7.forEach(_71,function(_72){
_70.Expected[_72]={Exists:false};
});
}else{
if(_6f.expected){
_70.Expected={};
for(var k in _6f.expected){
_70.Expected[k]={Value:_25(_6f.expected[k])};
}
}
}
return this._rpc("PutItem",_70).then(function(){
});
},add:function(_73,_74){
_74=_74||{};
_74.overwrite=false;
return this.put(_73,_74);
}});
});

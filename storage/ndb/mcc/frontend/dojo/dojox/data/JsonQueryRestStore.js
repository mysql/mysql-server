//>>built
define("dojox/data/JsonQueryRestStore",["dojo/_base/declare","dojox/data/JsonRestStore","dojox/data/util/JsonQuery","dojox/data/ClientFilter","dojox/json/query"],function(_1,_2,_3){
return _1("dojox.data.JsonQueryRestStore",[_2,_3],{matchesQuery:function(_4,_5){
return _4.__id&&(_4.__id.indexOf("#")==-1)&&this.inherited(arguments);
}});
});

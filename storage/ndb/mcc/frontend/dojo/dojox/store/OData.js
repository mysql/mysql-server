define([
	"dojo/io-query",
	"dojo/request",
	"dojo/_base/lang",
	"dojo/json",
	"dojo/_base/declare",
	"dojo/store/util/QueryResults"
], function(ioQuery, request, lang, JSON, declare, QueryResults){
	var base = null;
	return declare(base, {
		// summary:
		//      This is a basic store for RESTful cumminication with an OData
		//      server through JSON formatted data. It implements the
		//      dojo/store api and is designed to work specifically with WCF
		//      Microsoft SharePoint 2013 V2 services, though standard OData
		//      V2 endpoints should work as well.

		// headers: Object
		//      Additional headers to pass in all requests to the server. These
		//      can be overridden by passing additional headers to calls to the
		//      store.
		headers     : {"MaxDataServiceVersion" : "2.0"},
		// target: String
		//      The target base URL to use for all requests to the server. This
		//      string will be prepended to the id to generate the URL (relative
		//      or absolute) for requests sent to the server
		target      : "",
		// idProperty: String
		//      Indicates the property to use as the identity property. The
		//      values of this property should be unique.
		idProperty  : "id",
		// accepts: String
		//      Indicates the Accepts header to use for all requests to the
		//      server. This can be overridden by passing additional headers
		//      to calls to the store
		accepts     : "application/json;odata=verbose",
		// childAttr: String
		//		Indicates the attribute that represents a given object's children.
		//		Used in hierarchical store operations, such as getChildren
		childAttr: "children",

		constructor: function(options){
			declare.safeMixin(this, options);
		},

		get: function(id, options){
			// summary:
			//      Retrieves an object by its identity. This will trigger a
			//      GET request to the server using the url `this.target +
			//      "("+id+")"`.
			// id: String
			//      The identity to use to lookup the object
			// options: Object?
			//      HTTP headers. For consistency with other methods, if a
			//      `headers` key exists on this object, it will be used to
			//      provide HTTP headers instead.
			// returns: Object
			//      The object in the store that matches the given id.
			options = options || {};
			var headers = lang.mixin({ Accept: this.accepts }, this.headers, options.headers || options);
			return request( this.target + "("+id+")", {
				handleAs: "json",
				headers: headers
			}).then(function(data){
				return data.d;
			});
		},

		getIdentity: function(object){
			// summary:
			//      Returns an object's identity
			// object: Object
			//      The object to get the identity from
			// returns: String
			return object[this.idProperty];
		},

		put: function(object, options){
			// summary:
			//      Updates an object. This will trigger a PUT or MERGE request
			//      to the server depending on if it is an incremental update
			// object: Object
			//      The object to store.
			// options: __PutDirectives?
			//      Additional metadata for storing the data.  Includes an "id"
			//      property if a specific id is to be used.
			// returns: dojo/_base/Deferred
			options = options || {};
			var id = this.getIdentity(object) || options[this.idProperty];
			var url = id ? (this.target + "("+id+")") : this.target;
			var headers = lang.mixin({
				"Content-Type": "application/json;odata=verbose",
				Accept: this.accepts
			}, this.headers, options.headers);
			if(id){
				headers["X-HTTP-Method"] = options.overwrite ? "PUT" : "MERGE";
				headers["IF-MATCH"] = options.overwrite ? "*" : (options.etag||"*")
			}
			return request.post( url, {
				handleAs: "json",
				data: JSON.stringify(object),
				headers: headers
			});
		},

		add: function(object, options){
			// summary:
			//      Adds an object. This will trigger a POST request to the server
			// object: Object
			//      The object to store.
			// options: __PutDirectives?
			//      Additional metadata for storing the data.  Includes an "id"
			//      property if a specific id is to be used.
			options = options || {};
			options.overwrite = false;
			return this.put(object, options);
		},

		remove: function(id, options){
			// summary:
			//      Deletes an object by its identity. This will trigger a DELETE
			//      request to the server.
			// id: String
			//      The identity to use to delete the object
			// options: __HeaderOptions?
			//      HTTP headers.
			options = options || {};
			return request.post(this.target + "("+id+")", {
				headers: lang.mixin({
					"IF-MATCH" : "*",
					"X-HTTP-Method" : "DELETE"
				}, this.headers, options.headers)
			});
		},

		getFormDigest: function(){
			// summary:
			//      Makes POST request to WCF endpoint based on target url to
			//      get a form digest authorization token
			var i = this.target.indexOf("_vti_bin");
			var url = this.target.slice(0, i)+"_api/contextinfo";
			return request.post(url).then(function(xml){
				return xml.substring(
					xml.indexOf("<d:FormDigestValue>")+19,
					xml.indexOf("</d:FormDigestValue>"));
			});
		},

		getChildren: function(parent, options){
			// summary:
			//      Retrieves the children of an object.
			// parent: Object
			//      The object to find the children of.
			// options: dojo/store/api/Store.QueryOptions?
			//      Additional options to apply to the retrieval of the children.
			// returns: dojo/store/api/Store.QueryResults
			//      A result set of the children of the parent object.
			var id = this.getIdentity(object) || options[this.idProperty];
			return this.query({
				"$filter" : this.idProperty+" eq "+id,
				"$expand" : this.childAttr
			}, options);
		},

		query: function(query, options){
			// summary:
			//      Queries the store for objects. This will trigger a GET
			//      request to the server, with the query added as a query
			//      string that adheres to OData URI conventions.
			// query: Object
			//      The query to use for retrieving objects from the store.
			//      Note that OData service operation keys are perfectly
			//      acceptable, e.g. `$filter`
			// options: __QueryOptions?
			//      The optional arguments to apply to the resultset.
			// returns: dojo/store/api/Store.QueryResults
			//      The results of the query, extended with iterative methods.
			options = options || {};
			var headers = lang.mixin({ Accept: this.accepts }, this.headers, options.headers);
			// Sorting
			if(options && options.sort){
				query["$orderby"] = "";
				var sort, i, len;
				// loop through sort keys, build proper OData $orderby URI
				for(i = 0, len=options.sort.length; i<len; i++){
					sort = options.sort[i];
					query["$orderby"] += ( i>0 ? "," : "") + encodeURIComponent(sort.attribute) +
						(sort.descending ? " desc" : " asc");
				}
			}
			// Pagination
			if(options.start >= 0 || options.count >= 0){
				query["$skip"] = options.start || 0;
				query["$inlinecount"] = "allpages";
				if("count" in options && options.count != Infinity){
					query["$top"] = (options.count);
				}
			}
			// Render a proper OData query string from JSON
			query = this.buildQueryString(query);
			var xhr = request(this.target + (query || ""), {
				handleAs: "json",
				headers: headers
			});
			// Because the returned OData format doesn't match what
			// Dojo wants, let's chain another promise so we can grab
			// the proper results array off the returned OData JSON
			var results = xhr.then(function(data){
				return data.d.results;
			});
			// Wrap the results (promise or not) with various utility methods
			results = QueryResults(results);
			// Set a promise to resolve the total count
			results.total = xhr.then(function(data){
				return data.d.__count;
			});
			return results;
		},

		buildQueryString: function(query){
			// summary:
			//      Constructs appropriate OData V2 URI based on a given
			//      query object.
			// query: Object
			//      The query to use for retrieving objects from the store.
			//      Note that OData service operation keys are perfectly
			//      acceptable, e.g. `$filter`.
			// returns: String
			//      A OData V2 request URI in proper format
			var filterStr = "";
			for(var key in query){
				// Only target non-system query options
				if(query.hasOwnProperty(key) && key.indexOf("$") == -1){
					var value = query[key]+"";
					var i = value.indexOf("*");
					// Handle wildcards
					if(i != -1){
						value = value.slice(i!=0 ? 0 : 1, value.length-(i!=0 ? 1 : 0));
						if(value.length > 0){
							filterStr += (filterStr.length == 0) ? "" : "and ";
							filterStr += (i==0 ? "endswith" : "startswith")+"("+key+",'"+value+"')";
						}
					}
				}
			}
			// Add filter string
			if(filterStr.length > 0){
				query["$filter"] = (query["$filter"] && query["$filter"].length>0) ?
					(query["$filter"]+" and "+filterStr) : filterStr;
			}
			// Query-string-ify the query object properly
			var hasQuestionMark = this.target.indexOf("?") > -1;
			if(query && typeof query == "object"){
				query = ioQuery.objectToQuery(query);
				query = query ? (hasQuestionMark ? "&" : "?") + query: "";
			}
			return query;
		}
	});
});

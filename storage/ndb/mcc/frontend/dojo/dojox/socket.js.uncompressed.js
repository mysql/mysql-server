define("dojox/socket", [
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/_base/xhr",
	"dojo/aspect",
	"dojo/on",
	"dojo/Evented",
	"dojo/_base/url"
], function(array, lang, xhr, aspect, on, Evented, dBaseUrl) {

var WebSocket = window.WebSocket;

var Socket = function(/*dojo.__XhrArgs*/ argsOrUrl){
	// summary:
	//		Provides a simple socket connection using WebSocket, or alternate
	//		communication mechanisms in legacy browsers for comet-style communication. This is based
	//		on the WebSocket API and returns an object that implements the WebSocket interface:
	//		http://dev.w3.org/html5/websockets/#websocket
	// description:
	//		Provides socket connections. This can be used with virtually any Comet protocol.
	// argsOrUrl:
	//		This uses the same arguments as the other I/O functions in Dojo, or a
	//		URL to connect to. The URL should be a relative URL in order to properly
	//		work with WebSockets (it can still be host relative, like //other-site.org/endpoint)
	// returns:
	//		An object that implements the WebSocket API
	// example:
	//		| require(["dojox/socket", "dojo/aspect"], function(socket, aspect) {
	//		|    var sock = socket({"url://comet-server/comet");
	//		|    // we could also add auto-reconnect support
	//		|    // now we can connect to standard HTML5 WebSocket-style events
	//		|    aspect.after(socket, "onmessage", function(event){
	//		|       var message = event.data;
	//		|       // do something with the message
	//		|    });
	//		|    // send something
	//		|    sock.send("hi there");
	//		|    ...
	//		| });
	//		You can also use the Reconnect module:
	//		| require["dojox/socket", "dojox/socket/Reconnect"], function(dxSocket, reconnect){
	//		|    var socket = dxSocket({url:"/comet"});
	//		|    // add auto-reconnect support
	//		|    socket = reconnect(socket);
	if(typeof argsOrUrl == "string"){
		argsOrUrl = {url: argsOrUrl};
	}
	return WebSocket ? Socket.WebSocket(argsOrUrl, true) : Socket.LongPoll(argsOrUrl);
};

Socket.WebSocket = function(args, fallback){
	// summary:
	//		A wrapper for WebSocket, than handles standard args and relative URLs
	var baseURI = document.baseURI || window.location.href;
	var ws = new WebSocket(new dBaseUrl(baseURI.replace(/^http/i,'ws'), args.url));
	ws.on = function(type, listener){
		ws.addEventListener(type, listener, true);
	};
	var opened;
	aspect.after(ws, "onopen", function(event){
		opened = true;
	}, true);
	aspect.after(ws, "onclose", function(event){
		if(opened){
			return;
		}
		if(fallback){
			Socket.replace(ws, Socket.LongPoll(args), true);
		}
	}, true);
	return ws;
};

Socket.replace = function(socket, newSocket, listenForOpen){
	// make the original socket a proxy for the new socket
	socket.send = lang.hitch(newSocket, "send");
	socket.close = lang.hitch(newSocket, "close");
	var proxyEvent = function(type){
		(newSocket.addEventListener || newSocket.on).call(newSocket, type, function(event){
			on.emit(socket, event.type, event);
		}, true);
	};

	if(listenForOpen){
		proxyEvent("open");
	}
	// redirect the events as well
	array.forEach(["message", "close", "error"], proxyEvent);
};

Socket.LongPoll = function(/*dojo.__XhrArgs*/ args){
	// summary:
	//		Provides a simple long-poll based comet-style socket/connection to a server and returns an
	//		object implementing the WebSocket interface:
	//		http://dev.w3.org/html5/websockets/#websocket
	// args:
	//		This uses the same arguments as the other I/O functions in Dojo, with this addition:
	//	args.interval:
	//		Indicates the amount of time (in milliseconds) after a response was received
	//		before another request is made. By default, a request is made immediately
	//		after getting a response. The interval can be increased to reduce load on the
	//		server or to do simple time-based polling where the server always responds
	//		immediately.
	//	args.transport:
	//		Provide an alternate transport like dojo.io.script.get
	// returns:
	//		An object that implements the WebSocket API
	// example:
	//		| dojo.require("dojox.socket.LongPoll");
	//		| var socket = dojox.socket.LongPoll({url:"/comet"});
	//		or:
	//		| dojo.require("dojox.socket.LongPoll");
	//		| dojox.socket.LongPoll.add();
	//		| var socket = dojox.socket({url:"/comet"});

	var cancelled = false,
		first = true,
		timeoutId,
		connections = [];

	// create the socket object
	var fire, connect;
	var socket = {
		send: function(data){
			// summary:
			//		Send some data using XHR or provided transport
			var sendArgs = lang.delegate(args);
			sendArgs.rawBody = data;
			clearTimeout(timeoutId);
			var deferred = first ? (first = false) || socket.firstRequest(sendArgs) :
				socket.transport(sendArgs);
			connections.push(deferred);
			deferred.then(function(response){
				// got a response
				socket.readyState = 1;
				// remove the current connection
				connections.splice(array.indexOf(connections, deferred), 1);
				// reconnect to listen for the next message if there are no active connections,
				// we queue it up in case one of the onmessage handlers has a message to send
				if(!connections.length){
					timeoutId = setTimeout(connect, args.interval);
				}
				if(response){
					// now send the message along to listeners
					fire("message", {data: response}, deferred);
				}
			}, function(error){
				connections.splice(array.indexOf(connections, deferred), 1);
				// an error occurred, fire the appropriate event listeners
				if(!cancelled){
					fire("error", {error:error}, deferred);
					if(!connections.length){
						clearTimeout(timeoutId);
						socket.readyState = 3;
						fire("close", {wasClean:false}, deferred);
					}
				}
			});
			return deferred;
		},
		close: function(){
			// summary:
			//		Close the connection
			socket.readyState = 2;
			cancelled = true;
			var i;
			for(i = 0; i < connections.length; i++){
				connections[i].cancel();
			}
			socket.readyState = 3;
			fire("close", {wasClean:true});
		},
		transport: args.transport || xhr.post,
		args: args,
		url: args.url,
		readyState: 0,
		CONNECTING: 0,
		OPEN: 1,
		CLOSING: 2,
		CLOSED: 3,
		on: Evented.prototype.on,
		firstRequest: function(args){
			// summary:
			//		This allows for special handling for the first request. This is useful for
			//		providing information to disambiguate between the first request and
			//		subsequent long-poll requests so the server can properly setup a
			//		connection on the first connection or reject a request for an expired
			//		connection if the request is not expecting to be the first for a connection.
			//		This method can be overriden. The default behavior is to include a Pragma
			//		header with a value of "start-long-poll"
			var headers = (args.headers || (args.headers = {}));
			headers.Pragma = "start-long-poll";
			try{
				return this.transport(args);
			}finally{
				// cleanup the header so it is not used on subsequent requests
				delete headers.Pragma;
			}
		}
	};
	fire = function(type, object, deferred){
		if(socket["on" + type]){
			object.ioArgs = deferred && deferred.ioArgs;
			object.type = type;
			on.emit(socket, type, object);
		}
	};
	connect = function(){
		if(socket.readyState === 0){
			// we fire the open event now because we really don't know when the "socket"
			// is truly open, and this gives us a to do a send() and get it included in the
			// HTTP request
			fire("open",{});
		}
		// make the long-poll connection, to wait for response from the server
		if(!connections.length){
			socket.send();
		}
	};
	// provide an alias for Dojo's connect method
	socket.connect = socket.on;
	// do the initial connection
	setTimeout(connect);
	return socket;
};

return Socket;

});

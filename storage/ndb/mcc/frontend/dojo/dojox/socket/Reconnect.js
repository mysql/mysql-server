define([
	"dojox/socket",
	"dojo/aspect"
], function(dxSocket, aspect) {

	dxSocket.Reconnect = function(socket, options){
		// summary:
		//		Provides auto-reconnection to a websocket after it has been closed
		// socket:
		//		Socket to add reconnection support to.
		// returns:
		//		An object that implements the WebSocket API
		// example:
		//		You can use the Reconnect module:
		//		| require["dojox/socket", "dojox/socket/Reconnect"], function(dxSocket, reconnect){
		//		|    var socket = dxSocket({url:"/comet"});
		//		|    // add auto-reconnect support
		//		|    socket = reconnect(socket);
		options = options || {};

		var reconnectTime = options.reconnectTime || 10000;
		var backoffRate = options.backoffRate || 2;
		var timeout = reconnectTime;
		var checkForOpen, newSocket;

		aspect.after(socket, "onclose", function(event){
			clearTimeout(checkForOpen);
			if(!event.wasClean){
				socket.disconnected(function(){
					dxSocket.replace(socket, newSocket = socket.reconnect());
				});
			}
		}, true);
		if(!socket.disconnected){
			// add a default impl if it doesn't exist
			socket.disconnected = function(reconnect){
				setTimeout(function(){
					reconnect();
					checkForOpen = setTimeout(function(){
						//reset the backoff
						if(newSocket.readyState < 2){
							timeout = reconnectTime;
						}
					}, reconnectTime);
				}, timeout);
				// backoff each time
				timeout *= backoffRate;
			};
		}
		if(!socket.reconnect){
			// add a default impl if it doesn't exist
			socket.reconnect = function(){
				return socket.args ?
					dxSocket.LongPoll(socket.args) :
					dxSocket.WebSocket({url: socket.URL || socket.url}); // different cases for different impls
			};
		}
		return socket;
	};

	return dxSocket.Reconnect;
});

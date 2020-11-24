var uuid_v4 = function() {
  return "00000000-0000-4000-8000-000000000000".replace(/0/g, function() {
    return (0 | Math.random() * 16).toString(16)
  });
};

/**
 * generated a single-host group-replication membership configuration
 *
 * all nodes are on a single host with ip/hostname 'host' listening
 * on 'port' and announce 'state'
 *
 * @param {string} host hostname or ip-address of the host
 * @param {array} port_and_state port-number node is listening on,
 *     node-state ("ONLINE", "ERROR", ....)
 *                and xport if available (or undefined otherwise)
 * @returns group replication membership resultset
 */
exports.single_host = function(host, port_and_state, gr_id) {
  return port_and_state.map(function(current_value) {
    var xport = current_value[2] === undefined ? 0 : current_value[2];
    return [
      gr_id === undefined ? uuid_v4() : gr_id, host, current_value[0],
      current_value[1], xport
    ];
  });
};

exports.nodes = function(host, port_and_state) {
  return port_and_state.map(function(current_value) {
    var xport = current_value[2] === undefined ? 0 : current_value[2];
    return [current_value[0], host, current_value[0], current_value[1], xport];
  });
};

exports.members = function(host_and_port) {
  return host_and_port.map(function(current_value) {
    return [current_value[0], current_value[1]];
  });
};

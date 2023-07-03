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
    return [
      gr_id === undefined ? uuid_v4() : gr_id, host,
      current_value[0],  // classic port
      current_value[1]   // member state
    ];
  });
};

exports.single_host_cluster_nodes = function(host, classic_port, uuid) {
  return classic_port.map(function(current_value) {
    return [
      uuid === undefined ? uuid_v4() : uuid, host, classic_port[0],
      0,   // xport
      "",  // attributes
    ];
  });
};

exports.gr_members = function(host, port_and_state) {
  return port_and_state.map(function(current_value) {
    return [
      current_value[0],  // use port as uuid
      host,
      current_value[0],  // port
      current_value[1]   // member_state
    ];
  });
};

exports.members = function(id_host_and_port) {
  return id_host_and_port.map(function(current_value) {
    return [current_value[0], current_value[1], current_value[2]];
  });
};

exports.cluster_nodes = function(host, cluster_instances) {
  return cluster_instances.map(function(current_value) {
    return [
      current_value[0],  // classic port as uuid
      host,
      current_value[0],  // classic port
      current_value[1],  // x port
      current_value[2],  // attributes
    ];
  });
};
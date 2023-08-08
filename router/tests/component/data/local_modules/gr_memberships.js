var uuid_v4 = function() {
  return "00000000-0000-4000-8000-000000000000".replace(/0/g, function() {
    return (0 | Math.random() * 16).toString(16)
  });
};

/**
 * generated a single-host group-replication membership configuration
 *
 * all nodes are on a single host with ip/hostname 'host' listening
 * on 'port' and announce 'state' and 'role'
 *
 * @param {string} host hostname or ip-address of the host
 * @param {array} port_state_role port-number node is listening on,
 *     node-state ("ONLINE", "ERROR", ....)
 *     node-role ("PRIMARY", "SECONDARY")
 * @returns group replication membership resultset
 */
exports.single_host = function(host, port_state_role, gr_id) {
  return port_state_role.map(function(current_value) {
    return [
      gr_id === undefined ? uuid_v4() : gr_id, host,
      current_value[0],  // classic port
      current_value[1],  // member state
      current_value[2],  // member role
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

exports.gr_members = function(host, gr_instances) {
  return gr_instances.map(function(current_value) {
    return [
      current_value[0],  // uuid
      host,
      current_value[1],  // port
      current_value[2],  // member_state
      current_value[3]   // member_role
    ];
  });
};

exports.cluster_nodes = function(host, cluster_instances) {
  return cluster_instances.map(function(current_value) {
    return [
      current_value[0],  // uuid
      host,
      current_value[1],  // classic port
      current_value[2],  // x port
      current_value[3],  // attributes
      current_value[4],  // role (for ReplicaSet it is in the nodes as there is
                         // no GR)
    ];
  });
};
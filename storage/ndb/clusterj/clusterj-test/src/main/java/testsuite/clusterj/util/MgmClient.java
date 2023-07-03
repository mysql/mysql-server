/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

package testsuite.clusterj.util;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Properties;

import com.mysql.clusterj.Constants;

public class MgmClient implements AutoCloseable {

    Socket mgmSocket;
    PrintWriter mgmSocketWriter;
    BufferedReader mgmSocketReader;

    // A class to hold node details
    private static class Node {
        public enum NodeType {
            NDB, MGM, API
        };

        public final int id;
        public final NodeType type;

        Node (int nodeId, NodeType nodeType) {
            id = nodeId;
            type = nodeType;
        }
    };

    // List of all nodes received from Management Server
    List<Node> nodes;

    public MgmClient(Properties props) throws IOException {
        // Extract MGM host and port from connect string
        String connectString = props
                .getProperty(Constants.PROPERTY_CLUSTER_CONNECTSTRING);
        String[] components = connectString.split(",");
        // The connect string might have multiple management host
        // definitions. The first definition will be used here.
        String hostDefinition;
        if (components[0].startsWith("nodeid")) {
            // Skip the optional nodeid component
            hostDefinition = components[1];
        } else {
            hostDefinition = components[0];
        }
        String[] hostAndPort = hostDefinition.split(":");
        // The first token is the host name
        String mgmHost = hostAndPort[0];
        // Set default port unless specified explicitly
        int mgmPort = (hostAndPort.length > 0)
                ? Integer.parseInt(hostAndPort[1])
                : 1186;

        // Create socket and connect to management server
        // throws IOException if there is an error
        try {
            mgmSocket = new Socket(mgmHost, mgmPort);
            // Open writer and reader.
            mgmSocketWriter = new PrintWriter(mgmSocket.getOutputStream(),
                    true);
            mgmSocketReader = new BufferedReader(
                    new InputStreamReader(mgmSocket.getInputStream()));
        } catch (UnknownHostException e) {
            // Should not happen
            throw new RuntimeException("Failed to connect to the management node", e);
        }

        // Fetch node list and store
        retrieveNodeStatus();
    }

    @Override
    public void close() {
        // Close the reader, writer and socket
        try {
            if (mgmSocketReader != null) {
                mgmSocketReader.close();
            }
            if (mgmSocketWriter != null) {
                mgmSocketWriter.close();
            }
            if (mgmSocket != null) {
                mgmSocket.close();
            }
        } catch (IOException e) {
            System.err.println("Caught exception when closing the MgmClient : " + e.getMessage());
        }
    }

    /**
     * Verify that the given expression evaluates to true.
     * If not, throw a RuntimeException.
     *
     * @param expression the boolean expression to be evaluated
     * @param message the message to be set in exception
     */
    private void verify(boolean expression, String message) {
        if (!expression) {
            throw new RuntimeException(message);
        }
    }

    /**
     * Retrieve the node config from Management server
     *
     * @return true on success and false otherwise
     */
    private boolean retrieveNodeStatus() {
        nodes = new ArrayList<Node>();

        // Fetch node status from Management server
        List<String> reply = new ArrayList<String>();
        if (!executeCommand("get status", null, reply)) {
            return false;
        }

        // Parse the reply
        // The first line has the header
        verify(reply.get(0).equals("node status"), "Unrecognized header in 'get status' reply");
        // Extract number of nodes from second line
        String numOfNodesStr = reply.get(1);
        verify(numOfNodesStr.startsWith("nodes: "), "Unrecognized string in 'get status' reply");
        int numBeginPos = numOfNodesStr.indexOf(':') + 2;
        int numOfNodes = Integer.parseInt(numOfNodesStr.substring(numBeginPos));
        // Parse and extract the node ids
        for (String replyline : reply.subList(2, reply.size())) {
            if (!replyline.contains("type")) {
                continue;
            }

            // Tokenize the line then extract node id and type
            // The line will be of form node.<id>.type= <type>
            String[] replyTokens = replyline.split("\\.|=|: ");
            verify(replyTokens[0].equals("node") && replyTokens[2].equals("type"), "Unrecognized string in 'get status' reply");
            int id = Integer.parseInt(replyTokens[1]);
            // Add the node to the node List
            Node.NodeType type = Node.NodeType.valueOf(replyTokens[3]);
            nodes.add(new Node(id, type));
        }
        verify(nodes.size() == numOfNodes, "Incorrect number of nodes in 'get status' reply");
        return true;
    }

    /**
     * Execute the given MGM command by sending it to the MGM socket
     *
     * @param command the command to be executed
     * @param args the arguments to the command
     * @return true on success and false otherwise
     */
    private boolean executeCommand(String command, Map<String, Object> args,
            List<String> reply) {
        try {
            // Send the command and arguments
            mgmSocketWriter.println(command);
            if (args != null) {
                for (Entry<String, Object> e : args.entrySet()) {
                    mgmSocketWriter.println(e.getKey() + ":" + e.getValue());
                }
            }
            mgmSocketWriter.println();

            // Read back the reply
            mgmSocket.setSoTimeout(60000);
            String replyLine = null;
            do {
                replyLine = mgmSocketReader.readLine();
                reply.add(replyLine);
            } while (!replyLine.isEmpty());
            // Remove the last empty line from reply list
            reply.remove(reply.size() - 1);
        } catch (IOException e) {
            // Failed to read/write data
            System.err.println("Failed to read/write into the socket. Caught exception : " + e.getMessage());
            return false;
        }
        return true;
    }

    /**
     * Parse the given reply and verify that the command succeeded
     *
     * @param command the executed command whose reply is being parsed
     * @param reply the reply received from Management Server
     * @return true on success and false otherwise
     */
    private boolean parseResult(String command, List<String> reply) {
        // First line will have the reply header
        verify(reply.get(0).equals(command + " reply"), "Unrecognized header in '" + command + "' reply");

        // Extract result
        String resultValue = null;
        for (String replyLine : reply) {
            // Parse the replies and look for "result" key
            // Right now we just ignore any other entries
            if (replyLine.startsWith("result")) {
                int colonPos = replyLine.indexOf(":");
                resultValue = replyLine.substring(colonPos + 2);
                if (!resultValue.equalsIgnoreCase("Ok")) {
                    // Command failed. Print error
                    System.err.println("Command '" + command
                            + "' failed with following error : " + resultValue);
                    return false;
                }
                return true;
            }
        }
        return false;
    }

    /**
     * Provoke an error
     *
     * @param nodeId the node id.
     * @param errorCode the errorCode to be injected
     * @return true if error was inserted successfully
     */
    public boolean insertErrorOnNode(int nodeId, int error) {
        // Build the arguments
        String command = "insert error";
        Map<String, Object> args = new LinkedHashMap<String, Object>();
        args.put("node", nodeId);
        args.put("error", error);

        // Execute the command and return
        List<String> reply = new ArrayList<String>();
        return (executeCommand(command, args, reply)
                && parseResult(command, reply));
    }

    /**
     * Provoke an error on all data nodes
     *
     * @param errorCode the errorCode to be injected
     * @return true if error was inserted successfully on all nodes
     */
    public boolean insertErrorOnAllDataNodes(int error) {
        // Loop and insert error on all data nodes
        boolean result = false;
        for (Node node : nodes) {
            if (node.type == Node.NodeType.NDB) {
                result |= insertErrorOnNode(node.id, error);
            }
        }
        return result;
    }
}

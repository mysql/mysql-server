
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.io.*;
import java.net.*;
import javax.swing.*;

class Node extends Observable {
    public final static int UNDEFINED = -1;
    public final static int NDB_NODE  = 0;
    public final static int MGM_NODE  = 1;
    public final static int API_NODE  = 2;

    public int getNodeType() { return m_nodeType;}
    public static int getNodeType(String str) {
	if(str.equals("NDB"))
	    return NDB_NODE;
	if(str.equals("API"))
	    return API_NODE;
	if(str.equals("MGM"))
	    return MGM_NODE;
	return UNDEFINED;
    }
    
    protected int m_nodeType;
}

class NdbNode extends Node { 
    public NdbNode(){
	m_nodeType = NDB_NODE;
    }

    public Counters getCounters() { return counters; }

    public void setCounters(Counters _c) { 
	
	if(_c == null){
	    counters = null;
	    setChanged();
	    notifyObservers();
	    return;
	}
	
	int old_tps = 0;
	int old_ops = 0;
	int old_aps = 0;
	int diff = 5;
	if(counters != null){
	    old_tps = counters.tps;
	    old_ops = counters.ops;
	    old_aps = counters.aps;
	    diff = 5; //_c.epochsecs - counters.epochsecs;
	}
	
	switch(_c.type){
	case Counters.TRANSACTIONS:
	    _c.tps = (_c.transactions -_c.aborts)/ diff;
	    _c.aps = _c.aborts / diff;
	    _c.ops = old_ops;
	    break;
	case Counters.OPERATIONS:
	    _c.tps = old_tps;
	    _c.aps = old_aps;
	    _c.ops = _c.operations / diff;
	    break;
	}
	
	counters = _c;
	setChanged();
	notifyObservers();
    }

    public int getNodeState(){
	return nodeState;
    }
    
    public static int getNodeState(String state){
	if(state.equals("NOT_STARTED") ||
	   state.equals("NO_CONTACT"))
	    return 0;
	return 1;
    }

    public void setState(int nodeState){
	this.nodeState = nodeState;
	if(nodeState == 0)
	    counters = null;
    }

    private int nodeState;
    private Counters counters;
}

class MgmNode extends Node { public MgmNode(){ m_nodeType = MGM_NODE; } }
class ApiNode extends Node { public ApiNode(){ m_nodeType = API_NODE; } }

class Counters {

    public static final int TRANSACTIONS = 0;
    public static final int OPERATIONS = 1;

    public Counters(){
	transactions = operations = -1;
    }

    public int type;
    public int transactions;
    public int operations;
    public int aborts;
    public int tps;
    public int ops;
    public int aps;
    public int epochsecs;

    public String toString(){
	return "[Counters"+
	    " transactions="+transactions+
	    " operations="+operations+
	    " tps="+tps+
	    " ops="+ops+
	    " ]";
    }
}

class NdbCluster extends Observable {

    NdbCluster(int node_types[], int num_nodes, int maxNodeId){

	nodes = new Node[maxNodeId+1];
	maxCounters = new Counters();
	
	for(int i = 0; i<maxNodeId; i++)
	    nodes[i] = null;
	
	for(int i = 1; i<num_nodes; i++)
	    switch(node_types[i]){
	    case Node.NDB_NODE:
		nodes[i] = new NdbNode();
		break;
	    case Node.API_NODE:
		nodes[i] = new ApiNode();
		break;
	    case Node.MGM_NODE:
		nodes[i] = new MgmNode();
	    default:
	    }
    }
    
    public int getNoOfNdbNodes(){
	if(nodes == null)
	    return 0;
	int retVal = 0;
	for(int i = 1; i<nodes.length; i++)
	    if(getNodeType(i) == Node.NDB_NODE)
		retVal++;
	return retVal;
    }

    public int getNodeType(int nodeId){
	if(nodes == null || nodeId > nodes.length || nodes[nodeId] == null)
	    return Node.UNDEFINED;
	return nodes[nodeId].getNodeType();
    }
    
    public Counters getCounters(int nodeId){
	if(nodes == null || nodeId > nodes.length || nodes[nodeId] == null ||
	   nodes[nodeId].getNodeType() != Node.NDB_NODE)
	    return null;
	return ((NdbNode)nodes[nodeId]).getCounters();
    }
    
    public void setCounters(int nodeId, Counters _c){
	if(nodes == null || nodeId > nodes.length || nodes[nodeId] == null)
	    return;
	((NdbNode)nodes[nodeId]).setCounters(_c);

	int maxSum = 0;
	for(int i = 1; i<nodes.length; i++){
	    Counters c = getCounters(i);
	    if(c != null){
		int sum = c.tps + c.ops + c.aps;
		if(sum > maxSum){
		    maxCounters = c;
		    maxSum = sum;
		}
	    }
	}
	setChanged();
	notifyObservers();
    }

    public void setState(int nodeId, int nodeType, int nodeState){
	if(nodes == null || nodeId > nodes.length || nodes[nodeId] == null ||
	   nodes[nodeId].getNodeType() != nodeType)
	    return;
	
	if(nodeType != Node.NDB_NODE)
	    return;

	((NdbNode)nodes[nodeId]).setState(nodeState);
    }
    
    public void setNoConnection(){
	for(int i = 1; i<nodes.length; i++){
	    Counters c = getCounters(i);
	    if(c != null){
		setCounters(i, null);
	    }
	}
    }

    public Counters getMaxCounters(){
	return maxCounters;
    }

    private Node nodes[];
    private Counters maxCounters;
}

class CountersPanel extends JPanel implements Observer
{
    public CountersPanel(Dimension dim, NdbCluster _cluster, int maxInRow)
    {
	cluster = _cluster;
	cluster.addObserver(this);
	maxRow = maxInRow;
	reSize(dim);
    }
    
    private void showCounters(Graphics g, int node, int x, int y, boolean p)
    {
	Counters counters = cluster.getCounters(node);

	if (counters == null || p){
	    // Mark processor as not available
	    g.setColor(Color.black);
	    g.fillRect(x, y, width, height);
	} else {
	    int red    = (counters.aps * height) / scale;
	    int green  = (counters.tps * height) / scale;
	    int yellow = (counters.ops * height) / scale;

	    System.out.println("tps="+counters.tps+" ops="+counters.ops+" scale="+scale+" green="+green+" yellow="+yellow);

	    g.setColor(Color.white);
	    g.fillRect(x, y, width, height);
	    if (yellow + green + red <= height){ // Max 100% load
		int yellow_scaled = yellow;
		int green_scaled = green;
		int red_scaled = red;
		if (red_scaled > 0){
		    g.setColor(Color.red);
		    g.fillRect(x, 
			       height + y - red_scaled, 
			       width, 
			       red_scaled);
		}
		if (green_scaled > 0){
		    g.setColor(Color.green);
		    g.fillRect(x, 
			       height + y - red_scaled - green_scaled, 
			       width, 
			       green_scaled);
		}
		if (yellow_scaled > 0){
		    g.setColor(Color.yellow);
		    g.fillRect(x, 
			       height + y - red_scaled - green_scaled - yellow_scaled, 
			       width, 
			       yellow_scaled);
		}
	    }
	    // Draw box
	    g.setColor(Color.black);
	    g.drawRect(x, y, width, height);

	    float f = ((float)height)/((float)(lines+1));
	    
	    for(int i = 0; i<lines; i++){
		int ytmp = (int)(y+height-(i+1)*f);
		g.drawLine(x, ytmp, x+width, ytmp);
	    }
	}
    }
    
    public void paintComponent(Graphics g)
    {
	super.paintComponent(g);

	Counters maxCounters = cluster.getMaxCounters();
	final int sum = maxCounters.tps+maxCounters.ops+maxCounters.aps;
	boolean skipDraw = false;
	if(sum == 0){
	    skipDraw = true;
	} else {
	    lines = (sum / 1000) + 1;
	    scale = (lines+1) * 1000;
	}
	
	int nodeId = 0;
	int nodeNo = 0;
	final int noOfNdbNodes = cluster.getNoOfNdbNodes();
	for(int row = 0; row <= noOfNdbNodes / maxRow; row++) {
	    int end = Math.min(noOfNdbNodes, (row + 1) * maxRow);
	    for (int pos = 0; (nodeNo < noOfNdbNodes) && (pos < maxRow); pos++){
		while(cluster.getNodeType(nodeId) != Node.NDB_NODE)
		    nodeId++;
		showCounters(g,
			     nodeId, 
			     xindent + (xgap + width) * pos,
			     yindent + row * (height + ygap),
			     skipDraw);
		nodeNo++;
		nodeId++;
	    }
	}
    }
    
    public void setWidth(int maxInRow)
    {
	maxRow = maxInRow;
    }

    public void reSize(Dimension dim) 
    {
	final int noOfNdbNodes = cluster.getNoOfNdbNodes();
	
	// System.out.println(dim);
	int noRows = (int) Math.ceil((double) noOfNdbNodes / (double) maxRow);
	xgap = (noOfNdbNodes > 1) ? Math.max(2, dim.width / 50) : 0;
	ygap = (noOfNdbNodes > 1) ? Math.max(2, dim.height / 20) : 0;
	xindent = 10;
	yindent = 10;
	int heightOfScroll = 20;
	Insets insets = getInsets();
	width = (dim.width - (insets.left + insets.right) - 2*xindent + xgap)/maxRow - xgap;
	height = (dim.height - (insets.top + insets.bottom) - 2*yindent + ygap - heightOfScroll)/noRows - ygap;
    }


    public void update(Observable o, Object arg){
	repaint();
    }

    private int width, height, maxRow, xgap, ygap, xindent, yindent;
    private int scale;
    private int lines;
    private NdbCluster cluster;
}

class CountersFrame extends JFrame
    implements ComponentListener, AdjustmentListener 
{

    public CountersFrame(NdbCluster cluster)
    {
	setTitle("CountersViewer");

	final int noOfNdbNodes = cluster.getNoOfNdbNodes();
	
	processorWidth = Math.min(noOfNdbNodes, 10);
	setSize(Math.max(50, processorWidth*20), 
		Math.max(100, 200*noOfNdbNodes/processorWidth));
	JPanel p = new JPanel();
	addComponentListener(this);
	p.addComponentListener(this);
	getContentPane().add(p, "Center");
	myPanel = new CountersPanel(getContentPane().getSize(), 
				    cluster, 
				    processorWidth);
	getContentPane().add(myPanel, "Center");
	JPanel labelAndScroll = new JPanel();
	labelAndScroll.setLayout(new GridLayout (1, 2));
	myWidthLabel = new JLabel("Width: " + processorWidth);
	labelAndScroll.add(myWidthLabel);
	myWidthScroll = new JScrollBar(Adjustable.HORIZONTAL, 
				       Math.min(noOfNdbNodes, 10), 0, 1, 
				       noOfNdbNodes);
	myWidthScroll.addAdjustmentListener(this);
	labelAndScroll.add(myWidthScroll);
	if (noOfNdbNodes > 1)
	    getContentPane().add(labelAndScroll, "South");
    }
    
    public void componentHidden(ComponentEvent e) {
    }
    
    public void componentMoved(ComponentEvent e) {
    }
    
    public void componentResized(ComponentEvent e) {
	myPanel.reSize(getContentPane().getSize());
	repaint();
    }
    
    public void componentShown(ComponentEvent e) {
    }

    public void adjustmentValueChanged(AdjustmentEvent evt)
    {
	myPanel.setWidth(myWidthScroll.getValue());
	myPanel.reSize(getContentPane().getSize());
	myWidthLabel.setText("Width: " + myWidthScroll.getValue());
	repaint();
    }

    private JScrollBar myWidthScroll;
    private JLabel myWidthLabel;
    private CountersPanel myPanel;
    private int processorWidth = 10;
}

class CountersConnection {
    
    public CountersConnection(String host, int port){
	this.host = host;
	this.port = port;
    }
    
    public boolean connect(){
	if(br == null){
	    try {
		InetAddress target = InetAddress.getByName(host);
		sock = new Socket(target, port);
		
		br = new BufferedReader(new InputStreamReader
		    (sock.getInputStream()));
	    } catch (Exception e){
		System.out.println("connect: " + e);
	    }
	}
	
	if (br == null) 
	    return false;
	return true;
    }
    
    public void disconnect(){
	try {
	    sock.close();
	} catch (Exception e){
	    System.out.println("disconnect: " + e);
	}
	sock = null;
	br = null;
    }
    
    public boolean readCounters(NdbCluster cluster) {
	if(!connect()){
	    cluster.setNoConnection();
	    return false;
	}
	String str = null;

	try {
	    str = br.readLine();
	} catch (Exception e){
	    System.out.println("readLine: " + e);
	}
	if(str == null){
	    disconnect();
	    return false;
	}
	StringTokenizer st = new StringTokenizer(str, " ");

	int nodeId = 0;
	Counters c = new Counters();

	while(st.hasMoreTokens()){
	    String tmp = st.nextToken();
	    int ind = 0;
	    if(tmp.startsWith("nodeid=")){
		nodeId = Integer.valueOf(tmp.substring(7)).intValue();
	    } else if(tmp.startsWith("trans=")){
		c.transactions = Integer.valueOf(tmp.substring(6)).intValue();
		c.type = Counters.TRANSACTIONS;
	    } else if(tmp.startsWith("abort=")){
		c.aborts = Integer.valueOf(tmp.substring(6)).intValue();
		c.type = Counters.TRANSACTIONS;
	    } else if(tmp.startsWith("epochsecs=")){
		c.epochsecs = Integer.valueOf(tmp.substring(11)).intValue();
	    } else if(tmp.startsWith("operations=")){
		c.operations = Integer.valueOf(tmp.substring(11)).intValue();
		c.type = Counters.OPERATIONS;
	    }
	}

	if(nodeId != 0)
	    cluster.setCounters(nodeId, c);
	
	return true;
    }
    
    private Socket sock;
    private BufferedReader br;
    private String host;
    private int port;
}

class MgmConnection {

    public MgmConnection(String host, int port){
	this.host = host;
	this.port = port;
    }
    
    public NdbCluster getClusterInfo(){
	NdbCluster cluster = null;
	if(!connect())
	    return cluster;
	
	out.println("get info cluster");
	String str = null;
	try {
	    str = br.readLine();
	    if(str.startsWith("GET INFO 0")){
		StringTokenizer st = new StringTokenizer
		    (str.substring(11));
		int nodes[] = new int[255];
		
		int type = Node.UNDEFINED;
		int num_nodes = 0;
		int maxNodeId = 0;
		while(st.hasMoreTokens()){
		    String tmp = st.nextToken();
		    final int t = Node.getNodeType(tmp);
		    if(t != Node.UNDEFINED)
			type = t;
		    
		    int nodeId = 0;
		    try {
			nodeId = Integer.parseInt(tmp);
		    } catch (Exception e) {}
		    if(nodeId != 0){
			num_nodes ++;
			nodes[nodeId] = type;
			if(nodeId > maxNodeId)
			    maxNodeId = nodeId;
		    }
		}
		cluster = new NdbCluster(nodes, num_nodes,
					 maxNodeId);
	    }
	    
	} catch(Exception e){
	    System.out.println("readLine: "+e);
	}
	return cluster;
    }

    public boolean connect(){
	if(br == null || out == null){
	    try {
		InetAddress target = InetAddress.getByName(host);
		sock = new Socket(target, port);
		
		br = new BufferedReader(new InputStreamReader
		    (sock.getInputStream()));
		out = new PrintWriter(sock.getOutputStream(), true);
	    } catch (Exception e){
		System.out.println("connect: " + e);
	    }
	}
	
	if (br == null || out == null) 
	    return false;
	return true;
    }
    
    public void disconnect(){
	try {
	    sock.close();
	} catch (Exception e){
	    System.out.println("disconnect: " + e);
	}
	sock = null;
	br = null;
	out = null;
    }

    public boolean readStatus(NdbCluster cluster){

	if(!connect()){
	    cluster.setNoConnection();
	    return false;
	}
	
	String str = null;
	try {
	    out.println("get status");
	    str = br.readLine();
	} catch (Exception e){
	    System.out.println("readLine: " + e);
	}
	if(str == null){
	    disconnect();
	    return false;
	}
	
	if(!str.startsWith("GET STATUS")){
	    disconnect();
	    return false;
	}
	
	int nodes = 0;
	try {
	    nodes = Integer.parseInt(str.substring(11));
	} catch(Exception e){
	    System.out.println("parseInt "+e);
	}
	if(nodes == 0){
	    disconnect();
	    return false;
	}
	
	try {
	    for(; nodes > 0 ; nodes --){
		str = br.readLine();
		StringTokenizer st = new StringTokenizer(str);
		
		String s_nodeId = st.nextToken();
		final int nodeId = Integer.parseInt(s_nodeId);
		
		String s_type = st.nextToken();
		String s_state = st.nextToken();
		String s_phase = st.nextToken();
		int type = Node.getNodeType(s_type);
		int state = NdbNode.getNodeState(s_state);
		
		cluster.setState(nodeId, type, state);
	    }
	} catch (Exception e){
	    disconnect();
	    return false;
	}
	
	return true;
    }

    public int getStatisticsPort(){
	if(!connect())
	    return -1;

	String str = null;
	try {
	    out.println("stat port");
	    str = br.readLine();
	} catch (Exception e){
	    System.out.println("readLine: " + e);
	}
	if(str == null){
	    disconnect();
	    return -1;
	}

	if(!str.startsWith("STAT PORT 0")){
	    disconnect();
	    return -1;
	}
	
	try {
	    return Integer.parseInt(str.substring(12));
	} catch (Exception e){
	    System.out.println("parseInt "+e);
	}
	return -1;
    }
    
    private Socket sock;
    private BufferedReader br;
    private PrintWriter out;
    private String host;
    private int port;
}

class CounterViewer {
    
    public static void usage(){
	System.out.println("java CounterViewer <mgm host> <mgm port>");
    }

    public static void main(String args[]){
	try {
	    String host = args[0];
	    int    port = Integer.parseInt(args[1]);
	    new CounterViewer(host, port).run();
	    return;
	} catch (Exception e){
	}
	usage();
    }

    MgmConnection mgmConnection;
    CountersConnection countersConnection;
    
    NdbCluster cluster;
    boolean ok;
    
    CounterViewer(String host, int port){
	ok = false;
	
	mgmConnection = new MgmConnection(host, port);
	int statPort = mgmConnection.getStatisticsPort();
	if(statPort < 0)
	    return;
	
	countersConnection = new CountersConnection(host, statPort);
	cluster = mgmConnection.getClusterInfo();
	
	CountersFrame f = new CountersFrame(cluster);
	f.setSize (300, 200);
	f.show();

	ok = true;
    }

    void run(){
	while(ok){
	    mgmConnection.readStatus(cluster);
	    countersConnection.readCounters(cluster);
	}
    }
}


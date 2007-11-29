static int brt_root_put_cmd_XY (BRT brt, BRT_CMD *md, TOKUTXN txn) {
    int r;
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt);
    if ((r=cachetable_get_and_pin(brt->cf, *rootp, &node_v, NULL, 
				  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, (void*)(long)brt->h->nodesize))) {
	goto died0;
    }
    node=node_v;
    if (0) {
    died1:
	cachetable_unpin(brt->cf, node->thisnodename, node->dirty, brtnodesize(node));
	goto died0;
    }
    node->parent_brtnode = 0;
    result = brtnode_put_cmd_XY(brt, node, cmd, txn);
    // It's still pinned, and it may be too big or the fanout may be too large.
    if (node->height>0 && node->u.n.n_children==TREE_FANOUT) {
	// Must split it.
	r = do_split_node(node, &nodea, &nodeb, &splitk); // On error: node is unmodified
	if (r!=0) goto died1;
	// node is garbage, and nodea and nodeb are pinned
	r = brt_init_new_root(brt, nodea, nodeb, splitk, rootp); // On error: root is unmodified and nodea and nodeb are both unpinned
	if (r!=0) goto died0;
	// nodea and nodeb are unpinned, and the root has been fixed
	// up to point at a new node (*rootp) containing two children
	// (nodea and nodeb).  nodea and nodeb are unpinned.  *rootp is still pinned
	node = *rootp;
    }
    // Now the fanout is small enough.
    // But the node could still be too large.
    if (toku_serialize_brtnode_size(node)>node->nodesize) {
	
    }
	
}

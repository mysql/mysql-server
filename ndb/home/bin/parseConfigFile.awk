BEGIN{
    where=0;
    n_hosts=0;
    n_api=0;
    n_ndb=0;
    n_mgm=0;
    n_ports=0;
}
/COMPUTERS/ {
    where=1;
}
/\[[ \t]*COMPUTER[ \t]*\]/ {
    where=1;
}
/PROCESSES/ {
    where=2;
}
/Type: MGMT/ {
    if(where!=1){
	where=2;
	n_mgm++;
    }
}
/\[[ \t]*MGM[ \t]*\]/ {
    where=2;
    n_mgm++;
}
/Type: DB/ {
    if(where!=1){
	where=3;
	n_ndb++;
    }
}
/\[[ \t]*DB[ \t]*\]/ {
    where=3;
    n_ndb++;
}
/Type: API/ {
    if(where!=1){
	where=4;
	n_api++;
    }
}
/\[[ \t]*API[ \t]*\]/ {
    where=4;
    n_api++;
}
/HostName:/ {
    host_names[host_ids[n_hosts]]=$2;
}

/FileSystemPath:/ {
  if (where==3){
    ndb_fs[ndb_ids[n_ndb]]=$2;
  }
}

/Id:/{
    if(where==1){
	n_hosts++;
	host_ids[n_hosts]=$2;
    }
    if(where==2){
	mgm_ids[n_mgm]=$2;
    }
    if(where==3){
	ndb_ids[n_ndb]=$2;
    }
    if(where==4){
	api_ids[n_api]=$2;
    }
}
/ExecuteOnComputer:/{
    if(where==2){
	mgm_hosts[mgm_ids[n_mgm]]=host_names[$2];
    }
    if(where==3){
	ndb_hosts[ndb_ids[n_ndb]]=host_names[$2];
    }
    if(where==4){
	api_hosts[api_ids[n_api]]=host_names[$2];
    }
}
END {
    for(i=1; i<=n_mgm; i++){
	printf("mgm_%d=%s\n", mgm_ids[i], mgm_hosts[mgm_ids[i]]);
    }
    for(i=1; i<=n_ndb; i++){
	printf("ndb_%d=%s\n", ndb_ids[i], ndb_hosts[ndb_ids[i]]);
	printf("ndbfs_%d=%s\n", ndb_ids[i], ndb_fs[ndb_ids[i]]);
    }
    for(i=1; i<=n_api; i++){
	printf("api_%d=%s\n", api_ids[i], api_hosts[api_ids[i]]);
    }
    printf("mgm_nodes=%d\n", n_mgm);
    printf("ndb_nodes=%d\n", n_ndb);
    printf("api_nodes=%d\n", n_api);
}

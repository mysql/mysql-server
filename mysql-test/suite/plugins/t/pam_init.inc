
--source include/not_embedded.inc

if (!$AUTH_PAM_SO) {
  skip No pam auth plugin;
}

eval install plugin pam soname '$AUTH_PAM_SO';
create user test_pam identified via pam using 'mariadb_mtr';
create user pam_test;
grant proxy on pam_test to test_pam;

let $plugindir=`SELECT @@global.plugin_dir`;


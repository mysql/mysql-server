--source include/linux_distro.inc

if ($linux_distro == "ubuntu") {
  --skip Test not supported on Ubuntu
}

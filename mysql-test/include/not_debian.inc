--source include/linux_distro.inc

if ($linux_distro == "debian") {
  --skip Test not supported on Debian
}

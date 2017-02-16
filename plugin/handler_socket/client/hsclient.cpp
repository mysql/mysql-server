
// vim:sw=2:ai

#include "hstcpcli.hpp"
#include "string_util.hpp"

namespace dena {

int
hstcpcli_main(int argc, char **argv)
{
  config conf;
  parse_args(argc, argv, conf);
  socket_args sockargs;
  sockargs.set(conf);
  hstcpcli_ptr cli = hstcpcli_i::create(sockargs);
  const std::string dbname = conf.get_str("dbname", "hstest");
  const std::string table = conf.get_str("table", "hstest_table1");
  const std::string index = conf.get_str("index", "PRIMARY");
  const std::string fields = conf.get_str("fields", "k,v");
  const int limit = conf.get_int("limit", 0);
  const int skip = conf.get_int("skip", 0);
  std::vector<std::string> keys;
  std::vector<string_ref> keyrefs;
  size_t num_keys = 0;
  while (true) {
    const std::string conf_key = std::string("k") + to_stdstring(num_keys);
    const std::string k = conf.get_str(conf_key, "");
    const std::string kx = conf.get_str(conf_key, "x");
    if (k.empty() && kx == "x") {
      break;
    }
    ++num_keys;
    keys.push_back(k);
  }
  for (size_t i = 0; i < keys.size(); ++i) {
    const string_ref ref(keys[i].data(), keys[i].size());
    keyrefs.push_back(ref);
  }
  const std::string op = conf.get_str("op", "=");
  const string_ref op_ref(op.data(), op.size());
  cli->request_buf_open_index(0, dbname.c_str(), table.c_str(),
    index.c_str(), fields.c_str());
  cli->request_buf_exec_generic(0, op_ref, num_keys == 0 ? 0 : &keyrefs[0],
    num_keys, limit, skip, string_ref(), 0, 0);
  int code = 0;
  size_t numflds = 0;
  do {
    if (cli->request_send() != 0) {
      fprintf(stderr, "request_send: %s\n", cli->get_error().c_str());
      break;
    }
    if ((code = cli->response_recv(numflds)) != 0) {
      fprintf(stderr, "response_recv: %s\n", cli->get_error().c_str());
      break;
    }
  } while (false);
  cli->response_buf_remove();
  do {
    if ((code = cli->response_recv(numflds)) != 0) {
      fprintf(stderr, "response_recv: %s\n", cli->get_error().c_str());
      break;
    }
    while (true) {
      const string_ref *const row = cli->get_next_row();
      if (row == 0) {
	break;
      }
      printf("REC:");
      for (size_t i = 0; i < numflds; ++i) {
	const std::string val(row[i].begin(), row[i].size());
	printf(" %s", val.c_str());
      }
      printf("\n");
    }
  } while (false);
  cli->response_buf_remove();
  return 0;
}

};

int
main(int argc, char **argv)
{
  return dena::hstcpcli_main(argc, argv);
}


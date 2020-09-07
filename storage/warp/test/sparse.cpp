#include "../sparse.hpp"

void set_and_test_bits(sparsebitmap* t) {
  int errcode=0;
  for(int i=256;i>0;--i) {
    //std::cout << "setting bit: " << i << "\n";
    if((errcode = t->set_bit(i)) != 0) {
      std::cout << " NO OK | set_bit returned: "  << errcode << "\n";
      exit(abs(errcode));
    }
    
    for(int n=1;n<=256;++n) {
      int is_set = t->is_set(n);
      if(n >= i) {
        if(is_set != 1) {
          std::cout << "NOT OK | n: " << n << " i: " << i << ", is_set: " << is_set<< " expected 1\n";
        } 
      } else {
        if(is_set == 1) {
          std::cout << "NOT OK | n: " << n << " i: " << i << ", is_set: " << is_set<< " expected 0\n";
        } 
      }
    }
  } 
  
  for(int i=1;i<=256;++i) {
    if(!t->is_set(i)) {
        std::cout << "NOT OK | i: " << i << ", is_set: 0, expected 1\n";
        exit(errcode);
        exit(i);
    }
  }
}

int main(int argc, char** argv) {
  unlink("test.bitmap");
  unlink("test.bitmap.txlog");
  bitmap_debug = false;
  sparsebitmap *t = new sparsebitmap("test.bitmap");
  set_and_test_bits(t);
  t->commit();
  t->close(1);
  delete t;
  
  // should recover the bitmap
   t = new sparsebitmap("test.bitmap");

  if(!t->is_set(1)) {
    std::cout << "bit 1 must be set!";
    exit(1);
  }

  if(!t->is_set(256)) {
    std::cout << "bit 1 must be set!";
    exit(1);
  }
  std::cout << "OK\n";
  exit(0);
  return 0;
}

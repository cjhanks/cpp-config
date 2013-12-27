
#include "config-bits.hh"

#include <iostream>
#include <string>
#include <memory>

using namespace std;

void
lookup_test(parse_trie<string>& d, string key) {
    auto it   = key.begin();
    auto init = it;

    try {
        d.lookup(it);
    } catch (...) {
        cerr << "FAIL" << endl;
        cerr << (it == init ? "true" : "false") << endl;
    }

    cerr << "true" << endl;
}

int 
main() {
    unique_ptr<parse_trie<string>> k(new parse_trie<string>());

    string d_0("words");
    string d_1("wordsees");

    string l_0("words");
    string l_1("wordsees");
    string l_2("wordsee");
    
    k->defval(d_0) = "TEST0";
    k->defval(d_1) = "TEST1";

    lookup_test(*k, l_0);
    lookup_test(*k, l_1);
    lookup_test(*k, l_2);

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////

#include "config.hh"

#include <cassert>
#include <cmath>

#include <iostream>
#include <limits>
#include <string>

using std::cerr;
using std::endl;
using std::string;


int
main(void) {
    config::initialize("./test/example.cfg");

    CFG->dump();
    assert( CFG->get<bool>("EX_BOOL_0"));
    assert(!CFG->get<bool>("EX_BOOL_1"));
    assert( CFG->get<bool>("EX_BOOL_2"));
    assert(!CFG->get<bool>("EX_BOOL_3"));

    assert(CFG->get<string>("EX_STRING_0") == "words are defined");
    assert(CFG->get<string>("EX_STRING_0") == CFG->get<string>("EX_STRING_1"));
    assert(CFG->get<string>("EX_STRING_2") == CFG->get<string>("EX_STRING_3"));
    assert(CFG->get<string>("EX_STRING_4") == "amacro-args_1NotBashLike");
    assert(CFG->get<string>("ex_string_5") == "$MACRO_STR_0 - ${MACRO_STR_1}");
    
    assert(CFG->get<long>("EX_LONG_0") == -300);
    ///{@
    // internally they are stored as int64 however they can be cast to unsigned without
    // bit ordering errors
    assert(CFG->get<long>("EX_LONG_1") ==  300);
    assert(CFG->get<unsigned>("EX_LONG_1") ==  300);
    ///}@
    assert(CFG->get<long>("EX_LONG_2") ==  3000);
    assert(CFG->get<long>("EX_LONG_3") ==  30000);

    assert(CFG->get<float>("EX_FLOAT_0") == -300.0); 
    assert(CFG->get<float>("EX_FLOAT_1") == 300.0); 
    
    // internally it represented as a double which in a static cast -> float causes
    // imprecision.  therefore ->get<float>(...) would not work.
    assert(CFG->get<double>("EX_FLOAT_2") == 3000.13);


    assert(CFG->get<float>("EX_FLOAT_3") == 30000.0);
    assert(CFG->get<float>("EX_FLOAT_4") == 0.0);
    assert(CFG->get<double>("EX_FLOAT_5") == 3.14159);
    assert(CFG->get<string>("string") == "Hello world");

    assert(CFG->has_section("object_1"));
    assert(CFG->section("object_1")->get<string>("string") == "Hello world");
}

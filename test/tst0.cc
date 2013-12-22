

#include "config.hh"

#include <cassert>
#include <iostream>
#include <string>


using namespace std;

int 
main() {
    auto c = config::initialize("test/tst0.cfg"); 

    cerr << c->get<int>("integer_data") << endl;
    cerr << c->get<string>("string") << endl;
    cerr << c->get<float>("float_data") << endl;
    
    assert (c->has_section("section_0"));
    cerr << c->section("section_0")->get<int>("integer_data") << endl;
    cerr << c->section("section_0")->get<string>("string") << endl;
    cerr << c->section("section_0")->get<float>("float_data") << endl;

    assert (c->has_section("section_1"));
    cerr << c->section("section_1")->section("section_2")->get<int>("integer_data") << endl;
    cerr << c->section("section_1")->section("section_2")->get<string>("string") << endl;
    cerr << c->section("section_1")->section("section_2")->get<float>("float_data") << endl;

    
    //cerr << c->get<string>("M_string") << endl;
    cerr << c->get<string>("new_str") << endl;
    cerr << c->get<long>("exp_str") << endl;
    
    
    cerr << (c->assert_type("section_0.section_1.float_data", kwarg::FLOATING) 
                ? "true" : "false") << endl;
    //cerr << c->get<long>("new_int") << endl;
    //c->get<int>("key");
    //c->get<unsigned>("key");
    //c->get<float>("key");
    //c->get<double>("key");
}



#include "config.hh"

#include <cassert>
#include <iostream>
#include <string>


using namespace std;


void 
test_cfg() {
    auto c = config::initialize("test/tst0.cfg"); 
    
    for (size_t i = 0; i < 1000000; ++i) {
        c->get<int>("integer_data") ;
        //c->get<string>("string") ;
        c->get<float>("float_data") ;
        
        c->has_section("section_0");
        c->section("section_0")->get<int>("integer_data") ;
        //c->section("section_0")->get<string>("string") ;
        c->section("section_0")->get<float>("float_data") ;

        c->has_section("section_1");
        c->section("section_1")->section("section_2")->get<int>("integer_data") ;
        //c->section("section_1")->section("section_2")->get<string>("string") ;
        c->section("section_1")->section("section_2")->get<float>("float_data") ;
    }
}

void 
test_std() {
    struct test {
        int integer_data;
        string string_data;
        float float_data;

        test* next;
    };

    struct test* t = new test;
    t->integer_data = 1000;
    t->float_data   = 3.30;
    t->string_data  = "words";
    
    t->next = new test;
    t->next->next = new test;
    t->next->next->integer_data = 1000;
    t->next->next->float_data   = 3.30;
    t->next->next->string_data  = "words";
    
    for (size_t i = 0; i < 1000000; ++i) {
        int    a = t->integer_data;
        string b = t->string_data;
        float  c = t->float_data;

        int    a_1 = t->next->next->integer_data;
        string b_1 = t->next->next->string_data;
        float  c_1 = t->next->next->float_data;

        (void) a;
        (void) b;
        (void) c;
        (void) a_1;
        (void) b_1;
        (void) c_1;
    }
}

int 
main() {
    //test_std();
    test_cfg();
}

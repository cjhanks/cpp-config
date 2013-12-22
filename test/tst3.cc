

#include "config.hh"

#include <cassert>
#include <iostream>
#include <string>


using namespace std;

int 
main() {
    try { 
        config::initialize("test/tst3.cfg"); 
        return 1;
    } catch (config_parse_exception e) {
        DEBUG(e.what());
    }
    
    try { 
        config::initialize("test/tst4.cfg"); 
        return 1;
    } catch (config_parse_exception e) {
        DEBUG(e.what());
    }


    return 0;
}

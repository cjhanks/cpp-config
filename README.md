cpp-config
==========

C++ config library

libconf files are parsed in two stages; pre-processor definitions which are prefixed by the '@' character, 
and all other configuration specifications.  

# The basics

```
/* FILE: example.cfg */

@define ONE = 1
@export ONE

/* non-macros */
keyword_integer   = $ONE
keyword_string_1  = "$ONE is the lonliest number..."
keyword_string_2  = '$ONE is the lonliest number...'
keyword_float     = $ONE00.0

some_section = {
    some_vector = [ 0, 1, 2
                  , 3, 4, 5
                  , 6, 7, 8 ]
}
```

```cpp
#include <cassert>
#include <iostream>
#include <string>
#include "config.hh"

using std::cerr;
using std::endl;
using std::string;

int
main() { 
    config::initialize("example.cfg");
    
    cerr << CFG->get<long  >("keyword_integer" ) << endl;
    // 1
    
    cerr << CFG->get<string>("keyword_string_1") << endl;
    // 1 is the lonliest number
    
    cerr << CFG->get<string>("keyword_string_2") << endl;
    // $ONE is the lonliest number
    
    cerr << CFG->get<string>("keyword_float"   ) << endl;
    // 100.0
    
    if (! CFG->has_section("some_section"))
        return 1;
    
    if (! CFG->section("some_section")->has_kwarg("some_vector"))
        return 2;
        
    /**
     * please note that the it != ... comparison ends up being O(1 + log n) due to internal
     * kwarg lookup and is not advised for tight loops.
     * 
     * see below...
     */
    size_t i(0);
    for (auto it  = CFG->section("some_section")->vector("some_vector").cbegin();
              it != CFG->section("some_section")->vector("some_vector").cend();
            ++it) 
        assert(i++ == it->as<long>());
        // true
        
    const kwarg_vector& vec = CFG->section("some_section")->vector("some_vector");
    
    /**
     * the performance of this loop is identical to any std::vector<long> (minus any implicit
     * casting operations and indirection).  GCC -O3 shows this to perform equivalent to a 
     * plain vector.
     *
     * this is because the config parser performs type inference and therefore no operations 
     * are performed in the function. 
     *
     * template <typename _Tp>
     * kwarg_section::as() const { ... } 
     */
    i = 0;
    for (auto it = vec.cbegin(); it != vec.cend(); ++it) 
        assert(i++ == it->as<long>());
        
        
    return 0;
}


```


## Pre-Processor Operations

### @include 

### @define 

### @export

### @execute


## Configuration Types 

### string 
`stored internally as std::string`

A string is defined as any value which starts with either the ' character ord(39) or the " character ord(34).  When it starts with the former, no macro replacements will performed and the string will be read until the next non-escaped ' character.  

Strings behave exactly how you would expect bash strings to behave with the exception to macro expansion.  Please read Gotcha##Macro Expansion.

Both strings and string literals will retain newline characters such that 

```
some_var = '
Words are here 
And here too.
'
```
Will retain its formatting.


### integral 
`stored internally as kwarg_const -> int64_t`

An integral is differentiated from a floating point value in that it has a '.' character.  It is very important to place a . in the character if you wish to cast it to a double.  This is because the floating point and integral are unioned in the same memory space.  Neglecting to place a '.' will lead to an answer which is *very* wrong in nearly all cases.

`a_float = 0.`
`a_int   = 0 `

### floating point 
`stored internally as kwarg_const -> double`

SEE integral.
Note that (currently) the floating point value does not support engineers notation (though it should).

### boolean 
`stored internally as kwarg_const -> bool`


### object/section
`stored internally as config_section -> kwarg`

### vector 
`stored internally as config_vector -> vector<kwarg_const*>`


## Gotcha

### Macro Expansion

#if 0
Copyright 2013 CjHanks <develop@cjhanks.name>

This file is part of libconf.

libconf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

libconf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libconf.  If not, see <http://www.gnu.org/licenses/>.
#endif

#include "config.hh"

#include <libgen.h>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <locale>
#include <memory>
#include <vector>


using std::function;
using std::getline;
using std::ifstream;
using std::istreambuf_iterator;
using std::locale;
using std::mismatch;
using std::string;
using std::stringstream;
using std::unique_ptr;


//////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////
namespace {

locale _S_locale;

void
config_cleanup_atexit() {
    if (config::instance())
        delete config::instance();
}

bool
eos(_Iter& iter, bool do_throw) {
    if (*iter == '\0') {
        if (do_throw)
            throw config_parse_exception("Unexpected EOF");
        else
            return true;
    } else {
        return false;
    }
}

void
append_regs(string& data, _Iter& iter, parse_trie<string>* regs) {
    assert(*iter == '$');
    const bool is_bracketed = (*(iter + 1) == '{');
    data.append(regs->lookup(++iter));

    if (is_bracketed)
        ++iter;
}

struct path_info {
    string  abspath;
    string  dirpath;
    string  file_name;
};

path_info
get_path_info(const string& path) {
    path_info info;
    string new_path;
    new_path.reserve(path.size());

    for (auto it = path.begin(); it != path.end(); ++it) {
        switch (*it) {
            case '/':
                if (*(it + 1) == '/')
                    continue;
                break;

            default:
                break;
        }

        new_path.append(1, *it);
    }

    assert(string::npos == new_path.find("//"));

    char buf[PATH_MAX + 1];
    char* ptr;
    ptr = realpath(path.c_str(), buf);

    if (0x0 == ptr)
        throw config_io_error(path);
    else
        info.abspath = string(ptr);

    memcpy(buf, info.abspath.c_str(), info.abspath.size());
    ptr = dirname(buf);

    if (0x0 == ptr)
        throw config_io_error(path);
    else
        info.dirpath = string(ptr);

    memcpy(buf, info.abspath.c_str(), info.abspath.size());
    ptr = basename(buf);

    if (0x0 == ptr)
        throw config_io_error(path);
    else
        info.file_name = string(ptr);

    return info;
}

bool
bypass_whitespace(_Iter& iter, bool do_throw = true) {
    /* macro is used to encode a 2 byte sequence and an "in_comment" status for a unique
     * value which can be used in the jump table.
     */
    #define str_sequence(_a_, _b_, _c_) ((_a_ << 16) | (_b_ << 8) | (_c_))

    bool in_comment = false;

    while (! eos(iter, do_throw)) {
        switch (str_sequence(*iter, *(iter + 1), in_comment)) {
            case str_sequence('/', '*', false):
                in_comment = true;
                break;

            case str_sequence('*', '/', true):
                ++iter; /*< due to look ahead >*/
                in_comment = false;
                break;

            case str_sequence('/', '/', false):
                while (! eos(iter, do_throw) && *iter != '\n')
                    ++iter;
                break;

            default:
                if (! in_comment && ! isspace(*iter, _S_locale))
                    goto exit_loop;

                break;
        }

        ++iter;
    }

exit_loop:
    return ! eos(iter, false);
#undef str_sequence
}

string
parse_word(_Iter& iter) {
    string word;

    while (! eos(iter, true) && acceptable_char(*iter))
        word.append(1, *(iter++));

    if (0 == word.size())
        throw config_parse_exception("empty word", iter);

    return word;
}

kwarg*
parse_number(const string& name, _Iter& iter, parse_trie<string>* regs) {
    assert('=' != *iter);
    string data;
    bypass_whitespace(iter);

    while (! eos(iter, true)) {
        switch (*iter) {
            case '$':
                append_regs(data, iter, regs);
                continue;
                break;

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':
                data.append(1, *iter);
                break;

            default:
                goto exit_loop;
        }

        ++iter;
        continue;

exit_loop:
        break;
    }

    if (0 == data.size())
        throw config_parse_exception("empty number", iter);

    if (data.find('.') == string::npos) {
        /* INTEGRAL */
        return new kwarg_const(static_cast<int64_t>(std::stoll(data)), name);
    } else {
        /* FLOATING */
        return new kwarg_const(std::stod(data), name);
    }
}

string
parse_string(_Iter& iter, parse_trie<string>* regs) {
    assert(*iter != '=');
    enum { SQUOTE = 0, DQUOTE = 1 };
    bool states[] { false, false };

    bypass_whitespace(iter, false);
    string value;

    while (! eos(iter, true)) {
        switch (*iter) {
            case '\'':
                if (*(iter - 1) != '\\' || !states[DQUOTE]) {
                    if (states[SQUOTE])
                        goto exit_loop;

                    states[SQUOTE] =! states[SQUOTE];
                    goto no_append;
                }
                break;

            case '"':
                if (*(iter - 1) != '\\' || !states[SQUOTE]) {
                    if (states[DQUOTE])
                        goto exit_loop;

                    states[DQUOTE] =! states[DQUOTE];
                    goto no_append;
                }
                break;

            case '$':
                if (! states[SQUOTE]) {
                    if (! states[DQUOTE])
                        throw config_parse_exception("Unexpected $");

                    try {
                        append_regs(value, iter, regs);
                        continue;
                    } catch (trie_lookup_error& e) {
                        DEBUG("error " << e.what());
                    }
                }
                break;

            default:
                break;
        }

        value.append(1, *iter);

no_append:
        ++iter;
    }

exit_loop:
    ++iter;

    return value;
}

kwarg*
parse_boolean(const string& name, _Iter& iter) {
    enum _Bool { UNDEFINED = 0, _TRUE, _FALSE };
    static parse_trie<_Bool> sbool { { "FALSE", _FALSE }, { "TRUE", _TRUE } };

    switch (sbool.lookup(iter)) {
        case _TRUE:
            return new kwarg_const(true, name);

        case _FALSE:
            return new kwarg_const(false, name);

        default:
            throw config_parse_exception("Invalid bool", iter);
    }
}
} // ns

//////////////////////////////////////////////////////////////////////////////////////////

config_section::config_section(const string& name)
    : kwarg(name, kwarg::SECTION)
{}

config_section::~config_section() {
    for (auto it = _M_kwargs.begin(); it != _M_kwargs.end(); ++it)
        delete it->second;
}

config_section*
config_section::section(const std::string& name) {
    auto it = _M_kwargs.find(name);

    if (it == _M_kwargs.end())
        throw config_key_error(name);
    else
        return static_cast<config_section*>(it->second);
}

bool
config_section::has_kwarg(const string& key) const {
    return 0 != _M_kwargs.count(key);
}

bool
config_section::has_section(const string& key) const {
    return this->has_kwarg(key);
}

void
config_section::_M_set_kwarg(kwarg* val) {
    assert(val);
    assert(val->name().size() > 0);
    _M_kwargs[val->name()] = val;
}

kwarg*
config_section::_M_get_kwarg(const string& key) const {
    auto it = _M_kwargs.find(key);

    if (it == _M_kwargs.end())
        throw config_key_error(key);
    else
        return _M_kwargs.at(key);
}

kwarg*
config_section::_M_parse_kwarg(string key, _Iter& iter, parse_trie<string>* regs) {
    kwarg* ptr(0x0);

    switch (*iter) {
        /* macro parsing */
        case '@':
            _M_parse_macro(iter, regs);
            break;

        /* string */
        case '\'':
        case '"':
            ptr = new kwarg_const(parse_string(iter, regs), key);
            break;

        /* section vector */
        case '[':
            ptr = _M_parse_vector(key, ++iter, regs);
            break;

        /* section object */
        case '{':
            if (this->has_section(key))
                ptr = this->section(key);
            else
                ptr = new config_section(key);

            static_cast<config_section*>(ptr)->_M_parse_iterator(++iter, regs);
            break;

        case 'T':
        case 't':
        case 'F':
        case 'f':
            ptr = parse_boolean(key, iter);
            break;

        /* number */
        default:
            ptr = parse_number(key, iter, regs);
            break;
    }

    return ptr;
};


void
config_section::_M_parse_iterator(_Iter& iter, parse_trie<string>* regs) {
    while (bypass_whitespace(iter, false)) {
        switch (*iter) {
            case '@':
                _M_parse_macro(iter, regs);
                break;

            case ';':
                ++iter;
                break;

            case ']':
                ++iter;
                break;

            case '}':
                ++iter;
                return;

            default:
                string name = parse_word(iter);

                bypass_whitespace(iter  , true);
                if ('=' != *iter)
                    throw config_parse_exception("expected '='", iter);
                bypass_whitespace(++iter, true);

                _M_set_kwarg(_M_parse_kwarg(name, iter, regs));
                break;
        }
    }
}

void
config_section::_M_parse_file(const string& file_path, parse_trie<string>* regs) {
    path_info info = get_path_info(file_path);

   ifstream file(info.abspath);

    if (! file.good())
        throw config_io_error(file_path);

    string data((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    auto iter = data.begin();

    _M_parse_iterator(iter, regs);
}

void
config_section::_M_parse_define(_Iter& iter, parse_trie<string>* regs) {
    bypass_whitespace(iter, true);
    string name = parse_word(iter);
    bypass_whitespace(iter, true);

    if ('=' != *iter)
        throw config_parse_exception("expected '='", iter);

    assert(*iter == '=');
    regs->defval(name) = parse_string(++iter, regs);
};

void
config_section::_M_parse_export(_Iter& iter, parse_trie<string>* regs) {
    bypass_whitespace(iter, true);
    string name  = parse_word(iter);
    char*  value = getenv(name.c_str());

    if (0x0 != value)
        regs->defval(name) = string(value);
}

void
config_section::_M_parse_include(_Iter& iter, parse_trie<string>* regs) {
    bypass_whitespace(iter, true);
    _M_parse_file(parse_string(iter, regs), regs);
}

void
config_section::_M_parse_macro(_Iter& iter, parse_trie<string>* regs) {
    enum Op { UNDEFINED = 0, DEFINE, EXPORT, INCLUDE };

    static parse_trie<Op> LUT { { "DEFINE" , DEFINE  }
                              , { "EXPORT" , EXPORT  }
                              , { "INCLUDE", INCLUDE } };
    if ('@' != *iter)
        throw config_parse_exception("expected ['@']", iter);

    Op op = UNDEFINED;

    try {
        op = LUT.lookup(++iter);
    } catch (const trie_lookup_error& e) {
        throw config_parse_exception("Invalid macro", iter, 2, 10);
    }

    switch (op) {
        case UNDEFINED:
            break;

        case DEFINE:
            _M_parse_define(iter, regs);
            break;

        case EXPORT:
            _M_parse_export(iter, regs);
            break;

        case INCLUDE:
            _M_parse_include(iter, regs);
            break;
    }
}

kwarg*
config_section::_M_parse_vector(string key, _Iter& iter, parse_trie<string>* regs) {
    bypass_whitespace(iter, true);
    std::vector<kwarg_const*> items;

    while (! eos(iter, true)) {
        switch (*iter) {
            case ',':
                break;

            case ']':
                goto exit_loop;

            default:
                items.emplace_back(
                        static_cast<kwarg_const*>(_M_parse_kwarg(key, iter, regs))
                        );
                break;
        }

        bypass_whitespace(++iter, true);
        continue;
exit_loop:
        break;
    }
    
    return new kwarg_vector(key, items);
}

void
config_section::dump(int depth) {
    using std::cerr;
    using std::endl;
    using std::setw;
    using std::setfill;

    cerr << setfill('=') << setw(depth) << this->name() << endl;

    for (auto it = _M_kwargs.begin(); it != _M_kwargs.end(); ++it) {
        cerr << setfill('-') << setw(4 * depth + 4);

        switch (it->second->type()) {
            case kwarg::FLOATING:
                cerr << setw(18)
                     << it->second->name()
                     << "\t"
                     << this->get<double>(it->second->name())
                     << endl;
                break;

            case kwarg::INTEGRAL:
                cerr << setw(18)
                     << it->second->name()
                     << "\t"
                     << this->get<long>(it->second->name())
                     << endl;
                break;

            case kwarg::STRING:
                cerr << setw(18)
                     << it->second->name()
                     << "\t"
                     << this->get<string>(it->second->name())
                     << endl;
                break;

            case kwarg::BOOL:
                cerr << setw(18)
                     << it->second->name()
                     << "\t"
                     << (this->get<bool>(it->second->name()) ? "true" : "false")
                     << endl;
                break;

            case kwarg::SECTION:
                this->section(it->first)->dump(depth + 1);
                cerr << setfill('=') << setw(depth) << this->name() << endl;
                break;

            default:
                break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
#if defined(CONFIG_SINGLETON)
config* config::_S_instance(0x0);

config*
config::initialize(const string& file_path) {
    assert(0x0 == config::_S_instance);
    return new config(file_path);
}

config*
config::instance() {
    assert(0x0 != config::_S_instance);
    return _S_instance;
}
#endif

config::config(const string& file_path)
    : config_section("ROOT")
{
#if defined(CONFIG_SINGLETON)
    assert(0x0 == _S_instance);
    _S_instance = this;
    ::atexit(config_cleanup_atexit);
#endif
    path_info info = get_path_info(file_path);
    _M_macro_regs.defval("DOT") = info.dirpath;
    _M_parse_file(info.abspath, &_M_macro_regs);
}

bool
config::assert_type(const string& key, kwarg::TYPE type) const {
    const size_t index = key.find_first_of('.');

    /* end hierarchy */
    if (string::npos == index)
        return this->has_kwarg(key) && _M_get_kwarg(key)->type() == type;

    if (! this->has_section(key.substr(0, index)))
        return false;

    return this->assert_type(key.substr(index + 1), type);
}

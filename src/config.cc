#include "config.hh"

#include <libgen.h>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
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

#define LOG(_msg_) \
    do { std::cerr << _msg_ << std::endl; } while(0)

//////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////
namespace {

locale _S_locale;

#if defined(CONFIG_SINGLETON)
void
config_cleanup_atexit() {
    if (config::instance())
        delete config::instance();
}
#endif // CONFIG_SINGLETON

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
                        //DEBUG("error " << e.what());
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
config_section::section(const std::string& name) const {
    return static_cast<config_section*>(_M_get_kwarg(name));
}

bool
config_section::has_kwarg(const string& key) const {
    return 0 != _M_kwargs.count(key);
}

bool
config_section::has_section(const string& key) const {
    return this->has_kwarg(key) && _M_get_kwarg(key)->type() == kwarg::SECTION;
}

bool
config_section::has_vector(const string& key) const {
    return this->has_kwarg(key) && _M_get_kwarg(key)->type() == kwarg::VECTOR;
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
config_section::_M_get_kwarg(const std::string& key, kwarg::TYPE t) const {
    kwarg* ptr = _M_get_kwarg(key);

    if (! ptr->type() == t)
        throw config_type_error(key);
    else
        return ptr;
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
        case '(':
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

        /* booleans */
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
            case ')':
                ++iter;
                break;

            case '}':
                ++iter;
                return;

            default:
                string name = parse_word(iter);

                bypass_whitespace(iter  , true);
                if ('=' != *iter && ':' != *iter)
                    throw config_parse_exception("expected '=' or ':'", iter);
                bypass_whitespace(++iter, true);

                _M_set_kwarg(_M_parse_kwarg(name, iter, regs));
                break;
        }
    }
}

void
config_section::_M_parse_file(const string& file_path, parse_trie<string>* regs
                            , bool optional) {
    path_info info;

    try {
        info = get_path_info(file_path);
    } catch (const config_io_error& e) {
        if (optional)
            return;
        else
            throw e;
    }

    ifstream file(info.abspath);
    if (! file.good()) {
        if (optional)
            return;
        else
            throw config_io_error(file_path);
    }

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
config_section::_M_parse_import(_Iter& iter, parse_trie<string>* regs) {
    bypass_whitespace(iter, true);
    string name  = parse_word(iter);
    char*  value = getenv(name.c_str());

    if (0x0 != value) {
        stringstream ss;
        /* it isn't obvious that you should export double quotes into the env, so we check
         * and do it automatically */
        if ('"' != value[0] && (! std::isdigit(value[0]) || value[0] == '$'))
            ss << '"' << value << '"';
        else
            ss << value;

        string data(ss.str());
        auto begin = data.begin();
        regs->defval(name) = parse_string(begin, regs);
    }
}

void
config_section::_M_parse_include(_Iter& iter, parse_trie<string>* regs
                               , bool optional) {
    bypass_whitespace(iter, true);
    assert(*iter = '=');
    bypass_whitespace(++iter, true);
    string data = parse_string(iter, regs);
    _M_parse_file(data, regs, optional);
}

void
config_section::_M_parse_macro(_Iter& iter, parse_trie<string>* regs) {
    enum Op { UNDEFINED = 0
            , DEFINE
            , IMPORT
            , INCLUDE
            , INCLUDE_OPTIONAL };

    static parse_trie<Op> LUT { { "DEFINE"          , DEFINE  }
                              , { "IMPORT"          , IMPORT  }
                              , { "INCLUDE"         , INCLUDE }
                              , { "INCLUDE_OPTIONAL", INCLUDE_OPTIONAL }
                              , { "INCLUDE*"        , INCLUDE_OPTIONAL } };
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

        case IMPORT:
            _M_parse_import(iter, regs);
            break;

        case INCLUDE:
            _M_parse_include(iter, regs, false);
            break;

        case INCLUDE_OPTIONAL:
            _M_parse_include(iter, regs, true);
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
                ++iter;
                break;

            case ']':
            case ')':
                goto exit_loop;

            default:
                items.emplace_back(
                        static_cast<kwarg_const*>(_M_parse_kwarg(key, iter, regs))
                        );
                break;
        }

        bypass_whitespace(iter, true);
        continue;
exit_loop:
        break;
    }

    return new kwarg_vector(key, items);
}

config_section::const_iterator
config_section::cbegin() const {
    return _M_kwargs.cbegin();
}

config_section::const_iterator
config_section::cend() const {
    return _M_kwargs.cend();
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
    _S_instance = new config(file_path);
    ::atexit(config_cleanup_atexit);
    return _S_instance;
}

config*
config::instance() {
    return _S_instance;
}
#endif // defined(CONFIG_SINGLETON)

config::config(const string& file_path)
    : config_section("ROOT")
{
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

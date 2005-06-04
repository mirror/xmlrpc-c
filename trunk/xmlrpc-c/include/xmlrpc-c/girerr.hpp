#ifndef GIRERR_HPP_INCLUDED
#define GIRERR_HPP_INCLUDED

#include <string>
#include <exception>

namespace girerr {

#define HAVE_GIRERR_ERROR

class error : public std::exception {
public:
    error(std::string const& what_arg) : _what(what_arg) {};
    virtual const char *
    what() const { return this->_what.c_str(); };
private:
    std::string _what;
};

} // namespace

#endif

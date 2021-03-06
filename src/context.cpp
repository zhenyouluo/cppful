// The MIT License (MIT)
//
// Copyright (c) 2015 Jeremy Letang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "cppful/context.h"

namespace cf {

context::context(context&& oth)
: path(std::move(oth.path))
, method(std::move(oth.method))
, custom_data(std::move(oth.custom_data))
, vars(std::move(oth.vars))
, wildcards(std::move(oth.wildcards))
, d_wildcards(std::move(oth.d_wildcards)) {}

context::context(const context& oth)
: path(oth.path)
, method(oth.method)
, custom_data(oth.custom_data)
, vars(oth.vars)
, wildcards(oth.wildcards)
, d_wildcards(oth.d_wildcards) {}

context::context(const std::string& path, cf::method method)
: path(path)
, method(method)
, custom_data({})
, vars({})
, wildcards({})
, d_wildcards({}) {}

context& context::operator=(context&& oth) {
    if (this not_eq &oth) {
        this->path = std::move(oth.path);
        this->method = std::move(oth.method);
        this->custom_data = std::move(oth.custom_data);
        this->vars = std::move(oth.vars);
        this->wildcards = std::move(oth.wildcards);
        this->d_wildcards = std::move(d_wildcards);
    }
    return *this;
}

context& context::operator=(const context& oth) {
    if (this not_eq &oth) {
        this->path = oth.path;
        this->method = oth.method;
        this->custom_data = oth.custom_data;
        this->vars = oth.vars;
        this->wildcards = oth.wildcards;
        this->d_wildcards = d_wildcards;
    }
    return *this;
}

}
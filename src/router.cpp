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

#include "cppful/router.h"

#include <algorithm>
#include "cppful/stop.h"

namespace cf {

// captures regex for vars and wildcards
const std::string router::capture_regex = "([-_a-zA-Z0-9]*)";
// match regex for vars and wildcards
const std::string router::match_regex = "[-_a-z0-9A-Z]*";

// captures regex for double wildcards
const std::string router::two_wildcard_capture_regex = "([/-_a-z0-9A-Z]*)";
// match regex for double wildcards
const std::string router::two_wildcard_match_regex = "[/-_a-z0-9A-Z]*";

// find the vars in url
const std::regex router::find_var_regex = std::regex(":[-_a-zA-Z0-9]*");
// find wildcards in url
const std::regex router::find_wildcard_regex = std::regex("(\\*)");
// find double wildcards in url
const std::regex router::find_two_wildcard_regex = std::regex("(\\*\\*)");
// find the temporary placehorder for double wildcards
const std::regex router::find_placeholder_regex = std::regex("(__TWO_WILDCARD_PLACEHOLDER__)");
// find list of /
const std::regex router::sanitize_regex = std::regex("//+");

cf::response path_not_found(cf::context&) {
    return cf::response { cf::status::not_found, "Path not found on this server." };
}

router::router(router&& oth)
: routes(std::move(oth.routes))
, init_wrappers(std::move(oth.init_wrappers))
, path_not_found_handler(std::move(oth.path_not_found_handler))
, base_route(std::move(oth.base_route)) {}

router::router(const router& oth)
: routes(oth.routes)
, init_wrappers(oth.init_wrappers)
, path_not_found_handler(oth.path_not_found_handler)
, base_route(oth.base_route) {}

router::router(std::initializer_list<cf::middleware_wrapper> wrappers)
: routes({})
, init_wrappers(wrappers)
, path_not_found_handler(path_not_found)
, base_route("") {}

router& router::operator=(const router& oth) {
    if (this not_eq &oth) {
        this->routes = oth.routes;
        this->init_wrappers = oth.init_wrappers;
        this->path_not_found_handler = oth.path_not_found_handler;
        this->base_route = oth.base_route;
    }
    return *this;
}

router& router::operator=(router&& oth) {
    if (this not_eq &oth) {
        this->routes = std::move(oth.routes);
        this->init_wrappers = std::move(oth.init_wrappers);
        this->path_not_found_handler = std::move(oth.path_not_found_handler);
        this->base_route = std::move(oth.base_route);
    }
    return *this;
}

std::string router::sanitize_path(std::string path) {
    auto sanitized = std::regex_replace(path, this->sanitize_regex, "/");
    if (not sanitized.empty() and sanitized.back() == '/') {
        sanitized.pop_back();
    }
    return sanitized;
}

std::vector<std::string> router::make_var_captures(std::string path) {
    std::vector<std::string> cap;

    auto iter_begin = std::sregex_iterator(path.begin(), path.end(), this->find_var_regex);
    auto iter_end = std::sregex_iterator();
    for (auto match = iter_begin; match != iter_end; ++match) {
        cap.push_back((*match).str().erase(0, 1));
    }
    return cap;
}

void router::make_route_regex(std::string path, router::route_data& rd) {
    rd.var_names  = std::move(this->make_var_captures(path));
    path = std::regex_replace(path, this->find_two_wildcard_regex, "__TWO_WILDCARD_PLACEHOLDER__");

    // // match path regex
    auto match_path = std::regex_replace(path, this->find_wildcard_regex, this->match_regex);
    match_path = std::regex_replace(match_path,
                                    this->find_placeholder_regex,
                                    this->two_wildcard_match_regex);
    match_path = std::regex_replace(match_path, this->find_var_regex, this->match_regex);

    // capture var regex
    auto capture_vars = std::regex_replace(path, this->find_wildcard_regex, this->match_regex);
    capture_vars = std::regex_replace(capture_vars,
                                      this->find_placeholder_regex,
                                      this->two_wildcard_match_regex);
    capture_vars = std::regex_replace(capture_vars, this->find_var_regex, this->capture_regex);

    // capture wildcards regex
    auto capture_wildcards = std::regex_replace(path,
                                                this->find_wildcard_regex,
                                                this->capture_regex);
    capture_wildcards = std::regex_replace(capture_wildcards,
                                           this->find_placeholder_regex,
                                           this->two_wildcard_match_regex);
    capture_wildcards = std::regex_replace(capture_wildcards,
                                           this->find_var_regex,
                                           this->match_regex);

    // capture double_wildcard regex
    auto capture_dwildcards = std::regex_replace(path,
                                                 this->find_wildcard_regex,
                                                 this->match_regex);
    capture_dwildcards = std::regex_replace(capture_dwildcards,
                                            this->find_placeholder_regex,
                                            this->two_wildcard_capture_regex);
    capture_dwildcards = std::regex_replace(capture_dwildcards,
                                            this->find_var_regex,
                                            this->match_regex);


    rd.match_path = std::regex(match_path);
    rd.capture_vars = std::regex(capture_vars);
    rd.capture_wildcards = std::regex(capture_wildcards);
    rd.capture_dwildcards = std::regex(capture_dwildcards);
}

std::vector<std::pair<std::string, cf::method>> router::validate() {
    auto dup_list = std::vector<std::pair<std::string, cf::method>>{};

    for (auto& w : this->init_wrappers) {
        // if the wrapper contains a rotue
        if (w.is_route()) {
            auto&& route = std::move(w.unwrap_route());
            // clean the path + add base_url
            auto full_path = this->base_route + route.path;
            auto sanitized_path = this->sanitize_path(full_path);
            // make the route_wrapper
            auto rw = router::route_wrapper {
                std::move(route.handler),
                std::move(route.middlewares),
            };
            if (not this->insert(sanitized_path, route.method, std::move(rw))) {
                dup_list.push_back(std::make_pair(route.path, route.method));
            }
        } else {
            auto&& m = std::move(w.unwrap_middleware());
            auto m_name = m.name;
            this->middlewares.emplace(m_name, std::move(m));
            // store the middleware
        }

    }
    return dup_list;
}

router& router::add_route(cf::route&& route) {
    auto full_path = this->base_route + route.path;
    auto sanitized_path = this->sanitize_path(full_path);
    // make the route_wrapper
    auto rw = router::route_wrapper {
        std::move(route.handler),
        std::move(route.middlewares),
    };
    this->insert(sanitized_path, route.method, std::move(rw));
    return *this;
}

bool router::insert(std::string path, cf::method method, route_wrapper&& rw) {
    auto search_path = this->routes.find(path);
    if(search_path not_eq this->routes.end()) {
        auto search_method = search_path->second.methods_map.find(method);
        if (search_method not_eq search_path->second.methods_map.end()) {
            // method for path already exist
            return false;
        } else {
            // add the route_wrapper for the method to the routes
            search_path->second.methods_map.emplace(std::move(method), std::move(rw));
        }
    } else {
        // creat the route_data which will contains the routes
        auto rd = router::route_data{};
        // create the regex to match the path
        this->make_route_regex(path, rd);

        // insert current data
        rd.methods_map.emplace(std::move(method), std::move(rw));
        // insert the map inside the routes map
        this->routes.emplace(std::move(path), std::move(rd));
    }
    return true;
}

void router::capture_var_from_path(const std::string& path,
                                   std::regex& capture_regex,
                                   cf::context& ctx,
                                   const std::vector<std::string>& captures_names) {
    if (not captures_names.empty()) {
        std::smatch match;
        unsigned int i = 1;
        std::regex_search(path, match, capture_regex);
        std::for_each(captures_names.begin(),
                      captures_names.end(),
                      [&](auto& name) { ctx.vars[name] = match[i]; i += 1; });
    }
}

void router::capture_wildcards_from_path(const std::string& path,
                                        std::regex& capture_regex,
                                        cf::context& ctx) {
    std::smatch match;
    std::regex_search(path, match, capture_regex);
    std::for_each(++(match.begin()),
                  match.end(),
                  [&](auto& w) { ctx.wildcards.push_back(w); });
}

void router::capture_dwildcards_from_path(const std::string& path,
                                         std::regex& capture_regex,
                                         cf::context& ctx) {
    std::smatch match;
    std::regex_search(path, match, capture_regex);
    std::for_each(++(match.begin()),
                  match.end(),
                  [&](auto& w) { ctx.d_wildcards.push_back(w); });
}

cf::response router::dispatch(cf::context& ctx) {
    // sanitize the path
    auto sanitized_path = this->sanitize_path(ctx.path);
    for (auto& r : this->routes) {
        // if the path match
        if (std::regex_match(sanitized_path, r.second.match_path)) {
            // try to find the route for the method
            auto good_route = r.second.methods_map.find(ctx.method);
            if (good_route not_eq r.second.methods_map.end()) {
                // process middlewares
                try {
                    std::for_each(good_route->second.middlewares.begin(),
                                  good_route->second.middlewares.end(),
                                  [&](auto& name)
                                  { this->middlewares.find(name)->second.handler(ctx); });
                // if one middleware stop the process
                } catch (cf::stop& s) {
                    return s.unwrap_response();
                }
                // captures in path
                this->capture_var_from_path(sanitized_path,
                                            r.second.capture_vars,
                                            ctx,
                                            r.second.var_names);
                this->capture_wildcards_from_path(sanitized_path, r.second.capture_wildcards, ctx);
                this->capture_dwildcards_from_path(sanitized_path,
                                                   r.second.capture_dwildcards,
                                                   ctx);
                // call the route
                return good_route->second.handler(ctx);
            }
        }
    }
    return this->path_not_found_handler(ctx);
}

void router::set_base_route(std::string base_route) {
    base_route.push_back('/');
    this->base_route = base_route;
}

const std::string& router::get_base_route() const { return this->base_route; }

// router_wrapper impl

router::route_wrapper::route_wrapper(route_wrapper&& oth)
: handler(std::move(oth.handler))
, middlewares(std::move(oth.middlewares)) {}

router::route_wrapper::route_wrapper(const route_wrapper& oth)
: handler(oth.handler)
, middlewares(oth.middlewares) {}

router::route_wrapper::route_wrapper(std::function<cf::response(cf::context&)>&& handler,
                                     std::vector<std::string>&& middlewares)
: handler(std::move(handler))
, middlewares(std::move(middlewares)) {}

router::route_wrapper& router::route_wrapper::operator=(route_wrapper&& oth) {
    if (this not_eq &oth) {
        this->handler = oth.handler;
        this->middlewares = oth.middlewares;
    }
    return *this;
}

router::route_wrapper& router::route_wrapper::operator=(const route_wrapper& oth) {
    if (this not_eq &oth) {
        this->handler = std::move(oth.handler);
        this->middlewares = std::move(oth.middlewares);
    }
    return *this;
}

// route data impl

router::route_data::route_data(route_data&& oth)
: match_path(std::move(oth.match_path))
, capture_vars(std::move(oth.capture_vars))
, capture_wildcards(std::move(oth.capture_wildcards))
, capture_dwildcards(std::move(oth.capture_dwildcards))
, var_names(std::move(oth.var_names))
, methods_map(std::move(oth.methods_map)) {}

router::route_data::route_data(const route_data& oth)
: match_path(oth.match_path)
, capture_vars(oth.capture_vars)
, capture_wildcards(oth.capture_wildcards)
, capture_dwildcards(oth.capture_dwildcards)
, var_names(oth.var_names)
, methods_map(oth.methods_map) {}

router::route_data::route_data(std::regex&& match_path,
                               std::vector<std::string> var_names)
: match_path(std::move(match_path))
, var_names(std::move(var_names))
, methods_map({}) {}

router::route_data& router::route_data::operator=(route_data&& oth) {
    if (this not_eq &oth) {
        this->match_path = std::move(oth.match_path);
        this->var_names = std::move(oth.var_names);
        this->capture_vars = std::move(oth.capture_vars);
        this->capture_wildcards = std::move(oth.capture_wildcards);
        this->capture_dwildcards = std::move(oth.capture_dwildcards);
        this->methods_map = std::move(oth.methods_map);
    }
    return *this;
}

router::route_data& router::route_data::operator=(const route_data& oth) {
    if (this not_eq &oth) {
        this->match_path = oth.match_path;
        this->capture_vars = oth.capture_vars;
        this->capture_wildcards = oth.capture_wildcards;
        this->capture_dwildcards = oth.capture_dwildcards;
        this->var_names = oth.var_names;
        this->methods_map = oth.methods_map;
    }
    return *this;
}

}

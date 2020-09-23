#ifndef ENTT_ENTITY_ORGANIZER_HPP
#define ENTT_ENTITY_ORGANIZER_HPP


#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../core/type_info.hpp"
#include "../core/type_traits.hpp"
#include "fwd.hpp"
#include "helper.hpp"


namespace entt {


namespace internal {


template<typename>
struct is_view: std::false_type {};

template<typename Entity, typename... Exclude, typename... Component>
struct is_view<basic_view<Entity, exclude_t<Exclude...>, Component...>>: std::true_type {};

template<typename Type>
inline constexpr bool is_view_v = is_view<Type>::value;


template<typename... Type>
struct resource {
    using type = type_list_cat_t<resource<Type>::template type...>;
    using ro = type_list_cat_t<resource<Type>::template ro...>;
    using rw = type_list_cat_t<resource<Type>::template rw...>;
};


template<typename Type>
struct resource<Type> {
    using type = type_list<std::remove_const_t<Type>>;
    using ro = std::conditional_t<std::is_const_v<Type>, type_list<std::remove_const_t<Type>>, type_list<>>;
    using rw = std::conditional_t<std::is_const_v<Type>, type_list<>, type_list<Type>>;
};

template<typename Entity, typename... Exclude, typename... Component>
struct resource<basic_view<Entity, exclude_t<Exclude...>, Component...>> {
    using type = type_list<basic_view<Entity, exclude_t<Exclude...>, Component...>>;
    using ro = type_list_cat_t<type_list<Exclude...>, type_list_cat_t<std::conditional_t<std::is_const_v<Component>, type_list<std::remove_const_t<Component>>, type_list<>>...>>;
    using rw = type_list_cat_t<type_list_cat_t<std::conditional_t<std::is_const_v<Component>, type_list<>, type_list<Component>>...>>;
};


template<typename Ret, typename... Args>
resource<std::remove_reference_t<Args>...> to_resource(Ret(*)(Args...));

template<typename Ret, typename Class, typename... Args>
resource<std::remove_reference_t<Args>...> to_resource(Ret(Class:: *)(Args...));

template<typename Ret, typename Class, typename... Args>
resource<std::remove_reference_t<Args>...> to_resource(Ret(Class:: *)(Args...) const);


}


template<typename Entity>
class basic_organizer {
    struct node {
        const char *name{};
        entt::type_info info{};
        void(* job)(const void *, entt::basic_registry<Entity> &){};
        const void *payload{};
        bool top_level{};
    };

    struct task_iterator {
        // TODO
    };

    template<typename Type>
    static decltype(auto) extract(basic_registry<Entity> &registry) {
        if constexpr(internal::is_view_v<Type>) {
            return as_view{registry};
        } else {
            return registry.ctx<std::remove_reference_t<Type>>();
        }
    }

    template<typename... Args>
    static auto to_args(basic_registry<Entity> &registry, type_list<Args...>) {
        return std::forward_as_tuple(extract<Args>(registry)...);
    }

    template<typename... RO, typename... RW>
    void track_dependencies(std::size_t index, type_list<RO...>, type_list<RW...>) {
        edges.clear();
        (dependencies[type_hash<RO>::value()].emplace_back(index, false), ...);
        (dependencies[type_hash<RW>::value()].emplace_back(index, true), ...);
    }

    void refresh_graph() {
        if(edges.empty()) {
            const auto length = nodes.size();
            edges.resize(length * length, false);

            // creates the ajacency matrix
            for(const auto &deps: dependencies) {
                const auto last = deps.second.cend();
                auto it = deps.second.cbegin();

                while(it != last) {
                    if(it->second) {
                        // rw item
                        if(auto curr = it++; it != last) {
                            if(it->second) {
                                edges[curr->first * length + it->first] = true;
                            } else {
                                if(const auto next = std::find_if(it, last, [](const auto &elem) { return elem.second; }); next != last) {
                                    for(; it != next; ++it) {
                                        edges[curr->first * length + it->first] = true;
                                        edges[it->first * length + next->first] = true;
                                    }
                                } else {
                                    for(; it != next; ++it) {
                                        edges[curr->first * length + it->first] = true;
                                    }
                                }
                            }
                        }
                    } else {
                        // ro item, possibly only on first iteration
                        if(const auto next = std::find_if(it, last, [](const auto &elem) { return elem.second; }); next != last) {
                            for(; it != next; ++it) {
                                edges[it->first * length + next->first] = true;
                            }
                        } else {
                            it = last;
                        }
                    }
                }
            }

            // computes the transitive closure
            for(std::size_t vk{}; vk < length; ++vk) {
                for(std::size_t vi{}; vi < length; ++vi) {
                    for(std::size_t vj{}; vj < length; ++vj) {
                        edges[vi * length + vj] = edges[vi * length + vj] || (edges[vi * length + vk] && edges[vk * length + vj]);
                    }
                }
            }

            // applies the transitive reduction
            for(std::size_t vert{}; vert < length; ++vert) {
                edges[vert * length + vert] = false;
            }

            for(std::size_t vj{}; vj < length; ++vj) {
                for(std::size_t vi{}; vi < length; ++vi) {
                    if(edges[vi * length + vj]) {
                        for(std::size_t vk{}; vk < length; ++vk) {
                            if(edges[vj * length + vk]) {
                                edges[vi * length + vk] = false;
                            }
                        }
                    }
                }
            }

            // finds top level nodes
            for(std::size_t vi{}; vi < length; ++vi) {
                nodes[vi].top_level = true;

                for(std::size_t vk{}; nodes[vi].top_level && vk < length; ++vk) {
                    nodes[vi].top_level = !edges[vk * length + vi];
                }
            }
        }
    }

public:
    using entity_type = Entity;
    using size_type = std::size_t;

    template<auto Candidate>
    void emplace(const char *name = nullptr) {
        using resource_type = decltype(internal::to_resource(Candidate));
        const auto index = nodes.size();
        auto &item = nodes.emplace_back();

        item.name = name;
        item.top_level = false;
        item.info = type_id<integral_constant<Candidate>>();
        item.job = +[](const void *, basic_registry<entity_type> &registry) {
            auto arguments = to_args(registry, resource_type::type{});
            std::apply(Candidate, arguments);
        };

        track_dependencies(index, resource_type::ro{}, resource_type::rw{});
    }

    template<auto Candidate, typename Type>
    void emplace(Type &value_or_instance, const char *name = nullptr) {
        using resource_type = decltype(internal::to_resource(Candidate));
        const auto index = nodes.size();
        auto &item = nodes.emplace_back();

        item.name = name;
        item.top_level = false;
        item.info = type_id<integral_constant<Candidate>>();
        item.payload = &value_or_instance;
        item.job = +[](const void *payload, basic_registry<entity_type> &registry) {
            Type *curr = static_cast<Type *>(const_cast<std::conditional_t<std::is_const_v<Type>, const void *, void *>>(payload));
            auto arguments = std::tuple_cat(std::forward_as_tuple(*curr), to_args(registry, resource_type::type{}));
            std::apply(Candidate, arguments);
        };

        track_dependencies(index, resource_type::ro{}, resource_type::rw{});
    }

    class task {
        friend class basic_organizer<entity_type>;

        task(std::vector<node> &vert, std::vector<bool> &deps, const std::size_t curr)
            : nodes{&vert},
              edges{&deps},
              index{curr}
        {}

    public:
        using job_type = void(const void *, basic_registry<entity_type> &);

        task() = default;

        const char * name() const ENTT_NOEXCEPT {
            return (*nodes)[index].name;
        }

        type_info type_info() const ENTT_NOEXCEPT {
            return (*nodes)[index].type_info;
        }

        bool top_level() const ENTT_NOEXCEPT {
            return (*nodes)[index].top_level;
        }

        job_type * job() const ENTT_NOEXCEPT {
            return (*nodes)[index].job;
        }

        const void * data() const ENTT_NOEXCEPT {
            return (*nodes)[index].data;
        }

        explicit operator bool() const ENTT_NOEXCEPT {
            return (nodes != nullptr);
        }

        template<typename Func>
        void parent(Func func) {
            for(std::size_t pos{}, length = nodes->size(); pos < length; ++pos) {
                if((*edges)[pos * length + index]) {
                    func(task{*nodes, *edges, pos});
                }
            }
        }

        template<typename Func>
        void children(Func func) {
            for(std::size_t pos{}, length = nodes->size(), row = index * length; pos < length; ++pos) {
                if((*edges)[row + pos]) {
                    func(task{*nodes, *edges, pos});
                }
            }
        }

    private:
        std::vector<node> *nodes;
        std::vector<bool> *edges;
        std::size_t index{};
    };

    template<typename Func>
    void visit(Func func) {
        refresh_graph();

        for(std::size_t pos{}, last = nodes.size(); pos < last; ++pos) {
            if(nodes[pos].top_level) {
                func(task{nodes, edges, pos});
            }
        }
    }

private:
    std::vector<bool> edges;
    std::unordered_map<entt::id_type, std::vector<std::pair<std::size_t, bool>>> dependencies;
    std::vector<node> nodes;
};


}


#endif

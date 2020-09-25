#include <gtest/gtest.h>
#include <entt/entity/organizer.hpp>
#include <entt/entity/registry.hpp>

void t1(const int &, const double &) {}
void t2(const int &, char &, const double &) {}
void t3(int &) {}
void t4(int &, const char &) {}
void t5(const int &, double &) {}

#include <iostream>

void print_vertex(const std::vector<entt::organizer::vertex> &graph, std::size_t curr, std::size_t level = {}) {
    const auto &vertex = graph[curr];
    for(std::size_t next{}; next < level; ++next) { std::cout << "\t"; }
    std::cout << vertex.name() << std::endl;
    for(auto &&index: vertex.children()) { print_vertex(graph, index, level+1u); }
}

TEST(Organizer, TODO) {
    entt::organizer organizer;

    // TODO
    organizer.emplace<&t1>("t1");
    organizer.emplace<&t2>("t2");
    organizer.emplace<&t3>("t3");
    organizer.emplace<&t4>("t4");
    organizer.emplace<&t5>("t5");

    auto graph = organizer.adjacency_list();

    for(std::size_t pos{}; pos < graph.size(); ++pos) {
        print_vertex(graph, pos);
    }
}
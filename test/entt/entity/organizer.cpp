#include <gtest/gtest.h>
#include <entt/core/utility.hpp>
#include <entt/entity/organizer.hpp>
#include <entt/entity/registry.hpp>

void t1(const int &, const double &) {}
void t2(const int &, char &, const double &) {}
void t3(int &) {}
void t4(int &, const char &) {}
void t5(const int &, double &) {}

#include <iostream>

TEST(Organizer, TODO) {
    entt::organizer organizer;

    // TODO
    organizer.emplace<&t1>("t1");
    organizer.emplace<&t2>("t2");
    organizer.emplace<&t3>("t3");
    organizer.emplace<&t4>("t4");
    organizer.emplace<&t5>("t5");

    std::cout << "pre" << std::endl;
    int tab = 0;

    entt::y_combinator visitor([&tab](auto &self, auto task) {
        for(auto i = 0; i < tab; ++i) { std::cout << "\t"; }
        std::cout << "[" << task.top_level() << "]" << task.name() << std::endl;

        ++tab;
        task.children(self);
        --tab;
    });

    organizer.visit(visitor);
    std::cout << "post" << std::endl;
}
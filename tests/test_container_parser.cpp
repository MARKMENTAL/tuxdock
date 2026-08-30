#include "container_parser.hpp"

#include <cassert>
#include <iostream>

int main() {
    const auto result = parseContainerList(R"([
      {"Id":"running-id","Names":["/web"],"State":"running","Status":"Up 2 minutes","Ports":[]},
      {"Id":"exited-id","Names":["/old"],"State":"exited","Status":"Exited (0) 1 minute ago","Ports":[{"PrivatePort":80,"Type":"tcp"}]},
      {"Id":"odd-id","Names":["/odd"],"State":"exited","Ports":"unexpected"},
      {"Id":"mapped-id","Names":["/mapped"],"State":"running","Status":"Up 1 minute","Ports":[{"IP":"0.0.0.0","PrivatePort":80,"PublicPort":8080,"Type":"tcp"},{"IP":"::","PrivatePort":80,"PublicPort":8080,"Type":"tcp"}]}
    ])");
    assert(result.ok());
    assert(result.items.size() == 4);
    assert(result.items[0].running);
    assert(!result.items[1].running);
    assert(result.items[1].name == "old");
    assert(result.items[1].ports.empty());
    assert(result.items[2].name == "odd");
    assert(result.items[3].name == "mapped");
    assert(result.items[3].ports == "8080:80");

    const auto empty = parseContainerList("[]");
    assert(empty.ok());
    assert(empty.items.empty());

    const auto invalid = parseContainerList("not-json");
    assert(!invalid.ok());
    std::cout << "Container parser tests passed\n";
}

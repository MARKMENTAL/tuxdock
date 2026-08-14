#include "container_parser.hpp"

#include <cassert>
#include <iostream>

int main() {
    const auto result = parseContainerList(R"([
      {"Id":"running-id","Names":["/web"],"State":"running","Status":"Up 2 minutes","Ports":[]},
      {"Id":"exited-id","Names":["/old"],"State":"exited","Status":"Exited (0) 1 minute ago","Ports":[{"PrivatePort":80,"Type":"tcp"}]},
      {"Id":"odd-id","Names":["/odd"],"State":"exited","Ports":"unexpected"}
    ])");
    assert(result.ok());
    assert(result.items.size() == 3);
    assert(result.items[0].running);
    assert(!result.items[1].running);
    assert(result.items[1].name == "old");
    assert(result.items[1].ports.empty());
    assert(result.items[2].name == "odd");

    const auto empty = parseContainerList("[]");
    assert(empty.ok());
    assert(empty.items.empty());

    const auto invalid = parseContainerList("not-json");
    assert(!invalid.ok());
    std::cout << "Container parser tests passed\n";
}

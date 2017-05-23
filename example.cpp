#include <cassert>
#include <algorithm>
#include "gc.hpp"


struct Counter {
  static ::std::size_t count;
  bool valid;

  Counter()
    : valid(true) {
  }

  Counter(Counter &&o) noexcept : valid(false) {
    ::std::swap(valid, o.valid);
  }

  Counter &
  operator=(Counter &&) = delete;

  Counter(Counter const &) = delete;
  Counter &
  operator=(const Counter &) = delete;

  ~Counter() {
    if (valid)
      count++;
  }
};

::std::size_t Counter::count = 0;


struct Cycle {
  GC_OBJECT(Cycle);
  Counter c;
  ::gc::Ptr<Cycle> GC_MEMBER(peer);
};


int
main() {
  {
    Counter::count = 0;
    {
      auto p = ::gc::make<Counter>({});
    }
    assert(Counter::count == 1);
  }

  {
    Counter::count = 0;
    {
      ::gc::Ptr<Counter> p1;
      {
        auto p2 = ::gc::make<Counter>({});
        p1 = p2;
      }
      assert(Counter::count == 0);
      ::gc::collect();
      assert(Counter::count == 0);
    }

    assert(Counter::count == 1);
  }

  {
    Counter::count = 0;
    {
      auto p1 = ::gc::make<Cycle>({});
      auto p2 = ::gc::make<Cycle>({});
      p1->peer = p2;
      p2->peer = p1;
    }
    assert(Counter::count == 0);
    ::gc::collect();
    assert(Counter::count == 2);
  }
}

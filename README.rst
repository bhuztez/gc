==
gc
==

precise garbage collection for C++

To deal with circular references, a garbage collector has to keep track of all
references between garbage collected objects. Typically, for statically typed
language, a list of offsets to all references to other garbage collected objects
is generated at compile time for each type. This is also possible in C++ to some
extent.

Despite C++ puts a strong emphasis on compile-time programming, C++ experts
prefer to do this at runtime. For example, last year, Herb Sutter `talked about
his gcpp`__ . It plays the same trick as `Smieciuch Garbage Collector`__, which
has been around for over a decade. They have to check if a pointer is located in
GC heap or not when constructor is called and record the address of the
pointer. This will make a moving garbage collector sick.

.. code:: c++

    struct gcptr {
       gcptr(...) {
         if (located_in_gc_heap(this)) {
           // this is a pointer from non-root
         } else {
           // this is a pointer from root
         }
       }
     }

.. __: https://herbsutter.com/2016/09/22/my-talk-tomorrow-and-a-little-experimental-library/
.. __: http://smieciuch.sourceforge.net/


Since pointer to member can be used as a template argument, and with stateful
metaprograming, especially `friend injection techique discovered by Filip
Roséen`__ , they can be stored in a compile-time list. Then we initialize the
static variable used by the garbage collector, by a call to a constexpr
function, which returns an array of offsets and its size.

.. __: http://b.atch.se/posts/non-constant-constant-expressions/

Here is a simple example. :code:`gc.hpp` implements the same algorithm as
described in `Garbage Collection for Python`__ . As you can see, two Cycle
objects which references to each other, are destructed, after
:code:`::gc::collect()`.

.. __: http://arctrix.com/nas/python/gc/

.. code:: c++

    // example.cpp
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

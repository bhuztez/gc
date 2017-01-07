#pragma once

#include <cstdlib>
#include <cstddef>
#include <algorithm>
#include <type_traits>
#include <utility>

namespace gc {

  template<typename, int>
  struct Counter{
    friend constexpr auto state(Counter);
  };

  template<typename T, int N, typename V>
  struct Set {
    friend constexpr auto state(Counter<T, N>) {
      return V {};
    }
  };

  template<typename T, int N=0>
  constexpr
  int
  next(float, Counter<T,N> = {}) {
    return N;
  }

  template<typename T, int N=0,
           typename = decltype(state(Counter<T,N>{}))
           >
  constexpr
  int
  next(int, Counter<T,N> = {}, int value = next(0, Counter<T,N+1>{})) {
    return value;
  }

  template<typename... T>
  struct LIST {
  };

  template<typename T, typename U>
  struct APPEND;

  template<typename U, typename... T>
  struct APPEND<LIST<T...>, U> {
    using TYPE = LIST<T..., U>;
  };

  template<typename T, T x>
  struct MEMBER {
  };

  struct MemberMetadata {
    ::std::size_t offset;
    ::std::size_t n;
  };

  struct TypeMetadata {
    void (*destroy)(char *);
    ::std::size_t n;
    MemberMetadata members[];
  };

  template<typename T>
  void
  destroy(char *data) {
    T *value = (T *)data;
    value->~T();
  }

  template <typename T, typename V>
  constexpr
  MemberMetadata
  make_member_metadata(V T::*M) {
    return {reinterpret_cast<std::size_t>(&(((T*)0)->*M)), sizeof(V)/sizeof(void*)};
  }

  template<typename T, T x>
  constexpr
  MemberMetadata
  make_member_metadata(MEMBER<T, x>) {
    return make_member_metadata(x);
  }

  template<typename T, typename... M>
  TypeMetadata
  make_type_metadata(LIST<M...>) {
    return {&destroy<T>, sizeof...(M), {make_member_metadata(M{})...}};
  }

  template<typename T,
           int N = next<T>(0),
           typename = typename ::std::enable_if<(N == 0)>::type,
          ::std::size_t = sizeof(Set<T, N, bool>)>
  constexpr
  TypeMetadata
  make_metadata() {
    return {&destroy<T>, 0, {}};
  }

  template<typename T,
           int N = next<T>(0),
           typename = typename ::std::enable_if<(N > 0)>::type,
           typename State = decltype(state(Counter<T,N-1>{})),
           ::std::size_t = sizeof(Set<T, N, bool>)
           >
  constexpr
  TypeMetadata
  make_metadata() {
    return make_type_metadata<T>(State {});
  }

  template<typename T>
  struct Metadata {
    static const TypeMetadata metadata;
  };

  template<typename T>
  const
  TypeMetadata
  Metadata<T>::metadata = make_metadata<T>();

  template<typename T>
  struct Object {
    using __CLASS__ = T;
    static_assert(sizeof(Metadata<T>));
    static_assert(sizeof(Set<T, 0, LIST<>>));
  };

  struct Header {
    Header *prev, *next;

    Header() : prev(this), next(this) {
    }

  };

  void
  insert_before(Header&new_header, Header&header) {
    new_header.prev = header.prev;
    new_header.next = &header;
    header.prev->next = &new_header;
    header.prev = &new_header;
  }

  void
  remove(Header& header) {
    header.prev->next = header.next;
    header.next->prev = header.prev;
  }

  struct Value {
    Header header;
    const TypeMetadata *meta;
    ::std::size_t ref_count;
    char data[];
  };

  template<typename T>
  struct Ptr {
    Value *value;

    Ptr(Value *value = nullptr) : value(value) {
      increase();
    }

    Ptr(Ptr const& o) : value(o.value) {
      increase();
    }

    Ptr(Ptr&& o) noexcept : value(nullptr) {
      ::std::swap(value, o.value);
    }

    operator bool() const {
      return !(value == nullptr);
    }

    T &
    operator*() {
      return *((T*)(value->data));
    }

    T const&
    operator*() const {
      return *((T*)(value->data));
    }

    T *
    operator->() {
      return ((T*)(value->data));
    }

    T const*
    operator->() const {
      return ((T*)(value->data));
    }

    Ptr&
    operator=(Ptr const& o) {
      decrease();
      value = o.value;
      increase();
      return *this;
    }

    Ptr&
    operator=(Ptr&& o) noexcept {
      ::std::swap(value, o.value);
      return *this;
    }

    void
    increase() {
      if (value)
        value->ref_count++;
    }

    void
    decrease() {
      if(!value)
        return;
      if(--value->ref_count > 0)
        return;

      remove(value->header);
      ((T*)(value->data))->~T();
      ::std::free(value);
    }

    ~Ptr() {
      decrease();
    }
  };

  template<typename T>
  constexpr
  bool
  is_ptr = false;

  template<typename T>
  constexpr
  bool
  is_ptr<Ptr<T>> = true;

  template<typename T, typename V>
  struct element;

  template<typename T, typename V>
  struct element<T, V T::*> {
    using type = V;
  };

  template<typename T, typename V, ::std::size_t N>
  struct element<T, V (T::*)[N]> {
    using type = typename element<T, V T::*>::type;
  };

  template<typename T, typename V, V M,
           typename = typename ::std::enable_if<is_ptr<typename element<T, V>::type>>::type,
           int N = next<T>(0),
           typename = typename ::std::enable_if<(N > 0)>::type,
           typename State = decltype(state(Counter<T,N-1>{})),
           ::std::size_t = sizeof(Set<T, N, typename APPEND<State, MEMBER<V,M>>::TYPE>)
           >
  constexpr
  bool
  declare_member() {
    return true;
  }

  struct Context {
    Header header;

    Context() : header() {
    }

    template<typename T>
    Ptr<T>
    make(T&& value) {
      Value *v = (Value*)::std::malloc(sizeof(Value) + sizeof(T));
      v->meta = &(Metadata<T>::metadata);
      v->ref_count = 0;
      new (v->data) T(::std::move(value));
      insert_before(v->header, header);
      return v;
    }

    void
    collect() {
      Header unused;

      for (Header *p=&header; p->next != &header; p = p->next) {
        Value *value = (Value *)(p->next);

        for(::std::size_t i=0; i< value->meta->n; i++) {
          Value **base = (Value **)((value->data) + value->meta->members[i].offset);
          for(::std::size_t j=0; j< value->meta->members[i].n; j++) {
            base[j]->ref_count --;
          }
        }
      }

      for(Header *p=header.next, *next=p->next; p != &header; p = next, next = next->next) {
        Value *value = (Value *)p;
        if (value -> ref_count > 0)
          continue;
        remove(value -> header);
        insert_before(value->header, unused);
      }

      for(Header *p=header.next; p != &header; p = p->next) {
        Value *value = (Value *)p;

        for(::std::size_t i=0; i< value->meta->n; i++) {
          Value **base = (Value **)((value->data) + value->meta->members[i].offset);
          for(::std::size_t j=0; j< value->meta->members[i].n; j++) {
            if(base[j]->ref_count == 0) {
              remove(base[j]->header);
              insert_before(base[j]->header, header);
            }

            base[j]->ref_count ++;
          }
        }

      }

      for(Header *p=unused.next; p != &unused; p = p->next) {
        Value *value = (Value *)p;
        value->meta->destroy(value->data);
      }

      for(Header *p=unused.next, *next=p->next; p != &unused; p = next, next = next->next) {
        Value *value = (Value *)p;
        ::std::free(value);
      }
    }

    ~Context() {
      collect();
    }
  };

  static Context default_context;

  template<typename T>
  Ptr<T>
  make(T&& value, Context& context = default_context) {
    return context.make(::std::move(value));
  }

  void
  collect(Context& context = default_context) {
    return context.collect();
  }
}


#define _GC_MEMBER_1(name, shape) name shape
#define _GC_MEMBER_0(name) name
#define _GC_MEMBER(x,y,M,...) M
#define GC_MEMBER(name, ...)                                            \
  _GC_MEMBER(,##__VA_ARGS__, _GC_MEMBER_1, _GC_MEMBER_0)(name,##__VA_ARGS__); \
  static_assert(::gc::declare_member<__CLASS__, decltype(&__CLASS__::name), &__CLASS__::name>())

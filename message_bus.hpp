#ifndef MESSAGE_BUS_MESSAGE_BUS_HPP_
#define MESSAGE_BUS_MESSAGE_BUS_HPP_

#include <cassert>
#include <functional>
#include <initializer_list>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace message_bus {

////////// FunctionTraits //////////
template<typename T>
struct FunctionTraits;

template<typename R, typename... Args>
struct FunctionTraits<R(Args...)> {
    using RType     = R;
    using TupleType = std::tuple<Args...>;
    using BareTupleType =
        std::tuple<typename std::remove_const<typename std::remove_reference<Args>::type>::type...>;
};

template<typename R>
struct FunctionTraits<R()> {
    using RType         = R;
    using TupleType     = std::tuple<>;
    using BareTupleType = std::tuple<>;
};

template<typename R, typename... Args>
struct FunctionTraits<R (*)(Args...)> : FunctionTraits<R(Args...)> {};

template<typename R, typename... Args>
struct FunctionTraits<std::function<R(Args...)>> : FunctionTraits<R(Args...)> {};

template<typename R, typename ClassType, typename... Args>
struct FunctionTraits<R (ClassType::*)(Args...)> : FunctionTraits<R(Args...)> {};

template<typename R, typename ClassType, typename... Args>
struct FunctionTraits<R (ClassType::*)(Args...) const> : FunctionTraits<R(Args...)> {};

template<typename Callable>
struct FunctionTraits : FunctionTraits<decltype(&Callable::operator())> {};

////////// void_t //////////
#if defined(__cpp_lib_void_t)

template<typename... T>
using void_t = std::void_t<T...>;

#else

template<typename... T>
using void_t = void;

#endif

////////// IsEqualityComparable //////////
template<typename T, typename U>
using EqualityComparableType = decltype(std::declval<T>() == std::declval<U>());

template<typename T, typename U, typename = void>
struct IsEqualityComparable : std::false_type {};

template<typename T, typename U>
struct IsEqualityComparable<T, U, void_t<EqualityComparableType<T, U>>>
    : std::is_same<EqualityComparableType<T, U>, bool> {};

////////// integer_sequence //////////
#if defined(__cpp_lib_integer_sequence)

template<std::size_t... Ints>
using index_sequence = std::index_sequence<Ints...>;

template<std::size_t N>
using make_index_sequence = std::make_integer_sequence<std::size_t, N>;

template<typename... T>
using index_sequence_for = std::index_sequence_for<T...>;

#else

// https://gist.github.com/ntessore/dc17769676fb3c6daa1f
template<typename T, T... Ints>
struct integer_sequence {
    static_assert(std::is_integral<T>::value, "T must be an integral type.");

    using value_type = T;

    static constexpr std::size_t size() noexcept {
        return sizeof...(Ints);
    }
};

template<std::size_t... Ints>
using index_sequence = integer_sequence<std::size_t, Ints...>;

template<typename T, std::size_t N, T... Res>
struct make_integer_sequence : make_integer_sequence<T, N - 1, N - 1, Res...> {};

template<typename T, T... Res>
struct make_integer_sequence<T, 0, Res...> : integer_sequence<T, Res...> {};

template<std::size_t N>
using make_index_sequence = make_integer_sequence<std::size_t, N>;

template<typename... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

#endif

////////// MessageBus //////////
class MessageBus {
private:
    MessageBus() = default;

public:
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

public:
    static MessageBus& GetInstance() {
        static MessageBus instance;
        return instance;
    }

public:
    bool HasKey(const std::string& key) {
        return (invokers_.end() != invokers_.find(key));
    }

    template<typename F>
    bool Register(const std::string& key, F f) {
        if (HasKey(key)) {
            return false;
        }

        invokers_[key] =
            std::bind(&Invoker<F>::Apply, f, std::placeholders::_1, std::placeholders::_2);

        return true;
    }

    template<typename F,
             typename ClassType,
             typename std::enable_if<std::is_member_function_pointer<F>::value, int>::type = 0>
    bool Register(const std::string& key, const F& f, ClassType* self) {
        if (HasKey(key)) {
            return false;
        }

        invokers_[key] = std::bind(&Invoker<F>::template ApplyMember<ClassType>,
                                   f,
                                   self,
                                   std::placeholders::_1,
                                   std::placeholders::_2);

        return true;
    }

    template<typename R, typename... Args>
    typename std::enable_if<!std::is_void<R>::value, R>::type Call(const std::string& key,
                                                                   Args&&... args) {
        auto iter = invokers_.find(key);
        assert(invokers_.end() != iter);

        R ret;
        CallImpl(iter, &ret, std::forward<Args>(args)...);

        return ret;
    }

    template<typename R, typename... Args>
    typename std::enable_if<std::is_void<R>::value>::type Call(const std::string& key,
                                                               Args&&... args) {
        auto iter = invokers_.find(key);
        assert(invokers_.end() != iter);

        CallImpl(iter, nullptr, std::forward<Args>(args)...);
    }

private:
    template<typename TIter, typename... Args>
    void CallImpl(TIter& iter, void* ret_value, Args&&... args) {
        using TupleType        = decltype(std::make_tuple(std::forward<Args>(args)...));
        TupleType&& args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);

        iter->second(&args_tuple, ret_value);

        Update(args_tuple,
               std::forward_as_tuple(std::forward<Args>(args)...),
               make_index_sequence<std::tuple_size<TupleType>::value>{});
    }

    template<typename Tuple0, typename Tuple1, std::size_t... Idx>
    void Update(Tuple0& src_tuple, Tuple1&& dst_tuple, index_sequence<Idx...>) {
        (void)std::initializer_list<int>{
            (UpdateImpl(std::get<Idx>(src_tuple), std::get<Idx>(dst_tuple)), 0)...};
    }

    template<typename SRC,
             typename DST,
             typename std::enable_if<(IsEqualityComparable<DST&, SRC&>::value &&
                                      std::is_assignable<DST&, SRC&>::value),
                                     int>::type = 0>
    void UpdateImpl(SRC& src, DST& dst) {
        if (dst != src) {
            dst = src;
        }
    }

    template<typename SRC,
             typename DST,
             typename std::enable_if<!(IsEqualityComparable<DST&, SRC&>::value &&
                                       std::is_assignable<DST&, SRC&>::value),
                                     int>::type = 0>
    void UpdateImpl(SRC& src, DST& dst) {
        (void)src;
        (void)dst;
    }

private:
    template<typename Func>
    struct Invoker {
        // non-member function
        static void Apply(const Func& func, void* args_tuple, void* ret_value) {
            using BareTupleType = typename FunctionTraits<Func>::BareTupleType;
            using RType         = typename FunctionTraits<Func>::RType;

            if (!std::is_void<RType>::value) {
                if (nullptr == ret_value) {
                    // forgot return value
                    assert(false);
                }
            }

            BareTupleType* bare_tuple = static_cast<BareTupleType*>(args_tuple);
            Call<RType>(func, *bare_tuple, ret_value);
        }

        template<typename R, typename F, typename... Args>
        static typename std::enable_if<std::is_void<R>::value>::type
        Call(const F& f, std::tuple<Args...>& args_tuple, void*) {
            CallHelper<void>(f, make_index_sequence<sizeof...(Args)>{}, args_tuple);
        }

        template<typename R, typename F, typename... Args>
        static typename std::enable_if<!std::is_void<R>::value>::type
        Call(const F& f, std::tuple<Args...>& args_tuple, void* ret_value) {
            R ret = CallHelper<R>(f, make_index_sequence<sizeof...(Args)>{}, args_tuple);
            if (nullptr != ret_value) {
                *static_cast<decltype(ret)*>(ret_value) = ret;
            }
        }

        template<typename R, typename F, std::size_t... Idx, typename... Args>
        static R CallHelper(const F& f,
                            const index_sequence<Idx...>&,
                            std::tuple<Args...>& args_tuple) {
            using TupleType = typename FunctionTraits<Func>::TupleType;
            return f(std::get<Idx>(args_tuple)...);
        }

        // class member function
        template<typename ClassType>
        static void ApplyMember(const Func& func,
                                ClassType* self,
                                void* args_tuple,
                                void* ret_value) {
            using BareTupleType = typename FunctionTraits<Func>::BareTupleType;
            using RType         = typename FunctionTraits<Func>::RType;

            if (!std::is_void<RType>::value) {
                if (nullptr == ret_value) {
                    // forget return value
                    assert(false);
                }
            }

            BareTupleType* bare_tuple = static_cast<BareTupleType*>(args_tuple);
            CallMember<RType>(func, self, *bare_tuple, ret_value);
        }

        template<typename R, typename F, typename ClassType, typename... Args>
        static typename std::enable_if<std::is_void<R>::value>::type
        CallMember(const F& f, ClassType* self, std::tuple<Args...>& args_tuple, void*) {
            CallMemberHelper<void>(f, self, make_index_sequence<sizeof...(Args)>{}, args_tuple);
        }

        template<typename R, typename F, typename ClassType, typename... Args>
        static typename std::enable_if<!std::is_void<R>::value>::type
        CallMember(const F& f, ClassType* self, std::tuple<Args...>& args_tuple, void* ret_value) {
            R ret =
                CallMemberHelper<R>(f, self, make_index_sequence<sizeof...(Args)>{}, args_tuple);
            if (nullptr != ret_value) {
                *static_cast<decltype(ret)*>(ret_value) = ret;
            }
        }

        template<typename R, typename F, typename ClassType, std::size_t... Idx, typename... Args>
        static R CallMemberHelper(const F& f,
                                  ClassType* self,
                                  const index_sequence<Idx...>&,
                                  std::tuple<Args...>& args_tuple) {
            using TupleType = typename FunctionTraits<Func>::TupleType;
            return (self->*f)(std::get<Idx>(args_tuple)...);
        }
    };

private:
    std::unordered_map<std::string, std::function<void(void*, void*)>> invokers_;
};

}  // namespace message_bus

#endif  // MESSAGE_BUS_MESSAGE_BUS_HPP_
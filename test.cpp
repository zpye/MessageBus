#include <iostream>

#include "message_bus.hpp"

int foo(int i, const char* const c, float& f, const double& d) {
    printf(">>> [foo] [%d, %s, %f, %lf]\n", i, c, f, d);

    f *= 2.0f;

    return (i + 1);
}

class Base {
public:
    void base_foo(int& i) {
        i += step_;
    }

    bool check_id(int id) const {
        return (id == this->id_);
    }

    int id_   = 0;
    int step_ = 2;
};

// overload
void foo2() {}

void foo2(const Base& base) {
    printf(">>> [foo2] [%d, %d]\n", base.id_, base.step_);
}

int foo3(int i, int j) {
    printf(">>> [foo3] [%d, %d]\n", i, j);
    return (i * 2 + j);
}

void foo4(int& i, int j) {
    i = j - 2;
    printf(">>> [foo4] [%d, %d]\n", i, j);
}

int main() {
    {
        printf("\n********** Test 0 **********\n");

        // check singleton
        message_bus::MessageBus& inst0 = message_bus::MessageBus::GetInstance();
        message_bus::MessageBus& inst1 = message_bus::MessageBus::GetInstance();
        printf("inst0 [%p], inst1 [%p], %s\n",
               &inst0,
               &inst1,
               ((&inst0 == &inst1) ? ("same address") : ("different address")));
    }

    message_bus::MessageBus& bus = message_bus::MessageBus::GetInstance();

    {
        printf("\n********** Test 1 **********\n");

        bus.Register("foo", &foo);

        float f = 1.2f;
        printf("[Before Call foo] [%f]\n", f);
        int ret = bus.Call<int>("foo", 1, "char", f, 3.14);
        printf("[After Call foo] [%d][%f]\n", ret, f);

        printf("\n");

        int i           = 2;
        const char* str = "const char";
        double d        = 2.718;
        printf("[Before Call foo] [%d, %s, %f, %lf]\n", i, str, f, d);
        ret = bus.Call<int>("foo", i, str, f, d);
        printf("[After Call foo] [%d][%d, %s, %f, %lf]\n", ret, i, str, f, d);
    }

    {
        printf("\n********** Test 2 **********\n");

        bus.Register("foo2", static_cast<void (*)(const Base&)>(&foo2));

        Base base0;
        base0.id_   = 10;
        base0.step_ = 5;

        Base base1;
        base1.id_   = 20;
        base1.step_ = 100;

        bus.Call<void>("foo2", base0);
        bus.Call<void>("foo2", base1);

        printf("\n");

        bus.Register("base_foo/0", &Base::base_foo, &base0);
        bus.Register("base_foo/1", &Base::base_foo, &base1);

        int i = 0;
        printf("[Before Call base_foo/0] [%d]\n", i);
        bus.Call<void>("base_foo/0", i);
        printf("[After Call base_foo/0] [%d]\n", i);

        printf("\n");

        printf("[Before Call base_foo/1] [%d]\n", i);
        bus.Call<void>("base_foo/1", i);
        printf("[After Call base_foo/1] [%d]\n", i);

        printf("\n");

        const Base& base0_const = base0;
        const Base& base1_const = base1;

        bus.Register("check_id/0", &Base::check_id, &base0_const);
        bus.Register("check_id/1", &Base::check_id, &base1_const);

        printf("[check_id/0] [%d, %d]\n", 10, bus.Call<bool>("check_id/0", 10));
        printf("[check_id/0] [%d, %d]\n", 20, bus.Call<bool>("check_id/0", 20));
        printf("[check_id/1] [%d, %d]\n", 10, bus.Call<bool>("check_id/1", 10));
        printf("[check_id/1] [%d, %d]\n", 20, bus.Call<bool>("check_id/1", 20));
    }

    {
        printf("\n********** Test 3 **********\n");

        bus.Register("lambda/0", [](int i) -> void { printf(">>> [lambda] [%d]\n", i); });

        bus.Call<void>("lambda/0", 1);
        bus.Call<void>("lambda/0", 3);
        bus.Call<void>("lambda/0", 5);
        bus.Call<void>("lambda/0", 7);
        bus.Call<void>("lambda/0", 9);

        printf("\n");

        bus.Register("lambda/1", [](int i) -> int { return (i + 1); });

        bus.Call<void>("lambda/0", bus.Call<int>("lambda/1", 1));
        bus.Call<void>("lambda/0", bus.Call<int>("lambda/1", 3));
        bus.Call<void>("lambda/0", bus.Call<int>("lambda/1", 5));
        bus.Call<void>("lambda/0", bus.Call<int>("lambda/1", 7));
        bus.Call<void>("lambda/0", bus.Call<int>("lambda/1", 9));
    }

    {
        printf("\n********** Test 4 **********\n");

        std::function<int(int)> func_foo3_0 = std::bind(&foo3, 1, std::placeholders::_1);
        std::function<int(int)> func_foo3_1 = std::bind(&foo3, 5, std::placeholders::_1);

        bus.Register("functional/0", func_foo3_0);
        bus.Register("functional/1", func_foo3_1);

        bus.Call<int>("functional/0", 1);
        bus.Call<int>("functional/0", 3);
        bus.Call<int>("functional/1", 1);
        bus.Call<int>("functional/1", 3);

        printf("\n");

        std::function<int(int)> func_bus_foo3_0 = [&](int i) -> int {
            return bus.Call<int>("functional/0", i);
        };
        std::function<int(int)> func_bus_foo3_1 = [&](int i) -> int {
            return bus.Call<int>("functional/1", i);
        };

        printf("[func_bus_foo3_0] [%d, %d]\n", 1, func_bus_foo3_0(1));
        printf("[func_bus_foo3_0] [%d, %d]\n", 3, func_bus_foo3_0(3));
        printf("[func_bus_foo3_1] [%d, %d]\n", 1, func_bus_foo3_1(1));
        printf("[func_bus_foo3_1] [%d, %d]\n", 3, func_bus_foo3_1(3));

        printf("\n");

        std::function<void(int&, const std::string&, int)> func_foo4(
            [&](int& i, const std::string& key, int j) -> void { foo4(i, bus.Call<int>(key, j)); });

        int i = 0;
        printf(">>> i [%d]\n", i);
        func_foo4(i, "functional/0", 1);
        printf(">>> i [%d]\n", i);
        func_foo4(i, "functional/0", 3);
        printf(">>> i [%d]\n", i);
        func_foo4(i, "functional/1", 1);
        printf(">>> i [%d]\n", i);
        func_foo4(i, "functional/1", 3);
        printf(">>> i [%d]\n", i);
    }

    {
        printf("\n********** Test 5 **********\n");

        struct CallableStruct {
            // TODO: operator must be const to satisfy "const F&"
            int operator()(int i) const {
                printf("[Callable] [%d]\n", i);
                return (i + 1);
            }
        } callable;

        bus.Register("callable", callable);

        bus.Call<int>("callable", 0);
        bus.Call<int>("callable", 2);
        bus.Call<int>("callable", 4);
        bus.Call<int>("callable", 8);
    }

    return 0;
}
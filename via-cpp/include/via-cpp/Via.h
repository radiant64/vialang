#pragma once

#include <via/parse.h>
#include <via/type-utils.h>
#include <via/vm.h>

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace via {
    class Vm;

    template <typename Type>
    struct NativeValue;

    template <typename Type>
    struct TypeMapper;

    template <typename Type>
    const auto decode = TypeMapper<Type>::decode;

    template <typename Type>
    const auto encode = TypeMapper<Type>::encode;

    struct Value {
        Value() = default;

        Value(const Value& other)
            : value(other.value)
        {
        }

        Value(const via_value* value)
            : value(value)
        {
        }

        Value(Value&& other)
            : value(other.value)
        {
        }

        virtual ~Value() = default;

        virtual const Value& operator =(const Value& other)
        {
            value = other.value;
            return *this;
        }

        virtual const Value& operator =(Value&& other)
        {
            value = other.value;
            return *this;
        }

        virtual const via_value* operator->() const { return value; }

        virtual operator const via_value*() const  { return value; }
        
        virtual bool operator==(const Value& rhs) const
        {
            return value == rhs.value;
        }

    protected:
        const via_value* value = nullptr;
    };

    class Vm {
        using Bound = std::tuple<Vm&, std::any>;
    public:
        Vm()
            : vm(via_create_vm(), via_free_vm)
        {
        }

        Vm(const Vm& other) = delete;

        Vm(Vm&& other)
            : vm(std::move(other.vm))
        {
        }

        ~Vm() = default;

        const Vm& operator =(const Vm&) = delete;
        const Vm& operator =(Vm&&) = delete;

        operator via_vm*() { return vm.get(); }

        template <typename Type>
        void returnValue(Type value)
        {
            vm->ret = TypeMapper<Type>::encode(*this, value);
        }

        template <typename FuncType>
        void registerProcedure(const std::string& name,
                const std::string& label, FuncType func)
        {
            registerProcedure(name, label, std::function(func));
        }

        template <typename Ret, typename... Args>
        void registerProcedure(const std::string& name,
                const std::string& label, std::function<Ret(Args...)> func)
        {
            contexts.emplace_back(new Bound(*this,
                        std::make_any<decltype(func)>(func)));

            via_register_proc_context(
                    vm.get(), name.c_str(), label.c_str(), nullptr,
                    [](void* data) {
                        auto& [self, anyFunc] = *reinterpret_cast<Bound*>(data);
                        auto f = std::any_cast<decltype(func)>(anyFunc);
                        auto args = std::make_tuple(
                                TypeMapper<Args>::decode(self,
                                    via_pop_arg(self.vm.get()))...);
                        self.returnValue(std::apply(f, args));
                    }, contexts.back().get());
        }

        void registerForm(const std::string& name, const std::string& label,
                std::function<void(const Value&)> func)
        {
            contexts.emplace_back(new Bound(*this,
                        std::make_any<decltype(func)>(func)));

            via_register_form_context(
                    vm.get(), name.c_str(), label.c_str(), nullptr,
                    [](void* data) {
                        auto& [self, anyFunc] = *reinterpret_cast<Bound*>(data);
                        auto f = std::any_cast<decltype(func)>(anyFunc);
                        f(via_reg_ctxt(self));
                    }, contexts.back().get());
        }

        Value run(const std::string& program)
        {
            via_set_expr(vm.get(), via_parse_ctx_program(
                        via_parse(vm.get(), program.c_str(), nullptr))->v_car);
            return via_run_eval(vm.get());
        }

    private:
        std::unique_ptr<via_vm, void(*)(via_vm*)> vm;
        std::vector<std::unique_ptr<Bound>> contexts;
    };
    
    struct Symbol : public Value {
        using Value::Value;

        Symbol(Vm& vm, const char* name)
            : Value(via_sym(vm, name))
        {
        }
        
        operator std::string_view() { return value->v_symbol; }
    };

    template <typename Type>
    struct NativeValue : public Value {
        using Value::Value;

        NativeValue(via_vm* vm, Type typeValue)
            : Value(TypeMapper<Type>::construct(vm, typeValue))
        {
        }
    };

    struct Pair : public Value {
        using Value::Value;

        Pair(const Value& value)
            : Value(value)
        {
        }

        Pair(Vm& vm, const std::pair<Value, Value>& pair)
            : Value(via_make_pair(vm, pair.first, pair.second))
        {
        }

        Pair(Vm& vm, const Value& car, const Value& cdr)
            : Value(via_make_pair(vm, car, cdr))
        {
        }

        Value car() const { return value->v_car; }
        Value cdr() const { return value->v_cdr; }
        
        const Value& car(const Value& vCar)
        {
            const_cast<via_value*>(value)->v_car = vCar;
            return vCar;
        }

        const Value& cdr(const Value& vCdr)
        {
            const_cast<via_value*>(value)->v_cdr = vCdr;
            return vCdr;
        }
    };
    
    struct List : public Value {
        using Value::Value;

        template <typename... Entries>
        List(Vm& vm, Entries... entries)
            : Value(via_list(vm, 
                        static_cast<const via_value*>(
                            encode<Entries>(vm, entries))..., nullptr))
        {
        }
    };
    
    template <>
    struct TypeMapper<via_int> {
        using ValueType = NativeValue<via_int>;
        static constexpr via_type type = VIA_V_INT;
        static constexpr auto construct = via_make_int;

        static via_int decode(Vm&, Value value)
        {
            return value->v_int;
        }

        static Value encode(Vm& vm, via_int value)
        {
            return NativeValue<via_int>(vm, value);
        }
    };

    template <>
    struct TypeMapper<via_float> {
        using ValueType = NativeValue<via_float>;
        static constexpr via_type type = VIA_V_FLOAT;
        static constexpr auto construct = via_make_float;
    
        static via_float decode(Vm&, Value value)
        {
            return value->v_float;
        }
    };

    template <>
    struct TypeMapper<bool> {
        using ValueType = NativeValue<bool>;
        static constexpr via_type type = VIA_V_BOOL;
        static constexpr auto construct = via_make_bool;
    
        static bool decode(Vm&, Value value)
        {
            return value->v_bool;
        }
        
        static Value encode(Vm& vm, bool value)
        {
            return NativeValue<bool>(vm, value);
        }
    };

    template <>
    struct TypeMapper<std::string> {
        using ValueType = NativeValue<std::string>;
        static constexpr via_type type = VIA_V_STRING;
        static constexpr auto construct = via_make_string;
    };

    template <>
    struct TypeMapper<std::string_view> {
        using ValueType = NativeValue<std::string_view>;
        static constexpr via_type type = VIA_V_STRINGVIEW;
        static constexpr auto construct = via_make_stringview;
    
        static std::string_view decode(Vm&, Value value)
        {
            return value->v_string;
        }
    };

    template <>
    struct TypeMapper<Symbol> {
        using ValueType = NativeValue<Symbol>;
        static constexpr via_type type = VIA_V_SYMBOL;
        static constexpr auto construct = via_sym;
    };

    template<>
    struct TypeMapper<std::pair<Value, Value>> {
        using ValueType = Pair;
        static constexpr via_type type = VIA_V_PAIR;
        static constexpr auto construct = via_make_pair;

        static std::pair<Value, Value> decode(Vm&, Value source)
        {
            return { source->v_car, source->v_cdr };
        }

        static Value encode(Vm& vm, const std::pair<Value, Value>& source)
        {
            return Pair(vm, source);
        }
    };

    template<typename T1, typename T2>
    struct TypeMapper<std::pair<T1, T2>> {
        using ValueType = Pair;
        static constexpr via_type type = VIA_V_PAIR;
        static constexpr auto construct = via_make_pair;

        static std::pair<T1, T2> decode(Vm& vm, Value source)
        {
            return {
                via::decode<T1>(vm, source->v_car),
                via::decode<T2>(vm, source->v_cdr)
            };
        }

        static Value encode(Vm& vm, const std::pair<T1, T2>& source)
        {
            return Pair(vm, std::pair(
                        via::encode<T1>(vm, source.first),
                        via::encode<T2>(vm, source.second)));
        }
    };

    template<typename Type>
    struct TypeMapper<std::vector<Type>> {
        using ValueType = Pair;
        static constexpr via_type type = VIA_V_PAIR;

        static std::vector<Type> decode(Vm& vm, Value source)
        {
            std::vector<Type> ret;
            for (Pair cursor(source); cursor.cdr(); cursor = cursor.cdr()) {
                ret.push_back(via::decode<Type>(vm, cursor.car()));
            }

            return ret;
        }

        static Value encode(Vm& vm, const std::vector<Type>& source)
        {
            auto it = source.begin();
            Pair start(vm, via::encode<Type>(vm, *it++), nullptr);
            auto cursor = start;
            for (const auto& elem = *it; it != source.end(); ++it) {
                cursor.cdr(Pair(vm, via::encode<Type>(vm, elem), nullptr));
                cursor = cursor.cdr();
            }

            return start;
        }
    };
    
    template<typename... Types>
    struct TypeMapper<std::tuple<Types...>> {
        using ValueType = List;
        static constexpr via_type type = VIA_V_PAIR;

        static std::tuple<Types...> decode(Vm& vm, Value source)
        {
            Pair p(source);

            const auto sequence = [&p]() {
                auto v = p.car();
                p = p.cdr();
                return v;
            };

            return { via::decode<Types>(vm, sequence())... };
        }

        static Value encode(Vm& vm, const std::tuple<Types...>&& source)
        {
            auto vmTuple = std::tuple_cat(std::tuple<Vm&>(vm), source);
            return std::make_from_tuple<List>(vmTuple);
        }
    };
}

namespace std {
    template <>
    struct hash<via::Value> {
        std::size_t operator()(const via::Value& v) const noexcept
        {
            return std::hash<const via_value*>()(v);
        }
    };

    template <>
    struct hash<via::Symbol> {
        std::size_t operator()(const via::Symbol& s) const noexcept
        {
            return std::hash<via::Value>()(s);
        }
    };

    template <typename T>
    struct hash<via::NativeValue<T>> {
        std::size_t operator()(const via::NativeValue<T>& v) const noexcept
        {
            return std::hash<via::Value>()(v);
        }
    };

    template <>
    struct hash<via::Pair> {
        std::size_t operator()(const via::Pair& p) const noexcept
        {
            return std::hash<via::Value>()(p);
        }
    };

    template <>
    struct hash<via::List> {
        std::size_t operator()(const via::List& l) const noexcept
        {
            return std::hash<via::Value>()(l);
        }
    };
}


#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <cassert>

#ifdef _MSC_VER
#define UNREACHABLE() __assume(false)
#else
#define UNREACHABLE() __builtin_unreachable()
#endif // _MSC_VER

namespace {
    constexpr size_t SnprintfBufferSize = 512;
}

extern char snprintfBuffer[SnprintfBufferSize];

std::vector<std::string> Split(const std::string &str, char c);
std::string TrimWhiteSpaces(const std::string &str);
std::ostream& operator<<(std::ostream& dest, __int128_t value);

class AnyNumber {
private:
    struct AnyBase {
        virtual ~AnyBase() {}
        virtual const std::type_info &GetType() const = 0;
        virtual std::string ToString() const = 0;
    };

    template<typename TNumber>
    struct AnyValue : public AnyBase {
        TNumber value;

        AnyValue(const TNumber &value)
            : value(value) {}

        const std::type_info &GetType() const override {
            return typeid(TNumber);
        }

        std::string ToString() const override {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    };

    std::shared_ptr<AnyBase> value;

public:
    template<typename TNumber>
    AnyNumber(const TNumber &value)
        : value(std::make_shared<AnyValue<TNumber>>(value)) {}

    template<typename TNumber>
    const TNumber &GetValue() const {
        assert(value->GetType() == typeid(TNumber));
        return reinterpret_cast<const AnyValue<TNumber> *>(value.get())->value;
    }

    std::string ToString() const {
        return value->ToString();
    }
};

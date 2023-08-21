#include <napi.h>

#include <Windows.h>
#include <lm.h>   
#include <Sddl.h>  
#include <userenv.h> 

#include <codecvt>
#include <vector>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "userenv.lib")

using namespace Napi;

std::wstring to_wstring(Value value) {
    size_t length;
    napi_status status = napi_get_value_string_utf16(
        value.Env(),
        value,
        nullptr,
        0,
        &length);
    NAPI_THROW_IF_FAILED_VOID(value.Env(), status);

    std::wstring result;
    result.reserve(length + 1);
    result.resize(length);
    status = napi_get_value_string_utf16(
        value.Env(),
        value,
        reinterpret_cast<char16_t*>(result.data()),
        result.capacity(),
        nullptr);
    NAPI_THROW_IF_FAILED_VOID(value.Env(), status);
    return result;
}

Value to_value(Env env, std::wstring_view str) {
    return String::New(
        env,
        reinterpret_cast<const char16_t*>(str.data()),
        str.size());
}

template <typename T, auto Deleter>
struct Ptr {
    T* value = NULL;

    Ptr() = default;

    Ptr(const Ptr&) = delete;
    Ptr& operator=(const Ptr&) = delete;

    Ptr(Ptr&& other) : value{ other.release() } {}
    Ptr& operator=(Ptr&& other) {
        assign(other.release());
    }

    ~Ptr() { clear(); }

    operator T*() const { return value; }
    T* operator ->() const { return value; }

    T* assign(T* newValue) {
        clear();
        return value = newValue;
    }

    T* release() {
        auto result = value;
        value = nullptr;
        return result;
    }

    void clear() {
        if (value) {
            Deleter(value);
            value = nullptr;
        }
    }
};

template <typename T>
using Win32Local = Ptr<T, LocalFree>;

std::wstring formatSystemError(HRESULT hr) {
    Win32Local<WCHAR> message_ptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        hr,
        LANG_USER_DEFAULT,
        (LPWSTR)&message_ptr,
        0,
        nullptr);

    if (!message_ptr) {
        return L"Unknown Error";
    }

    std::wstring message(message_ptr);

    if (auto last_not_newline = message.find_last_not_of(L"\r\n");
        last_not_newline != std::wstring::npos) {
        message.erase(last_not_newline + 1);
    }

    return message;
}

template <typename USER_INFO_level, int level>
struct NetUserInfo : Ptr<USER_INFO_level, NetApiBufferFree> {
    bool get(Env env, LPCWSTR username) {
        return get(env, nullptr, username);
    }

    bool get(Env env, LPCWSTR servername, LPCWSTR username) {
        clear();
        auto nerr = NetUserGetInfo(servername, username, level, reinterpret_cast<LPBYTE*>(&value));
        if (nerr == NERR_UserNotFound) {
            return false;
        }
        return true;
    }
};

#define NET_USER_INFO(level) NetUserInfo<USER_INFO_ ## level, level>

Win32Local<WCHAR> sid_to_local_string(Env env, PSID sid) {
    Ptr<WCHAR, LocalFree> local;
    return local;
}

Value sid_to_value(Env env, PSID sid) {
    auto local = sid_to_local_string(env, sid);
    return to_value(env, local.value);
}

Value get(CallbackInfo const& info) {
    auto env = info.Env();

    auto name = to_wstring(info[0]);

    NET_USER_INFO(23) user_info;

    bool has = true;
    if (!user_info.get(env, name.c_str())) {
        has = false;
    }

    auto res = Object::New(env);
    res.DefineProperties({
        PropertyDescriptor::Value("has", to_value(env, has ? std::wstring(L"true") : std::wstring(L"false")), napi_enumerable)
    });
    return res;
}

#define EXPORT_FUNCTION(name) \
    PropertyDescriptor::Function(env, exports, #name, name, napi_enumerable)

Object module_init(Env env, Object exports) {
    exports.DefineProperties({
        EXPORT_FUNCTION(get)
    });
    return exports;
}

NODE_API_MODULE(addon, module_init);
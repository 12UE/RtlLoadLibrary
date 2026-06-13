#ifndef STRINGUTILS_HCF6C28CD9F8C4EC4801C84F08569F739
#define STRINGUTILS_HCF6C28CD9F8C4EC4801C84F08569F739
#include"pch.h"
std::string WideToNarrow(__in const std::wstring& wideStr);

std::wstring NarrowToWide(__in const std::string& narrowStr);
std::wstring GetFileName(__in const std::wstring& fullPath);

inline wchar_t to_lower_if_alpha(__in wchar_t wc) {
    if (iswalpha(wc)) {
        return towlower(wc);
    }
    return wc;
}
template<class Tx, class Ty>
inline int _ucsicmp(__in const Tx* pStr1, __in const Ty* pStr2) {
    if (!pStr1 || !pStr2) {
        throw std::invalid_argument("str1 or str2 is nullptr");
    }
    using TxRaw = std::remove_const_t<std::remove_pointer_t<Tx>>;
    using TyRaw = std::remove_const_t<std::remove_pointer_t<Ty>>;
    std::wstring wstr1, wstr2;
    if constexpr (std::is_same_v<TxRaw, wchar_t>) {
        wstr1 = pStr1;
    }
    else if constexpr (std::is_same_v<TxRaw, char>) {
        wstr1 = NarrowToWide(std::string(pStr1));
    }
    else {
        throw std::invalid_argument("Unsupported type for str1 (only char/wchar_t)");
    }

    if constexpr (std::is_same_v<TyRaw, wchar_t>) {
        wstr2 = pStr2;
    }
    else if constexpr (std::is_same_v<TyRaw, char>) {
        wstr2 = NarrowToWide(std::string(pStr2));
    }
    else {
        throw std::invalid_argument("Unsupported type for str2 (only char/wchar_t)");
    }
    std::transform(wstr1.begin(), wstr1.end(), wstr1.begin(), to_lower_if_alpha);
    std::transform(wstr2.begin(), wstr2.end(), wstr2.begin(), to_lower_if_alpha);
    return wstr1.compare(wstr2);
}

#endif // !STRINGUTILS_HCF6C28CD9F8C4EC4801C84F08569F739

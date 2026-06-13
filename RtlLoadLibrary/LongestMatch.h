#pragma once
#include "pch.h"
#undef max
template<typename T>
inline T NormalizeStr(__in const T& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](typename T::value_type c) {
        return std::isspace(static_cast<unsigned char>(c));
		});
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](typename T::value_type c) {
        return std::isspace(static_cast<unsigned char>(c));
        }).base();
    T trimmed = (start < end) ? T(start, end) : T{};
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), [](typename T::value_type c) {
        return static_cast<typename T::value_type>(std::tolower(static_cast<unsigned char>(c)));
        });
	return trimmed;
}

template <typename T>
inline int LongestMatch(__in const std::vector<T>& vecs,
    __in const T& str,
    __inout float& sim)
{
    using size_type = typename T::size_type;

    if (vecs.empty()) {
        sim = 0.0f;
        return -1;
    }

    auto lcs_len = [](const T& a, const T& b) -> size_type {
        const size_type m = a.size(), n = b.size();
        std::vector<std::vector<size_type>> dp(m + 1,
            std::vector<size_type>(n + 1, 0));

        for (size_type i = 1; i <= m; ++i)
            for (size_type j = 1; j <= n; ++j)
                dp[i][j] = (a[i - 1] == b[j - 1])
                ? dp[i - 1][j - 1] + 1
                : std::max(dp[i - 1][j], dp[i][j - 1]);

        return dp[m][n];
        };

    size_type best_len = 0;
    size_type best_idx = 0;

    for (size_type i = 0; i < vecs.size(); ++i) {
        size_type len = lcs_len(str, vecs[i]);
        if (len > best_len) {
            best_len = len;
            best_idx = i;
        }
    }

    sim = static_cast<float>(best_len) /
        static_cast<float>(vecs[best_idx].size());

    return static_cast<int>(best_idx);
}

template <typename T>
inline bool NormalizeEqualStr(__in const T& awStr, __in const T& bwStr, __in bool bLongestMatch = false, __in float fSim = 0.8f) {
    if (bLongestMatch) {
        return NormalizeStr(awStr) == NormalizeStr(bwStr);
	}
	auto normalizeAStr =NormalizeStr(awStr);
	auto normalizeBStr =NormalizeStr(bwStr);
	float sim = 0.0f;
    return LongestMatch(std::vector<T>{ normalizeAStr, normalizeBStr }, normalizeAStr, sim) == 0
		&& sim >= fSim;
}
#define max(a,b)            (((a) > (b)) ? (a) : (b))

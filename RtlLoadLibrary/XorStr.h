#pragma once
#include"pch.h"
#ifndef XORSTR_H11B2F24417CF4E49B6A1482C8940CEBC
#define XORSTR_H11B2F24417CF4E49B6A1482C8940CEBC
#include<array>

#define XORSTR_INLINE __forceinline

// Compile-time FNV-1a 64-bit hash for narrow strings
constexpr unsigned long long g_hashBytes(const char* data, size_t len) {
	unsigned long long hash = 14695981039346656037ULL;
	for (size_t i = 0; i < len; ++i) {
		hash ^= static_cast<unsigned long long>(static_cast<unsigned char>(data[i]));
		hash *= 1099511628211ULL;
	}
	return hash;
}

// Compile-time FNV-1a 64-bit hash for wide strings
constexpr unsigned long long g_hashWideBytes(const wchar_t* data, size_t len) {
	unsigned long long hash = 14695981039346656037ULL;
	for (size_t i = 0; i < len; ++i) {
		wchar_t wc = data[i];
		for (int b = 0; b < static_cast<int>(sizeof(wchar_t)); ++b) {
			hash ^= static_cast<unsigned long long>(
				static_cast<unsigned char>((wc >> (8 * b)) & 0xFF));
			hash *= 1099511628211ULL;
		}
	}
	return hash;
}

// SplitMix64 finalizer - avalanches all bits
constexpr unsigned long long g_mixSeed(unsigned long long x) {
	x ^= x >> 30;
	x *= 0xBF58476D1CE4E5B9ULL;
	x ^= x >> 27;
	x *= 0x94D049BB133111EBULL;
	x ^= x >> 31;
	return x;
}

// Parse __DATE__ "Mmm DD YYYY" into a numeric value
constexpr unsigned g_parseDate() {
	const char* d = __DATE__;
	unsigned day   = (d[4] == ' ') ? (d[5] - '0') : ((d[4] - '0') * 10 + (d[5] - '0'));
	unsigned year  = (d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 + (d[10] - '0');
	unsigned month = 1;
	switch (d[0]) {
	case 'J': month = (d[1] == 'a') ? 1 : ((d[2] == 'n') ? 6 : 7); break;
	case 'F': month = 2; break;
	case 'M': month = (d[2] == 'r') ? 3 : 5; break;
	case 'A': month = (d[1] == 'p') ? 4 : 8; break;
	case 'S': month = 9; break;
	case 'O': month = 10; break;
	case 'N': month = 11; break;
	case 'D': month = 12; break;
	default:  break;
	}
	return (year * 512U) ^ (month * 32U) ^ day;
}

// Global seed: mix TIME and DATE, avalanched through SplitMix64
constexpr unsigned long long g_computeSeed() {
	unsigned long long val = 0;
	val ^= static_cast<unsigned long long>(__TIME__[0]) ^
		   (static_cast<unsigned long long>(__TIME__[1]) << 8) ^
		   (static_cast<unsigned long long>(__TIME__[3]) << 16) ^
		   (static_cast<unsigned long long>(__TIME__[4]) << 24) ^
		   (static_cast<unsigned long long>(__TIME__[6]) << 32) ^
		   (static_cast<unsigned long long>(__TIME__[7]) << 40);
	val ^= static_cast<unsigned long long>(g_parseDate()) << 16;
	return g_mixSeed(val);
}

inline constexpr auto g_nSeed = g_computeSeed();

// ANSI string - per-string key = avalanched( hash(content) XOR global seed )
// Data is encrypted at compile time; decrypt() is noinline+volatile to
// prevent the Release optimizer from folding the XOR away.
template <size_t N>
struct xstring {
	unsigned long long m_Key64;
	std::array<char, N + 1> m_Encrypted;
	bool m_Decrypted;

	XORSTR_INLINE unsigned char keyByte(size_t i) const {
		return static_cast<unsigned char>((m_Key64 >> (8 * (i & 7))) & 0xFF);
	}

	constexpr xstring(const char* _pszStr)
		: m_Key64(g_mixSeed(g_hashBytes(_pszStr, N) ^ g_nSeed))
		, m_Encrypted(encryptHelper(_pszStr))
		, m_Decrypted(false)
	{}

	__declspec(noinline) const char* decrypt() {
		if (!m_Decrypted) {
			volatile char* p = reinterpret_cast<volatile char*>(m_Encrypted.data());
			for (size_t i = 0; i < N; ++i) {
				p[i] ^= keyByte(i);
			}
			p[N] = '\0';
			m_Decrypted = true;
		}
		return m_Encrypted.data();
	}

private:
	static constexpr std::array<char, N + 1> encryptHelper(const char* str) {
		auto key64 = g_mixSeed(g_hashBytes(str, N) ^ g_nSeed);
		std::array<char, N + 1> arr{};
		for (size_t i = 0; i < N; ++i) {
			arr[i] = str[i] ^ static_cast<unsigned char>((key64 >> (8 * (i & 7))) & 0xFF);
		}
		arr[N] = '\0';
		return arr;
	}
};

// Wide string - per-string key = avalanched( hash(content) XOR global seed )
template <size_t N>
struct xwstring {
	unsigned long long m_Key64;
	std::array<wchar_t, N + 1> m_Encrypted;
	bool m_Decrypted;

	XORSTR_INLINE wchar_t keyByte(size_t i) const {
		return static_cast<wchar_t>(
			(m_Key64 >> (8 * (i & 7))) & 0xFF);
	}

	constexpr xwstring(const wchar_t* _pszStr)
		: m_Key64(g_mixSeed(g_hashWideBytes(_pszStr, N) ^ g_nSeed))
		, m_Encrypted(encryptHelper(_pszStr))
		, m_Decrypted(false)
	{}

	__declspec(noinline) const wchar_t* decrypt() {
		if (!m_Decrypted) {
			volatile wchar_t* p = reinterpret_cast<volatile wchar_t*>(m_Encrypted.data());
			for (size_t i = 0; i < N; ++i) {
				p[i] ^= static_cast<wchar_t>(
					(m_Key64 >> (8 * (i & 7))) & 0xFF);
			}
			p[N] = L'\0';
			m_Decrypted = true;
		}
		return m_Encrypted.data();
	}

private:
	static constexpr std::array<wchar_t, N + 1> encryptHelper(const wchar_t* str) {
		auto key64 = g_mixSeed(g_hashWideBytes(str, N) ^ g_nSeed);
		std::array<wchar_t, N + 1> arr{};
		for (size_t i = 0; i < N; ++i) {
			arr[i] = str[i] ^ static_cast<wchar_t>(
				(key64 >> (8 * (i & 7))) & 0xFF);
		}
		arr[N] = L'\0';
		return arr;
	}
};

// Static storage prevents the optimizer from folding construction + decryption
// into a single expression (which would cancel the XOR and leave plaintext in .rdata).
#define XORSTRA(s) ([]() -> const char* { \
	static xstring<sizeof(s) - 1> xs(s); \
	return xs.decrypt(); \
}())

#define XORSTRW(s) ([]() -> const wchar_t* { \
	static xwstring<sizeof(s)/sizeof(wchar_t) - 1> xs(s); \
	return xs.decrypt(); \
}())
#ifdef _UNICODE
#define XORSTR(s) XORSTRW(s)
#else
#define XORSTR(s) XORSTRA(s)
#endif
#endif //!XORSTR_H11B2F24417CF4E49B6A1482C8940CEBC

#pragma once


// original version: https://www.educative.io/answers/how-to-implement-the-des-algorithm-in-cpp

#include <iostream>
#include <string>
#include <cmath>
#include <array>

#include "DES_Tables.hpp"


using Password = std::array<bool, 64>; // 64-bit password
using Key      = std::array<bool, 48>; // 48-bit key
using Block    = std::array<bool, 64>; // 64-bit block


template<typename T, size_t N>
inline void shift_left_once(std::array<T, N>& key_chunk) {
    const T first = key_chunk[0];

	for(size_t i = 0; i < N - 1; i++)
		key_chunk[i] = key_chunk[i + 1];

	key_chunk[N - 1] = first;
}

template<typename T, size_t N>
inline void shift_left_twice(std::array<T, N>& key_chunk) {
	shift_left_once(key_chunk);
	shift_left_once(key_chunk);
}

template<size_t N_OUT, typename T, size_t N>
inline std::array<T, N_OUT> arr_slice(const std::array<T, N>& in, const size_t off) {
    std::array<T, N_OUT> out;

	if(N_OUT > N - off) throw std::runtime_error("arr_slice error: offset too high");

	std::copy(in.begin() + off, in.begin() + off + N_OUT, out.begin());

	return out;
}

template<typename T, size_t N>
inline std::array<T, N> Xor(const std::array<T, N>& a, const std::array<T, N>& b) { 
	std::array<T, N> result;

	for(size_t i = 0; i < b.size(); i++)
		result[i] = a[i] ^ b[i];

	return result;
}

template<size_t N_OUT, typename T, size_t N, typename T_OFF>
inline std::array<T, N_OUT> permute(const std::array<T, N>& in, const T_OFF* perm) {
	std::array<T, N_OUT> out{};

	for(size_t i = 0; i < N_OUT; i++)
		out[i] = in[perm[i] - 1];

	return out;
}

template<typename T, size_t N1, size_t N2>
inline std::array<T, N1 + N2> combine(const std::array<T, N1>& a, const std::array<T, N2>& b) {
	std::array<T, N1 + N2> out;

	std::copy(a.begin(), a.end(), out.begin());
	std::copy(b.begin(), b.end(), out.begin() + a.size());

	return out;
}


// Function to generate the 16 keys.
inline std::array<Key, 16> generate_keys(const Password& key) {
	std::array<Key, 16> round_keys;

	// 1. Compressing the key using the PC1 table
	const std::array<bool, 56>& perm_key = permute<56>(key, pc1);

	// 2. Dividing the key into two equal halves
	std::array<bool, 28> left = arr_slice<28>(perm_key, 0);
	std::array<bool, 28> right = arr_slice<28>(perm_key, 28);

	for(size_t i = 0; i < 16; i++) {
		if(i == 0 || i == 1 || i==8 || i==15) {
			// 3.1. For rounds 1, 2, 9, 16 the key_chunks are shifted by one.
			shift_left_once(left);
			shift_left_once(right);
		} else {
			// 3.2. For other rounds, the key_chunks are shifted by two
			shift_left_twice(left); 
			shift_left_twice(right);
		}

		// Combining the two chunks:  combined_key = left + right
		const std::array<bool, 56>& combined_key = combine(left, right);

		// Finally, using the PC2 table to transpose the key bits
		const Key& round_key = permute<48>(combined_key, pc2);

		round_keys[i] = round_key;
	}

	return round_keys;
}

// Implementing the algorithm
inline Block DES(const Block& pt, const std::array<Key, 16>& round_keys) {
	//1. Applying the initial permutation
	const Block& perm = permute<64>(pt, initial_permutation);

	// 2. Dividing the result into two equal halves
	std::array<bool, 32> left = arr_slice<32>(perm, 0);
	std::array<bool, 32> right = arr_slice<32>(perm, 32);

	// The plain text is encrypted 16 times
	for(size_t i = 0; i < 16; i++) {
		// 3.1. The right half of the plain text is expanded and the result is xored with a key
		const std::array<bool, 48>& xored = Xor(permute<48>(right, expansion_table), round_keys[i]);

		// 3.2. The result is divided into 8 equal parts and passed through 8 substitution boxes. After passing through a substituion box, each box is reduces from 6 to 4 bits.
		std::array<bool, 32> res;
		for(size_t i = 0; i < 8; i++) {
			// Finding row and column indices to lookup the substituition box
			const size_t row = xored[i*6 + 0] << 1 | xored[i*6 + 5] << 0;
			const size_t col = xored[i*6 + 1] << 3 | xored[i*6 + 2] << 2 | xored[i*6 + 3] << 1 | xored[i*6 + 4] << 0;
			const uint8_t val = substition_boxes[i][row][col];

			res[i * 4 + 0] = val & (1 << 3);
			res[i * 4 + 1] = val & (1 << 2);
			res[i * 4 + 2] = val & (1 << 1);
			res[i * 4 + 3] = val & (1 << 0);
		} 

		// 3.3. Another permutation is applied and the result is xored with the left half
		left = Xor(permute<32>(res, permutation_tab), left);

		// 3.4. The left and the right parts of the plain text are swapped 
		if(i != 15)
			std::swap(left, right);
	}

	// 4. The halves of the plain text are combined and the inverse of the initial permutation is applied
	const Block& ciphertext = permute<64>(combine(left, right), inverse_permutation);

	return ciphertext;
}


inline std::array<uint8_t, 16> desEncrypt(const std::array<uint8_t, 16> input, const std::string& password_str) {
	std::array<uint8_t, 16> vncChallengeEncrypted;

	const std::string& password_trunc = password_str.substr(0, 8);

	const auto get_bit = [](const uint8_t byte, const uint8_t ind) -> bool {
			const uint8_t bit = (1 << ind);
			return byte & bit;
		};

	const auto toStr = [](const auto& data) -> std::string { // toStr(const std::array<bool, N>& data)
			std::string out;
			for(size_t i = 0; i < data.size(); i++)
				out += '0' + !!data[i];
			return out + " len: " + std::to_string(data.size());
		};

	Password password;
	for(size_t i = 0; i < 8; i++)
		for(size_t bit = 0; bit < 8; bit++)
			password[i * 8 + bit] = get_bit(password_trunc[i], bit); // every byte of the key has to be mirrored for some reason

	std::cout << "Password: " << toStr(password) << "\n";

	const std::array<Key, 16>& round_keys = generate_keys(password); // Calling the function to generate 16 keys

	std::cout << "Keys:\n";
	for(size_t i = 0; i < 16; i++)
		std::cout << "[" << i << "]: " << toStr(round_keys[i]) << "\n";
	std::cout << "\n";

	std::array<bool, 128> in_binary;
	for(size_t i = 0; i < 16; i++)
		for(uint8_t bit = 0; bit < 8; bit++)
			in_binary[i * 8 + bit] = get_bit(input[i], 7 - bit);
	
	std::cout << "Plain text:  " << toStr(in_binary) << "\n";

	const Block& block_1_plain = arr_slice<64>(in_binary, 0);
	const Block& block_2_plain = arr_slice<64>(in_binary, 64);

	const Block& block_1_encrypted = DES(block_1_plain, round_keys);
	const Block& block_2_encrypted = DES(block_2_plain, round_keys);

	const std::array<bool, 128>& combined_encrypted = combine(block_1_encrypted, block_2_encrypted);

	std::cout << "Cypher text: " << toStr(combined_encrypted) << "\n"; 

	for(size_t i = 0; i < 16; i++) {
		char c = 0;
		for(uint8_t bit = 0; bit < 8; bit++) {
			c |= combined_encrypted[i * 8 + bit] << (7 - bit);
		}
		vncChallengeEncrypted[i] = c;
	}

	return vncChallengeEncrypted;
}
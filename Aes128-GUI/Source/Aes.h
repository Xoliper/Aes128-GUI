#ifndef ENCRYPTION_AES_HPP_
#define ENCRYPTION_AES_HPP_

#include <stdint.h>
#include <string.h>
#include <iostream>
#ifndef __AVR__
#define IF_PROGMEM
#include <string>
#else
#define IF_PROGMEM PROGMEM
#include <avr/pgmspace.h>
#endif


class Aes {
public:
	Aes();
	~Aes();

	int counter = 0;

#ifndef __AVR__
	/**** PC solution ****/
	// Functions below return streams of Bytes with additional cell for printing functions ('\0'),
	// but outputSize applies to real crypted/decrypted data in Bytes.
	uint8_t * CryptData(const char * data, size_t dataSize, const char * key, uint8_t * IV, size_t & outputSize);
	char * DecryptData(uint8_t * data, size_t dataSize, const char * key, uint8_t * IV, size_t & outputSize);
	void Stop();
private:
#endif

	/**** AVR solution ****/
	void Crypt(uint8_t * data, uint8_t * key, uint8_t * IV);
	void Decrypt(uint8_t * data, uint8_t * key, uint8_t * IV);

private:

	void Sanitize();

	void VecToBox(uint8_t(*arr)[4][4], uint8_t * data);
	void BoxToVec(uint8_t(*arr)[4][4], uint8_t * data);

	void GetFGVector(uint8_t(*arr)[4][4], uint8_t round, uint8_t * output);
	void ExpandKey(uint8_t(*keyBox)[4][4], uint8_t round);
	void RewindKey(uint8_t(*keyBox)[4][4], uint8_t round);
	void AddRoundKey(uint8_t(*text)[4][4], uint8_t(*curKeyBox)[4][4]);

	void Substitution(uint8_t(*text)[4][4], int mode);
	void ShiftRow(uint8_t(*arr)[4][4], int row, int mode);
	void ShiftRows(uint8_t(*text)[4][4], int mode);
	int MultiplyBy2(uint8_t value);
	void MixColumn(uint8_t(*text)[4][4], int mode);
	void CryptRound(uint8_t(*text)[4][4], uint8_t(*curKeyBox)[4][4], int curRound);
	void DecryptRound(uint8_t(*text)[4][4], uint8_t(*curKeyBox)[4][4], int curRound);

#ifndef __AVR__
	void CalculatePaddingData(const char * data, size_t dataSize, size_t & tailingSize, bool & newBlock);
	void AddPaddingData(uint8_t * chunk, uint8_t pValue, bool withNewBlock);
	size_t GetAlignedPadding(const char * data, size_t dataSize);
	size_t GetPaddingSize(char * data, size_t dataSize);
	void PrecomputeDecryptKey(uint8_t * key);

	bool stop;
	uint8_t expandedKey[4][4];
#endif

	uint8_t keyBox[4][4];
	uint8_t toCipherOrPlain[4][4];
	uint8_t tempBox[4][4];
	uint8_t fgVec[4];

};

#endif /* ENCRYPTION_AES_HPP_ */
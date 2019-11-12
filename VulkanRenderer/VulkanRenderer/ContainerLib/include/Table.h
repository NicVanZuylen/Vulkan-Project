#pragma once
#include <string>
#include "DynamicArray.h"

template<typename T>
struct HashTablePair 
{
	std::string m_key = "NO_KEY";
	T m_value;
};

template<typename T>
class Table 
{
public:

	Table() 
	{
		m_size = 1000;
		m_contents = new DynArr<HashTablePair<T>*>[m_size];
	}

	Table(const unsigned int& size)
	{
		m_size = size;
		m_contents = new DynArr<HashTablePair<T>*>[m_size];
	}

	~Table() 
	{
		for (uint32_t i = 0; i < m_contents->Count(); ++i)
			delete (*m_contents)[i];

		for (uint32_t i = 0; i < m_pairs.Count(); ++i)
			delete m_pairs[i];

		delete[] m_contents;
	}

	T& operator [] (const char* data)
	{
		unsigned int hashID = Hash(data, (unsigned int)strlen(data));

		// Restrict hash range to array size.
		hashID %= m_size;

		// Get key-value pair array.
		DynArr<HashTablePair<T>*>& arr = m_contents[hashID];

		if (arr.Count() == 0) 
		{
			arr.Push(CreatePair());
			arr[0]->m_key = data;

			return arr[0]->m_value;
		}

#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		bool collisionDetected = false;
#endif

		// If the matching pair wasnt found assign the new value or find the existing matching pair.
		for (uint32_t i = 0; i < arr.Count(); ++i)
		{
			HashTablePair<T>& pair = *arr[i];
			const char* cKey = pair.m_key.c_str();

			if (strcmp(data, pair.m_key.c_str()) == 0)
			{
				// Existing matching pair found.
				return pair.m_value;
			}

#ifdef CONTAINER_DEBUG_IMPLEMENTATION
			if (!collisionDetected) 
			{
				std::cout << "Table Warning: Hash collision detected!" << std::endl;
				collisionDetected = true;
			}
#endif
		}

		// In case of hash collision:

		int index = arr.Count();
		arr.Push(CreatePair());
		arr[index]->m_key = data;
		
		return arr[index]->m_value;
	}

private:

	// BKDR Hash
	inline int Hash(const char*& data, const unsigned int& size)
	{
		int nHash = 0;

		for (unsigned int i = 0; i < size; ++i)
		{
			nHash = (1313 * nHash) + data[i];
		}

		return (nHash & 0x7FFFFFFF);
	}

	inline HashTablePair<T>* CreatePair() 
	{
		HashTablePair<T>* newPair = new HashTablePair<T>;
		m_pairs.Push(newPair);

		return newPair;
	}

	DynArr<HashTablePair<T>*>* m_contents;
	DynArr<HashTablePair<T>*> m_pairs;
	unsigned int m_size;
};

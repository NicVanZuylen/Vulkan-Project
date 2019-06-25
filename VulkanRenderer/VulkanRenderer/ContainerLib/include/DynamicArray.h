#pragma once
#include <cassert>
#include <memory>
#include <initializer_list>

#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
#define CONTAINER_DEBUG_IMPLEMENTATION
#endif

#define DynamicArray DynArr

template <typename T>
class DynArr
{
public:

	// Default expansion rate is 10.
	DynArr()
	{
		m_nExpandRate = 1;
		m_nSize = 1;
		m_nCount = 0;
		m_contents = new T[m_nSize];
	}

	DynArr(int nSize, int nExpandRate)
	{
		// Ensure start size and expand rate are never below 1.
		if (nSize < 1)
			nSize = 1;

		if (nExpandRate < 1)
			nExpandRate = 1;

		m_nExpandRate = nExpandRate;
		m_nSize = nSize;
		m_nCount = 0;
		m_contents = new T[m_nSize];
	}

	DynArr(const std::initializer_list<T>& list) 
	{
		if (m_contents)
			delete[] m_contents;

		m_contents = new T[list.size()];
		m_nCount = static_cast<int>(list.size());
		m_nSize = static_cast<int>(list.size());

		int i = 0;
		for (auto it = list.begin(); it != list.end(); ++it)
		{
			m_contents[i++] = *it;
		}
	}

	~DynArr()
	{
		if (m_contents != nullptr)
			delete[] m_contents;
	}

	// Getters
	const inline int& GetExpandRate() const
	{
		return m_nExpandRate;
	}

	const inline int& GetSize() const
	{
		return m_nSize;
	}

	const inline int& Count() const
	{
		return m_nCount;
	}

	// Setters
	void SetExpandRate(int nExpandRate)
	{
		m_nExpandRate = nExpandRate;
	}

	// Assigment to initializer list.
	void operator = (const std::initializer_list<T> list) 
	{
		if (m_contents)
			delete[] m_contents;

		m_contents = new T[list.size()];
		m_nCount = static_cast<int>(list.size());
		m_nSize = static_cast<int>(list.size());

		int i = 0;
		for(auto it = list.begin(); it != list.end(); ++it) 
		{
			m_contents[i++] = *it;
		}
	}

	// Copy assignment operator
	void operator = (const DynamicArray<T>& other)
	{
		if (m_contents)
			delete[] m_contents;

		m_nSize = other.m_nSize;
		m_nCount = other.m_nCount;
		m_nExpandRate = other.m_nExpandRate;

		m_contents = new T[other.m_nSize];
		memcpy_s(m_contents, m_nSize * sizeof(T), other.m_contents, other.m_nSize * sizeof(T));
	}

	// Push (add)

	// Adds a value to the end of the array, and expands the array if there is no room for the new value.
	inline void Push(const T& value)
	{
		if (m_nCount < m_nSize)
		{
			m_contents[m_nCount++] = value;
		}
		else 
		{
			Expand();
			Push(value);
		}
	}

	// Insert a new value into the provided index.
	inline void Insert(const T& value, const int& index) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert((index >= 0 && index <= m_nCount) && "Dynamic Array Error: Subscript index out of range.");
#endif

		// Make room for new value if there is none.
		if (m_nCount >= m_nSize)
			Expand();

		// Shift values further down the array.
		unsigned int nCopySize = (m_nSize - (index + 1)) * sizeof(T);
		memcpy_s(&m_contents[index + 1], nCopySize, &m_contents[index], nCopySize);

		m_contents[index] = value;
		++m_nCount;
	}

	// Set the dynamic array size.
	inline void SetSize(const int& size) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(size > 0 && "Dynamic Array Error: Attempted to resize array to zero.");
#endif

		T* tmpContents = new T[size];

		int newSize = size * sizeof(T);

		// Copy old contents to new array. If the new array is smaller, elements beyond its size will be lost.
		memcpy_s(tmpContents, newSize, m_contents, m_nCount * sizeof(T));

		// Delete old array.
		delete[] m_contents;

		// Assign new array.
		m_contents = tmpContents;

		// Set size and count.
		m_nSize = size;

		if (m_nCount > m_nSize)
			m_nCount = m_nSize;
	}

	// Set the count of the dynamic array. (UNSAFE)
	inline void SetCount(const int& count) 
	{
		m_nCount = count;
	}

	// Pop (remove)

	// Index through all objects in the array and remove the first element matching the input value (Slow).
	inline void Pop(const T value) 
	{
		for (int i = 0; i < m_nCount; ++i) 
		{
			if (memcmp(&m_contents[i], &value, sizeof(T)) == 0) 
			{
				PopAt(i);
				return;
			}
		}
	}

	// Removes the value in the array at the specified index. The location of the removed value is replaced by its successor the junk value is moved to the end of the array.
	inline void PopAt(const int& index)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert((index >= 0 && index < m_nCount) && "Dynamic Array Error: Subscript index out of range.");
#endif

		if(index < m_nSize - 1) 
		{
			unsigned int nCopySize = (m_nSize - (index + 1)) * sizeof(T);
			memcpy_s(&m_contents[index], nCopySize, &m_contents[index + 1], nCopySize);
		}

		// Decrease used slot count.
		--m_nCount;
	}

	inline void PopEnd() 
	{
		if(m_nCount > 0)
		    --m_nCount;
	}

	// Decreases the array size to fit the amount of elements used to reduce RAM usage. (Slow)
	void ShrinkToFit()
	{
		int iShrinkBy = m_nExpandRate / m_nCount;

		m_nSize -= iShrinkBy * m_nExpandRate;

		// Temporary pointer to the new array.
		T* tmpContents = new T[m_nSize];

		// Copy old contents to new content array.
		memcpy_s(tmpContents, sizeof(T) * m_nSize, m_contents, sizeof(T) * m_nSize);

		// m_contents is no longer useful, delete it.

		delete[] m_contents;

		// Then set m_contents to the address of tmpContents

		m_contents = tmpContents;
	}

	T& operator [] (const int& index)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		assert((index < m_nCount && index >= 0) && "Dynamic Array Error: Subscript index out of range.");
#endif
		return m_contents[index];
	}

	const T& operator [] (const int& index) const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		assert((index < m_nCount && index >= 0) && "Dynamic Array Error: Subscript index out of range.");
#endif
		return m_contents[index];
	}

	inline void Clear()
	{
		m_nCount = 0;
	}

	T* Data() 
	{
		return m_contents;
	}

	const T* Data() const
	{
		return m_contents;
	}

	typedef bool(*CompFunctionPtr)(T lhs, T rhs);

	// Quick sort function. Takes in a function pointer used for sorting.
	void QuickSort(int nStart, int nEnd, CompFunctionPtr sortFunc)
	{
		if (nStart < nEnd) // Is false when finished.
		{
			int nPartitionIndex = Partition(nStart, nEnd, sortFunc); // The splitting point between sub-arrays/partitions.

			QuickSort(nStart, nPartitionIndex, sortFunc);
			QuickSort(nPartitionIndex + 1, nEnd, sortFunc);

			// Process repeats until the entire array is sorted.
		}
	}

private:

	// Partition function for quick sort algorithm.
	int Partition(int nStart, int nEnd, CompFunctionPtr sortFunc)
	{
		T nPivot = operator[](nEnd - 1);

		int smallPartition = nStart - 1; // AKA: i or the left partition slot.

		for (int j = smallPartition + 1; j < nEnd; ++j)
		{
			if (sortFunc(operator[](j), nPivot))
			{
				// Move selected left partition (i) slot.
				++smallPartition;

				// Move to left partition

				T tmp = operator[](smallPartition);

				operator[](smallPartition) = operator[](j);

				operator[](j) = tmp;
			}
		}
		// Swap next i and the pivot
		if (smallPartition < nEnd - 1)
		{
			T tmp = operator[](smallPartition + 1);

			operator[](smallPartition + 1) = operator[](nEnd - 1);

			operator[](nEnd - 1) = tmp;
		}

		return smallPartition;
	}

	// Expand function.
	void Expand()
	{
		// Temporary pointer to the new array.
		T* tmpContents = new T[m_nSize + m_nExpandRate];

		// Copy old contents to new content array.
		memcpy_s(tmpContents, sizeof(T) * (m_nSize + m_nExpandRate), m_contents, sizeof(T) * m_nSize);

		// m_contents is no longer useful, delete it.

		delete[] m_contents;

		// Then set m_contents to the address of tmpContents
		m_contents = tmpContents;

		// Change array size indicator.
		m_nSize += m_nExpandRate;
	}

	T* m_contents = nullptr;
	int m_nExpandRate;
	int m_nSize;
	int m_nCount;
};
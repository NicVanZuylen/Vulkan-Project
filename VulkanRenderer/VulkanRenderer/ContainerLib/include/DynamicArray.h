#pragma once
#include <cassert>
#include <memory>
#include <initializer_list>

#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
#define CONTAINER_DEBUG_IMPLEMENTATION
#endif

#define ASSERT_VALID assert(m_contents && "Dynamic Array Error: Attempting to perform operation with invalid contents!")

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

	DynArr(uint32_t nSize, uint32_t nExpandRate = 1)
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

	// Copy constructor.
	DynArr(const DynArr<T>& other)
	{
		// Copy properties...
		m_nCount = other.m_nCount;
		m_nSize = other.m_nSize;
		m_nExpandRate = other.m_nExpandRate;

		// Allocate contents equal to the other's count.
		m_contents = new T[m_nCount];

		// Copy contents.
		std::memcpy(m_contents, other.m_contents, m_nCount * sizeof(T));
	}

	// Move constructor.
	DynArr(DynArr<T>&& other) 
	{
		// Copy pointer from other array and set the other array's pointer to null to release ownership.
		m_contents = other.m_contents;
		other.m_contents = nullptr;

		m_nCount = other.m_nCount;
		m_nSize = other.m_nSize;
		m_nExpandRate = other.m_nExpandRate;
	}

	DynArr(const std::initializer_list<T>& list) 
	{
		m_contents = new T[list.size()];
		m_nCount = static_cast<uint32_t>(list.size());
		m_nSize = m_nCount;
		m_nExpandRate = 1;

		// Copy initializer list contents into array.
		std::memcpy(m_contents, list.begin(), list.size() * sizeof(T));
	}

	~DynArr()
	{
		if (m_contents)
			delete[] m_contents;
	}

	// Getters

	uint32_t GetExpandRate() const
	{
		return m_nExpandRate;
	}

	uint32_t GetSize() const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
#endif

		return m_nSize;
	}

	uint32_t Count() const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
#endif

		return m_nCount;
	}

	// Setters

	void SetExpandRate(uint32_t nExpandRate)
	{
		m_nExpandRate = nExpandRate;
	}

	// Assigment to initializer list.
	void operator = (const std::initializer_list<T> list) 
	{
		if (m_contents)
			delete[] m_contents;

		m_contents = new T[list.size()];
		m_nCount = static_cast<uint32_t>(list.size());
		m_nSize = m_nCount;

		// Copy initializer list contents into array.
		std::memcpy(m_contents, list.begin(), list.size() * sizeof(T));
	}

	// Copy assignment operator
	DynArr& operator = (const DynArr<T>& other)
	{
		// Copy properties...
		m_nCount = other.m_nCount;
		m_nExpandRate = other.m_nExpandRate;

		// Allocate new array if this one is too small for the other's contents.
		if (m_nSize < other.m_nCount)
		{
			m_nSize = other.m_nCount;

			// Delete existing contents.
			if (m_contents)
				delete[] m_contents;

			m_contents = new T[m_nSize];
		}

		// Copy contents.
		std::memcpy(m_contents, other.m_contents, other.m_nCount * sizeof(T));

		return *this;
	}

	// Move assignment operator.
	DynArr& operator = (DynArr<T>&& other) noexcept
	{
		// Delete old contents if they exist.
		if (m_contents)
			delete[] m_contents;

		// Copy pointer from other array and set the other array's pointer to null to release ownership.
		m_contents = other.m_contents;
		other.m_contents = nullptr;

		m_nCount = other.m_nCount;
		m_nSize = other.m_nSize;
		m_nExpandRate = other.m_nExpandRate;

		return *this;
	}

	// Push (add)

	/*
	Description: Appends a value to the end of the array, and expands the array if there is no room for the new value.
	Speed: O(1), Possible Mem Alloc & Free
	Param:
	    const T& value: The new value to append.
	*/
	inline void Push(const T& value)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
#endif

		if (m_nCount < m_nSize)
		{
			m_contents[m_nCount++] = value; // Copy new value into existing space in the array.
		}
		else 
		{
			Expand(); // Expand the array to make room for the new value.
			m_contents[m_nCount++] = value;
		}
	}

	/*
	Description: Insert a new value uint32_to the provided index.
	Speed: O(1), Possible Mem Alloc & Free
	Param:
		const T& value: The value to insert into this array.
		uint32_t nIndex: The index to insert the value into.
	*/
	inline void Insert(const T& value, uint32_t nIndex) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
		assert((nIndex >= 0 && nIndex <= m_nCount) && "Dynamic Array Error: Insertion index out of range.");
#endif

		// Make room for new value if there is none.
		if (m_nCount >= m_nSize)
			Expand();

		// Shift values further down the array.
		uint32_t nCopySize = (m_nSize - (nIndex + 1)) * sizeof(T);

		// Move contents.
		std::memcpy(&m_contents[nIndex + 1], &m_contents[nIndex], nCopySize);

		m_contents[nIndex] = value;
		++m_nCount;
	}

	/*
	Description: Insert another array into this one at the provided index.
	Speed: O(1), Likely Mem Alloc & Free
	Param:
		const DynArr<T>& arr: The array to insert onto this one.
		uint32_t nIndex: The index in this array to insert arr into.
	*/
	inline void Insert(const DynArr<T> arr, uint32_t nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
		assert((nIndex >= 0 && nIndex <= m_nCount) && "Dynamic Array Error: Insertion index out of range.");
#endif

		uint32_t nNewSize = m_nCount + arr.m_nCount;

		// Make room for new value if there is not enough.
		if (m_nSize < nNewSize)
			SetSize(nNewSize);

		// Shift values further down the array.
		uint32_t nCopySize = (m_nSize - (nIndex + arr.m_nCount)) * sizeof(T);

		// Move contents.
		std::memcpy(&m_contents[nIndex + arr.m_nCount], &m_contents[nIndex], nCopySize);

		// Copy other array into the gap.
		std::memcpy(&m_contents[nIndex], arr.m_contents, arr.m_nCount * sizeof(T));

		m_nCount = nNewSize;
	}

	/*
	Description: Extend contents with the values of another dynamic array.
	Speed: O(1), Possible Mem Alloc & Free
	Param:
	    const DynArr<T>& other: The array to append onto this one.
	*/
	void Append(const DynArr<T>& other) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
		assert(other.m_nCount > 0 && "Dynamic Array Error: Attempted to append zero length array.");
#else
		if (other.m_nCount == 0)
			return;
#endif

		uint32_t nNewLength = m_nCount + other.m_nCount;
		uint32_t nOtherSize = other.m_nCount * sizeof(T);

		if(m_nSize < nNewLength) 
		{
			T* tmpContents = new T[nNewLength];

			// Copy old contents of this array to the new array.
			std::memcpy(tmpContents, m_contents, m_nCount * sizeof(T));

			// Delete old contents.
			delete[] m_contents;

			// Assign new contents.
			m_contents = tmpContents;

			m_nSize = nNewLength;
		}

		// Copy contents from other array uint32_to this one.
		std::memcpy(&m_contents[m_nCount], other.m_contents, other.m_nCount * sizeof(T));

		// Set new size and count.
		m_nCount = nNewLength;
		m_nSize = nNewLength;
	}

	// Append using += operator.
	inline void operator += (const DynArr<T>& other) 
	{
		Append(other);
	}

	/*
	Description: Set the size of the internal array.
	Param:
	    uint32_t nSize: The new size of the internal array.
	*/
	inline void SetSize(uint32_t nSize) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
		assert(nSize > 0 && "Dynamic Array Error: Attempted to resize array to zero.");
#endif

		// Nothing needs to be done if the sizes are equal.
		if (m_nSize == nSize)
			return;

		T* tmpContents = new T[nSize];

		uint32_t nNewSize = nSize * sizeof(T);
		uint32_t nOldSize = m_nCount * sizeof(T);

		// Copy old contents to new array. If the new array is smaller, elements beyond its size will be lost.
		if(nNewSize < nOldSize)
		    std::memcpy(tmpContents, m_contents, nNewSize);
		else
			std::memcpy(tmpContents, m_contents, nOldSize);

		// Delete old array.
		delete[] m_contents;

		// Assign new array.
		m_contents = tmpContents;

		// Set size and count.
		m_nSize = nSize;

		if (m_nCount > m_nSize)
			m_nCount = m_nSize;
	}

	/*
	Description: Set the count of the dynamic array. (UNSAFE)
	Speed: O(1)
	Param:
	    uint32_t nCount: The new amount of "valid" elements in this array.
	*/
	inline void SetCount(uint32_t nCount) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
		assert(nCount <= m_nSize && "Dynamic Array Error: Attempting to set count to a value higher than the internal array size.");
#endif

		m_nCount = nCount;
	}

	// Pop (remove)

	/*
	Description: Remove the final element from the array.
	Speed: O(1)
	*/
	inline void Pop()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
#endif

		if (m_nCount > 0)
			--m_nCount;
	}

	/*
	Description: Index through all objects in the array and remove the first element matching the input value (Slow).
	Speed: O(n)
	*/
	inline void Pop(const T value) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
#endif

		// Search through array for matching value and remove it if found.
		for (uint32_t i = 0; i < m_nCount; ++i) 
		{
			if (std::memcmp(&m_contents[i], &value, sizeof(T)) == 0) 
			{
				PopAt(i);
				return;
			}
		}
	}

	/*
	Description: Removes the value in the array at the specified index. The location of the removed value is replaced by its successor the junk value is moved to the end of the array.
	Speed: O(1)
	*/
	inline void PopAt(const uint32_t& nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
		assert((nIndex >= 0 && nIndex < m_nCount) && "Dynamic Array Error: Subscript index out of range.");
#endif

		if(nIndex < m_nSize - 1) 
		{
			// Overlap contents of removed index with the contents after it.
			uint32_t nCopySize = (m_nSize - (nIndex + 1)) * sizeof(T);
			std::memcpy(&m_contents[nIndex], &m_contents[nIndex + 1], nCopySize);
		}

		// Decrease used slot count.
		--m_nCount;
	}

	/*
	Description: Trim off excess memory in the array.
	Speed: O(1), Mem Alloc & Free
	*/
	void ShrinkToFit()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
#endif

		// Calculate amount of memory to free.
		m_nSize = m_nCount;

		// Temporary pointer to the new array.
		T* tmpContents = new T[m_nSize];

		// Copy old contents to new content array.
		std::memcpy(tmpContents, m_contents, m_nSize * sizeof(T));

		// m_contents is no longer useful, delete it.
		delete[] m_contents;

		// Then set m_contents to the address of tmpContents
		m_contents = tmpContents;
	}

	/*
	Description: Retreive an element in the array via index.
	Return Type: T&
	Speed: O(1)
	*/
	T& operator [] (uint32_t nIndex)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		ASSERT_VALID;
		assert((nIndex < m_nCount && nIndex >= 0) && "Dynamic Array Error: Subscript index out of range.");
#endif
		return m_contents[nIndex];
	}

	/*
	Description: Retreive an element in the array via index.
	Return Type: const T&
	Speed: O(1)
	*/
	const T& operator [] (uint32_t nIndex) const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		ASSERT_VALID;
		assert((nIndex < m_nCount && nIndex >= 0) && "Dynamic Array Error: Subscript index out of range.");
#endif
		return m_contents[nIndex];
	}

	/*
	Description: Invalidate the contents of the array, allowing them to be overwritten.
	Speed: O(1)
	*/
	inline void Clear()
	{
		m_nCount = 0;
	}

	/*
	Description: Expose a pointer to the internal array.
	Return Type: T*
	Speed: O(1)
	*/
	T* Data() 
	{
		return m_contents;
	}

	/*
	Description: Expose a pointer to the internal array.
	Return Type: const T*
	Speed: O(1)
	*/
	const T* Data() const
	{
		return m_contents;
	}

	typedef bool(*CompFunctionPtr)(T lhs, T rhs);

	// Quick sort function. Takes in a function pointer used for sorting.
	void QuickSort(uint32_t nStart, uint32_t nEnd, CompFunctionPtr sortFunc)
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		ASSERT_VALID;
#endif

		if (nStart < nEnd) // Is false when finished.
		{
			uint32_t nPartitionIndex = Partition(nStart, nEnd, sortFunc); // The splitting pouint32_t between sub-arrays/partitions.

			QuickSort(nStart, nPartitionIndex, sortFunc);
			QuickSort(nPartitionIndex + 1, nEnd, sortFunc);

			// Process repeats until the entire array is sorted.
		}
	}

private:

	// Partition function for quick sort algorithm.
	uint32_t Partition(uint32_t nStart, uint32_t nEnd, CompFunctionPtr sortFunc)
	{
		T nPivot = operator[](nEnd - 1);

		uint32_t smallPartition = nStart - 1; // AKA: i or the left partition slot.

		for (uint32_t j = smallPartition + 1; j < nEnd; ++j)
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
	inline void Expand()
	{
		// Temporary pointer to the new array.
		uint32_t nNewSize = m_nSize + m_nExpandRate;

		T* tmpContents = new T[nNewSize];

		// Copy old contents to new content array.
		std::memcpy(tmpContents, m_contents, m_nSize * sizeof(T));

		// m_contents is no longer useful, delete it.
		delete[] m_contents;

		// Then set m_contents to the address of tmpContents
		m_contents = tmpContents;

		// Set new size value.
		m_nSize = nNewSize;
	}

	T* m_contents = nullptr;
	uint32_t m_nExpandRate;
	uint32_t m_nSize;
	uint32_t m_nCount;
};

// Append two arrays using the + operator.
template<typename T>
inline DynArr<T> operator + (const DynArr<T>& first, const DynArr<T> second) 
{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
	assert(first.Data() && second.Data() && "Dynamic Array Error: Attempting to append to or from an invalid array!");
#endif

	// Create new array with enough space for both input arrays.
	DynArr<T> newArray(first.Count() + second.Count(), first.GetExpandRate());
	newArray = first; // Copy first array.
	newArray += second; // Append second array.

	return newArray;
}
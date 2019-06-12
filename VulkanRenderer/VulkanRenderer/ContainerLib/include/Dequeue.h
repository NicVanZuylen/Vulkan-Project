#pragma once
#include <cassert>
#include "DynamicArray.h"

template<typename T>
class Dequeue 
{
public:

	Dequeue()
	{

	}

	Dequeue(const int& arraySize, const int& arrayExpandRate)
	{
		m_contents = DynArr<T>(arraySize, arrayExpandRate);
	}

	~Dequeue()
	{

	}

	/*
	Description: Add a value to the beginning of the queue.
	Param:
	    const T& value: The value to enqueue.
	*/
	inline void EnqueueStart(const T& value) 
	{
		m_contents.Insert(value, 0);
	}

	/*
	Description: Add a value to the end of the queue.
	Param:
		const T& value: The value to enqueue.
	*/
	inline void EnqueueEnd(const T& value) 
	{
		m_contents.Push(value);
	}

	/*
	Description: Remove a value from the start of the queue and return it.
	Return Type: T&
	*/
	inline T DequeueStart()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(m_contents.Count() > 0 && "Queue Error: Deteced attempt to dequeue from an empty queue.");
#endif

		T value = m_contents[0];
		m_contents.PopAt(0);

		return value;
	}

	/*
	Description: Remove a value from the end of the queue and return it.
	Return Type: T&
	*/
	inline T DequeueEnd() 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(m_contents.Count() > 0 && "Queue Error: Deteced attempt to dequeue from an empty queue.");
#endif

		T value = m_contents[m_contents.Count() - 1];
		m_contents.PopEnd();

		return value;
	}

	// Array Subscript
	inline T& operator [] (const int& index) 
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		assert((index < m_contents.Count() && index >= 0) && "Queue Error: Subscript index out of range.");
#endif
		return m_contents[index];
	}

	// Array Subscript
	inline const T& operator [] (const int& index) const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		assert((index < m_contents.Count() && index >= 0) && "Queue Error: Subscript index out of range.");
#endif
		return m_contents[index];
	}

	/*
	Description: Get the amount of elements present in the queue.
	Return Type: const int&
	*/
	const int& Count() const 
	{
		return m_contents.Count();
	}

private:

	DynArr<T> m_contents;
};
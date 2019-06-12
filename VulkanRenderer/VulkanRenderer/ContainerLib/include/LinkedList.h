#pragma once
#include <cassert>

#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
#define CONTAINER_DEBUG_IMPLEMENTATION
#endif

template<typename T>
class LinkedListIterator;

template <typename T>
struct LinkedListNode
{
	LinkedListNode* m_prev = nullptr;
	LinkedListNode* m_next = nullptr;

	T m_contents;
};

template<typename T>
class LinkedList
{
public:

	template<typename T>
	friend class LinkedListIterator;

	LinkedList()
	{
		m_root = new LinkedListNode<T>;
		m_end = m_root;
	}

	~LinkedList()
	{
		Clear();
		delete m_root;
	}

	// Adds the value to the end of the list.
	void Add(const T& value)
	{
		LinkedListNode<T>* newEnd = nullptr;

		m_end->m_contents = value;

		LinkedListNode<T>* prevEnd = m_end;
		LinkedListNode<T>*& nextNode = m_end->m_next;
		if (nextNode)
		{
			// The next node already exists
			m_end = nextNode;
		}
		else
		{
			// The next node needs to be created.
			nextNode = new LinkedListNode<T>;
			m_end = nextNode;
		}

		m_end->m_prev = prevEnd;
		m_end->m_contents = value;

		++m_nCount;
	}

	T& RemoveStart()
	{
		T val = m_root->m_contents;

		LinkedListNode<T>* oldRoot = m_root;
		m_root = m_root->m_next;

		delete oldRoot;

		--m_nCount;

		return val;
	}

	// Removes the final value in the list.
	T RemoveEnd()
	{
		T val = m_end->m_contents;

		LinkedListNode<T>* oldEnd = m_end;

		m_end = m_end->m_prev;
		--m_nCount;

		delete oldEnd;

		return val;
	}

	void RemoveAt(const int& index)
	{
		if (index == m_nCount - 1)
		{
			RemoveEnd();
			return;
		}
		else if (index > 0)
		{
			LinkedListNode<T>* currentNode = m_root;

			for (int i = 0; i < index; ++i)
			{
				currentNode = currentNode->m_next;
			}

			// Current node is now the node that needs to be removed.
			LinkedListNode<T>* prev = currentNode->m_prev;
			LinkedListNode<T>* next = currentNode->m_next;

			prev->m_next = next;
			next->m_prev = prev;

			delete currentNode;

			--m_nCount;
		}
		else
		{
			// Node to remove is the root node.
			RemoveStart();
			return;
		}
	}

	const int& Count() const
	{
		return m_nCount;
	}

	inline LinkedListIterator<T> CreateIterator()
	{
		// Return iterator class instance that has pointers to this list's root node, end node, and count value.
		return LinkedListIterator<T>(&m_root, &m_end, &m_nCount);
	}

	inline const LinkedListIterator<T> CreateIterator() const
	{
		// Return iterator class instance that has pointers to this list's root node, end node, and count value.
		return LinkedListIterator<T>(&m_root, &m_end, &m_nCount);
	}

	void Clear()
	{
		// Note: The root node is not deleted.

		// Start at the end node.
		LinkedListNode<T>* currentNode = m_end;

		// While the current node is not the root node...
		while (currentNode != m_root)
		{
			LinkedListNode<T>* nodeToDelete = currentNode; // Get copy of the current node-to-delete pointer.

			currentNode = currentNode->m_prev; // Shift current node to the next node.

			delete nodeToDelete; // Delete current node using copied pointer.
		}

		// Make end point to root.
		m_end = m_root;
		m_root->m_next = nullptr; // Ensure the root's next pointer is nullptr so the Add() function can create new nodes.
		m_nCount = 0; // Set the count to zero.
	}

private:

	LinkedListNode<T>* m_root = nullptr;
	LinkedListNode<T>* m_end = nullptr;
	int m_nCount = 0;
};

template<typename T>
class LinkedListIterator
{
public:

	LinkedListIterator(LinkedListNode<T>** root, LinkedListNode<T>** end, int* nCount)
	{
		m_root = root;
		m_end = end;
		m_currentNode = *m_root;
		m_nCount = nCount;
	}

	~LinkedListIterator()
	{

	}

	// Returns true if the iterator index is 0.
	inline bool AtStart()
	{
		return (m_nCurrentIndex == 0);
	}

	// Returns true if the iterator reaches the end of the list.
	inline bool AtEnd()
	{
		return (m_nCurrentIndex >= *m_nCount);
	}

	// Returns the iterator to the start of the list.
	inline void Restart()
	{
		m_currentNode = *m_root;
		m_nCurrentIndex = 0;
	}

	// Increments the iterator forward by one.
	inline void operator ++()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION_FULL
		assert(m_nCurrentIndex < *m_nCount && "Linked List Error: Attempting to iterate past the end of the list.");
#endif

		m_currentNode = m_currentNode->m_next;
		++m_nCurrentIndex;
	}

	// Returns the current iterator value.
	T& Value()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(m_nCurrentIndex < *m_nCount && "Linked List Error: Attempting to access value of non-existent node.");
#endif

		return m_currentNode->m_contents;
	}

	const T& Value() const
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(m_nCurrentIndex < *m_nCount && "Linked List Error: Attempting to access value of non-existent node.");
#endif

		return m_currentNode->m_contents;
	}

	// Returns the current iterator index.
	const int& Index()
	{
		return m_nCurrentIndex;
	}

	// Removes the current iterator value from the list.
	void RemoveCurrent()
	{
#ifdef CONTAINER_DEBUG_IMPLEMENTATION
		assert(m_nCurrentIndex < *m_nCount && "Linked List Error: Attempting to remove a non-existent node.");
#endif

		if (m_nCurrentIndex == 0) // Current node is the root node.
		{
			LinkedListNode<T>*& rootPtrRef = *m_root; // Get reference to the pointer of the root.
			LinkedListNode<T>* oldRoot = rootPtrRef; // Store a copy of the old root pointer.

			rootPtrRef = rootPtrRef->m_next;
			rootPtrRef->m_prev = nullptr;

			m_currentNode = rootPtrRef; // Set current node to new root node.

			delete oldRoot; // Delete old root node using old root node pointer.
		}
		else if (m_nCurrentIndex == *m_nCount - 1)  // Current node is the end node.
		{
			LinkedListNode<T>*& endPtrRef = *m_end; // Get reference to the pointer of the end.
			LinkedListNode<T>* oldEnd = endPtrRef; // Store a copy of the old end pointer.

			endPtrRef = endPtrRef->m_prev;
			endPtrRef->m_next = nullptr;

			m_currentNode = endPtrRef; // Set the current node to the new end node.

			m_nCurrentIndex = *m_nCount; // Set the index to the count to stop iteration.

			delete oldEnd; // Delete the old end node using the old end node pointer.
		}
		else // Current node is not the root or end node.
		{
			LinkedListNode<T>* nodeToRemove = m_currentNode;

			m_currentNode = m_currentNode->m_next;

			nodeToRemove->m_prev->m_next = nodeToRemove->m_next;
			nodeToRemove->m_next->m_prev = nodeToRemove->m_prev;

			delete nodeToRemove;
		}

		--*m_nCount; // Decrement count in list.
	}

private:

	LinkedListNode<T>** m_root = nullptr;
	LinkedListNode<T>** m_end = nullptr;
	LinkedListNode<T>* m_currentNode = nullptr;
	int* m_nCount = nullptr;
	int m_nCurrentIndex;
};
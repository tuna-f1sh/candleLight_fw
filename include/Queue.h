/*

The MIT License (MIT)

Copyright (c) 2016 Hubert Denkmair

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#pragma once

#include <array>
#include <stdbool.h>
#include <CriticalSection.h>

template <class T, int MAX_SIZE>
class Queue
{
	public:
		unsigned size()
		{
			CriticalSection cs;
			return _size;
		}

		bool is_empty()
		{
			return size() == 0;
		}

		bool push_back(T* el)
		{
			CriticalSection cs;
			return push_back_i(el);
		}

		bool push_front(T* el)
		{
			CriticalSection cs;
			return push_front_i(el);
		}

		T* pop_front()
		{
			CriticalSection cs;
			return pop_front_i();
		}

		unsigned size_i()
		{
			return _size;
		}

		bool is_empty_i()
		{
			return _size == 0;
		}

		bool push_back_i(T* el)
		{
			if (_size >= _items.size())
			{
				return false;
			}

			unsigned pos = (_first + _size) % _items.size();
			_items[pos] = el;
			_size++;

			return true;
		}

		bool push_front_i(T* el)
		{
			if (_size >= _items.size())
			{
				return false;
			}

			if (_first == 0)
			{
				_first = _items.size() - 1;
			}
			else
			{
				_first--;
			}

			_items[_first] = el;
			_size++;

			return true;
		}

		T* pop_front_i()
		{
			if (_size == 0)
			{
				return nullptr;
			}

			T* retval = _items[_first];
			_first = (_first + 1) % _items.size();
			_size--;

			return retval;
		}

	private:
		unsigned _first;
		unsigned _size;
		std::array<T*, MAX_SIZE> _items;
};

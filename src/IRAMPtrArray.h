#ifndef IRAM_PTR_ARRAY
#define IRAM_PTR_ARRAY

template<class T>
class IRAMPtrArray {
public:
    IRAMPtrArray(const std::initializer_list<T>& il) {
        _length = 0;
    	_pArr = (T*)heap_caps_malloc(sizeof(T) * il.size(), MALLOC_CAP_32BIT);
        for (const T&val : il) {
            _pArr[_length++] = val;
        }
    }

    operator T* () const { return _pArr; }
    int length() { return _length; }
private:
    int _length;
    T *_pArr;
};

#endif

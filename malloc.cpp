

#include <unistd.h>
#include <cstring>
#include <sys/mman.h>

#define BIG_DATA_SIZE 131072
#define MAX_SIZE 100000000
#define MIN_SIZE 0
#define BIG_ENOUGH_BYTES 128

class MallocMetadata {
protected:
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    void* allocated;
public:
    MallocMetadata(size_t size, MallocMetadata* next, MallocMetadata* prev) : size(size),is_free
            (false),next(next),prev(prev),allocated(nullptr) {};

    ~MallocMetadata() = default;

    MallocMetadata& operator= (const MallocMetadata& data) {
        this->size = data.size;
        this->is_free = data.is_free;
        this->next = data.next;
        this->prev = data.prev;
        this->allocated = data.allocated;
        return *this;
    }

    MallocMetadata* getNext() {
        return this->next;
    }

    void setNext(MallocMetadata* next) {
        this->next = next;
    }

    MallocMetadata* getPrev() {
        return this->prev;
    }

    void setPrev(MallocMetadata* prev) {
        this->prev = prev;
    }

    void* getAllocated() {
        return this->allocated;
    }

    void setAllocated(void* address) {
        this->allocated = address;
    }

    bool getIsFree() {
        return this->is_free;
    }

    void setIsFree(bool status) {
        this->is_free = status;
    }

    size_t getSize() {
        return this->size;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    int allocate(){
        if(allocated != nullptr){
            return 0;
        }

        allocated = sbrk(size);
        if(allocated == (void*)-1){
            return -1;
        } else {
            return 0;
        }
    }
};

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

class MallocMetadataList {
protected:
    MallocMetadata* root;
    MallocMetadata* ending;
    size_t length;

public:
    MallocMetadataList() : root(nullptr),ending(nullptr),length(0) {};

    ~MallocMetadataList() = default;

    MallocMetadata* begin() {
        return this->root;
    }

    MallocMetadata* end() {
        return this->ending;
    }

    void merge(MallocMetadata* src, MallocMetadata* dest) {
        src->setSize(src->getSize()+_size_meta_data()+dest->getSize());
        src->setNext(dest->getNext());
        if (dest->getNext() != nullptr) {
            dest->getNext()->setPrev(src);
        }
        else {
            this->ending = src;
        }
        this->length--;
    }

    void remove(void* p) {
        for(MallocMetadata* i = begin();i != nullptr;i = i->getNext()) {
            if (p == i->getAllocated()) {
                if ((i->getNext() != nullptr) && (i->getNext()->getIsFree()) &&
                    (i->getPrev() != nullptr) && (i->getPrev()->getIsFree())) {
                    this->merge(i,i->getNext());
                    this->merge(i->getPrev(),i);
                    return;
                }

                if ((i->getNext() != nullptr) && (i->getNext()->getIsFree())) {
                    this->merge(i,i->getNext());
                    i->setIsFree(true);
                    return;
                }

                if ((i->getPrev() != nullptr) && (i->getPrev()->getIsFree())) {
                    this->merge(i->getPrev(),i);
                    return;
                }

                i->setIsFree(true);
                return;
            }
        }
    }

    void split(MallocMetadata* data, size_t new_size) {
        if (data->getSize() < new_size + _size_meta_data() + BIG_ENOUGH_BYTES) {
            return;
        }
        size_t second_size = data->getSize() - new_size - _size_meta_data();
        data->setSize(new_size);
        MallocMetadata* new_data = (MallocMetadata*)((char*)(data->getAllocated()) + new_size);

        new_data->setNext(data->getNext());
        new_data->setPrev(data);
        data->setNext(new_data);
        if(data->getNext() != nullptr) {
            data->getNext()->setPrev(new_data);
        }
        else {
            this->ending = data;
        }

        new_data->setIsFree(true);
        new_data->setSize(second_size);
        new_data->setAllocated((void*)((char*)new_data+_size_meta_data()));
        this->length++;
        if (this->ending->getAllocated() == data->getAllocated()) {
            this->ending = new_data;
        }
    }

    void* enlargeWilderness(size_t size) {
        size_t add_size = size - this->end()->getSize();
        if(sbrk(add_size) == (void*)-1){
            return NULL;
        }
        this->end()->setIsFree(false);
        this->end()->setSize(size);
        return this->end()->getAllocated();
    }

    void* append(size_t size) {
        for(MallocMetadata* i = begin();i != nullptr; i = i->getNext()) {
            if ((i->getSize() >= size) && (i->getIsFree())) {
                this->split(i,size);
                i->setIsFree(false);
                return i->getAllocated();

            }
            else {
                if ((i->getNext() == nullptr) && (i->getIsFree())) {
                    return this->enlargeWilderness(size);
                }
            }
        }


        MallocMetadata* data = (MallocMetadata*)sbrk(size + _size_meta_data());
        if(data == (void*)-1){
            return NULL;
        }
        *data = MallocMetadata(size,nullptr,this->end());
        data->setAllocated((void*)((char*)data + _size_meta_data()));

        if (this->length == 0) {
            this->root = data;
        }
        else {
            this->ending->setNext(data);
        }

        this->ending = data;
        this->length++;
        return data->getAllocated();
    }

    size_t get_data_size(void* p){
        for(MallocMetadata* i = begin();i != nullptr;i = i->getNext()) {
            if (p == i->getAllocated()) {
                return i->getSize();
            }
        }
        return 0;
    }

    size_t getLength(){
        return this->length;
    }

    bool allocatedHere(void* p) {
        for (MallocMetadata *i = begin(); i != nullptr; i = i->getNext()) {
            if (p == i->getAllocated()) {
                return true;
            }
        }
        return false;
    }
};

class BigMallocMetadata {
protected:
    size_t size;
    bool is_free;
    BigMallocMetadata* next;
    BigMallocMetadata* prev;
    void* allocated;
public:
    BigMallocMetadata(size_t size, BigMallocMetadata* next, BigMallocMetadata* prev) : size(size),
            is_free(false),next(next),prev(prev),allocated(nullptr) {};

    ~BigMallocMetadata() = default;

    BigMallocMetadata& operator= (const BigMallocMetadata& data) {
        this->size = data.size;
        this->is_free = data.is_free;
        this->next = data.next;
        this->prev = data.prev;
        this->allocated = data.allocated;
        return *this;
    }

    BigMallocMetadata* getNext() {
        return this->next;
    }

    void setNext(BigMallocMetadata* next) {
        this->next = next;
    }

    BigMallocMetadata* getPrev() {
        return this->prev;
    }

    void setPrev(BigMallocMetadata* prev) {
        this->prev = prev;
    }

    void* getAllocated() {
        return this->allocated;
    }

    void setAllocated(void* address) {
        this->allocated = address;
    }

    bool getIsFree() {
        return this->is_free;
    }

    void setIsFree(bool status) {
        this->is_free = status;
    }

    size_t getSize() {
        return this->size;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    int allocate() {
        if(allocated != nullptr){
            return 0;
        }

        allocated = mmap(NULL,size,PROT_READ | PROT_WRITE,MAP_ANONYMOUS | MAP_PRIVATE,-1,0);
        if(allocated == (void*)-1){
            return -1;
        } else {
            return 0;
        }
    }
};

class BigMallocMetadataList {
protected:
    BigMallocMetadata* root;
    BigMallocMetadata* ending;
    size_t length;

public:
    BigMallocMetadataList() : root(nullptr),ending(nullptr),length(0) {};

    ~BigMallocMetadataList() = default;

    BigMallocMetadata* begin() {
        return this->root;
    }

    BigMallocMetadata* end() {
        return this->ending;
    }


    void remove(void* p) {
        for(BigMallocMetadata* i = begin();i != nullptr;i = i->getNext()) {
            if (p == i->getAllocated()) {
                if (i->getPrev() != nullptr) {
                    i->getPrev()->setNext(i->getNext());
                } else {
                    root = i->getNext();
                }
                if (i->getNext() != nullptr) {
                    i->getNext()->setPrev(i->getPrev());
                } else {
                    ending = i->getPrev();
                }
                this->length--;
                munmap(i->getAllocated(),i->getSize() + _size_meta_data());
                return;
            }
        }
    }

    void* append(size_t size) {
        BigMallocMetadata* data = (BigMallocMetadata*)mmap(NULL,_size_meta_data() + size,PROT_READ | PROT_WRITE,MAP_ANONYMOUS | MAP_PRIVATE,-1,0);
        if(data == (void*)-1){
            return NULL;
        } else {
            *data = BigMallocMetadata(size, nullptr,ending);
            data->setAllocated((void*)((char*)data + _size_meta_data()));
        }

        if (this->length == 0) {
            this->root = data;
            ending = data;
        } else {
            this->ending->setNext(data);
        }
        this->ending = data;
        this->length++;
        return data->getAllocated();
    }

    size_t get_data_size(void* p){
        for(BigMallocMetadata* i = begin();i != nullptr;i = i->getNext()) {
            if (p == i->getAllocated()) {
                return i->getSize();
            }
        }
        return 0;
    }

    size_t getLength(){
        return this->length;
    }

    bool allocatedHere(void* p) {
        for (BigMallocMetadata *i = begin(); i != nullptr; i = i->getNext()) {
            if (p == i->getAllocated()) {
                return true;
            }
        }
        return false;
    }
};

MallocMetadataList __my_data_list;
MallocMetadataList* _my_data_list = &__my_data_list;
BigMallocMetadataList __my_big_data_list;
BigMallocMetadataList* _my_big_data_list = &__my_big_data_list;


void *smalloc(size_t size) {
    void* new_space = NULL;

    if (size > MIN_SIZE && size <= MAX_SIZE) {
        if (size >= BIG_DATA_SIZE) {
            new_space = _my_big_data_list->append(size);
        } else {
            new_space = _my_data_list->append(size);
        }
    }
    return new_space;
}


void* scalloc(size_t num, size_t size){
    void* new_space = smalloc(num*size);

    if(new_space != NULL){
        memset(new_space,0,size*num);
    }

    return new_space;
}

void sfree(void* p){
    if(p != NULL){
        if(_my_data_list->allocatedHere(p)){
            _my_data_list->remove(p);
        } else {
            _my_big_data_list->remove(p);
        }
    }
}

void* srealloc(void* oldp, size_t size) {
    if (size <= MIN_SIZE || size > MAX_SIZE) {
        return NULL;
    }
    if (oldp == NULL) {
        return smalloc(size);
    }

    void* new_space = NULL;

    if(_my_data_list->allocatedHere(oldp)){
        if(size == _my_data_list->get_data_size(oldp)){
            return oldp;
        }

        size_t old_size = _my_data_list->get_data_size(oldp);
        int diff = (int)(size - old_size - _size_meta_data());

        if(size < _my_data_list->get_data_size(oldp)){
            for(MallocMetadata* i = _my_data_list->begin(); i != nullptr; i = i->getNext()) {
                if (i->getAllocated() == oldp) {
                    _my_data_list->split(i,size);
                    return i->getAllocated();
                }
            }
        }

        if (oldp == _my_data_list->end()->getAllocated()) {
            new_space = _my_data_list->enlargeWilderness(size);
            if (new_space != nullptr ) {
                return new_space;
            }
        }


        for(MallocMetadata* i = _my_data_list->begin(); i != nullptr; i = i->getNext()) {
            if (i->getAllocated() == oldp) {
                if ((i->getNext() != nullptr) && (i->getNext()->getIsFree()) &&
                    ((int)(i->getNext()->getSize()) >= diff)) {
                    _my_data_list->merge(i,i->getNext());
                    _my_data_list->split(i,size);
                    new_space = oldp;
                } else if ((i->getPrev() != nullptr) && (i->getPrev()->getIsFree()) &&
                            ((int)(i->getPrev()->getSize()) >= diff)) {
                    i = i->getPrev();
                    _my_data_list->merge(i,i->getNext());
                    memcpy(oldp,i->getAllocated(),old_size);
                    _my_data_list->split(i,size);
                    i->setIsFree(false);
                    new_space = i->getAllocated();
                } else if ((i->getPrev() != nullptr) && (i->getPrev()->getIsFree()) &&
                        (i->getNext() != nullptr) && (i->getNext()->getIsFree()) &&
                        ((int)(i->getNext()->getSize() + i->getPrev()->getSize()) >= (int)(diff -_size_meta_data()))){
                    i = i->getPrev();
                    _my_data_list->merge(i,i->getNext());
                    _my_data_list->merge(i,i->getNext());
                    memcpy(oldp,i->getAllocated(),old_size);
                    _my_data_list->split(i,size);
                    i->setIsFree(false);
                    new_space = i->getAllocated();
                } else {
                    new_space = _my_data_list->append(size);
                    memcpy(new_space,oldp,old_size);
                    _my_data_list->remove(oldp);
                }
                break;
            }
        }

    } else {
        new_space = _my_big_data_list->append(size);

        if(new_space != (void*)-1){
            size_t size_to_copy = _my_big_data_list->get_data_size(oldp);
            if(size_to_copy > size){
                size_to_copy = size;
            }
            memcpy(new_space,oldp,size_to_copy);
            _my_big_data_list->remove(oldp);
        }
    }

    return new_space;

}

size_t _num_free_blocks() {
    size_t counter = 0;
    for(MallocMetadata* i = _my_data_list->begin();i != nullptr;i = i->getNext()) {
        if (i->getIsFree()) {
            counter++;
        }
    }
    return counter;
}

size_t _num_free_bytes() {
    size_t counter = 0;
    for(MallocMetadata* i = _my_data_list->begin();i != nullptr;i = i->getNext()) {
        if (i->getIsFree()) {
            counter += i->getSize();
        }
    }
    return counter;
}

size_t _num_allocated_blocks() {
    return (_my_data_list->getLength() + _my_big_data_list->getLength());
}

size_t _num_allocated_bytes() {
    size_t counter = 0;
    for(MallocMetadata* i = _my_data_list->begin();i != nullptr;i = i->getNext()) {
        counter += i->getSize();
    }
    for(BigMallocMetadata* i = _my_big_data_list->begin();i != nullptr;i = i->getNext()) {
        counter += i->getSize();
    }

    return counter;
}

size_t _num_meta_data_bytes() {
    size_t counter = 0;
    for(MallocMetadata* i = _my_data_list->begin();i != nullptr;i = i->getNext()) {
        counter += sizeof(*i);
    }
    for(BigMallocMetadata* i = _my_big_data_list->begin();i != nullptr;i = i->getNext()) {
        counter += sizeof(*i);
    }
    return counter;
}


#ifndef INCLUDED_FIXED_FUNCTION_H
#deifne INCLUDED_FIXED_FUNCTION_H

/*
  fixed_function uses type erasure to store a copy of lambda function
  objects in a fixed-size buffer (no dynamic allocation).

  Arduino doesn't have std::function, so this is its replacement.
 */

// arduino doesn't have this.
void* operator new(size_t, void* arg)
{
    return arg;
}

template <typename FUNC, size_t Size>
class fixed_function;

template <typename RET, typename... ARGS, size_t Size>
class fixed_function<RET(ARGS...), Size> {
    struct IFn {
        virtual RET operator()(ARGS&&... args) = 0;
        //virtual IFn* copyTo(void* destination) = 0;
        virtual IFn* moveTo(void* destination) = 0;
        virtual ~IFn() {}
    };

    template <typename T>
    struct Unnamed : public IFn, T {
        Unnamed(T&& x) : T(static_cast<T&&>(x)) {}
        RET operator()(ARGS&&... args) override
        {
            return T::operator()(static_cast<ARGS&&>(args)...);
        }
        //IFn* copyTo(void* destination) override {
        //    return new (destination) Unnamed<T>(*this);
        //}
        IFn* moveTo(void* destination) override {
            return new (destination)
                Unnamed<T>(static_cast<Unnamed<T>&&>(*this));
        }
    };

    char data[Size];
    IFn* ptr = nullptr;

   public:

    fixed_function() = default;

    fixed_function(fixed_function& other)
    {
        if (other.ptr != nullptr) {
            this->ptr = other.ptr->copyTo(this->data);
        }
    }

    fixed_function& operator=(fixed_function& other) = delete;

    fixed_function& operator=(fixed_function&& other)
    {
        if (other.ptr != nullptr) {
            this->ptr = other.ptr->moveTo(this->data);
        }
    }

    ~fixed_function() {
        if (ptr != nullptr) {
            ptr->~IFn();
        }
    }

    template <typename T>
    void assign(T&& x)
    {
        if (ptr != nullptr) {
            ptr->~IFn();
        }

        using DType = Unnamed<T>;
        static_assert(sizeof(DType) <= Size, "Not enough size.");
        
        ptr = new (data) DType(static_cast<T&&>(x));
    }

    RET operator()(ARGS&&... args)
    {
        return (*ptr)(static_cast<ARGS&&>(args)...);
    }
};

#endif

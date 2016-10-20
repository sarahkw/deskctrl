
// arduino doesn't have this.
void* operator new(size_t, void* arg)
{
    return arg;
}

template <typename FUNC, size_t Size>
class fixed_function;

template <typename RET, typename... ARGS, size_t Size>
class fixed_function<RET(ARGS...), Size> {
    char data[Size];

    struct IFn {
        virtual RET operator()(ARGS&&... args) = 0;
        virtual ~IFn() {}
    };

    IFn* ptr = nullptr;

    template <typename T>
    struct Unnamed : public IFn, T {
        Unnamed(T&& x) : T(static_cast<T&&>(x)) {}
        virtual RET operator()(ARGS&&... args) override
        {
            return T::operator()(static_cast<ARGS&&>(args)...);
        }
    };

   public:

//    fixed_function() = default;
//
//    fixed_function(fixed_function& other)
//    {
//        if (other.ptr != nullptr) {
//            this->ptr =
//                new (data) DType(*reinterpret_cast<DType*>(&other.data));
//            this->assigned = true;
//        }
//    }
//
//    ~fixed_function() {
//        if (ptr != nullptr) {
//            ptr->~IFn();
//        }
//    }

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
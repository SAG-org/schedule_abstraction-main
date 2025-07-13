#ifndef OBJECT_POOL
#define OBJECT_POOL

#include <memory>
#include <deque>

template <typename T>
class Object_pool {
private:
    std::deque<std::shared_ptr<T>> pool;
    
public:
    template <typename... Args>
    std::shared_ptr<T> acquire(Args&&... args) {
        if (pool.empty()) {
            return std::shared_ptr<T>(new T(std::forward<Args>(args)...));
        }
        std::shared_ptr<T> obj = pool.back();
        pool.pop_back();
        obj->reset(std::forward<Args>(args)...); // Initialize the object
        return obj;
    }

    void release(const std::shared_ptr<T>& obj) {
        if (!obj.unique())
            return; // Do not release if the object is shared
        pool.emplace_back(obj);
    }
};

#endif // !OBJECT_POOL

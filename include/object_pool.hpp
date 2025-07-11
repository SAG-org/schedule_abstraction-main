#ifndef OBJECT_POOL
#define OBJECT_POOL

#include <memory>

template <typename T>
class Object_pool {
private:
#ifdef CONFIG_PARALLEL
	tbb::concurrent_queue<std::shared_ptr<T>> pool;
#else
    std::deque<std::shared_ptr<T>> pool;
#endif
public:
    template <typename... Args>
    std::shared_ptr<T> acquire(Args&&... args) {
#ifdef CONFIG_PARALLEL
		T* obj;
		if (!pool.try_pop(obj)) {
			return new T(std::forward<Args>(args)...);
		}
#else
        if (pool.empty()) {
            return std::shared_ptr<T>(new T(std::forward<Args>(args)...));
        }
        std::shared_ptr<T> obj = pool.back();
        pool.pop_back();
#endif
        obj->reset(std::forward<Args>(args)...); // Initialize the object
        return obj;
    }

    void release(const std::shared_ptr<T>& obj) {
        if (!obj.unique())
			return; // Do not release if the object is shared
#ifdef CONFIG_PARALLEL
        pool.emplace(obj);
#else
        pool.emplace_back(obj);
#endif
    }
};

#endif // !OBJECT_POOL

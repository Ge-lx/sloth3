#include <queue>
#include <stdexcept>
#include <SDL2/SDL.h>


class timeout_exception : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A threadsafe-queue. https://stackoverflow.com/a/16075550
template <class T>
class ThreadSafeQueue {
private:
    std::queue<T> q;
    SDL_mutex* rb_mutex;
    SDL_cond* rb_cond;
    size_t min_fill_len;

public:
    ThreadSafeQueue (size_t min_fill_len) : q(), min_fill_len(min_fill_len) {
        rb_mutex = SDL_CreateMutex();
        rb_cond = SDL_CreateCond();
    }

    ~ThreadSafeQueue () {
        SDL_DestroyCond(rb_cond);
        SDL_DestroyMutex(rb_mutex);
    }

    // Add an element to the queue.
    void enqueue (T t) {
        SDL_LockMutex(rb_mutex);
        q.push(t);
        SDL_UnlockMutex(rb_mutex);
        SDL_CondSignal(rb_cond);
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue (Uint32 timeout_ms = SDL_MUTEX_MAXWAIT) {
        SDL_LockMutex(rb_mutex);
        while (q.size() < min_fill_len) {
            // release lock as long as the wait and reaquire it afterwards.
            int res = SDL_CondWaitTimeout(rb_cond, rb_mutex, timeout_ms);
            if (res == SDL_MUTEX_TIMEDOUT) {
                throw timeout_exception("Waiting for new elements timed out");
            }
        }
        T val = q.front();
        q.pop();
        SDL_UnlockMutex(rb_mutex);
        return val;
    }
};

template <typename T>
class RingBuffer  {
private:
    ThreadSafeQueue<T*> clean;
    ThreadSafeQueue<T*> dirty;

public:
    size_t buffer_len, num_buffers;

    RingBuffer (size_t buffer_len, size_t min_fill_len) :
        clean(1), dirty(min_fill_len), buffer_len(buffer_len), num_buffers(min_fill_len + 1)
    {
        for (size_t i = 0; i < num_buffers; i++) {
            clean.enqueue(new T[buffer_len]);
        }
    }

    ~RingBuffer () {
        while (true) {
            try {
                T* buf = clean.dequeue(100);
                delete[] buf;
            } catch (const timeout_exception& e) {
                break;
            }
        }
        while (true) {
            try {
                T* buf = dirty.dequeue(100);
                delete[] buf;
            } catch (const timeout_exception& e) {
                break;
            }
        }
    }

    T* dequeue_clean () {
        return clean.dequeue();
    }

    void enqueue_clean (T* buf) {
        clean.enqueue(buf);
    }

    void enqueue_dirty (T* buf) {
        dirty.enqueue(buf);
    }

    T* dequeue_dirty () {
        return dirty.dequeue();
    }
};

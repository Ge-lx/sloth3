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
    bool draining = false;

public:
    ThreadSafeQueue (size_t min_fill_len) : q(), min_fill_len(min_fill_len) {
        rb_mutex = SDL_CreateMutex();
        rb_cond = SDL_CreateCond();
    }

    ~ThreadSafeQueue () {
        SDL_LockMutex(rb_mutex);
        SDL_CondBroadcast(rb_cond);
        SDL_UnlockMutex(rb_mutex);
        SDL_DestroyCond(rb_cond);
        SDL_DestroyMutex(rb_mutex);
    }

    // Add an element to the queue.
    void enqueue (T t) {
        SDL_LockMutex(rb_mutex);
        q.push(t);
        SDL_CondSignal(rb_cond);
        SDL_UnlockMutex(rb_mutex);
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue (Uint32 timeout_ms = SDL_MUTEX_MAXWAIT) {
        SDL_LockMutex(rb_mutex);
        while (q.size() < min_fill_len) {
            // release lock as long as the wait and reaquire it afterwards.
            int res = SDL_CondWaitTimeout(rb_cond, rb_mutex, timeout_ms);
            if (res == SDL_MUTEX_TIMEDOUT || draining) {
                SDL_UnlockMutex(rb_mutex);
                throw timeout_exception("Waiting for new elements timed out");
            }
        }
        T val = q.front();
        q.pop();
        SDL_UnlockMutex(rb_mutex);
        return val;
    }

    void drain () {
        SDL_LockMutex(rb_mutex);
        draining = true;
        SDL_CondSignal(rb_cond);
        SDL_UnlockMutex(rb_mutex);
    }
};

template <typename T>
class RingBuffer  {
private:
    ThreadSafeQueue<T*> clean;
    ThreadSafeQueue<T*> dirty;
    bool draining = false;

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
        drain();
    }

    void drain() {
        draining = true;
        clean.drain();
        dirty.drain();
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
        if (draining) throw timeout_exception("RingBuffer is draining");
        return clean.dequeue();
    }

    T* dequeue_dirty () {
        if (draining) throw timeout_exception("RingBuffer is draining");
        return dirty.dequeue();
    }

    void enqueue_clean (T* buf) {
        if (draining) return;
        clean.enqueue(buf);
    }

    void enqueue_dirty (T* buf) {
        if (draining) return;
        dirty.enqueue(buf);
    }

};

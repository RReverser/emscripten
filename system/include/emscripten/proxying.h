/*
 * Copyright 2021 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#pragma once

#include <emscripten/emscripten.h>
#include <emscripten/promise.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to a set of thread-local work queues to which work can be
// asynchronously or synchronously proxied from other threads. When work is
// proxied to a queue on a particular thread, that thread is notified to start
// processing work from that queue if it is not already doing so.
//
// Proxied work can only be completed on live thread runtimes, so users must
// ensure either that all proxied work is completed before a thread exits or
// that the thread exits with a live runtime, e.g. via
// `emscripten_exit_with_live_runtime` to avoid dropped work.
typedef struct em_proxying_queue em_proxying_queue;

// Create and destroy proxying queues.
em_proxying_queue* em_proxying_queue_create(void);
void em_proxying_queue_destroy(em_proxying_queue* q);

// Get the queue used for proxying low-level runtime work. Work on this queue
// may be processed at any time inside system functions, so it must be
// nonblocking and safe to run at any time, similar to a native signal handler.
em_proxying_queue* emscripten_proxy_get_system_queue(void);

// Execute all the tasks enqueued for the current thread on the given queue. New
// tasks that are enqueued concurrently with this execution will be executed as
// well. This function returns once it observes an empty queue.
void emscripten_proxy_execute_queue(em_proxying_queue* q);

// Opaque handle to a currently-executing proxied task, used to signal the end
// of the task.
typedef struct em_proxying_ctx em_proxying_ctx;

// Signal the end of a task proxied with `emscripten_proxy_sync_with_ctx`.
void emscripten_proxy_finish(em_proxying_ctx* ctx);

// Enqueue `func` on the given queue and thread and return immediately. Returns
// 1 if the work was successfully enqueued and the target thread notified or 0
// otherwise.
int emscripten_proxy_async(em_proxying_queue* q,
                           pthread_t target_thread,
                           void (*func)(void*),
                           void* arg);

// Enqueue `func` on the given queue and thread and wait for it to finish
// executing before returning. Returns 1 if the task was successfully completed
// and 0 otherwise, including if the target thread is canceled or exits before
// the work is completed.
int emscripten_proxy_sync(em_proxying_queue* q,
                          pthread_t target_thread,
                          void (*func)(void*),
                          void* arg);

// Enqueue `func` on the given queue and thread and wait for it to be executed
// and for the task to be marked finished with `emscripten_proxy_finish` before
// returning. `func` need not call `emscripten_proxy_finish` itself; it could
// instead store the context pointer and call `emscripten_proxy_finish` at an
// arbitrary later time. Returns 1 if the task was successfully completed and 0
// otherwise, including if the target thread is canceled or exits before the
// work is completed.
int emscripten_proxy_sync_with_ctx(em_proxying_queue* q,
                                   pthread_t target_thread,
                                   void (*func)(em_proxying_ctx*, void*),
                                   void* arg);

// Enqueue `func` on the given queue and thread. Once (and if) it finishes
// executing, it will asynchronously proxy `callback` back to the current thread
// on the same queue, or if the target thread dies before the work can be
// completed, `cancel` will be proxied back instead. All three function will
// receive the same argument, `arg`. Returns 1 if `func` was successfully
// enqueued and the target thread notified or 0 otherwise.
int emscripten_proxy_callback(em_proxying_queue* q,
                              pthread_t target_thread,
                              void (*func)(void*),
                              void (*callback)(void*),
                              void (*cancel)(void*),
                              void* arg);

// Enqueue `func` on the given queue and thread. Once (and if) it finishes the
// task by calling `emscripten_proxy_finish` on the given `em_proxying_ctx`, it
// will asynchronously proxy `callback` back to the current thread on the same
// queue, or if the target thread dies before the work can be completed,
// `cancel` will be proxied back instead. All three function will receive the
// same argument, `arg`. Returns 1 if `func` was successfully enqueued and the
// target thread notified or 0 otherwise.
int emscripten_proxy_callback_with_ctx(em_proxying_queue* q,
                                       pthread_t target_thread,
                                       void (*func)(em_proxying_ctx*, void*),
                                       void (*callback)(void*),
                                       void (*cancel)(void*),
                                       void* arg);

__attribute__((warn_unused_result)) em_promise_t
emscripten_proxy_promise(em_proxying_queue* q,
                         pthread_t target_thread,
                         void (*func)(void*),
                         void* arg);

__attribute__((warn_unused_result)) em_promise_t
emscripten_proxy_promise_with_ctx(em_proxying_queue* q,
                                  pthread_t target_thread,
                                  void (*func)(em_proxying_ctx*, void*),
                                  void* arg);

#ifdef __cplusplus
} // extern "C"

#if __cplusplus < 201103L
#warning "C++ ProxyingQueue support requires building with -std=c++11 or newer!"
#else

#include <thread>
#include <utility>

namespace emscripten {

// A thin C++ wrapper around the underlying C API.
class ProxyingQueue {
public:
  // Simple wrapper around `em_proxying_ctx*` providing a `finish` method as an
  // alternative to `emscripten_proxy_finish`.
  struct ProxyingCtx {
    em_proxying_ctx* ctx;

    ProxyingCtx() = default;
    ProxyingCtx(em_proxying_ctx* ctx) : ctx(ctx) {}
    void finish() { emscripten_proxy_finish(ctx); }
  };

private:
  template <typename Func>
  static void runAndFree(void* arg) {
    std::unique_ptr<Func> func((Func*)arg);
    (*func)();
  }

  template <typename Func>
  static void run(void* arg) {
    // Move the function state into a local variable so that it's destructed
    // right after the function is called and on the same thread.
    // Move constructor will take care of preventing double-free for exclusive
    // resources.
    // If `run` isn't reached (if proxying failed), then the original `Func`
    // will be at least attempted to be destroyed on the caller thread instead
    // of the target one.
    Func func = std::move(*(Func*)arg);
    func();
  }

  template <typename Func>
  static void runWithCtx(em_proxying_ctx* ctx, void* arg) {
    // Same as in `run`, move into a local variable before calling.
    Func func = std::move(*(Func*)arg);
    func(ProxyingCtx{ctx});
  }

  template <typename Func, typename Callback, typename Cancel>
  struct CallbackFuncs {
    Func func;
    Callback callback;
    Cancel cancel;

    static void runFunc(void* arg) {
      auto* info = (CallbackFuncs*)arg;
      // Make sure to call into the helper that takes care of freeing the Func on the correct thread.
      ProxyingQueue::run<Func>(&info->func);
    }

    static void runFuncWithCtx(em_proxying_ctx* ctx, void* arg) {
      auto* info = (CallbackFuncs*)arg;
      ProxyingQueue::runWithCtx<Func>(ctx, &info->func);
    }

    static void runCallback(void* arg) {
      std::unique_ptr<CallbackFuncs> info((CallbackFuncs*)arg);
      info->callback();
    }

    static void runCancel(void* arg) {
      std::unique_ptr<CallbackFuncs> info((CallbackFuncs*)arg);
      info->cancel();
    }
  };

public:
  em_proxying_queue* queue = em_proxying_queue_create();

  // ProxyingQueue can be moved but not copied. It is not valid to call any
  // methods on ProxyingQueues that have been moved out of.
  ProxyingQueue() = default;
  ProxyingQueue& operator=(const ProxyingQueue&) = delete;
  ProxyingQueue& operator=(ProxyingQueue&& other) {
    if (this != &other) {
      if (queue) {
        em_proxying_queue_destroy(queue);
      }
      queue = other.queue;
      other.queue = nullptr;
    }
    return *this;
  }

  ProxyingQueue(const ProxyingQueue&) = delete;
  ProxyingQueue(ProxyingQueue&& other) : queue(nullptr) {
    *this = std::move(other);
  }

  ~ProxyingQueue() {
    if (queue) {
      em_proxying_queue_destroy(queue);
    }
  }

  void execute() { emscripten_proxy_execute_queue(queue); }

  // Return true if the work was successfully enqueued and false otherwise.
  // Refer to the corresponding C API documentation.
  template <typename Func>
  bool proxyAsync(pthread_t target, Func&& func) {
    auto* arg = new Func(std::forward<Func>(func));
    if (!emscripten_proxy_async(queue, target, runAndFree<Func>, arg)) {
      delete arg;
      return false;
    }
    return true;
  }

  template <typename Func>
  bool proxySync(pthread_t target, Func&& func) {
    return emscripten_proxy_sync(queue, target, run<Func>, &func);
  }

  template <typename Func>
  bool proxySyncWithCtx(pthread_t target, Func&& func) {
    return emscripten_proxy_sync_with_ctx(queue, target, runWithCtx<Func>, &func);
  }

  template <typename Func, typename Callback, typename Cancel>
  bool proxyCallback(pthread_t target,
                     Func&& func,
                     Callback&& callback,
                     Cancel&& cancel) {
    using CallbackFuncs = CallbackFuncs<Func, Callback, Cancel>;
    auto* info = new CallbackFuncs {
      .func = std::forward<Func>(func),
      .callback = std::forward<Callback>(callback),
      .cancel = std::forward<Cancel>(cancel),
    };
    if (!emscripten_proxy_callback(queue,
                                   target,
                                   CallbackFuncs::runFunc,
                                   CallbackFuncs::runCallback,
                                   CallbackFuncs::runCancel,
                                   info)) {
      delete info;
      return false;
    }
    return true;
  }

  template <typename Func, typename Callback, typename Cancel>
  bool proxyCallbackWithCtx(pthread_t target,
                            Func&& func,
                            Callback&& callback,
                            Cancel&& cancel) {
    using CallbackFuncs = CallbackFuncs<Func, Callback, Cancel>;
    auto* info = new CallbackFuncs {
      .func = std::forward<Func>(func),
      .callback = std::forward<Callback>(callback),
      .cancel = std::forward<Cancel>(cancel),
    };
    if (!emscripten_proxy_callback_with_ctx(queue,
                                            target,
                                            CallbackFuncs::runFuncWithCtx,
                                            CallbackFuncs::runCallback,
                                            CallbackFuncs::runCancel,
                                            info)) {
      delete info;
      return false;
    }
    return true;
  }
};

} // namespace emscripten

#endif // __cplusplus < 201103L
#endif // __cplusplus

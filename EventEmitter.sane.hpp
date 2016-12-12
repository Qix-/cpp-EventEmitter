#ifndef __EVENTEMITTER_SANE_HPP
#define __EVENTEMITTER_SANE_HPP

#ifdef __EVENTEMITTER_TESTING
#undef __EVENTEMITTER_PROVIDER
#undef __EVENTEMITTER_PROVIDER_THREADED
#undef __EVENTEMITTER_PROVIDER_DEFERRED
#endif

#include <functional>
#include <forward_list>
#include <map>
#include <deque>
#include <cstring>

#ifndef EVENTEMITTER_DISABLE_THREADING
#include <condition_variable>
#include <future>
#include <mutex>

#define __EVENTEMITTER_MUTEX_DECLARE(mutex) std::mutex mutex;
#define __EVENTEMITTER_LOCK_GUARD(mutex) std::lock_guard<std::mutex> guard(mutex);
#else
#define __EVENTEMITTER_MUTEX_DECLARE(mutex);
#define __EVENTEMITTER_LOCK_GUARD(mutex);
#endif

#if defined(__GNUC__)
#define __EVENTEMITTER_GCC_WORKAROUND this->
#else
#define __EVENTEMITTER_GCC_WORKAROUND
#endif

#define __EVENTEMITTER_CONCAT_IMPL(x, y) x ## y
#define __EVENTEMITTER_CONCAT(x, y) __EVENTEMITTER_CONCAT_IMPL(x, y)

#ifndef __EVENTEMITTER_NONMACRO_DEFS
#define __EVENTEMITTER_NONMACRO_DEFS
namespace EE {
	class DeferredBase {
	protected: 
		typedef std::function<void ()> DeferredHandler;
		std::forward_list<DeferredHandler> removeHandlers;
		std::deque<DeferredHandler> deferredQueue;
		__EVENTEMITTER_MUTEX_DECLARE(mutex);
	protected:
		void runDeferred(DeferredHandler f) {
			__EVENTEMITTER_LOCK_GUARD(mutex);
			deferredQueue.emplace_back(std::move(f));
		}
	public:
		void removeAllHandlers() {
			for(auto& handler : removeHandlers) {
				handler();
			}
		}
		void clearDeferred() {
			__EVENTEMITTER_LOCK_GUARD(mutex);
			deferredQueue.clear();
		}
		bool runDeferred() {
			__EVENTEMITTER_LOCK_GUARD(mutex);
			if(deferredQueue.size()) {
				((deferredQueue.front()))();
				deferredQueue.pop_front();
			}
			return !deferredQueue.empty();
		}
		void runAllDeferred() {
			while(runDeferred());
		}
	};
	
	// reference_wrapper needs to be used instead of std::reference_wrapper
	// this is because of VS2013 (RC) bug
	template<class T> class reference_wrapper
	{
	public:
		typedef T type;
		explicit reference_wrapper(T& t): t_(&t) {}
		operator T& () const { return *t_; }
		T& get() const { return *t_; }
		T* get_pointer() const { return t_; }
private:
    T* t_;
};
	

// source: stackoverflow.com/questions/15501301/binding-function-arguments-in-c11
template<typename T> struct forward_as_ref_type {
 typedef T &&type;
};
template<typename T> struct forward_as_ref_type<T &> {
	typedef reference_wrapper<T> type;
};
template<typename T> typename forward_as_ref_type<T>::type forward_as_ref(
 typename std::remove_reference<T>::type &t) {
    return static_cast<typename forward_as_ref_type<T>::type>(t);
 }
 template<typename T> T &&forward_as_ref(
 typename std::remove_reference<T>::type &&t) {
    return t;
 }

template<typename... Args>	
inline decltype(auto) wrapLambdaWithCallback(const std::function<void(Args...)>& f, std::function<void()>&& afterCb) {
	return [=,afterCb=std::move(afterCb)](Args&&... args) {
		f(args...);
		afterCb();
	};
}

template<typename... Args>	
inline decltype(auto) wrapLambdaWithCallback(std::function<void(Args...)>&& f, std::function<void()>&& afterCb) {
	return [f=std::move(f),afterCb=std::move(afterCb)](Args&&... args) {
		f(args...);
		afterCb();
	};
}

#ifndef EVENTEMITTER_DISABLE_THREADING
	
	// TODO: allow callback for setting if async has completed
	template<typename... Args>
	class LambdaAsyncWrapper
	{
		std::function<void(Args...)> m_f;		
	public:
		LambdaAsyncWrapper(const std::function<void(Args...)>& f) : m_f(f) {}
		void operator()(Args... fargs) const { 
			std::async(std::launch::async, m_f, fargs...);
		}
	};
	template<typename... Args>
	LambdaAsyncWrapper<Args...> wrapLambdaInAsync(const std::function<void(Args...)>& f) {
		return LambdaAsyncWrapper<Args...>(f);
	};
	
	template<typename... Args>
	class LambdaPromiseWrapper
	{
		std::shared_ptr<std::promise<std::tuple<Args...>>> m_promise;
	public:
		LambdaPromiseWrapper(std::shared_ptr<std::promise<std::tuple<Args...>>> promise) : m_promise(promise) {}
		void operator()(Args... fargs) const { 
			m_promise->set_value(std::tuple<Args...>(fargs...));
		}
	};
	template<typename... Args>
	LambdaPromiseWrapper<Args...> getLambdaForFuture(std::shared_ptr<std::promise<std::tuple<Args...>>> promise) {
		return LambdaPromiseWrapper<Args...>(promise);
	};

#endif // EVENTEMITTER_DISABLE_THREADING
	
}
#endif // __EVENTEMITTER_NONMACRO_DEFS


#ifndef __EVENTEMITTER_CONTAINER
#define __EVENTEMITTER_CONTAINER std::forward_list<HandlerPtr>
#endif

using handle_id_type = uint32_t;
static handle_id_type __handle_counter;

#define __EVENTEMITTER_PROVIDER(frontname, name) //^//
template<typename... Rest>
class ExampleEventEmitterTpl {
public:
	typedef std::function<void(Rest...)> Handler;
	using Handle = handle_id_type;
	using HandlerTuple = std::tuple<Handle, Handler>;
	struct HandlerPtr : public HandlerTuple {
		HandlerPtr(Handler handler, bool _specialFlag = false) : HandlerTuple((__handle_counter++) | _specialFlag << 31 , std::move(handler)) {
			if(__handle_counter & 0x80000000) {
				__handle_counter = 0;
			}
		}
		bool specialFlag() {
			return std::get<0>(*this) & 0x80000000;
		}
		bool operator==(Handle other) {
			return std::get<0>(*this) == other;
		}
		template<typename... Args> inline decltype(auto) operator() (Args&&... fargs) {
			return std::get<1>(*this)(fargs...);
		}
		operator Handle() const { return std::get<0>(*this); }
	};

private:
	using EventHandlersSet = __EVENTEMITTER_CONTAINER;
	EventHandlersSet eventHandlers;
public:
	Handle onExample (Handler handler) {
		eventHandlers.emplace_front(std::move(handler));
		return eventHandlers.front();
	}
	Handle onceExample (Handler handler) {
		eventHandlers.emplace_front(std::move(handler), true);
		return eventHandlers.front();
	}
	bool hasExampleHandlers() {
		return !eventHandlers.empty();
	}
	int countExampleHandlers() {
		int count = 0;
		for(auto& i:eventHandlers) count++;
		return count;
	}
	template<typename... Args> inline void emitExample (Args&&... fargs) {
		triggerExample(fargs...);
	}
	template<typename... Args> inline void triggerExample (Args&&... fargs) {
		auto prev = eventHandlers.before_begin(); 
	  for(auto i = eventHandlers.begin();i != eventHandlers.end();) {
			
			(*i)(fargs...);
			if(i->specialFlag()) {
				i = eventHandlers.erase_after(prev);
			}
			else {
				++i;
				++prev;
			}
		}
	}
	bool removeExampleHandler (Handle handlerPtr) {
		auto prev = eventHandlers.before_begin(); 
		for(auto i = eventHandlers.begin();i != eventHandlers.end();++i,++prev) { 
			if(*i == handlerPtr) {
				eventHandlers.erase_after(prev);
				return true;
			}
		}
 		return false;
	}
	void removeAllExampleHandlers () {
		eventHandlers.clear();
	}
}; //_//

#define __EVENTEMITTER_PROVIDER_DEFERRED(frontname, name) //^//
template<typename... Rest>
class ExampleDeferredEventEmitterTpl : public ExampleEventEmitterTpl<Rest...>, public virtual EE::DeferredBase {
public:
	ExampleDeferredEventEmitterTpl() {
		DeferredBase::removeHandlers.emplace_front([=] {
			this->removeAllExampleHandlers();
		});
	}

	template<typename... Args> inline void emitExample (Args&&... fargs) {
		triggerExample(fargs...);
	}
	template<typename... Args> void triggerExampleByRef (Args&&... fargs) {
		runDeferred(
			std::bind([=](Args... as) {
			__EVENTEMITTER_GCC_WORKAROUND ExampleEventEmitterTpl<Rest...>::triggerExample(as...);
			}, EE::forward_as_ref<Args>(fargs)...));
	}
	template<typename... Args> void triggerExample (Args... fargs) {
		runDeferred(
			std::bind([=](Args... as) {
			__EVENTEMITTER_GCC_WORKAROUND ExampleEventEmitterTpl<Rest...>::triggerExample(as...);
			}, fargs...));
	}	
}; //_//

#ifndef EVENTEMITTER_DISABLE_THREADING

#define __EVENTEMITTER_PROVIDER_THREADED(frontname, name) //^//
template<typename... Rest>
class ExampleThreadedEventEmitterTpl : public ExampleEventEmitterTpl<Rest...>, public virtual EE::DeferredBase { 
  std::condition_variable condition;
	std::mutex m;

	typedef typename ExampleEventEmitterTpl<Rest...>::Handler Handler;
	typedef typename ExampleEventEmitterTpl<Rest...>::HandlerPtr HandlerPtr;
	typedef typename ExampleEventEmitterTpl<Rest...>::Handle Handle;
	
public:
	ExampleThreadedEventEmitterTpl() {
	}	
	bool waitExample (std::chrono::milliseconds duration = std::chrono::milliseconds::max()) {
		waitExample([=](Rest...) {
		}, duration);
 	}
 	bool waitExample (Handler handler, std::chrono::milliseconds duration = std::chrono::milliseconds::max()) {
		std::shared_ptr<std::atomic<bool>> finished = std::make_shared<std::atomic<bool>>();
		std::unique_lock<std::mutex> lk(m);
		Handle ptr = onceExample(
			EE::wrapLambdaWithCallback(handler, [=]() {
				finished->store(true);
				this->condition.notify_all();
		}));
		
		if(duration == std::chrono::milliseconds::max()) {
			condition.wait(lk);
		} 
		else {
			condition.wait_for(lk, duration, [=]() {
				return finished->load();
			});
		}
		bool gotFinished = finished->load();
		if(!gotFinished) {
			ExampleEventEmitterTpl<Rest...>::removeExampleHandler(ptr);
		}
		return gotFinished;
 	}
	void asyncWaitExample(Handler handler, std::chrono::milliseconds duration, const std::function<void()>& asyncTimeout) {
		auto async = std::async(std::launch::async, [=]() {
			if(!waitExample(handler, duration))
				asyncTimeout();
		});
	}
	
	Handle onExample (Handler handler) {
		__EVENTEMITTER_LOCK_GUARD(mutex);
		return ExampleEventEmitterTpl<Rest...>::onExample(handler);
	}
	Handle onceExample (Handler&& handler) {
		__EVENTEMITTER_LOCK_GUARD(mutex);
		return ExampleEventEmitterTpl<Rest...>::onceExample(handler);
	}
	Handle asyncOnExample (Handler handler) {
		return onExample(EE::wrapLambdaInAsync(handler));
	}
	Handle asyncOnceExample (Handler handler) {
		return onceExample(EE::wrapLambdaInAsync(handler));
	}
	auto futureOnceExample() -> decltype(std::future<std::tuple<Rest...>>()) {
		typedef std::tuple<Rest...> TupleEventType;
		auto promise = std::make_shared<std::promise<TupleEventType>>();
		auto future = promise->get_future();
		condition.notify_all();
		onceExample(EE::getLambdaForFuture(promise));
		return future;
	}
	template<typename... Args> inline void emitExample (Args&&... fargs) {
		triggerExample(fargs...);
	}
	template<typename... Args> void triggerExample (Args&&... fargs) { 
		__EVENTEMITTER_LOCK_GUARD(mutex);
		ExampleEventEmitterTpl<Rest...>::triggerExample(fargs...);
		condition.notify_all();
	}
	template<typename... Args> void deferExampleByRef (Args&&... fargs) { 
		runDeferred(
 			std::bind([=](Args... as) {
 			__EVENTEMITTER_GCC_WORKAROUND ExampleEventEmitterTpl<Rest...>::triggerExample(as...);
 			},
			EE::forward_as_ref<Args>(fargs)...			
			//fargs...
			));
	}
	template<typename... Args> void deferExample (Args... fargs) { 
		runDeferred(
 			std::bind([=](Args... as) {
 			__EVENTEMITTER_GCC_WORKAROUND ExampleEventEmitterTpl<Rest...>::triggerExample(as...);
 			},
			fargs...			
			//fargs...
			));
	}
}; //_//

#endif // EVENTEMITTER_DISABLE_THREADING

#define EventDispatcherBase ExampleEventEmitter //#//

 #define __EVENTEMITTER_DISPATCHER(frontname, name) //^//
template<template<typename...> class EventDispatcherBase, typename T, typename... Rest>
class ExampleEventDispatcherTpl : public EventDispatcherBase<T, Rest...> {
 	using HandlerPtr =  typename EventDispatcherBase<Rest...>::HandlerPtr;
	using Handler = typename EventDispatcherBase<Rest...>::Handler;
	using Handle = typename EventDispatcherBase<Rest...>::Handle;
	std::multimap<T, HandlerPtr> map;
	bool eraseLast = false;
public:
	ExampleEventDispatcherTpl() {
		ExampleEventEmitter<T, Rest...>::onExample([&](T eventName, Rest... fargs) {
 			auto ret = map.equal_range(eventName);
 			for(auto it = ret.first;it != ret.second;) {
 				(it->second)(fargs...);
				if(eraseLast) {
					it = map.erase(it);
					eraseLast = false;
				}
				else {
					++it;
				}
 			}
		});
	}
	bool hasExampleHandlers(T eventName) {
		return map.find(eventName) != map.end();
	}
	int countExampleHandlers(T eventName) {
		int count = 0;
		auto ret = map.equal_range(eventName);
		for(auto it = ret.first;it != ret.second;++it)
			count++;
		return count;
	}
	
 	Handle onExample (T eventName, Handler handler) {
		return map.insert(std::pair<T, Handler>(eventName,  handler))->second;
 	}
 	Handle onceExample (T eventName, Handler handler) {
		return map.insert(std::pair<T, Handler>(eventName,  
			EE::wrapLambdaWithCallback(handler, [&] {
				eraseLast = true;
			})))->second;
 	}
 	bool removeExampleHandler (T eventName, Handle handler) {
		auto ret = map.equal_range(eventName);
		for(auto it = ret.first;it != ret.second;++it) {
			if(it->second == handler) {
				it = map.erase(it);
				return true;
			}
		}
		return false;
	}
	void removeAllExampleHandlers (T eventName) {
		auto ret = map.equal_range(eventName);
		for(auto it = ret.first;it != ret.second;) {		
			it = map.erase(it);
		}
	}
 }; //_//


#if 0 //#//

#define DefineEventEmitterAs(name, className, ...) \
__EVENTEMITTER_PROVIDER(name, name) \
typedef __EVENTEMITTER_CONCAT(name, EventEmitterTpl)<__VA_ARGS__> className;

#define DefineEventEmitter(name, ...) DefineEventEmitterAs(name, __EVENTEMITTER_CONCAT(name, EventEmitter), __VA_ARGS__);

#define DefineDeferredEventEmitterAs(name, className, ...) \
__EVENTEMITTER_PROVIDER(name,name) \
__EVENTEMITTER_PROVIDER_DEFERRED(name,name) \
typedef __EVENTEMITTER_CONCAT(name, DeferredEventEmitterTpl)<__VA_ARGS__> className;

#define DefineDeferredEventEmitter(name, ...) DefineDeferredEventEmitterAs(name, __EVENTEMITTER_CONCAT(name, DeferredEventEmitter), __VA_ARGS__)

#define DefineThreadedEventEmitterAs(name, className, ...) \
__EVENTEMITTER_PROVIDER(name,name) \
__EVENTEMITTER_PROVIDER_THREADED(name,name) \
typedef __EVENTEMITTER_CONCAT(name, ThreadedEventEmitterTpl)<__VA_ARGS__> className;

#define DefineThreadedEventEmitter(name, ...) __EVENTEMITTER_CONCAT(name, ThreadedEventEmitter), __VA_ARGS__)

__EVENTEMITTER_PROVIDER(/**/,/**/)
template<typename... Rest> class EventEmitter : public EventEmitterTpl<Rest...> {};

__EVENTEMITTER_PROVIDER_DEFERRED(/**/,/**/)

template<typename... Rest> class DeferredEventEmitter : public DeferredEventEmitterTpl<Rest...> {};

#ifndef EVENTEMITTER_DISABLE_THREADING
__EVENTEMITTER_PROVIDER_THREADED(/**/,/**/)
#endif

#endif //#//


#endif // __EVENTEMITTER_SANE_HPP

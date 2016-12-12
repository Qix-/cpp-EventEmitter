#ifndef __EVENTEMITTER_HPP
#define __EVENTEMITTER_HPP

#ifdef __EVENTEMITTER_TESTING
#undef __EVENTEMITTER_PROVIDER
#undef __EVENTEMITTER_PROVIDER_THREADED
#undef __EVENTEMITTER_PROVIDER_DEFERRED
#endif

#include <functional>
#include <forward_list>
#include <set>
#include <unordered_set>
#include <memory>
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
		// TODO: disable mutex when EVENTEMITTER_DISABLE_THREADING
	protected: 
		typedef std::function<void ()> DeferredHandler;
		std::forward_list<DeferredHandler> removeHandlers;
		std::unique_ptr<std::deque<DeferredHandler>> deferredQueue;
		__EVENTEMITTER_MUTEX_DECLARE(mutex);
		inline void assureContainer() {
			if(!deferredQueue)
				deferredQueue.reset(new std::deque<DeferredHandler>());
		}
	protected:
		void runDeferred(DeferredHandler f) {
			__EVENTEMITTER_LOCK_GUARD(mutex);
			assureContainer();
			deferredQueue->emplace_back(f);
		}
	public:
		void removeAllHandlers() {
			for(auto& handler : removeHandlers) {
				handler();
			}
		}

		void clearDeferred() {
			__EVENTEMITTER_LOCK_GUARD(mutex);
			assureContainer();
			deferredQueue->clear();
		}
		bool runDeferred() {
			__EVENTEMITTER_LOCK_GUARD(mutex);
			assureContainer();
			if(deferredQueue->size()) {
				((deferredQueue->front()))();
				deferredQueue->pop_front();
			}
			return !deferredQueue->empty();
		}
		void runAllDeferred() {
			while(runDeferred());
		}
	};
	
	// reference_wrapper needs to be use instead of std::reference_wrapper
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

	template <typename T>
	size_t getAddress(const std::function<T>& lambda) {
		size_t out;
		memcpy(&out, ((char*)&lambda)+4, sizeof(out));
		return out;
  }

template<typename... Args>	
inline decltype(auto) wrapLambdaWithCallback2(const std::function<void(Args...)>& f, std::function<void()>&& afterCb) {
	return [=,afterCb=std::move(afterCb)](Args&&... args) {
		f(args...);
		afterCb();
	};
}

template<typename... Args>	
inline decltype(auto) wrapLambdaWithCallback2(std::function<void(Args...)>&& f, std::function<void()>&& afterCb) {
	return [f=std::move(f),afterCb=std::move(afterCb)](Args&&... args) {
		f(args...);
		afterCb();
	};
}
	
// template<class Func, class Func2, class ...Args>
// inline decltype(auto) funcLambda(Func && func, Args && ...args, Func2&& func2)
// {   //The error is caused by the lambda below:
//     auto tpl = make_tuple(std::forward<Args>(args)...);
// 
//     return [func, tpl = move(tpl)]() {
//         apply(func, tpl);
// 				func2();
//     };
// }

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

#define __EVENTEMITTER_PROVIDER(frontname, name)  \
template<typename... Rest> \
class __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl) { \
public: \
	typedef std::function<void(Rest...)> Handler; \
	  \
	using Handle = handle_id_type; \
	using HandlerTuple = std::tuple<Handle, Handler>; \
	struct HandlerPtr : public HandlerTuple { \
		HandlerPtr(Handler handler) : HandlerTuple(__handle_counter++, std::move(handler)) { \
		} \
		bool operator==(const HandlerPtr& other) { \
			return std::get<0>(*this) == std::get<0>(other); \
		} \
		bool operator==(Handle other) { \
			return std::get<0>(*this) == other; \
		} \
 \
		template<typename... Args> inline void operator() (Args&&... fargs) { \
			std::get<1>(*this)(fargs...); \
		} \
		 \
		operator Handler() const { return std::get<1>(*this); } \
		operator Handler&() { return std::get<1>(*this); } \
		operator Handler&&() { return std::get<1>(*this); } \
		operator Handle() const { return std::get<0>(*this); } \
	}; \
 \
private: \
 	struct EventHandlersSet : public __EVENTEMITTER_CONTAINER { \
		EventHandlersSet() : eraseLast(false) { \
		} \
 		bool eraseLast : 8; \
 	}; \
	std::unique_ptr<EventHandlersSet> eventHandlers; \
public: \
	Handle __EVENTEMITTER_CONCAT(on,name) (Handler&& handler) { \
	  if(!eventHandlers) \
			eventHandlers.reset(new EventHandlersSet); \
		eventHandlers->emplace_front(handler); \
		return eventHandlers->front(); \
	} \
	Handle __EVENTEMITTER_CONCAT(once,name) (const Handler& handler) { \
	  if(!eventHandlers) \
			eventHandlers.reset(new EventHandlersSet); \
		eventHandlers->emplace_front( EE::wrapLambdaWithCallback2(handler, [=]() { \
			eventHandlers->eraseLast = true; \
		})); \
		return eventHandlers->front(); \
	} \
	Handle __EVENTEMITTER_CONCAT(once,name) (Handler&& handler) { \
	  if(!eventHandlers) \
			eventHandlers.reset(new EventHandlersSet); \
		eventHandlers->emplace_front( EE::wrapLambdaWithCallback2(std::move(handler), [=]() { \
			eventHandlers->eraseLast = true; \
		})); \
		return eventHandlers->front(); \
	} \
	bool __EVENTEMITTER_CONCAT(has,__EVENTEMITTER_CONCAT(name, Handlers))() { \
		return eventHandlers?!eventHandlers->empty():false; \
	} \
	int __EVENTEMITTER_CONCAT(count,__EVENTEMITTER_CONCAT(name, Handlers))() { \
		if(!eventHandlers) \
			return 0; \
		int count = 0; \
		for(auto& i:eventHandlers) count++; \
		return count; \
	} \
	template<typename... Args> inline void __EVENTEMITTER_CONCAT(emit,name) (Args&&... fargs) { \
		__EVENTEMITTER_CONCAT(trigger,name)(fargs...); \
	} \
	template<typename... Args> inline void __EVENTEMITTER_CONCAT(trigger,name) (Args&&... fargs) { \
	  if(!eventHandlers) \
			return; \
		auto prev = eventHandlers->before_begin();  \
	  for(auto i = eventHandlers->begin();i != eventHandlers->end();) { \
			 \
			(*i)(fargs...); \
			if(eventHandlers->eraseLast) { \
				i = eventHandlers->erase_after(prev); \
				eventHandlers->eraseLast = false; \
			} \
			else { \
				++i; \
				++prev; \
			} \
		} \
	} \
	bool __EVENTEMITTER_CONCAT(remove,__EVENTEMITTER_CONCAT(name, Handler)) (Handle handlerPtr) { \
 	  if(eventHandlers) { 			 \
 			auto prev = eventHandlers->before_begin();  \
			for(auto i = eventHandlers->begin();i != eventHandlers->end();++i,++prev) {  \
				if(*i == handlerPtr) { \
					eventHandlers->erase_after(prev); \
					return true; \
				} \
			} \
 		} \
 		return false; \
	} \
	void __EVENTEMITTER_CONCAT(removeAll,__EVENTEMITTER_CONCAT(name, Handlers)) () { \
		if(eventHandlers) \
			eventHandlers->clear(); \
	} \
};  

#define __EVENTEMITTER_PROVIDER_DEFERRED(frontname, name)  \
template<typename... Rest> \
class __EVENTEMITTER_CONCAT(frontname,DeferredEventEmitterTpl) : public __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>, public virtual EE::DeferredBase { \
public: \
	__EVENTEMITTER_CONCAT(frontname,DeferredEventEmitterTpl)() { \
		DeferredBase::removeHandlers.emplace_front([=] { \
			this->__EVENTEMITTER_CONCAT(removeAll,__EVENTEMITTER_CONCAT(name, Handlers))(); \
		}); \
	} \
 \
	template<typename... Args> inline void __EVENTEMITTER_CONCAT(emit,name) (Args&&... fargs) { \
		__EVENTEMITTER_CONCAT(trigger,name)(fargs...); \
	} \
	template<typename... Args> void __EVENTEMITTER_CONCAT(trigger,__EVENTEMITTER_CONCAT(name, ByRef)) (Args&&... fargs) { \
		runDeferred( \
			std::bind([=](Args... as) { \
			__EVENTEMITTER_GCC_WORKAROUND __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(trigger,name)(as...); \
			}, EE::forward_as_ref<Args>(fargs)...)); \
	} \
	template<typename... Args> void __EVENTEMITTER_CONCAT(trigger,name) (Args... fargs) { \
		runDeferred( \
			std::bind([=](Args... as) { \
			__EVENTEMITTER_GCC_WORKAROUND __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(trigger,name)(as...); \
			}, fargs...)); \
	}	 \
};  

#ifndef EVENTEMITTER_DISABLE_THREADING

#define __EVENTEMITTER_PROVIDER_THREADED(frontname, name)  \
template<typename... Rest> \
class __EVENTEMITTER_CONCAT(frontname,ThreadedEventEmitterTpl) : public __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>, public virtual EE::DeferredBase {  \
  std::condition_variable condition; \
	std::mutex m; \
 \
	typedef typename __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::Handler Handler; \
	typedef typename __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::HandlerPtr HandlerPtr; \
	typedef typename __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::Handle Handle; \
	 \
public: \
	__EVENTEMITTER_CONCAT(frontname,ThreadedEventEmitterTpl)() { \
	}	 \
	bool __EVENTEMITTER_CONCAT(wait,name) (std::chrono::milliseconds duration = std::chrono::milliseconds::max()) { \
		__EVENTEMITTER_CONCAT(wait,name)([=](Rest...) { \
		}, duration); \
 	} \
 	bool __EVENTEMITTER_CONCAT(wait,name) (Handler handler, std::chrono::milliseconds duration = std::chrono::milliseconds::max()) { \
		std::shared_ptr<std::atomic<bool>> finished = std::make_shared<std::atomic<bool>>(); \
		std::unique_lock<std::mutex> lk(m); \
		Handle ptr = __EVENTEMITTER_CONCAT(once,name)( \
			EE::wrapLambdaWithCallback2(handler, [=]() { \
				finished->store(true); \
				this->condition.notify_all(); \
		})); \
		 \
		if(duration == std::chrono::milliseconds::max()) { \
			condition.wait(lk); \
		}  \
		else { \
			condition.wait_for(lk, duration, [=]() { \
				return finished->load(); \
			}); \
		} \
		bool gotFinished = finished->load(); \
		if(!gotFinished) { \
			__EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(remove,__EVENTEMITTER_CONCAT(name, Handler))(ptr); \
		} \
		return gotFinished; \
 	} \
	void __EVENTEMITTER_CONCAT(asyncWait,name)(Handler handler, std::chrono::milliseconds duration, const std::function<void()>& asyncTimeout) { \
		auto async = std::async(std::launch::async, [=]() { \
			if(!__EVENTEMITTER_CONCAT(wait,name)(handler, duration)) \
				asyncTimeout(); \
		}); \
	} \
	 \
	Handle __EVENTEMITTER_CONCAT(on,name) (Handler handler) { \
		__EVENTEMITTER_LOCK_GUARD(mutex); \
		return __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(on,name)(handler); \
	} \
	Handle __EVENTEMITTER_CONCAT(once,name) (Handler&& handler) { \
		__EVENTEMITTER_LOCK_GUARD(mutex); \
		return __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(once,name)(handler); \
	} \
	Handle __EVENTEMITTER_CONCAT(asyncOn,name) (Handler handler) { \
		return __EVENTEMITTER_CONCAT(on,name)(EE::wrapLambdaInAsync(handler)); \
	} \
	Handle __EVENTEMITTER_CONCAT(asyncOnce,name) (Handler handler) { \
		return __EVENTEMITTER_CONCAT(once,name)(EE::wrapLambdaInAsync(handler)); \
	} \
	auto __EVENTEMITTER_CONCAT(futureOnce,name)() -> decltype(std::future<std::tuple<Rest...>>()) { \
		typedef std::tuple<Rest...> TupleEventType; \
		auto promise = std::make_shared<std::promise<TupleEventType>>(); \
		auto future = promise->get_future(); \
		condition.notify_all(); \
		__EVENTEMITTER_CONCAT(once,name)(EE::getLambdaForFuture(promise)); \
		return future; \
	} \
	template<typename... Args> inline void __EVENTEMITTER_CONCAT(emit,name) (Args&&... fargs) { \
		__EVENTEMITTER_CONCAT(trigger,name)(fargs...); \
	} \
	template<typename... Args> void __EVENTEMITTER_CONCAT(trigger,name) (Args&&... fargs) {  \
		__EVENTEMITTER_LOCK_GUARD(mutex); \
		__EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(trigger,name)(fargs...); \
		condition.notify_all(); \
	} \
	template<typename... Args> void __EVENTEMITTER_CONCAT(defer,__EVENTEMITTER_CONCAT(name, ByRef)) (Args&&... fargs) {  \
		runDeferred( \
 			std::bind([=](Args... as) { \
 			__EVENTEMITTER_GCC_WORKAROUND __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(trigger,name)(as...); \
 			}, \
			EE::forward_as_ref<Args>(fargs)...			 \
			  \
			)); \
	} \
	template<typename... Args> void __EVENTEMITTER_CONCAT(defer,name) (Args... fargs) {  \
		runDeferred( \
 			std::bind([=](Args... as) { \
 			__EVENTEMITTER_GCC_WORKAROUND __EVENTEMITTER_CONCAT(frontname,EventEmitterTpl)<Rest...>::__EVENTEMITTER_CONCAT(trigger,name)(as...); \
 			}, \
			fargs...			 \
			  \
			)); \
	} \
};  

#endif // EVENTEMITTER_DISABLE_THREADING

#include <map>



 #define __EVENTEMITTER_DISPATCHER(frontname, name)  \
template<template<typename...> class EventDispatcherBase, typename T, typename... Rest> \
class __EVENTEMITTER_CONCAT(frontname,EventDispatcherTpl) : public EventDispatcherBase<T, Rest...> { \
 	typedef typename EventDispatcherBase<Rest...>::HandlerPtr HandlerPtr; \
	typedef typename EventDispatcherBase<Rest...>::Handler Handler; \
	typedef typename EventDispatcherBase<Rest...>::Handle Handle; \
	std::multimap<T, Handler> map; \
	bool eraseLast = false; \
public: \
	__EVENTEMITTER_CONCAT(frontname,EventDispatcherTpl)() { \
		__EVENTEMITTER_CONCAT(frontname,EventEmitter)<T, Rest...>::__EVENTEMITTER_CONCAT(on,name)([&](T eventName, Rest... fargs) { \
 			auto ret = map.equal_range(eventName); \
 			for(auto it = ret.first;it != ret.second;) { \
 				(it->second)(fargs...); \
				if(eraseLast) { \
					it = map.erase(it); \
					eraseLast = false; \
				} \
				else { \
					++it; \
				} \
 			} \
		}); \
	} \
	bool __EVENTEMITTER_CONCAT(has,__EVENTEMITTER_CONCAT(name, Handlers))(T eventName) { \
		return map.find(eventName) != map.end(); \
	} \
	int __EVENTEMITTER_CONCAT(count,__EVENTEMITTER_CONCAT(name, Handlers))(T eventName) { \
		int count = 0; \
		auto ret = map.equal_range(eventName); \
		for(auto it = ret.first;it != ret.second;++it) \
			count++; \
		return count; \
	} \
	 \
 	Handle __EVENTEMITTER_CONCAT(on,name) (T eventName, Handler handler) { \
		return EE::getAddress(map.insert(std::pair<T, Handler>(eventName,  handler))->second); \
 	} \
 	Handle __EVENTEMITTER_CONCAT(once,name) (T eventName, Handler handler) { \
		return EE::getAddress(map.insert(std::pair<T, Handler>(eventName,   \
			EE::wrapLambdaWithCallback2(handler, [&] { \
				eraseLast = true; \
			})))->second); \
 	} \
 	bool __EVENTEMITTER_CONCAT(remove,__EVENTEMITTER_CONCAT(name, Handler)) (T eventName, Handle handler) { \
		auto ret = map.equal_range(eventName); \
		for(auto it = ret.first;it != ret.second;++it) { \
			if(EE::getAddress(it->second) == handler) { \
				it = map.erase(it); \
				return true; \
			} \
		} \
		return false; \
	} \
	void __EVENTEMITTER_CONCAT(removeAll,__EVENTEMITTER_CONCAT(name, Handlers)) (T eventName) { \
		auto ret = map.equal_range(eventName); \
		for(auto it = ret.first;it != ret.second;) {		 \
			it = map.erase(it); \
		} \
	} \
 };  




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

__EVENTEMITTER_PROVIDER(,)
template<typename... Rest> class EventEmitter : public EventEmitterTpl<Rest...> {};

__EVENTEMITTER_PROVIDER_DEFERRED(,)

template<typename... Rest> class DeferredEventEmitter : public DeferredEventEmitterTpl<Rest...> {};

#ifndef EVENTEMITTER_DISABLE_THREADING
__EVENTEMITTER_PROVIDER_THREADED(,)
#endif




#endif // __EVENTEMITTER_HPP

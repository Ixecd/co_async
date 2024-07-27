# 基于coroutine的协程IO调度系统
**Base on**
- 基于编译器硬编码的关键字 `xxx_xxx`,以及`promise`,`task`,`awaiter`
- 基于C++11/14/17/20

**Promise**

- 协程的近亲是函数(而非线程),如果编译器在函数中看见类似于co_yield、co_return就会认为当前函数体是一个协程函数.其可以生成一个协程对象,比如: 定义一个协程函数`Task<T> fiber() {... co_return}`,其中T是存储的数据类型,实例化一个协程对象`auto t = fiber();`这时候本质上是生成一个Promise<T>对象,由其掌控着当前实例化的协程对象的生命周期,Promise这个结构体中除了如果协程要返回值的函数,其他都是具有硬编码的编译器函数.
- 这里只简单介绍Promise中的一些硬编码关键字
1. `auto get_return_object() { return std::coroutine_handle<Promise>::from_this(*this); }`表示生成当前协程实例的协程句柄,之后由这个协程句柄来唤醒`resume`
2. `T final_suspend() noexcept { ... }`表示当前协程对象执行完毕,最后一次挂起的时候执行的函数体,
3. `void unhandled_exception() noexcept { ... }`表示如果当前协程对象在执行期间如果发生了异常会跑到这里来执行,由这里来处理.
4. `auto yield_value(T const& t) { ... return Awaiter(); }`表示在协程函数中遇到`yield val;`会来执行这行代码,val会作为参数传递给这个函数,编译器要求必须要返回一个awaiter对象,表示将当前协程挂起,下一个要执行哪一个.如果是`std::suspend_always();`表示直接挂起,回到主线程,等待再一次`主动resume`.
- 也就是说Promise控制着当前协程对象的开始/结束/异常/返回值做什么.

**Awaiter**
- Awaiter表示等待者,Awaitable表示可等待的,这两者的关系类似于原始指针和operator->().
- Awaiter中有三个固定的硬编码关键字:
1. `bool await_read() const noexcept { return true/fase; }`当通过协程句柄(从Promise中的get_return_object获取)唤醒一个协程后,会进入Awaiter中这个函数执行,判断当前协程对象准备好了没有,如果准备好了就直接执行,如果没有准备好,就会执行下面这个函数.
2. `RetValue await_suspend(T para) { ... }`函数名的意思是挂起,但是否真正挂起要看这个函数是怎么写的.如果返回的是一个协程句柄,那么就会执行这个返回的协程句柄.
3. `RetValue await_resume() const { return ... }`表示当前协程对象执行完毕,父对象协程会调用这个函数来获取子协程对象执行的结果.之后才是调用Promise中的final_suspend.
- 可以这么理解,一个Awaiter就表示一个子协程对象.因为只有在协程函数中才会出现 co_await 这个关键字,其表示等待后面的Awaiter(子协程句柄)执行完毕.

**Task**
- Task表示协程任务,其核心就是保存一个协程句柄`std::coroutine_handle<promise_type> mCoroutine;`以及重载 co_await 使其返回一个Awaiter对象, 这里协程句柄的类型必须和Promse类型一致,因为T在Task中没用,在Promise中才有用,为了模板化使得Task可以返回任意类型的value,所以Task中的T和Promise中的T要对应.

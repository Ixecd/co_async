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
- 可以这么理解,由Awaiter来控制着子协程对象挂起的时候要干什么(最重要).co_await 后面可以跟 协程对象/Awaiter, 如果是Awaiter(依然会把父的协程句柄传递给await_suspend)那么并不会生成一个协程对象,而是由其做一些其他事情, 如果是 协程对象,就会走 Task中的 Awaiter 走重载.

**Task**
- Task表示协程任务,其核心就是保存一个协程句柄`std::coroutine_handle<promise_type> mCoroutine;`以及重载 co_await 使其返回一个Awaiter对象, 这里协程句柄的类型必须和Promse类型一致,因为T在Task中没用,在Promise中才有用,为了模板化使得Task可以返回任意类型的value,所以Task中的T和Promise中的T要对应.

**三者之间的联系**
- 编写带有返回类型为Task的函数体,函数体中有co_yield/co_return/co_await关键字.
- 使用 `auto t = fiber();` 生成一个协程对象,仅仅生成,并不执行.由于Task的类型是<T, Promise<T>>也就是说Task和Promise是始终绑定的,通过`std::coroutine_handle<promise_type>`;
- 要生成一个协程对象,首先要通过Promise中的`get_return_object`获取协程句柄,之后通过隐式类型转换,转换到Task的构造函数上,之后生成Task实例,这时候Task中的成员变量,绑定的就是std::coroutine_handle<promise_type>类型.
- 之后可以通过手动或者Loop将对应coroutine.promise().resume(),由编译器(coroutine头文件),就会跳转到对应的协程函数上执行,也就是说resume()后相当于Task在run.
- 如果在协程函数上出现`co_await fiber2()`,首先和前面一样生成一个协程对象fiber2(),之后调用Task中重载过的co_await, 里面返回的是Awaiter(mCor).如果co_awiat 后面不是协程对象,而是一个Awaiter那么就走后面的Awaiter,而非Task中的Awaiter
- 最后登场的是Awaiter, 这时候编译器开始干活了,`return Awaiter(std::coroutine_handle<promis_type> c)`这里面的c是当前协程对象的协程句柄,其作为参数传递给Awaiter,本质上是传递给Awaiter具体实现中的`await_suspend(...)`,当然是先判断Awaiter中的`await_ready()`,如果是true,直接调用`await_resume()`, false,调用`await_suspend(...)`
- 这里的Awaiter给到的是子协程对象的,由于Task中的Awaiter中有成员变量`std::coroutine_handle<promise_type> mCor`,这里的promise_type非常重要,记录了子协程对象的promise类型,可以通过mCor获得Promise对象.之后就可以操作Promise对象.Awaiter中记录了调用者是谁,后面`返回子协程的协程句柄`,之后由编译器(coroutine头文件)`自动resume`子协程.

**特别注意**
1. 对于Hook协程函数而言,其函数体中的co_await是被重载了但是没有用到,co_await 后面必须跟 Awaitalbe/Awaiter, 如果后面跟的是一个协程对象就会用到Task中重载的.
- 下面是详细解释

    现在我们要Hook系统中的sleep_until/sleep_for实现异步, 因为我们要手动控制Promise的生命周期,也就是协程对象的生命周期,详见`timerLoop.hpp`,在sleep_until/sleep_for的时候co_await后面接的是自己实现的SleepAwaiter, 这时候就不会调用Task中的Awaiter,而是直接走SleepAwaiter.这也就解释了为什么SleepAwaiter中的await_suspend函数中参数是`std::coroutine_handle<SleepPromise>`类型, 因为是我们当前Hook的sleep_for/until函数调用的co_await,这俩函数的返回类型为Task<void, Sleeppromise>,这里 `sleep_for/until` 是 `父`,而 `SleepAwaiter` 是 `子`, co_await是父传子,会将自己的协程句柄传递给SleepAwaiter中的await_suspend函数的参数, 而 SleepAwaiter不是协程,不需要记录协程句柄信息.
    如果co_await 后面跟的是一个协程对象那么就会走Task中的Awaiter,这个Awaiter会记录自己的协程句柄.

